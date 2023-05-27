/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/key_enumeration_metric.hpp"

#include "metrisca/core/logger.hpp"
#include "metrisca/core/errors.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/distinguishers/distinguisher.hpp"
#include "metrisca/models/model.hpp"
#include "metrisca/core/lazy_function.hpp"
#include "metrisca/core/indicators.hpp"
#include "metrisca/core/arg_list.hpp"
#include "metrisca/core/parallel.hpp"
#include "metrisca/core/lazy_function.hpp"

#include <limits>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <sstream>
#include <functional>
#include <algorithm>
#include <math.h>


#define DOUBLE_NAN (std::numeric_limits<double>::signaling_NaN())
#define DOUBLE_INFINITY (std::numeric_limits<double>::infinity())

typedef std::string PartialKey; // Use to partialKey each byte of the key

struct EnumeratedElement
{
    double score;
    PartialKey partialKey;

    inline EnumeratedElement(double score, PartialKey partialKey)
    : partialKey(partialKey), score(score)
    { }

    inline EnumeratedElement()
    : EnumeratedElement(0, {})
    { }
};
typedef std::function<bool(std::vector<EnumeratedElement>&, size_t)> LazyGeneratorFn;


class Enumerator : public std::enable_shared_from_this<Enumerator>
{
public:
    Enumerator(LazyGeneratorFn lazyRowGen, LazyGeneratorFn lazyColGen, size_t bufferSize) 
    : m_LazyRowGen(std::move(lazyRowGen)),
      m_LazyColGen(std::move(lazyColGen)),
      m_ListOfCandidates({ 0 }),
      m_BufferSize(bufferSize)
    { }

    /**
     * @brief Enumerate next @p N elements 
     * @return true if there is no longer any element within the stream
     */
    bool next(std::vector<EnumeratedElement>& output, size_t N)
    {
        // Ensures that the row/col is at least one element (for bootstrap)
        if (m_Row.empty()) {
            if (m_LazyRowGen(m_Row, 1)) {
                return true;
            }
        }

        if (m_Col.empty()) {
            if (m_LazyColGen(m_Col, 1)) {
                return true;
            }
        }

        // For each of the entry
        while (N-- != 0) {
            // Find the best candidate
            size_t bj = std::numeric_limits<size_t>::max();
            double bestScore = -DOUBLE_INFINITY;

            for (size_t j = 0; j != m_ListOfCandidates.size(); ++j) {
                // Skip candidates for column already fully explored                
                if (m_ListOfCandidates[j] == std::numeric_limits<size_t>::max()) continue;

                // Compute the score of the current element
                double score = m_Row[j].score + m_Col[m_ListOfCandidates[j]].score;

                // If this is the best score so-far then use it as reference
                if (score > bestScore) {
                    bestScore = score;
                    bj = j;
                }
            }

            // No element where found, aborting the operation
            if (bj == std::numeric_limits<size_t>::max()) {
                return true;
            }

            // Generate the next enumerated element
            output.emplace_back(
                bestScore,
                m_Row[bj].partialKey + m_Col[m_ListOfCandidates[bj]].partialKey
            );

            // If the current candidate is the last row, we can add a new row as candidate
            if (m_ListOfCandidates[bj] == 0) {
                METRISCA_ASSERT(bj == m_ListOfCandidates.size() - 1);

                // Attempt to push the row
                if (!m_RowMaxOut) {
                    m_RowMaxOut = m_LazyRowGen(m_Row, m_BufferSize);
                }

                // Attempt to push a new candidate for the last row, if possible otherwise just skip this step
                if (m_ListOfCandidates.size() < m_Row.size()) {
                    m_ListOfCandidates.emplace_back(0); 
                }
            }

            // Update the list of candidates
            m_ListOfCandidates[bj]++;

            // Generating more of the column array if required
            if (m_ListOfCandidates[bj] == m_Col.size() && !m_ColMaxOut) {
                m_ColMaxOut = m_LazyColGen(m_Col, m_BufferSize);
            }

            // In the case where we cannot fetch more column, then simply mark this column as filled
            if (m_ListOfCandidates[bj] == m_Col.size()) {
                m_ListOfCandidates[bj] = std::numeric_limits<size_t>::max();
            }
        }

        // Return false -> there are still keys that were not enumerated
        return false;
    }

    LazyGeneratorFn asLazyGenerator()
    {
        std::shared_ptr<Enumerator> shared_ptr = shared_from_this();
        return [shared_ptr](std::vector<EnumeratedElement>& output, size_t N) -> bool {
            return shared_ptr->next(output, N);
        };
    }

private:
    std::vector<size_t> m_ListOfCandidates;
    std::vector<EnumeratedElement> m_Row;
    std::vector<EnumeratedElement> m_Col;
    LazyGeneratorFn m_LazyRowGen;
    LazyGeneratorFn m_LazyColGen;

    bool m_RowMaxOut{false};
    bool m_ColMaxOut{false};
    size_t m_BufferSize;
};

static LazyGeneratorFn asLazyGenerator(const std::array<double, 256>& elements)
{
    struct Context {
        std::vector<size_t> reorderBuffer;
        size_t cIdx;
    };

    // Create the context object
    std::shared_ptr<Context> context = std::make_shared<Context>();
    context->cIdx = 0;
    context->reorderBuffer.reserve(256);
    for (size_t i = 0; i != 256; ++i) {
        context->reorderBuffer.emplace_back(i);
    }

    // Sort the reorderBuffer so that the corresponding score is in descending order
    std::sort(context->reorderBuffer.begin(), context->reorderBuffer.end(), [&](size_t lhs, size_t rhs) {
        if (std::isnan(elements[lhs])) {
            return false;
        }
        else if (std::isnan(elements[rhs])) {
            return true;
        }
        else {
            return elements[lhs] > elements[rhs];
        }
    });

    return [elements, context](std::vector<EnumeratedElement>& output, size_t N) -> bool {
        size_t& cIdx = context->cIdx;
        
        while (N-- != 0) {
            if (cIdx >= 256) {
                return true;
            }

            size_t key = context->reorderBuffer[cIdx++];
            if (std::isnan(elements[key])) { // Ensure no `nan` value get past this stage
                cIdx = 256;
                return true;
            }

            output.emplace_back(elements[key], PartialKey({ (char) key }));
        }

        return false;
    };
}

/**
 * @brief Combine each LazyGeneratorFn pairwise to construct the generation tree
 * 
 * @param array the input LazyGeneratorFn
 * @return std::vector<LazyGeneratorFn> the resulted combined array
 */
static std::vector<LazyGeneratorFn> Combine(const std::vector<LazyGeneratorFn>& array)
{
    if (array.size() <= 1) return array;

    std::vector<LazyGeneratorFn> result;
    for (size_t i = 0; i < array.size(); i += 2) {
        result.emplace_back(std::make_shared<Enumerator>(array[i], array[i+1], 1)->asLazyGenerator());
    }

    return result;
}



namespace metrisca {

    Result<void, Error> KeyEnumerationMetric::Init(const ArgumentList& args)
    {
        // First initialize the base metric
        auto base_result = MetricPlugin::Init(args);
        if(base_result.IsError())
            return base_result;

        // Secondly retrieve both database
        auto dataset = args.GetDataset(ARG_NAME_DATASET);
        auto training_dataset = args.GetDataset(ARG_NAME_TRAINING_DATASET);

        if(!dataset.has_value())
            return Error::MISSING_ARGUMENT;

        if(!training_dataset.has_value())
            return Error::MISSING_ARGUMENT;

        m_Dataset = dataset.value();
        m_TrainingDataset = training_dataset.value();

        // Ensures the actual dataset has fixed key
        if (m_Dataset->GetHeader().KeyMode != KeyGenerationMode::FIXED) {
            METRISCA_ERROR("KeyEnumerationMetric requires the key to be fixed accross the entire dataset");
            return Error::UNSUPPORTED_OPERATION;
        }

        // Ensure that the size of the key for both dataset matches (as well as all other parameters)
        const auto& header1 = m_Dataset->GetHeader();
        const auto& header2 = m_TrainingDataset->GetHeader();

        if (header1.KeySize != header2.KeySize ||
            header1.PlaintextSize != header2.PlaintextSize ||
            header1.EncryptionType != header2.EncryptionType ||
            header1.NumberOfSamples != header2.NumberOfSamples) {
            METRISCA_ERROR("KeyEnumerationMetric requires both the training and testing dataset to have the same configuration");
            return Error::UNSUPPORTED_OPERATION;
        }

        // Find the model
        auto model = args.GetString(ARG_NAME_MODEL);
        if(!model.has_value())
            return Error::MISSING_ARGUMENT;

        auto model_or_error = PluginFactory::The().ConstructAs<PowerModelPlugin>(PluginType::PowerModel, model.value(), args);
        if(model_or_error.IsError())
            return model_or_error.Error();
        m_Model = model_or_error.Value();

        // Retrieve pratical information about the dataset
        auto sample_start = args.GetUInt32(ARG_NAME_SAMPLE_START);
        auto sample_end = args.GetUInt32(ARG_NAME_SAMPLE_END);
        auto trace_count = args.GetUInt32(ARG_NAME_TRACE_COUNT);
        auto trace_step = args.GetUInt32(ARG_NAME_TRACE_STEP);

        m_SampleStart = sample_start.value_or(0);
        m_SampleEnd = sample_end.value_or(header1.NumberOfSamples);

        m_TraceCount = trace_count.value_or(header1.NumberOfTraces);
        m_TraceStep = trace_step.value_or(0);

        if (m_SampleStart >= m_SampleEnd) return Error::INVALID_ARGUMENT;
        if (m_SampleEnd > header1.NumberOfSamples) return Error::INVALID_ARGUMENT;
        if (m_TraceCount > header1.NumberOfTraces) return Error::INVALID_ARGUMENT;
        if (m_TraceStep >= m_TraceCount) return Error::INVALID_ARGUMENT;

        // Retrieve the number of enumerated key count
        auto enumerated_key_count = args.GetUInt32(ARG_NAME_ENUMERATED_KEY_COUNT);
        if (!enumerated_key_count.has_value()) {
            return Error::INVALID_ARGUMENT;
        }
        m_EnumeratedKeyCount = enumerated_key_count.value();

        // Retrieve the number of output enumerated key count
        auto output_enumerated_key_count = args.GetUInt32(ARG_NAME_OUTPUT_KEY_COUNT);
        if (!output_enumerated_key_count.has_value()) {
            return Error::INVALID_ARGUMENT;
        }
        m_OutputEnumeratedKeyCount = output_enumerated_key_count.value();

        if (m_OutputEnumeratedKeyCount > m_EnumeratedKeyCount) {
            METRISCA_ERROR("Cannot output more keys than the total number of key enumerated");
            return Error::UNSUPPORTED_OPERATION;
        }

        // Retrieve the key from the dataset
        auto key_span = m_Dataset->GetKey(0);
        m_Key.insert(m_Key.end(), key_span.begin(), key_span.end());

        // Maximum key size
        if (m_Key.size() > 256) {
            METRISCA_ERROR("Maximum key size reached, risk of stack overflow");
            return Error::UNSUPPORTED_OPERATION;
        }

        // Finally return the success of the initialization of the current plugin
        return {};
    }

    Result<void, Error> KeyEnumerationMetric::Compute()
    {
        // Generate a list of steps
        std::vector<uint32_t> steps = (m_TraceStep > 0) ?
            numerics::ARange(m_TraceStep, m_TraceCount + 1, m_TraceStep) :
            std::vector<uint32_t>{ m_TraceCount };

        // Open output csv file
        CSVWriter writer(m_OutputFile);

        // Profiling stage
        METRISCA_INFO("Profiling stage");
        auto profileResult = ProfileStage();

        if (profileResult.IsError()) return profileResult.Error();
        ProfileOutput profileOutput = profileResult.Value();

        // Modelize expected result for each entry
        METRISCA_INFO("Modelize the testing dataset using the provided modelling function");
        std::vector<Matrix<int32_t>> models(m_Key.size());
        m_Model->SetDataset(m_Dataset);
        for (size_t byteIdx = 0; byteIdx != m_Key.size(); ++byteIdx) {
            m_Model->SetByteIndex(byteIdx);
            
            auto model = m_Model->Model();
            if (model.IsError()) {
                return model.Error();
            }

            models[byteIdx] = std::move(model.Value());
        }
 
        // First compute the score for each key for each key byte
        METRISCA_INFO("Computing score(s) for each key and key byte");
        std::vector<std::vector<std::array<double, 256>>> scores; // [step][byte][byteValue]

        // Required in order to ensure that the vector won't be reallocated during parrallel execution of the program
        scores.resize(steps.size());
        for (size_t i = 0; i != steps.size(); ++i) {
            scores[i].resize(m_Key.size());
        }

        std::atomic_bool isError = false;
        Error error;

        metrisca::parallel_for("Computing scores", 0, steps.size() * m_Key.size(), [&](size_t idx) {
            if (isError) return; 

            size_t keyByteIdx = idx % m_Key.size();
            size_t stepIdx = idx / m_Key.size();

            auto result = ComputeScores(profileOutput, steps[stepIdx], keyByteIdx, models);
            if (result.IsError()) {
                METRISCA_ERROR("Fail to compute scores with {} traces (key-byte {})", steps[stepIdx], keyByteIdx);
                isError = true;
                error = result.Error();
                return;
            }
            scores[stepIdx][keyByteIdx] = result.Value();
        });
        
        if (isError) {
            return error;
        }

        // Dump the score to the output file
        METRISCA_INFO("Writing scores to the output csv file");
        writer << "trace-count" << "keyByte" << "scores..." << csv::EndRow;
        for (size_t stepIdx = 0; stepIdx != steps.size(); stepIdx++) {
            for (size_t byte = 0; byte != m_Key.size(); ++byte) {
                writer << steps[stepIdx] << byte;
                for (size_t key = 0; key != 256; ++key) {
                    writer << scores[stepIdx][byte][key];
                }
                writer << csv::EndRow;
            }
        }

        // Secondly for each step, enumerate all possible keys
        std::vector<std::vector<EnumeratedElement>> outputPerSteps;
        outputPerSteps.resize(steps.size());

        metrisca::parallel_for(0, steps.size(), [&](size_t stepIdx)
        {
            // Retrieve the score corresponding with the current step
            const std::vector<std::array<double, 256>>& score = scores[stepIdx];
        
            // Constructing the enumerating context
            std::vector<LazyGeneratorFn> lazyGenerators;

            for (size_t keyByte = 0; keyByte != score.size(); ++keyByte) {
                lazyGenerators.emplace_back(asLazyGenerator(score[keyByte]));
            }

            while (lazyGenerators.size() > 1) {
                lazyGenerators = Combine(lazyGenerators);
            }

            // Finally use the enumerator
            std::vector<EnumeratedElement> output;
            lazyGenerators[0](output, m_EnumeratedKeyCount);
            outputPerSteps[stepIdx] = std::move(output);
        });

        // Finally write the output to the file
        METRISCA_INFO("Writing result to the output file");
        writer << "trace-count" << "rank" << "score" << "keys/scores" << csv::EndRow;

        // Generate the key string
        PartialKey key(m_Key.begin(), m_Key.end());

        for (size_t stepIdx = 0; stepIdx != steps.size(); stepIdx++) {
            // Find the rank and score of the correct key
            size_t rank = 0;
            double score = DOUBLE_NAN;
            for (const EnumeratedElement& element : outputPerSteps[stepIdx]) {
                rank++;
                if (element.partialKey == key) {
                    score = element.score;
                    break;
                }
            }

            writer << steps[stepIdx] << rank << score;

            // Enumerate the required amount of first key
            for (size_t i = 0; i < m_OutputEnumeratedKeyCount; ++i) {
                if (i >= outputPerSteps[stepIdx].size()) {
                    METRISCA_WARN("Due to `nan` being in the resulting score matrix, the number of enumerated key is smaller than the expected one");
                    break;
                }

                const auto& element = outputPerSteps[stepIdx][i];
                std::ostringstream ss;

                for (char c : element.partialKey) {
                    ss << std::setw(2) << std::setfill('0') << std::hex << (int) (uint8_t) c;
                }

                writer << std::move(ss).str() << element.score;
            }

            // Finally end the row of the current steps
            writer << csv::EndRow;
        }

        return {};
    }

    Result<std::array<double, 256>, Error> KeyEnumerationMetric::ComputeScores(const ProfileOutput& profileOutput, size_t traceCount, size_t keyByteIdx, const std::vector<Matrix<int32_t>>& models)
    {
        size_t const sampleCount = m_SampleEnd - m_SampleStart;
        std::array<double, 256> scores;
        scores.fill(0.0);

        // Finally compute the score under each key hypothesis
        std::vector<double> error_vector(sampleCount, 0.0);
        std::vector<double> temp_vector(sampleCount, 0.0);

        for (size_t key = 0; key != 256; key++)
        {
            // Compute the error vecotr
            for (size_t sampleIdx = 0; sampleIdx != sampleCount; ++sampleIdx) {
                double v = 0.0;
                double vSquared = 0.0;
                auto sample = m_Dataset->GetSample(sampleIdx + m_SampleStart);

                for (size_t traceIdx = 0; traceIdx != traceCount; ++traceIdx) {
                    uint8_t expected_output = (uint8_t) (uint32_t) models[keyByteIdx](key, traceIdx);
                    double value = ((double) sample[traceIdx]) - ((double) profileOutput[keyByteIdx][sampleIdx][expected_output]);
                    v += value;
                    vSquared += value * value;
                }

                v /= traceCount;
                vSquared /= traceCount;

                error_vector[sampleIdx] = v / std::sqrt(vSquared - v * v);

                if (std::isnan(error_vector[sampleIdx])) {
                    error_vector[sampleIdx] = 0.0; // Ignore the sample
                }
            }

            // Finally compute dot product between the error_vector and the temp_vector
            double score = 0.0;
            for (size_t i = 0; i != sampleCount; ++i) {
                score += error_vector[i] * error_vector[i];
            }
            scores[key] = -std::log2(1e-9 + score); // score >= 0
        }

        return scores;
    }

    Result<KeyEnumerationMetric::ProfileOutput, Error> KeyEnumerationMetric::ProfileStage()
    {
        std::vector<std::vector<std::array<double, 256>>> result;
        result.resize(m_Key.size());
        for (size_t i = 0; i != result.size(); ++i) {
            result[i].resize(m_SampleEnd - m_SampleStart);
        }

        std::mutex glock;
        m_Model->SetDataset(m_TrainingDataset);
        
        std::atomic_bool isError = false;
        Error error;

        // Computing profile output
        metrisca::parallel_for("Profiling for each key byte ", 0, result.size(), [&](size_t byteIdx)
        {
            // If any error
            if (isError) return;

            // Modelize the trace dataset
            metrisca::Matrix<int32_t> model;

            {
                std::lock_guard<std::mutex> guard(glock);
                m_Model->SetByteIndex(byteIdx);
                auto model_result = m_Model->Model();
                if (model_result.IsError()) {
                    if (!isError.exchange(true)) {
                        error = model_result.Error();
                    }
                    return;
                }
                model = model_result.Value();
            }

            // Group every traces by expected output
            std::array<std::vector<size_t>, 256> traceIdxByExpectedOutput;

            for (size_t i = 0; i != m_TrainingDataset->GetHeader().NumberOfTraces; ++i) {
                uint32_t expectedOutput = (uint32_t) model(m_TrainingDataset->GetKey(i)[byteIdx], i);
                
                if (expectedOutput >= 256) {
                    if (!isError.exchange(true)) {
                        METRISCA_INFO("Currently this metric does not support exepected result not in [0, 255]");
                        error = Error::UNSUPPORTED_OPERATION;
                    }
                    return;
                }

                traceIdxByExpectedOutput[expectedOutput].push_back(i);
            }

            // Compute per-group average for each sample, this is our "prior" knowledge
            for (size_t expectedOutput = 0; expectedOutput != 256; expectedOutput++) {
                for (size_t sIdx = m_SampleStart; sIdx != m_SampleEnd; sIdx++) {

                    double value = 0.0;
                    size_t N = 0;
                    for (size_t i : traceIdxByExpectedOutput[expectedOutput]) {
                        N++;
                        value += m_TrainingDataset->GetSample(sIdx)[i];
                    }

                    if (N > 0) {
                        result[byteIdx][sIdx - m_SampleStart][expectedOutput] = value / N;
                    }
                    else {
                        result[byteIdx][sIdx - m_SampleStart][expectedOutput] = DOUBLE_NAN;
                    }
                }
            }
        });

        // Compute the total average
        METRISCA_INFO("Computing total average");
        auto mean_sample = m_TrainingDataset->GetMeanSample();
        double average = 0.0;
        for (size_t sIdx = m_SampleStart; sIdx != m_SampleEnd; sIdx++) {
            average += mean_sample[sIdx];
        }
        average /= (m_SampleEnd - m_SampleStart);

        // Replace all values by total average
        for (size_t keyByte = 0; keyByte != m_Key.size(); ++keyByte) {
            for (size_t sIdx = m_SampleStart; sIdx != m_SampleEnd; sIdx++) {
                for (size_t key = 0; key != 256; key++) {
                    double& value = result[keyByte][sIdx - m_SampleStart][key];
                    if (std::isnan(value)) {
                        value = average;
                    }
                }
            }
        }

        return result;
    }
}
