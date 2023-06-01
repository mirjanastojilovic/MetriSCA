#include "metrisca/utils/template_attack.hpp"

#include "metrisca/utils/numerics.hpp"
#include "metrisca/core/parallel.hpp"
#include "metrisca/core/logger.hpp"

#include <tuple>
#include <mutex>
#include <unordered_set>

using namespace metrisca;

#define DOUBLE_NAN (std::numeric_limits<double>::signaling_NaN())
#define DOUBLE_INFINITY (std::numeric_limits<double>::infinity())
#define IS_VALID_DOUBLE(value) (std::isfinite(value) && !std::isnan(value))

struct ProfiledResult
{
    std::vector<std::vector<size_t>> poi; // the points of interest for each key byte
    std::vector<double> bias; // the bias for each sample
};

static Result<ProfiledResult, Error> profile(
    std::shared_ptr<TraceDataset> profilingDataset,
    std::shared_ptr<PowerModelPlugin> powerModel,
    size_t sampleStart,
    size_t sampleEnd,
    size_t sampleFilterCount /*<! Number of poi */   
) {
    // Conveniance aliases
    const size_t sampleCount = sampleEnd - sampleStart;
    const size_t byteCount = profilingDataset->GetHeader().KeySize;

    // Allocate the result
    ProfiledResult result;
    result.poi.resize(byteCount);

    // Modelize the traces
    METRISCA_INFO("Modelizing traces");
    std::vector<Matrix<int32_t>> models(byteCount); // for each key byte
    powerModel->SetDataset(profilingDataset);

    for (size_t byteIdx = 0; byteIdx != byteCount; ++byteIdx) {
        powerModel->SetByteIndex(byteIdx);
        auto result = powerModel->Model();
        if (result.IsError()) {
            return result.Error();
        }
        models[byteIdx] = std::move(result.Value());
    }

    // Second pass to find the points of interest
    METRISCA_INFO("Finding points of interest");
    metrisca::parallel_for(0, byteCount, [&](size_t byteIdx) {
        // Compute the noise vector for each samples
        std::vector<double> correlation(sampleCount, 0.0);

        for (size_t sampleIdx = 0; sampleIdx != sampleCount; ++sampleIdx) {
            double xi = 0.0;
            double xi2 = 0.0;
            double yi = 0.0;
            double yi2 = 0.0; 
            double xiyi = 0.0;

            for (size_t traceIdx = 0; traceIdx != profilingDataset->GetHeader().NumberOfTraces; ++traceIdx) {
                double x = profilingDataset->GetSample(sampleIdx)[traceIdx];
                double y = models[byteIdx](profilingDataset->GetKey(traceIdx)[byteIdx], traceIdx);

                xi += x;
                xi2 += x * x;
                yi += y;
                yi2 += y * y;
                xiyi += x * y;
            }

            size_t N = profilingDataset->GetHeader().NumberOfTraces;

            correlation[sampleIdx] = (N * xiyi - xi * yi) / std::sqrt((N * xi2 - xi * xi) * (N * yi2 - yi * yi));
        }
            
        // Find the points of interest (points for which the absolute of the correlation is maximized)
        std::vector<size_t> reorderBuffer;
        reorderBuffer.reserve(sampleCount);
        for (size_t i = 0; i != sampleCount; ++i) reorderBuffer.push_back(i);
        std::sort(reorderBuffer.begin(), reorderBuffer.end(), [&](size_t a, size_t b) {
            if (std::isnan(correlation[a])) return false;
            else if (std::isnan(correlation[b])) return true;
            else return correlation[a] > correlation[b];
        });

        // Keep only the first sampleFilterCount points
        result.poi[byteIdx].resize(sampleFilterCount);
        std::copy(reorderBuffer.begin(), reorderBuffer.begin() + sampleFilterCount, result.poi[byteIdx].begin());
    });

    // Compute the bias
    METRISCA_INFO("Computing bias");
    result.bias.resize(sampleCount, 0.0);
    metrisca::parallel_for(0, sampleCount, [&](size_t sampleIdx) {
        // Find if current sample in poi, is so compute bias
        double bias = 0.0;

        for (size_t byteIdx = 0; byteIdx != byteCount; ++byteIdx) {
            double partialBias = 0.0;

            if (std::find(result.poi[byteIdx].begin(), result.poi[byteIdx].end(), sampleIdx) != result.poi[byteIdx].end()) {
                for (size_t traceIdx = 0; traceIdx != profilingDataset->GetHeader().NumberOfTraces; ++traceIdx) {
                    double u = profilingDataset->GetSample(sampleIdx + sampleStart)[traceIdx];
                    double v = models[byteIdx](profilingDataset->GetKey(traceIdx)[byteIdx], traceIdx);

                    partialBias += u - v;
                }
            }

            partialBias /= profilingDataset->GetHeader().NumberOfTraces;
            bias += partialBias;
        }

        result.bias[sampleIdx] = bias / byteCount;
    });

    // Return the result
    return result;
}

namespace metrisca
{
    Result<TemplateAttackResult, Error> runTemplateAttack(
        std::shared_ptr<TraceDataset> profilingDataset,
        std::shared_ptr<TraceDataset> attackDataset,
        std::shared_ptr<PowerModelPlugin> powerModel,
        size_t traceCount,
        size_t traceStep,
        size_t sampleStart,
        size_t sampleEnd,
        size_t sampleFilterCount /*<! Number of poi */        
    ) {
        // Conveniance aliases
        const size_t sampleCount = sampleEnd - sampleStart;
        const size_t byteCount = profilingDataset->GetHeader().KeySize;

        // Sanity checks
        METRISCA_ASSERT(sampleStart < sampleEnd);
        METRISCA_ASSERT(sampleEnd <= profilingDataset->GetHeader().NumberOfSamples);
        METRISCA_ASSERT(profilingDataset->GetHeader().NumberOfSamples == attackDataset->GetHeader().NumberOfSamples);
        METRISCA_ASSERT(profilingDataset->GetHeader().KeySize == attackDataset->GetHeader().KeySize);   
        METRISCA_ASSERT(profilingDataset->GetHeader().KeyMode == attackDataset->GetHeader().KeyMode);

        // Beginning of the profiling phase
        METRISCA_INFO("Starting profiling phase");
        ProfiledResult profiledResult;

        {
            auto profilingResult = profile(profilingDataset, powerModel, sampleStart, sampleEnd, sampleFilterCount);
            if (profilingResult.IsError()) {
                return profilingResult.Error();
            }
            profiledResult = std::move(profilingResult.Value());
        }

        // Beginning of the attack phase
        METRISCA_INFO("Starting attack phase");
        std::vector<Matrix<int32_t>> models(byteCount); // for each key byte
        powerModel->SetDataset(attackDataset);

        for (size_t byteIdx = 0; byteIdx != byteCount; ++byteIdx) {
            powerModel->SetByteIndex(byteIdx);
            auto result = powerModel->Model();
            if (result.IsError()) {
                return result.Error();
            }
            models[byteIdx] = std::move(result.Value());
        }

        // Determines steps size
        std::vector<size_t> steps = (traceStep > 0) ?
            numerics::ARange(traceStep, traceCount + 1, traceStep) :
            std::vector<size_t>{ traceCount };

        // Result of the template attack
        TemplateAttackResult result;
        result.resize(steps.size());
        for(auto& r : result) r.resize(byteCount);

        metrisca::parallel_for(0, steps.size() * byteCount, [&](size_t idx) {
            // Retrieve the byte index and the step index
            size_t byteIdx = idx % byteCount;
            size_t stepIdx = idx / byteCount;

            // For each key hypothesis
            for (size_t key = 0; key != 256; key++) {
                // Compute the noise vector for each samples
                size_t const sampleCount = profiledResult.poi[byteIdx].size();
                std::vector<double> noise(sampleCount, 0.0);

                for (size_t sampleIdx = 0; sampleIdx != sampleCount; ++sampleIdx) {
                    for (size_t traceIdx = 0; traceIdx != attackDataset->GetHeader().NumberOfTraces; ++traceIdx) {
                        size_t realSampleIdx = profiledResult.poi[byteIdx][sampleIdx];
                        double u = attackDataset->GetSample(realSampleIdx)[traceIdx] - profiledResult.bias[realSampleIdx - sampleStart];
                        double v = models[byteIdx](key, traceIdx);

                        noise[sampleIdx] += u - v;
                    }
                }

                // Compute covariance matrice
                Matrix<double> covarianceMatrix(sampleCount, sampleCount);
                for (size_t row = 0; row != sampleCount; ++row) {
                    for (size_t col = 0; col != sampleCount; ++col) {
                        double ui = 0.0, vi = 0.0, uivi = 0.0;

                        for (size_t traceIdx = 0; traceIdx != attackDataset->GetHeader().NumberOfTraces; ++traceIdx) {
                            size_t realSampleIdxRow = profiledResult.poi[byteIdx][row];
                            size_t realSampleIdxCol = profiledResult.poi[byteIdx][col];

                            double u = attackDataset->GetSample(realSampleIdxRow)[traceIdx] - profiledResult.bias[realSampleIdxRow - sampleStart] - models[byteIdx](key, traceIdx);
                            double v = attackDataset->GetSample(realSampleIdxCol)[traceIdx] - profiledResult.bias[realSampleIdxCol - sampleStart] - models[byteIdx](key, traceIdx);

                            ui += u;
                            vi += v;
                            uivi += u * v;
                        }

                        covarianceMatrix(row, col) = (uivi - ui * vi / attackDataset->GetHeader().NumberOfTraces) / (attackDataset->GetHeader().NumberOfTraces - 1);
                    }
                }

                // Compute the inverse of the covariance matrix
                Matrix<double> inverseCovarianceMatrix = covarianceMatrix.CholeskyInverse();

                // Compute the log likelyhood
                double logLikelyhood = 0.0;

                for (size_t i = 0; i != sampleCount; ++i) {
                    for (size_t j = 0; j != sampleCount; ++j) {
                        logLikelyhood += noise[i] * inverseCovarianceMatrix(i, j) * noise[j];
                    }
                }

                result[stepIdx][byteIdx][key] = -0.5 * logLikelyhood;
            }
        });

        // Return the result
        return result;
    }
}

