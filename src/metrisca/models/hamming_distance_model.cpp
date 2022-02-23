/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/models/hamming_distance_model.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/core/matrix.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/utils/crypto.hpp"

namespace metrisca {

    void HammingDistanceSBOX(Matrix<int>& out, std::shared_ptr<TraceDataset> dataset, unsigned int byte_index)
    {
        uint8_t sbox0 = crypto::SBox(0);
        for (uint32_t t = 0; t < dataset->GetHeader().NumberOfTraces; ++t)
        {
            auto plaintext = dataset->GetPlaintext(t);
            for (uint32_t k = 0; k < 256; ++k)
            {
                out(k, t) = crypto::HammingDistance(sbox0, crypto::SBox(plaintext[byte_index] ^ k));
            }
        }
    }

    void HammingDistanceAES128(Matrix<int>& out, std::shared_ptr<TraceDataset> dataset, unsigned int byte_index)
    {
        for (uint32_t t = 0; t < dataset->GetHeader().NumberOfTraces; ++t)
        {
            auto ciphertext = dataset->GetCiphertext(t);
            for (uint32_t k = 0; k < 256; ++k)
            {
                out(k, t) = crypto::HammingDistance(ciphertext[crypto::aes128::ShiftRowIndex(byte_index)], crypto::SBoxInverse(k ^ ciphertext[byte_index]));
            }
        }
    }

    Result<Matrix<int32_t>, Error> HammingDistanceModel::Model()
    {
        TraceDatasetHeader header = m_Dataset->GetHeader();
        
        // Compute model for every key and traces
        Matrix<int> result(header.NumberOfTraces, 256);
        switch (header.EncryptionType)
        {
        case EncryptionAlgorithm::S_BOX: {
            HammingDistanceSBOX(result, m_Dataset, m_ByteIndex);
        } break;
        case EncryptionAlgorithm::AES_128: {
            HammingDistanceAES128(result, m_Dataset, m_ByteIndex);
        } break;
        default: return Error::UNSUPPORTED_OPERATION;
        }

        return result;
    }

}