/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca.hpp"
#include "metrisca/core/indicators.hpp"

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <filesystem>


namespace fs = std::filesystem;

namespace metrisca
{
    template<size_t TraceCount, size_t SampleCount>
    class BinLoader : public LoaderPlugin {
    public:
        virtual Result<void, Error> Init(const ArgumentList& args) override
        {
            auto file_name_arg = args.GetString("file");
            if (!file_name_arg.has_value()) {
                return Error::MISSING_ARGUMENT;
            }

            m_DbFilePath = fs::path(file_name_arg.value());
            if (!fs::exists(m_DbFilePath) || !fs::is_regular_file(m_DbFilePath)) {
                METRISCA_ERROR("The specified file does not exists");
                return Error::FILE_NOT_FOUND;
            }

            m_DbFileKeys = m_DbFilePath.parent_path() / "keys.bin";
            if (!fs::exists(m_DbFileKeys) || !fs::is_regular_file(m_DbFileKeys)) {
                METRISCA_ERROR("The keys file could not be found at {}", m_DbFileKeys.string());
                return Error::FILE_NOT_FOUND;
            }
            else {
                METRISCA_INFO("Keys file found at {}", m_DbFileKeys.string());
            }

            m_DbFilePlaintexts = m_DbFilePath.parent_path() / "plaintexts.bin";
            if (!fs::exists(m_DbFilePlaintexts) || !fs::is_regular_file(m_DbFilePlaintexts)) {
                METRISCA_ERROR("The plaintexts file could not be found at {}", m_DbFilePlaintexts.string());
                return Error::FILE_NOT_FOUND;
            }
            else {
                METRISCA_INFO("Plaintexts file found at {}", m_DbFilePlaintexts.string());
            }

            return {};
        }

        virtual Result<void, Error> Load(TraceDatasetBuilder& builder) override
        {
            builder.EncryptionType = EncryptionAlgorithm::AES_128;
            builder.KeyMode = KeyGenerationMode::FIXED;
            builder.KeySize = 16;
            builder.PlaintextMode = PlaintextGenerationMode::RANDOM;
            builder.PlaintextSize = 16;
            builder.NumberOfSamples = SampleCount;
            builder.NumberOfTraces = TraceCount;
            builder.ReserveInternals();

            // First read all of the samples
            try {
                METRISCA_INFO("Reading file at {} (expected {} samples and {} traces)", m_DbFilePath.string(), SampleCount, TraceCount);
                std::ifstream file;
                file.open(m_DbFilePath, std::ifstream::in | std::ifstream::binary);
                file.exceptions(std::ifstream::badbit | std::ifstream::failbit | std::ifstream::eofbit);

                indicators::show_console_cursor(false);
                indicators::BlockProgressBar bar{
                    indicators::option::BarWidth(60),
                    indicators::option::MaxProgress(TraceCount),
                    indicators::option::PrefixText("Extracting traces from BIN "),
                    indicators::option::ShowElapsedTime(true),
                    indicators::option::ShowRemainingTime(true)
                };

                std::vector<int32_t> trace(SampleCount);
                std::vector<uint8_t> raw_trace(SampleCount);
                for (uint32_t t = 0; t < TraceCount; ++t)
                {
                    if (t % (1 + (TraceCount / 1000)) == 0) {
                        bar.set_progress((float)t);
                    }
                    file.read((char*) raw_trace.data(), SampleCount);
                    std::copy(raw_trace.begin(), raw_trace.end(), trace.begin());
                    builder.AddTrace(trace);
                }

                bar.mark_as_completed();
                indicators::show_console_cursor(true);
            }
            catch (std::exception e) {
                METRISCA_ERROR("Failed to perform the task, exception was thrown: {}", e.what());
                return Error::IO_FAILURE;
            }

            // Secondly read the file containing all of the keys
            try {
                METRISCA_INFO("Reading file at {} containing keys", m_DbFileKeys.string());
                std::ifstream file;
                file.open(m_DbFileKeys, std::ifstream::in | std::ifstream::binary);
                file.exceptions(std::ifstream::badbit | std::ifstream::failbit | std::ifstream::eofbit);

                std::vector<uint8_t> key(builder.KeySize);
                file.read((char*)key.data(), builder.KeySize);
                builder.AddKey(key);
            }
            catch (std::exception e) {
                METRISCA_ERROR("Failed to perform the task, exception was thrown: {}", e.what());
                return Error::IO_FAILURE;
            }

            // Finally read the file containing the plaintexts
            try {
                METRISCA_INFO("Reading file at {} containing plaintext", m_DbFilePlaintexts.string());
                std::ifstream file;
                file.open(m_DbFilePlaintexts, std::ifstream::in | std::ifstream::binary);
                file.exceptions(std::ifstream::badbit | std::ifstream::failbit | std::ifstream::eofbit);

                indicators::show_console_cursor(false);
                indicators::BlockProgressBar bar{
                    indicators::option::BarWidth(60),
                    indicators::option::MaxProgress(TraceCount),
                    indicators::option::PrefixText("Extracting traces from BIN "),
                    indicators::option::ShowElapsedTime(true),
                    indicators::option::ShowRemainingTime(true)
                };

                std::vector<uint8_t> plaintext(builder.PlaintextSize);
                for (uint32_t t = 0; t < TraceCount; ++t)
                {
                    if (t % (1 + (TraceCount / 1000)) == 0) {
                        bar.set_progress((float)t);
                    }
                    file.read((char*)plaintext.data(), builder.PlaintextSize);
                    builder.AddPlaintext(plaintext);
                }

                bar.mark_as_completed();
                indicators::show_console_cursor(true);
            }
            catch (std::exception e) {
                METRISCA_ERROR("Failed to perform the task, exception was thrown: {}", e.what());
                return Error::IO_FAILURE;
            }

            return {};
        }

    private:
        fs::path m_DbFilePath{};
        fs::path m_DbFileKeys{};
        fs::path m_DbFilePlaintexts{};
    };
}
