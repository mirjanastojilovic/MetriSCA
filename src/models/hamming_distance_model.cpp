/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "hamming_distance_model.hpp"

#include "core/errors.hpp"
#include "core/matrix.hpp"
#include "core/trace_dataset.hpp"
#include "utils/crypto.hpp"

namespace metrisca {

    void HammingDistanceSBOX(Matrix<int>& out, const TraceDataset& dataset, unsigned int byte_index)
    {
        uint8_t sbox0 = crypto::SBox(0);
        for(uint32_t t = 0; t < dataset.GetHeader().NumberOfTraces; ++t)
        {
            auto plaintext = dataset.GetPlaintext(t);
            for(uint32_t k = 0; k < 256; ++k)
            {
                out(k, t) = crypto::HammingDistance(sbox0, crypto::SBox(plaintext[byte_index] ^ k));
            }
        }
    }

    void HammingDistanceAES128(Matrix<int>& out, const TraceDataset& dataset, unsigned int byte_index)
    {
        for(uint32_t t = 0; t < dataset.GetHeader().NumberOfTraces; ++t)
        {
            auto ciphertext = dataset.GetCiphertext(t);
            for(uint32_t k = 0; k < 256; ++k)
            {
                out(k, t) = crypto::HammingDistance(ciphertext[crypto::aes128::ShiftRowIndex(byte_index)], crypto::SBoxInverse(k ^ ciphertext[byte_index]));
            }
        }
    }

    int models::HammingDistanceModel(Matrix<int>& out, const TraceDataset& dataset, unsigned int byte_index)
    {
        TraceDatasetHeader header = dataset.GetHeader();

        // Compute model for every key and traces
        out = Matrix<int>(header.NumberOfTraces, 256);

        switch(header.EncryptionType)
        {
        case EncryptionAlgorithm::S_BOX: {
            HammingDistanceSBOX(out, dataset, byte_index);
        } break;
        case EncryptionAlgorithm::AES_128: {
            HammingDistanceAES128(out, dataset, byte_index);
        } break;
        default: return SCA_UNSUPPORTED_OPERATION;
        }

        return SCA_OK;
    }

}