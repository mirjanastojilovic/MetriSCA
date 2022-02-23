/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/core/matrix.hpp"
#include "metrisca/core/result.hpp"

#include <nonstd/span.hpp>
#include <vector>
#include <string>
#include <cassert>
#include <unordered_map>
#include <memory>

namespace metrisca {

    enum class EncryptionAlgorithm : uint32_t {
        UNKNOWN = 0,
        S_BOX = 1,
        AES_128 = 2,
    };

    static std::string to_string(EncryptionAlgorithm algo)
    {
        switch(algo)
        {
        case EncryptionAlgorithm::UNKNOWN: return "unknown";
        case EncryptionAlgorithm::S_BOX: return "s-box";
        case EncryptionAlgorithm::AES_128: return "aes-128";
        default: assert(false); return "unknown";
        }
    }

    enum class PlaintextGenerationMode : uint32_t {
        UNKNOWN = 0,
        FIXED = 1,
        RANDOM = 2,
        CHAINED = 3,
    };

    enum class KeyGenerationMode: uint32_t {
        UNKNOWN = 0,
        FIXED = 1,
    };

#define DATASET_HEADER_MAGIC_VALUE 0x7265646165687364
    struct TraceDatasetHeader {
        uint64_t _MagicValue;
        double TimeResolution;
        double CurrentResolution;
        uint32_t NumberOfTraces;
        uint32_t NumberOfSamples;
        EncryptionAlgorithm EncryptionType;
        PlaintextGenerationMode PlaintextMode;
        uint32_t PlaintextSize;
        KeyGenerationMode KeyMode;
        uint32_t KeySize;
    };

    /**
     * Represent a dataset of traces.
     */
    class TraceDataset {
        friend class TraceDatasetBuilder;
    public:
        TraceDataset() = default;
        ~TraceDataset() = default;

        /// Methods that can be used to save/load a dataset from a file
        void SaveToFile(const std::string& filename);
        static Result<std::shared_ptr<TraceDataset>, int> LoadFromFile(const std::string& filename);

        /// Get a read-only view of the underlying data. These methods do not
        /// perform any copy.
        const nonstd::span<const int32_t> GetSample(uint32_t sample) const;
        const nonstd::span<const uint8_t> GetPlaintext(uint32_t trace) const;
        const nonstd::span<const uint8_t> GetKey(uint32_t trace) const;
        const nonstd::span<const uint8_t> GetCiphertext(uint32_t trace) const;

        /// Get the mean trace over every sample
        std::vector<double> GetMeanSample() const;

        /// Split a dataset into two new ones at a specific trace index
        void SplitDataset(TraceDataset& out1, TraceDataset& out2, uint32_t trace_split) const;

        TraceDatasetHeader GetHeader() const;
        size_t Size() const;

    private:
        /// Generate the plaintexts if the mode is chained
        void GenerateChainedPlaintexts();
        void GenerateCiphertexts();

    private:
        TraceDatasetHeader m_Header{};
        Matrix<int32_t> m_Traces;
        Matrix<uint8_t> m_Plaintexts;
        Matrix<uint8_t> m_Keys;
        Matrix<uint8_t> m_Ciphertexts;
    };

    /**
     * Represent a builder that constructs datasets from unstructured data.
     * To produce a valid trace dataset, the programmer must assign a value
     * to each public fields of this class and must call the various Add* methods
     * accordingly.
     */
    class TraceDatasetBuilder {
    public:
        TraceDatasetBuilder() = default;
        ~TraceDatasetBuilder() = default;
    
        /**
         * \brief Add a trace to the builder.
         * The size of the provided vector should match the value provided in 
         * \c NumberOfSamples. The number of calls to this method before calling
         * \c Build should match the value provided in \c NumberOfTraces.
         */
        void AddTrace(const std::vector<int32_t>& trace);

        /**
         * \brief Add a plaintext to the builder.
         * The size of the provided vector should match the value provided in 
         * \c PlaintextSize. The number of calls to this method before calling
         * \c Build depends on the value provided in \c PlaintextMode. If the
         * value is FIXED, then the number of calls should be exactly 1. If the
         * value is RANDOM, then the number of calls should match the value provided
         * in \c NumberOfTraces. If the value if CHAINED, then the number of calls
         * should be exactly 1 (the plaintexts are automatically generated based on 
         * the algorithm specified in the value of \c EncryptionType).
         */
        void AddPlaintext(const std::vector<uint8_t>& plaintext);

        /**
         * \brief Add a key to the builder.
         * The size of the provided vector should match the value provided in
         * \c KeySize. The number of calls to this method before calling \c
         * Build depends on the value provided in \c KeyMode. If the value is FIXED,
         * then the number of calls should be exactly 1.
         */
        void AddKey(const std::vector<uint8_t>& key);

        /// Construct a new structured dataset
        Result<std::shared_ptr<TraceDataset>, int> Build();

    public:
        double TimeResolution{ 0.0 };               /// The time in seconds between two samples
        double CurrentResolution{ 0.0 };            /// The current resolution of the measurements
        uint32_t NumberOfTraces{ 0 };               /// The number of measured traces
        uint32_t NumberOfSamples{ 0 };              /// The number of samples of each trace
        EncryptionAlgorithm EncryptionType{ 0 };    /// The encryption algorithm used. Note that only S-BOX is currently supported
        PlaintextGenerationMode PlaintextMode{ 0 }; /// The plaintext generation mode, either fixed, random or chained
        // TODO(dorian): We may want to remove PlaintextSize and KeySize as we know their value based on the encryption algorithm
        uint32_t PlaintextSize{ 0 };                /// The size of each plaintext in byte
        KeyGenerationMode KeyMode{ 0 };             /// The key generation mode, only a fixed key is currently supported
        uint32_t KeySize{ 0 };                      /// The size of each key in byte

    private:
        friend TraceDataset;
        std::vector<int32_t> m_Traces;
        std::vector<uint8_t> m_Plaintexts;
        std::vector<uint8_t> m_Keys;
    };

}
