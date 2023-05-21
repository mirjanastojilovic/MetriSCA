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
#include <functional>
#include <algorithm>
#include <math.h>


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

            // Update the list of candidates
            m_ListOfCandidates[bj]++;

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
        return elements[lhs] > elements[rhs];
    });

    return [elements, context](std::vector<EnumeratedElement>& output, size_t N) -> bool {
        size_t& cIdx = context->cIdx;
        
        while (N-- != 0) {
            if (cIdx >= 256) {
                return true;
            }

            size_t key = context->reorderBuffer[cIdx++];
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
        // First initialize the base plugin
        auto base_result = BasicMetricPlugin::Init(args);
        if(base_result.IsError())
            return base_result.Error();

        // Ensure the dataset has fixed key
        if (m_Dataset->GetHeader().KeyMode != KeyGenerationMode::FIXED) {
            METRISCA_ERROR("KeyEnumerationMetric require the key to be fixed accross the entire dataset");
            return Error::UNSUPPORTED_OPERATION;
        }

        // Retrieve the number of enumerated key count
        auto enumerated_key_count = args.GetUInt32(ARG_NAME_ENUMERATED_KEY_COUNT);
        if (!enumerated_key_count.has_value()) {
            return Error::INVALID_ARGUMENT;
        }
        m_EnumeratedKeyCount = enumerated_key_count.value();

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
        // First compute the score for each key for each key byte
        METRISCA_INFO("Computing score(s) for each key and key byte");
        std::unordered_map<uint32_t, std::vector<std::array<double, 256>>> scoresPerSteps;
        {
            auto result = ComputeScores();
            if (result.IsError()) {
                return result.Error();
            }
            scoresPerSteps = std::move(result.Value());
        }

        // Secondly for each step, enumerate all possible keys
        std::vector<uint32_t> steps(scoresPerSteps.size());
        std::transform(scoresPerSteps.begin(), scoresPerSteps.end(), steps.begin(), [](auto x) { return x.first; });

        std::unordered_map<uint32_t, std::vector<EnumeratedElement>> outputPerSteps;

        metrisca::parallel_for(0, scoresPerSteps.size(), [&](size_t idx)
        {
            // Retrieve the score corresponding with the current step
            const std::vector<std::array<double, 256>>& scores = scoresPerSteps[steps[idx]];
        
            // Constructing the enumerating context
            std::vector<LazyGeneratorFn> lazyGenerators;

            for (size_t keyByte = 0; keyByte != scores.size(); ++keyByte) {
                lazyGenerators.emplace_back(asLazyGenerator(scores[keyByte]));
            }

            while (lazyGenerators.size() > 1) {
                lazyGenerators = Combine(lazyGenerators);
            }

            // Finally use the enumerator
            std::vector<EnumeratedElement> output;
            lazyGenerators[0](output, m_EnumeratedKeyCount);
            outputPerSteps[steps[idx]] = std::move(output);
        });

        // Finally write the output to the file
        METRISCA_INFO("Writing result to the output file");

        CSVWriter writer(m_OutputFile);
        writer << "trace-count" << "rank";

        // Generate the key string
        PartialKey key(m_Key.begin(), m_Key.end());

        for (size_t idx = 0; idx != steps.size(); idx++) {
            // Find the rank of the correct key
            size_t rank = 1;
            for (const EnumeratedElement& element : outputPerSteps[steps[idx]]) {
                rank++;
                if (element.partialKey == key) {
                    break;
                }
            }
            
            writer << steps[idx] << rank << csv::EndRow; 
        }


        return {};
    }

    Result<std::unordered_map<uint32_t, std::vector<std::array<double, 256>>>, Error>
    KeyEnumerationMetric::ComputeScores()
    {
        std::unordered_map<uint32_t, std::vector<std::array<double, 256>>> scoresPerSteps;

        for (size_t keyByteIdx = 0; keyByteIdx != m_Key.size(); ++keyByteIdx) {
            m_Distinguisher->GetPowerModel()->SetByteIndex(keyByteIdx);

            METRISCA_INFO("Run distinguisher for key byte {}", keyByteIdx);
            auto result = m_Distinguisher->Distinguish();
            if (result.IsError()) {
                METRISCA_ERROR("KeyEnumerationMetric failed to compute scores for key byte {}", keyByteIdx);
                return result.Error();
            }

            if (keyByteIdx == 0) {
                for (auto [steps, matrix] : result.Value()) {
                    std::vector<std::array<double, 256>> v;
                    v.resize(16);
                    scoresPerSteps.insert(std::make_pair(steps, std::move(v)));
                }
            }

            for (auto [steps, matrix] : result.Value()) {
                
                for (size_t key = 0; key != 256; key++) {
                    double score = -DOUBLE_INFINITY; 

                    // Find the sample that "leaks" the most, this is the score
                    for (size_t sample = 0; sample != matrix.GetWidth(); ++sample) {
                        score = std::max(matrix(key, sample), score);
                    }

                    scoresPerSteps[steps][keyByteIdx][key] = score;
                }

            }
        }

        return scoresPerSteps;
    }
}
