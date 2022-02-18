/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca.hpp"

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

int main(int argc, char *argv[])
{
    Application* app = new CLIApplication();

    // This line registers the loader into the application so that it can
    // be called using the command 'load'
    // app->RegisterLoader("txtloader", txtloader);

    int result = app->Start(argc, argv);

    delete app;

    return result;
}