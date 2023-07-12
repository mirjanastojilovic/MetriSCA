/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca.hpp"
#include "metrisca/core/indicators.hpp"

#include "app/application.hpp"
#include "app/bin_loader.hpp"

#include <iostream>
#include <filesystem>

using namespace metrisca;

/*
 * This is an example of trace file loader for a simple measurement output. You will have to
 * implement support for your own file formats.
 * In this file, the trace values are stored as time and current readings from an oscilloscope.
 * Here is an example of a few lines in this file:
 *     1.100000e-02 9.500000e-05
 *     1.200000e-02 9.500000e-05
 *     1.300000e-02 9.500000e-05
 *     1.400000e-02 9.500000e-05
 * The first value represents the timestamp and the second value represents the current measurement.
 * This file stores the samples of each trace in order, this means that if the file contains 10 traces
 * of 4 samples, the first 4 values are the samples of trace 1, the next 4 of trace 2, etc...
 * 
 * int txtloader(TraceDatasetBuilder& builder, const std::string& filename) {
 *
 *     // The file contains 256 traces of 5000 samples each
 *     unsigned int num_traces = 256;
 *     unsigned int num_samples = 5000;
 *
 *     // Fill in the required fields in the builder
 *     builder.EncryptionType = EncryptionAlgorithm::S_BOX;
 *     builder.CurrentResolution = 1e-6;
 *     builder.TimeResolution = 1e-3;
 *     builder.PlaintextMode = PlaintextGenerationMode::RANDOM;
 *     builder.PlaintextSize = 1;
 *     builder.KeyMode = KeyGenerationMode::FIXED;
 *     builder.KeySize = 1;
 *     builder.NumberOfTraces = num_traces;
 *     builder.NumberOfSamples = num_samples;
 * 
 *     // Open the file
 *     std::ifstream file(filename);
 *     if(!file)
 *     {
 *         // If the file does not exist, return the appropriate error code.
 *         return SCA_FILE_NOT_FOUND;
 *     }
 * 
 *     std::string line;
 *     unsigned int line_count = 0;
 * 
 *     std::vector<int> trace;
 *     trace.resize(num_samples);
 * 
 *     while (std::getline(file, line)) {
 *         // Parse a line in the file and extract the trace measurement
 *         std::string trace_val_str = line.substr(line.find(' ') + 1, line.size());
 *         double trace_val = stod(trace_val_str);
 *
 *         // Convert the current measurement into an integer value and accumulate the trace
 *         uint32_t sample_num = line_count % num_samples;
 *         trace[sample_num] = (int)(trace_val / builder.CurrentResolution);
 * 
 *         // Once the end of the trace is reached, add to the builder
 *         if(sample_num == num_samples - 1)
 *         {
 *             builder.AddTrace(trace);
 *         }
 *
 *         line_count++;
 *     }
 *
 *     // The plaintexts go from 0 to 255 in order
 *     for(unsigned int p = 0; p < 256; ++p)
 *     {
 *         builder.AddPlaintext({(unsigned char)p});
 *     }
 *
 *     // The key is fixed to 0
 *     builder.AddKey({(unsigned char)0});
 *
 *     return SCA_OK;
 * }
 */

/*
int binloader(TraceDatasetBuilder& builder, const std::string& filename)
{
    uint32_t num_traces = 25000;
    uint32_t num_samples = 256;

    builder.EncryptionType = EncryptionAlgorithm::AES_128;
    builder.KeyMode = KeyGenerationMode::FIXED;
    builder.NumberOfSamples = num_samples;
    builder.NumberOfTraces = num_traces;
    builder.PlaintextMode = PlaintextGenerationMode::CHAINED;

    std::ifstream file(filename);
    if(!file)
    {
        return SCA_FILE_NOT_FOUND;
    }

    std::vector<int32_t> trace(num_samples);
    std::vector<uint8_t> raw_trace(num_samples);
    for(uint32_t t = 0; t < num_traces; ++t)
    {
        file.read((char*)raw_trace.data(), num_samples);
        std::copy(raw_trace.begin(), raw_trace.end(), trace.begin());
        builder.AddTrace(trace);
    }

    builder.AddPlaintext({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
    builder.AddKey({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0 });

    return SCA_OK;
}
*/

namespace fs = std::filesystem;


class TxtLoader : public LoaderPlugin {
public:
    virtual Result<void, Error> Init(const ArgumentList& args) override
    {
        auto file_name_arg = args.GetString("file");
        if (!file_name_arg.has_value()) {
            return Error::MISSING_ARGUMENT;
        }

        m_DbFilePath = fs::path(file_name_arg.value());
        if (!std::filesystem::exists(m_DbFilePath) || !std::filesystem::is_regular_file(m_DbFilePath)) {
            METRISCA_ERROR("The specified file does not exists");
            return Error::FILE_NOT_FOUND;
        }

        m_KeyFilePath = m_DbFilePath.parent_path() / "key.txt";
        if (!std::filesystem::exists(m_KeyFilePath) || !std::filesystem::is_regular_file(m_KeyFilePath)) {
            METRISCA_ERROR("Cannot find the key file at default location : {}", m_KeyFilePath.string());
            return Error::FILE_NOT_FOUND;
        }

        m_PlaintextFilePath = m_DbFilePath.parent_path() / "plaintexts.txt";
        if (!std::filesystem::exists(m_PlaintextFilePath) || !std::filesystem::is_regular_file(m_PlaintextFilePath)) {
            METRISCA_ERROR("Cannot find the key file at default location : {}", m_PlaintextFilePath.string());
            return Error::FILE_NOT_FOUND;
        }

        return {};
    }

    virtual Result<void, Error> Load(TraceDatasetBuilder& builder) override
    {
        uint32_t num_traces = 1000; 
        uint32_t num_samples = 8000;

        builder.EncryptionType = EncryptionAlgorithm::AES_128;
        builder.KeyMode = KeyGenerationMode::FIXED;
        builder.KeySize = 16;
        builder.PlaintextMode = PlaintextGenerationMode::RANDOM;
        builder.PlaintextSize = 16;
        builder.NumberOfSamples = num_samples;
        builder.NumberOfTraces = num_traces;
        builder.CurrentResolution = 0.0039215;
        builder.ReserveInternals();
    
        // First of all read the traces from the file
        {
            std::ifstream file(m_DbFilePath);
            if (!file.is_open()) {
                METRISCA_ERROR("Failed to open file at path {} for reading", m_DbFilePath.string());
                return Error::IO_FAILURE;
            }

            // Read each line of the file
            std::string line;
            std::vector<int> trace;
            trace.resize(num_samples);
    
            size_t line_number = 0;

            indicators::BlockProgressBar bar {
                indicators::option::BarWidth(60),
                indicators::option::MaxProgress(num_traces * num_samples),
                indicators::option::PrefixText("Extracting traces from TXT "),
                indicators::option::ShowElapsedTime(true),
                indicators::option::ShowRemainingTime(true)
            };

            std::vector<double> traces;
            while (std::getline(file, line)) {
                if (line_number % 10000 == 0) {
                    bar.set_option(indicators::option::PostfixText{
                        std::to_string(line_number) + "/" + std::to_string(num_traces * num_samples)
                    });
                    bar.set_progress(line_number);
                    bar.tick();
                }

                // Parse a line in the file and extract the trace measurement
                std::string trace_val_str = line.substr(line.find(' ') + 1, line.size());
                double trace_val = stod(trace_val_str);

                // Convert the current measurement into an integer value and accumulate the trace
                uint32_t sample_num = line_number % num_samples;
                int value = (int) (trace_val / builder.CurrentResolution);
                trace[sample_num] = value;
                if (value < 0 || value >= 256) {
                    METRISCA_ERROR("Invalid sample value (out of range) {} for value {}. Make sure the resolution is set up in such as way data matches this range", value, trace_val);
                    return Error::INVALID_DATA_TYPE;
                }
                
                
                if(sample_num == num_samples - 1) {
                    builder.AddTrace(trace);
                }

                // Increment the line number
                line_number++;
            }

            bar.mark_as_completed();
        }

        // Secondly load the plainText
        {
            std::ifstream file(m_PlaintextFilePath);
            if (!file.is_open()) {
                METRISCA_ERROR("Failed to open file at path {} for reading", m_PlaintextFilePath.string());
                return Error::IO_FAILURE;
            }

            std::string line;
            while (std::getline(file, line)) {
                std::vector<uint8_t> plaintext;
                for (size_t i = 0; i != 16; ++i) {
                    uint8_t result = 0;
                    for (size_t j = 0; j != 8; ++j) {
                        result <<= 1;
                        result += (int) (line[8 * i + j] == '1');
                    }
                    plaintext.push_back(result);
                }

                builder.AddPlaintext(std::move(plaintext));
            }
        }

        // Finally add the key
        {
            std::ifstream file(m_KeyFilePath);
            if (!file.is_open()) {
                METRISCA_ERROR("Failed to open file at path {} for reading", m_KeyFilePath.string());
                return Error::IO_FAILURE;
            }

            std::string line;
            if (!std::getline(file, line)) {
                METRISCA_ERROR("Empty key file");
                return Error::IO_FAILURE;
            }

            std::vector<uint8_t> key;
            for (size_t i = 0; i != 16; ++i) {
                uint8_t result = 0;
                for (size_t j = 0; j != 8; ++j) {
                    result <<= 1;
                    result += (int) (line[8 * i + j] == '1');
                }
                key.push_back(result);
            }
            
            builder.AddKey(std::move(key));
        }

        return {};
    }

private:
    fs::path m_DbFilePath{};
    fs::path m_KeyFilePath{};
    fs::path m_PlaintextFilePath{};
};

int main(int argc, char *argv[])
{
    Application& app = Application::The();

    // This line registers the loader into the application so that it can
    // be called using the command 'load'
    // app.RegisterLoader("txtloader", txtloader);
    METRISCA_REGISTER_PLUGIN(TxtLoader, "txtloader");
    typedef metrisca::BinLoader<100000, 535> MyBinLoader;
    METRISCA_REGISTER_PLUGIN(MyBinLoader, "binloader");

    auto result = app.Start(argc, argv);

    if (result.IsError())
        return static_cast<int>(result.Error());

    return 0;
}