/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/core/trace_dataset.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/utils/crypto.hpp"
#include "metrisca/utils/numerics.hpp"

#include <fstream>
#include <cassert>
#include <cstdint>

namespace metrisca {

    TraceDatasetHeader TraceDataset::GetHeader() const
    {
        return this->m_Header;
    }

    size_t TraceDataset::Size() const
    {
        return sizeof(TraceDatasetHeader) +
            this->m_Traces.Size() +
            this->m_Plaintexts.Size() +
            this->m_Keys.Size() +
            this->m_Ciphertexts.Size();
    }

    void TraceDataset::SaveToFile(const std::string& filename)
    {
        std::ofstream file;
        file.open(filename, std::ofstream::out | std::ofstream::binary);

        if(file)
        {
            file.write((char*)&this->m_Header, sizeof(TraceDatasetHeader));

            // Write plaintext data
            if(this->m_Header.PlaintextMode == PlaintextGenerationMode::CHAINED)
            {
                // If the plaintext mode is CHAINED, only save the first plaintext
                const auto& first_plaintext = this->m_Plaintexts.GetRow(0);
                file.write((char*)first_plaintext.data(), first_plaintext.size_bytes());
            }
            else
            {
                file.write((char*)this->m_Plaintexts.Data(), m_Plaintexts.Size());
            }

            // Write key data
            file.write((char*)m_Keys.Data(), m_Keys.Size());

            // Write trace data
            file.write((char*)m_Traces.Data(), m_Traces.Size());
        }
        else
        {
            assert(false);
        }
    }
    
    Result<std::shared_ptr<TraceDataset>, Error> TraceDataset::LoadFromFile(const std::string& filename)
    {
        std::shared_ptr<TraceDataset> result = std::make_shared<TraceDataset>();

        std::ifstream file;
        file.open(filename, std::ifstream::in | std::ifstream::binary);

        if(file)
        {
            // Read trace file header
            file.read((char*)&result->m_Header, sizeof(TraceDatasetHeader));
            if(result->m_Header._MagicValue != DATASET_HEADER_MAGIC_VALUE)
            {
                return Error::INVALID_HEADER;
            }

            // Read plaintext data
            uint32_t plaintext_count = 0;
            uint32_t plaintext_load_count = 0;
            switch(result->m_Header.PlaintextMode)
            {
                case PlaintextGenerationMode::FIXED: {
                    plaintext_count = 1;
                    plaintext_load_count = 1;
                } break;
                case PlaintextGenerationMode::RANDOM: {
                    plaintext_count = result->m_Header.NumberOfTraces;
                    plaintext_load_count = result->m_Header.NumberOfTraces;
                } break;
                case PlaintextGenerationMode::CHAINED: {
                    plaintext_count = result->m_Header.NumberOfTraces;
                    plaintext_load_count = 1; // Load only the first plaintext and generate the others
                } break;
                default: {
                    assert(false);
                } break;
            }
            result->m_Plaintexts = Matrix<uint8_t>(result->m_Header.PlaintextSize, plaintext_count);
            file.read((char*)result->m_Plaintexts.Data(), result->m_Header.PlaintextSize * plaintext_load_count * sizeof(uint8_t));

            // Read key data
            uint32_t key_count = 0;
            switch(result->m_Header.KeyMode)
            {
                case KeyGenerationMode::FIXED: {
                    key_count = 1;
                } break;
                default: {
                    assert(false);
                } break;
            }
            result->m_Keys = Matrix<uint8_t>(result->m_Header.KeySize, key_count);
            file.read((char*)result->m_Keys.Data(), result->m_Keys.Size());
            
            // Read trace data
            result->m_Traces = Matrix<int32_t>(result->m_Header.NumberOfTraces, result->m_Header.NumberOfSamples);
            file.read((char*)result->m_Traces.Data(), result->m_Traces.Size());

            // Generate the chained plaintexts if the mode is set to CHAINED
            if(result->m_Header.PlaintextMode == PlaintextGenerationMode::CHAINED) {
                result->GenerateChainedPlaintexts();
            }

            // Generate the ciphertexts
            // NOTE: It would be better to provide the size of the ciphertexts as parameters
            //       even if the plaintext and ciphertext sizes are often the same
            result->m_Ciphertexts = Matrix<uint8_t>(result->m_Header.PlaintextSize, plaintext_count);
            result->GenerateCiphertexts();
        }
        else
        {
            return Error::FILE_NOT_FOUND;
        }

        return result;
    }

    const nonstd::span<const int32_t> TraceDataset::GetSample(uint32_t sample) const
    {
        return this->m_Traces.GetRow(sample);
    }

    const nonstd::span<const uint8_t> TraceDataset::GetPlaintext(uint32_t trace) const
    {
        switch(this->m_Header.PlaintextMode)
        {
        case PlaintextGenerationMode::FIXED: {
            return this->m_Plaintexts.GetRow(0);
        }
        case PlaintextGenerationMode::RANDOM: {
            return this->m_Plaintexts.GetRow(trace);
        }
        case PlaintextGenerationMode::CHAINED: {
            return this->m_Plaintexts.GetRow(trace);
        }
        default: {
            assert(false);
            return nonstd::span<const uint8_t>();
        }
        }
    }

    const nonstd::span<const uint8_t> TraceDataset::GetKey(uint32_t trace) const
    {
        switch(this->m_Header.KeyMode)
        {
        case KeyGenerationMode::FIXED: {
            return this->m_Keys.GetRow(0);
        }
        default: {
            assert(false);
            return nonstd::span<const uint8_t>();
        }
        }
    }

    const nonstd::span<const uint8_t> TraceDataset::GetCiphertext(uint32_t trace) const
    {
        switch (this->m_Header.PlaintextMode)
        {
        case PlaintextGenerationMode::FIXED:
        {
            // NOTE: This would not work if the key is not fixed
            return this->m_Ciphertexts.GetRow(0);
        }
        case PlaintextGenerationMode::RANDOM:
        {
            return this->m_Ciphertexts.GetRow(trace);
        }
        case PlaintextGenerationMode::CHAINED:
        {
            return this->m_Ciphertexts.GetRow(trace);
        }
        default:
        {
            assert(false);
            return nonstd::span<const uint8_t>();
        }
        }
    }

    std::vector<double> TraceDataset::GetMeanSample() const
    {
        std::vector<double> mean;
        mean.resize(this->m_Header.NumberOfSamples);

        for(uint32_t s = 0; s < this->m_Header.NumberOfSamples; ++s)
        {
            mean[s] = numerics::Mean(this->GetSample(s));
        }

        return mean;
    }

    void TraceDataset::SplitDataset(TraceDataset& out1, TraceDataset& out2, uint32_t trace_split) const
    {
        assert(trace_split < m_Header.NumberOfTraces);

        out1.m_Header = m_Header;
        out2.m_Header = m_Header;

        out1.m_Header.NumberOfTraces = trace_split;
        out2.m_Header.NumberOfTraces = m_Header.NumberOfTraces - trace_split;

        // Split and copy the traces
        out1.m_Traces = m_Traces.Submatrix(0, 0, m_Header.NumberOfSamples, out1.m_Header.NumberOfTraces);
        out2.m_Traces = m_Traces.Submatrix(0, out1.m_Header.NumberOfTraces, m_Header.NumberOfSamples, m_Header.NumberOfTraces);

        // Split and copy the plaintexts
        switch (this->m_Header.PlaintextMode)
        {
        case PlaintextGenerationMode::FIXED: {
            out1.m_Plaintexts = m_Plaintexts.Copy();
            out2.m_Plaintexts = m_Plaintexts.Copy();
            out1.m_Ciphertexts = m_Ciphertexts.Copy();
            out2.m_Ciphertexts = m_Ciphertexts.Copy();
        } break;
        case PlaintextGenerationMode::CHAINED: {
            out1.m_Plaintexts = m_Plaintexts.Submatrix(0, 0, out1.m_Header.NumberOfTraces, m_Header.PlaintextSize);
            out2.m_Plaintexts = m_Plaintexts.Submatrix(out1.m_Header.NumberOfTraces, 0, m_Header.NumberOfTraces, m_Header.PlaintextSize);
            // NOTE: Ideally, the ciphertext size should be provided by the user
            out1.m_Ciphertexts = m_Ciphertexts.Submatrix(0, 0, out1.m_Header.NumberOfTraces, m_Header.PlaintextSize);
            out2.m_Ciphertexts = m_Ciphertexts.Submatrix(out1.m_Header.NumberOfTraces, 0, m_Header.NumberOfTraces, m_Header.PlaintextSize);
            // NOTE: We could figure out which plaintext to copy to maintain the chain
            out1.m_Header.PlaintextMode = PlaintextGenerationMode::RANDOM;
            out2.m_Header.PlaintextMode = PlaintextGenerationMode::RANDOM;
        } break;
        case PlaintextGenerationMode::RANDOM: {
            out1.m_Plaintexts = m_Plaintexts.Submatrix(0, 0, out1.m_Header.NumberOfTraces, m_Header.PlaintextSize);
            out2.m_Plaintexts = m_Plaintexts.Submatrix(out1.m_Header.NumberOfTraces, 0, m_Header.NumberOfTraces, m_Header.PlaintextSize);
            // NOTE: Ideally, the ciphertext size should be provided by the user
            out1.m_Ciphertexts = m_Ciphertexts.Submatrix(0, 0, out1.m_Header.NumberOfTraces, m_Header.PlaintextSize);
            out2.m_Ciphertexts = m_Ciphertexts.Submatrix(out1.m_Header.NumberOfTraces, 0, m_Header.NumberOfTraces, m_Header.PlaintextSize);
        } break;
        default: {
            assert(false);
        }
        }

        // Split and copy the keys
        switch (this->m_Header.KeyMode)
        {
        case KeyGenerationMode::FIXED: {
            out1.m_Keys = m_Keys.Copy();
            out2.m_Keys = m_Keys.Copy();
        } break;
        default: {
            assert(false);
        }
        }
    }

    void TraceDataset::GenerateChainedPlaintexts()
    {
        switch(this->m_Header.EncryptionType)
        {
        case EncryptionAlgorithm::S_BOX: {
            const uint8_t key = this->m_Keys(0, 0);
            uint8_t previous_plaintext = this->m_Plaintexts(0, 0);
            for(uint32_t t = 1; t < this->m_Header.NumberOfTraces; t++)
            {
                uint8_t next_plaintext = crypto::sbox::Encrypt(previous_plaintext, key);
                this->m_Plaintexts(t, 0) = next_plaintext;
                previous_plaintext = next_plaintext;
            }
        } break;
        case EncryptionAlgorithm::AES_128: {
            // Copy key and first plaintext from provided user data
            assert(this->m_Keys.GetWidth() == AES128_BLOCK_SIZE);
            assert(this->m_Plaintexts.GetWidth() == AES128_BLOCK_SIZE);
            std::array<uint8_t, AES128_BLOCK_SIZE> key;
            for (size_t i = 0; i < key.size(); ++i)
                key[i] = this->m_Keys(0, i);
            std::array<uint8_t, AES128_BLOCK_SIZE> previous_plaintext;
            for (size_t i = 0; i < previous_plaintext.size(); ++i)
                previous_plaintext[i] = this->m_Plaintexts(0, i);

            // Expand key once for efficiency
            auto expanded_key = crypto::aes128::ExpandKey(key);

            // Generate the chain of plaintexts
            for (uint32_t t = 1; t < this->m_Header.NumberOfTraces; ++t)
            {
                auto next_plaintext = crypto::aes128::Encrypt(previous_plaintext, expanded_key);
                this->m_Plaintexts.SetRow(t, next_plaintext);
                previous_plaintext = next_plaintext;
            }
        } break;
        default: {
            assert(false);
        }
        }
    }

    void TraceDataset::GenerateCiphertexts()
    {
        uint32_t ciphertext_count = 0;
        switch (this->m_Header.PlaintextMode)
        {
        case PlaintextGenerationMode::CHAINED:
        {
            ciphertext_count = this->m_Header.NumberOfTraces;
        } break;
        case PlaintextGenerationMode::RANDOM:
        {
            ciphertext_count = this->m_Header.NumberOfTraces;
        } break;
        case PlaintextGenerationMode::FIXED:
        {
            ciphertext_count = 1;
        } break;
        default:
        {
            assert(false);
            return;
        }
        }

        switch (this->m_Header.EncryptionType)
        {
        case EncryptionAlgorithm::S_BOX:
        {
            const uint8_t key = this->m_Keys(0, 0);
            for (uint32_t c = 0; c < ciphertext_count; ++c)
            {
                auto plaintext = GetPlaintext(c);
                this->m_Ciphertexts.SetRow(c, { crypto::sbox::Encrypt(plaintext[0], key) });
            }
        } break;
        case EncryptionAlgorithm::AES_128:
        {
            std::array<uint8_t, AES128_BLOCK_SIZE> key;
            for (size_t i = 0; i < key.size(); ++i)
                key[i] = this->m_Keys(0, i);
            // Expand key once for efficiency
            auto expanded_key = crypto::aes128::ExpandKey(key);

            std::array<uint8_t, AES128_BLOCK_SIZE> plaintext;
            for (uint32_t c = 0; c < ciphertext_count; ++c)
            {
                auto p = GetPlaintext(c);
                for (size_t i = 0; i < m_Header.PlaintextSize; ++i)
                {
                    plaintext[i] = p[i];
                }
                this->m_Ciphertexts.SetRow(c, crypto::aes128::Encrypt(plaintext, expanded_key));
            }
        } break;
        default:
        {
            assert(false);
        }
        }
    }

    void TraceDatasetBuilder::AddTrace(const std::vector<int32_t>& trace)
    {
        this->m_Traces.insert(this->m_Traces.end(), trace.begin(), trace.end());
    }

    void TraceDatasetBuilder::AddPlaintext(const std::vector<uint8_t>& plaintext)
    {
        this->m_Plaintexts.insert(this->m_Plaintexts.end(), plaintext.begin(), plaintext.end());
    }

    void TraceDatasetBuilder::AddKey(const std::vector<uint8_t>& key)
    {
        this->m_Keys.insert(this->m_Keys.end(), key.begin(), key.end());
    }

    Result<std::shared_ptr<TraceDataset>, Error> TraceDatasetBuilder::Build()
    {
        std::shared_ptr<TraceDataset> result = std::make_shared<TraceDataset>();

        // If the plaintext and key sizes are not provided
        // guess them from the algorithm
        if (this->PlaintextSize == 0)
        {
            switch (this->EncryptionType)
            {
            case EncryptionAlgorithm::S_BOX: this->PlaintextSize = 1; break;
            case EncryptionAlgorithm::AES_128: this->PlaintextSize = 16; break;
            default: assert(false); return Error::INVALID_DATA;
            }
        }

        if (this->KeySize == 0)
        {
            switch (this->EncryptionType)
            {
            case EncryptionAlgorithm::S_BOX: this->KeySize = 1; break;
            case EncryptionAlgorithm::AES_128: this->KeySize = 16; break;
            default: assert(false); return Error::INVALID_DATA;
            }
        }

        // Copy the trace dataset
        // Traces are stored with each sample as a row of the matrix
        if(this->NumberOfTraces * this->NumberOfSamples != this->m_Traces.size())
        {
            return Error::INVALID_DATA;
        }
        result->m_Traces = Matrix<int32_t>(this->NumberOfTraces, this->NumberOfSamples);
        for(size_t s = 0; s < this->NumberOfSamples; ++s)
        {
            std::vector<int32_t> sample(this->NumberOfTraces);
            for(size_t t = 0; t < this->NumberOfTraces; ++t)
            {
                sample[t] = this->m_Traces[t * this->NumberOfSamples + s];
            }
            result->m_Traces.SetRow(s, sample);
        }

        // Copy the plaintext dataset
        uint32_t plaintext_count = 0;
        uint32_t plaintext_copy_count = 0;
        switch(this->PlaintextMode)
        {
            case PlaintextGenerationMode::FIXED: {
                plaintext_count = 1;
                plaintext_copy_count = 1;
            } break;
            case PlaintextGenerationMode::RANDOM: {
                plaintext_count = this->NumberOfTraces;
                plaintext_copy_count = this->NumberOfTraces;
            } break;
            case PlaintextGenerationMode::CHAINED: {
                plaintext_count = this->NumberOfTraces;
                plaintext_copy_count = 1;
            } break;
            default: {
                assert(false);
                return Error::INVALID_DATA;
            } break;
        }
        if(plaintext_copy_count * this->PlaintextSize != this->m_Plaintexts.size())
        {
            return Error::INVALID_DATA;
        }

        result->m_Plaintexts = Matrix<uint8_t>(this->PlaintextSize, plaintext_count);
        for(size_t p = 0; p < plaintext_copy_count; ++p)
        {
            std::vector<uint8_t> plaintext(this->m_Plaintexts.begin() + (p * this->PlaintextSize), this->m_Plaintexts.begin() + ((p+1) * this->PlaintextSize));
            result->m_Plaintexts.SetRow(p, plaintext);
        }

        // Copy the key dataset
        uint32_t key_count = 0;
        switch(this->KeyMode)
        {
            case KeyGenerationMode::FIXED: {
                key_count = 1;
            } break;
            default: {
                assert(false);
                return Error::INVALID_DATA;
            } break;
        }
        if(key_count * this->KeySize != this->m_Keys.size())
        {
            return Error::INVALID_DATA;
        }

        result->m_Keys = Matrix<uint8_t>(this->KeySize, key_count);
        for(size_t k = 0; k < key_count; ++k)
        {
            std::vector<uint8_t> key(this->m_Keys.begin() + (k * this->KeySize), this->m_Keys.begin() + ((k+1) * this->KeySize));
            result->m_Keys.SetRow(k, key);
        }

        // Configure the dataset header
        result->m_Header._MagicValue = DATASET_HEADER_MAGIC_VALUE;
        result->m_Header.CurrentResolution = this->CurrentResolution;
        result->m_Header.TimeResolution    = this->TimeResolution;
        result->m_Header.NumberOfSamples   = this->NumberOfSamples;
        result->m_Header.NumberOfTraces    = this->NumberOfTraces;
        result->m_Header.EncryptionType    = this->EncryptionType;
        result->m_Header.PlaintextSize     = this->PlaintextSize;
        result->m_Header.PlaintextMode     = this->PlaintextMode;
        result->m_Header.KeySize           = this->KeySize;
        result->m_Header.KeyMode           = this->KeyMode;

        // Generate the plaintexts if the mode is set to CHAINED
        if(this->PlaintextMode == PlaintextGenerationMode::CHAINED)
        {
            result->GenerateChainedPlaintexts();
        }

        // Generate the ciphertexts
        // NOTE: It would be better to provide the size of the ciphertexts as parameters
        //       even if the plaintext and ciphertext sizes are often the same
        result->m_Ciphertexts = Matrix<uint8_t>(this->PlaintextSize, plaintext_count);
        result->GenerateCiphertexts();

        return result;
    }

}