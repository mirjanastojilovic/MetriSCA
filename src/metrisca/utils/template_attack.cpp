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

struct ProfileResult {
    std::vector<std::array<double, 256>> means;
    std::vector<size_t> selectedSample;
};

static Result<std::tuple<ProfileResult, LinearCorrectionFactor>, Error> profile(
    std::shared_ptr<TraceDataset> profilingDataset, 
    std::shared_ptr<PowerModelPlugin> powerModel,
    size_t sampleStart,
    size_t sampleEnd,
    size_t sampleFilterCount)
{
    // Conveniance variable
    size_t numberOfTraces = profilingDataset->GetHeader().NumberOfTraces;
    size_t numberOfBytes = profilingDataset->GetKey(0).size();

    // Use the profiling dataset to compute the mean
    powerModel->SetDataset(profilingDataset);

    // Allocate the result of the operation (for each byte and each samples)
    ProfileResult result; 
    result.means.resize(sampleEnd - sampleStart);
    for (auto& x : result.means) x.fill(0.0);

    result.selectedSample.reserve(sampleFilterCount);

    // For each bytes, perform the profiling
    std::mutex globalLock;
    std::atomic_bool isError = false;
    Error error = (Error) 0;

    // Compute models and correction factor for each byte
    std::vector<Matrix<int32_t>> modelsPerByte;
    modelsPerByte.resize(numberOfBytes);
    std::atomic<double> uiSum = 0.0, viSum = 0.0, ui2Sum = 0.0, vi2Sum = 0.0, uiviSum = 0.0;

    metrisca::parallel_for(0, numberOfBytes, [&](size_t byteIdx)
    {
        // Abort if error
        if (isError) {
            return;
        }

        // Compute the modelization matrix for the current byte
        {
            // Lock the global lock
            std::lock_guard<std::mutex> lock(globalLock);
            powerModel->SetByteIndex(byteIdx);

            // Modelize
            auto modelizationResult = powerModel->Model();
            if (modelizationResult.IsError()) {
                METRISCA_ERROR("Error while modelizing the training dataset during profiling");
                isError = true;
                error = modelizationResult.Error();
                return;
            }

            modelsPerByte[byteIdx] = modelizationResult.Value();
        }

        double uiSum_ = 0.0, viSum_ = 0.0, ui2Sum_ = 0.0, vi2Sum_ = 0.0, uiviSum_ = 0.0;

        // Compute the above sums
        for (size_t sampleIdx = sampleStart; sampleIdx != sampleEnd; ++sampleIdx) {
            for (size_t traceIdx = 0; traceIdx != numberOfTraces; ++traceIdx) {
                double ui = profilingDataset->GetSample(sampleIdx)[traceIdx];
                double vi = modelsPerByte[byteIdx](profilingDataset->GetKey(traceIdx)[byteIdx], traceIdx);

                uiSum_ += ui;
                viSum_ += vi;
                ui2Sum_ += ui * ui;
                vi2Sum_ += vi * vi;
                uiviSum_ += ui * vi;
            }
        }

        // Accumulate the atomic variables
        // Notice that atomic<double> does not overload default arithmetic operators 
        // before C++20 (https://en.cppreference.com/w/cpp/atomic/atomic) as such we 
        // need to use compare_exchange_strong to perform the accumulation
        for (double v = uiSum; !uiSum.compare_exchange_strong(v, v + uiSum_, std::memory_order_seq_cst); v = uiSum);
        for (double v = viSum; !viSum.compare_exchange_strong(v, v + viSum_, std::memory_order_seq_cst); v = viSum);
        for (double v = ui2Sum; !ui2Sum.compare_exchange_strong(v, v + ui2Sum_, std::memory_order_seq_cst); v = ui2Sum);
        for (double v = vi2Sum; !vi2Sum.compare_exchange_strong(v, v + vi2Sum_, std::memory_order_seq_cst); v = vi2Sum);
        for (double v = uiviSum; !uiviSum.compare_exchange_strong(v, v + uiviSum_, std::memory_order_seq_cst); v = uiviSum);
    });

    // Compute the correction factor
    LinearCorrectionFactor linearCorrectionFactor;
    double N = (double) (numberOfBytes * (sampleEnd - sampleStart) * numberOfTraces);
    linearCorrectionFactor.alpha = (N * uiviSum - viSum) / (N * ui2Sum - uiSum * uiSum);
    linearCorrectionFactor.beta = (viSum - linearCorrectionFactor.alpha * uiSum) / N;
    std::atomic_bool uniqueWarning = false;

    // Begin the second phase of the profiling
    METRISCA_INFO("Begin of the profiling phase");
    metrisca::parallel_for(0, numberOfBytes, [&](size_t byteIdx)
    {
        // Abort if error
        if (isError) {
            return;
        }

        // Compute the modelization matrix for the current byte
        const Matrix<int32_t>& models = modelsPerByte[byteIdx];

        // Using our prior knowledge of the key, group
        // each traces by their "expected" output using the model
        // (only store indices of the traces)
        std::array<std::vector<size_t>, 256> groupedByExpectedResult;
        std::unordered_set<size_t> groupWithoutModel;

        for (size_t i = 0; i != numberOfTraces; ++i) {
            int32_t expectedResult = models(profilingDataset->GetKey(i)[byteIdx], i);

            // Ensures that the expected result is in the range [0, 255], otherwise error
            if (expectedResult < 0 || expectedResult >= 256) {
                METRISCA_ERROR("Currently only model producing values in the range [0, 255] are supported");
                isError = true;
                error = Error::UNSUPPORTED_OPERATION;
                return;
            }

            // Group the trace
            groupedByExpectedResult[expectedResult].push_back(i);
        }

        // Find all groups that are empty
        for (size_t groupIdx = 0; groupIdx != 256; ++groupIdx) {
            if (groupedByExpectedResult[groupIdx].empty()) {
                groupWithoutModel.insert(groupIdx);
            }
        }

        // Log a warning if some groups are empty
        if (!groupWithoutModel.empty() && uniqueWarning.exchange(true) == false) {
            std::stringstream ss;
            ss << "Groups corresponding with output values ";
            for (auto groupIdx : groupWithoutModel) {
                ss << groupIdx << " ";
            }
            ss << "are empty, this may lead to unexpected results";
            METRISCA_WARN(ss.str());
        }

        // For each group, and each sample, compute the mean
        std::vector<std::array<double, 256>> groupAverage;
        groupAverage.resize(sampleEnd - sampleStart);

        for (size_t sampleIdx = sampleStart; sampleIdx != sampleEnd; ++sampleIdx) {
            for (size_t groupIdx = 0; groupIdx != 256; groupIdx++) {
                double mean = 0.0;

                // Compute the mean for each sample
                for (size_t traceIdx : groupedByExpectedResult[groupIdx]) {
                    mean += linearCorrectionFactor(profilingDataset->GetSample(sampleIdx)[traceIdx]);
                }

                // If no such traces exists
                size_t matchingTraceCount = groupedByExpectedResult[groupIdx].size();
                groupAverage[sampleIdx - sampleStart][groupIdx] = (matchingTraceCount == 0) ?
                    DOUBLE_NAN :
                    mean / matchingTraceCount;
            }
        }

        // Accumulate the result
        {
            std::lock_guard<std::mutex> lock(globalLock);

            // Increment the scores of the result with the group average
            for (size_t sampleIdx = 0; sampleIdx != sampleEnd - sampleStart; ++sampleIdx) {
                for (size_t groupIdx = 0; groupIdx != 256; ++groupIdx) {
                    result.means[sampleIdx][groupIdx] += groupAverage[sampleIdx][groupIdx] / numberOfBytes;
                }
            }
        }
    });

    // Because of numerical stability issues and optimization problems
    // we do not compute the covariance on all samples, only on the most
    // useful ones. To achieve this goal we will sort all sample by there best
    // diff, and select sampleFilterCount of the best samples

    // Determine which sample is the most benefic to have
    std::vector<double> bestDiffs;
    bestDiffs.resize(sampleEnd - sampleStart, 0.0);

    for(size_t sampleIdx = 0; sampleIdx != sampleEnd - sampleStart; ++sampleIdx) {
        for (size_t i = 0; i != 256; ++i) {
            for (size_t j = 0; j != 256; ++j) {
                if (std::isnan(result.means[sampleIdx][i]) || std::isnan(result.means[sampleIdx][j]))
                    continue;
                    
                double diff = std::abs(result.means[sampleIdx][i] - result.means[sampleIdx][j]);

                if (diff > bestDiffs[sampleIdx]) {
                    bestDiffs[sampleIdx] = diff;
                }
            }
        }
    }

    // Find the best sample
    {
        std::vector<size_t> reorderBuffer;
        reorderBuffer.reserve(sampleEnd - sampleStart);
        for (size_t i = 0; i != sampleEnd - sampleStart; ++i) {
            reorderBuffer.push_back(i);
        }

        std::sort(reorderBuffer.begin(), reorderBuffer.end(), [&](size_t lhs, size_t rhs) {
            if (std::isnan(lhs)) return false;
            else if (std::isnan(rhs)) return true;
            return bestDiffs[lhs] > bestDiffs[rhs];
        });

        // Keep best sampleFilterCount samples
        for (size_t i = 0; i != sampleFilterCount; ++i) {
            if (i >= reorderBuffer.size()) {
                break;
            }
            result.selectedSample.push_back(reorderBuffer[i]);
        }
    }

    // Finally return the result of the profiling operation
    if (isError) {
        return error;
    }
    else {
        return std::make_tuple(result, linearCorrectionFactor);
    }
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
        // Conveniance variable
        size_t numberOfBytes = attackDataset->GetKey(0).size();
        
        // Sanity assertions
        METRISCA_ASSERT(sampleEnd - sampleStart <= profilingDataset->GetHeader().NumberOfSamples);
        METRISCA_ASSERT(profilingDataset->GetHeader().NumberOfSamples == attackDataset->GetHeader().NumberOfSamples);
        METRISCA_ASSERT(profilingDataset->GetKey(0).size() == attackDataset->GetKey(0).size());
        METRISCA_ASSERT(sampleEnd <= profilingDataset->GetHeader().NumberOfSamples);

        // Perform the profiling phase
        ProfileResult profiledResult;
        LinearCorrectionFactor linearCorrectionFactor;
        {
            auto profilingResultResult = profile(profilingDataset, powerModel, sampleStart, sampleEnd, sampleFilterCount);
            if (profilingResultResult.IsError()) {
                return profilingResultResult.Error();
            }
            profiledResult = std::get<0>(profilingResultResult.Value());
            linearCorrectionFactor = std::get<1>(profilingResultResult.Value());
        }

        // Perform the attack phase
        METRISCA_INFO("Begin of the attack phase");
        powerModel->SetDataset(attackDataset);

        // Compute the steps list
        std::vector<size_t> steps = (traceStep > 0) ?
            numerics::ARange(traceStep, traceCount + 1, traceStep) :
            std::vector<size_t>{ traceCount };

        // Allocate the result of the operation (for each step and each byte)
        TemplateAttackResult result;
        result.resize(steps.size());
        for (auto& stepResult : result) {
            stepResult.resize(numberOfBytes);
        }

        // For each step, each bytes, perform the attack
        powerModel->SetDataset(attackDataset);
        std::mutex globalLock;
        std::atomic_bool isError = false;
        Error error = (Error) 0;

        metrisca::parallel_for("Beginning of the attack phase ", 0, steps.size() * numberOfBytes, [&](size_t idx){
            // Abort if any error occurs
            if (isError) {
                return;
            }

            // Compute the step and byte index
            size_t stepIdx = idx / numberOfBytes;
            size_t byteIdx = idx % numberOfBytes;

            // Compute the modelization matrix for the current byte
            Matrix<int32_t> models;
            {
                // Lock the global lock
                std::lock_guard<std::mutex> lock(globalLock);
                powerModel->SetByteIndex(byteIdx);

                // Modelize
                auto modelizationResult = powerModel->Model();
                if (modelizationResult.IsError()) {
                    METRISCA_ERROR("Error while modelizing the dataset during attack");
                    isError = true;
                    error = modelizationResult.Error();
                    return;
                }

                models = modelizationResult.Value();
            }

            // Each following computation are performed against each key hypothesis
            for (size_t key = 0; key != 256; ++key) {
                std::vector<double> noise;
                noise.reserve(profiledResult.selectedSample.size());

                // Compute the noise for each sample
                for (size_t sampleIdx : profiledResult.selectedSample) {
                    double sampleNoise = 0.0;
                    for (size_t traceIdx = 0; traceIdx != steps[stepIdx]; ++traceIdx) {
                        uint32_t expectedResult = models(key, traceIdx);
                        double value = (linearCorrectionFactor(attackDataset->GetSample(sampleIdx)[traceIdx]) - 
                                       profiledResult.means[sampleIdx - sampleStart][expectedResult]);
                        if (std::isnan(value)) {
                            value = linearCorrectionFactor(255);
                        }
                        sampleNoise += value;
                    }
                    noise.push_back(sampleNoise / steps[stepIdx]);
                }

                // Compute the covariance matrix (the upper triangular part is enough)
                Matrix<double> covarianceMatrix(noise.size(), noise.size());

                for (size_t row = 0; row != noise.size(); row++) {
                    for (size_t col = row; col != noise.size(); ++col) {
                        double covariance = 0.0;
                        double a = 0.0, b = 0.0;
                        size_t N = 1;
                        for (size_t traceIdx = 0; traceIdx != steps[stepIdx]; ++traceIdx) {
                            uint32_t expectedResult = models(key, traceIdx);

                            double u = (linearCorrectionFactor(attackDataset->GetSample(profiledResult.selectedSample[row])[traceIdx]) - profiledResult.means[profiledResult.selectedSample[row] - sampleStart][expectedResult]);
                            double v = (linearCorrectionFactor(attackDataset->GetSample(profiledResult.selectedSample[col])[traceIdx]) - profiledResult.means[profiledResult.selectedSample[col] - sampleStart][expectedResult]);
                            
                            double acc = u * v;

                            if (!std::isnan(acc)) {
                                covariance += acc;
                                a += u;
                                b += v;
                                N++;
                            }
                        }

                        covarianceMatrix(row, col) = (covariance / N) - ((a / N) * (b / N));
                    }
                }

                // Complete the covariance matrix
                for (size_t row = 0; row != noise.size(); row++) {
                    for (size_t col = 0; col != row; ++col) {
                        covarianceMatrix(row, col) = covarianceMatrix(col, row);
                    }
                }

                // Compute the inverse of the covariance matrix
                // Matrix<double> covarianceMatrixInverse = covarianceMatrix.CholeskyInverse();

                // Compute the log probability
                double logProbability = 0.0;

                // for (size_t row = 0; row != noise.size(); ++row) {
                //     for (size_t col = 0; col != noise.size(); ++col) {
                //         logProbability += covarianceMatrixInverse(row, col) * noise[row] * noise[col];
                //     }
                // }

                for (size_t i = 0; i != noise.size(); ++i) {
                    logProbability += noise[i] * noise[i]; // / covarianceMatrix(i, i);
                }

                // Store the log probability
                result[stepIdx][byteIdx][key] = -0.5 * logProbability;
            }
        });

        // If any error abort
        if (isError) {
            return error;
        }

        // Finally return the result of the attack operation
        METRISCA_INFO("End of the attack phase");
        return result;
    }

}

