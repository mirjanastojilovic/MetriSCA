/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/models/identity_model.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/core/matrix.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/utils/crypto.hpp"

namespace metrisca {

    void IdentitySBOX(Matrix<int>& out, std::shared_ptr<TraceDataset> dataset, unsigned int byte_index)
    {
        for (uint32_t t = 0; t < dataset->GetHeader().NumberOfTraces; ++t)
        {
            auto ciphertext = dataset->GetCiphertext(t);
            for (uint32_t k = 0; k < 256; ++k)
            {
                out(k, t) = ciphertext[0];
            }
        }
    }

    void IdentityAES128(Matrix<int>& out, std::shared_ptr<TraceDataset> dataset, unsigned int byte_index)
    {
        for (uint32_t t = 0; t < dataset->GetHeader().NumberOfTraces; ++t)
        {
            auto ciphertext = dataset->GetCiphertext(t);
            for (uint32_t k = 0; k < 256; ++k)
            {
                out(k, t) = crypto::SBoxInverse(k ^ ciphertext[byte_index]);
            }
        }
    }

    Result<Matrix<int32_t>, Error> IdentityModel::Model()
    {
        TraceDatasetHeader header = m_Dataset->GetHeader();

        // Compute model for every combination of plaintext + key
        Matrix<int> result(header.NumberOfTraces, 256);

        switch (header.EncryptionType)
        {
        case EncryptionAlgorithm::S_BOX: {
            IdentitySBOX(result, m_Dataset, m_ByteIndex);
        } break;
        case EncryptionAlgorithm::AES_128: {
            IdentityAES128(result, m_Dataset, m_ByteIndex);
        } break;
        default: return Error::UNSUPPORTED_OPERATION;
        }

        return result;
    }

}