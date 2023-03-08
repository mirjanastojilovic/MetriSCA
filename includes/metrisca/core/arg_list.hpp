/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/forward.hpp"

#include <unordered_map>
#include <string>
#include <any>
#include <optional>
#include <memory>
#include <tuple>
#include <vector>

namespace metrisca {

    class ArgumentList {
    public:
        ArgumentList() = default;
        ~ArgumentList() = default;

        std::optional<uint8_t> GetUInt8(const std::string& name) const { return Get<uint8_t>(name); }
        std::optional<int32_t> GetInt32(const std::string& name) const { return Get<int32_t>(name); }
        std::optional<uint32_t> GetUInt32(const std::string& name) const { return Get<uint32_t>(name); }
        std::optional<bool> GetBool(const std::string& name) const { return Get<bool>(name); }
        std::optional<std::shared_ptr<TraceDataset>> GetDataset(const std::string& name) const { return Get<std::shared_ptr<TraceDataset>>(name); }
        std::optional<std::string> GetString(const std::string& name) const { return Get<std::string>(name); }
        std::optional<double> GetDouble(const std::string& name) const { return Get<double>(name); }
        std::optional<std::tuple<uint32_t, uint32_t>> GetTupleUInt32(const std::string& name) const { return Get<std::tuple<uint32_t, uint32_t>>(name); }
        std::optional<std::vector<ArgumentList>> GetSubList(const std::string& name) const { return Get<std::vector<ArgumentList>>(name); }

        void SetUInt8(const std::string& name, uint8_t value) { Set<uint8_t>(name, value); }
        void SetInt32(const std::string& name, int32_t value) { Set<int32_t>(name, value); }
        void SetUInt32(const std::string& name, uint32_t value) { Set<uint32_t>(name, value); }
        void SetBool(const std::string& name, bool value) { Set<bool>(name, value); }
        void SetDataset(const std::string& name, std::shared_ptr<TraceDataset> value) { Set<std::shared_ptr<TraceDataset>>(name, value); }
        void SetString(const std::string& name, std::string value) { Set<std::string>(name, value); }
        void SetDouble(const std::string& name, double value) { Set<double>(name, value); }
        void SetTupleUInt32(const std::string& name, std::tuple<uint32_t, uint32_t> value) { Set<std::tuple<uint32_t, uint32_t>>(name, value); }
        void SetSubList(const std::string& name, const std::vector<ArgumentList>& sublist) { Set<std::vector<ArgumentList>>(name, sublist); }

        bool HasArgument(const std::string& name) const { return m_Arguments.find(name) != m_Arguments.end(); }
        void Clear() { m_Arguments.clear(); }

    private:

        template<typename T>
        std::optional<T> Get(const std::string& name) const
        {
            auto it = m_Arguments.find(name);
            if(it == m_Arguments.end())
                return {};
            try
            {
                return std::any_cast<T>(it->second);
            }
            catch(std::bad_any_cast&)
            {
                return {};
            }
        }

        template<typename T>
        void Set(const std::string& name, T value)
        {
            m_Arguments[name] = value;
        }

    private:
        std::unordered_map<std::string, std::any> m_Arguments;
    };

#define ARG_NAME_SAMPLE_START             "start"
#define ARG_NAME_SAMPLE_END               "end"
#define ARG_NAME_SAMPLE_TUPLE             "start:end"
#define ARG_NAME_DATASET                  "dataset"
#define ARG_NAME_MODEL                    "model"
#define ARG_NAME_DISTINGUISHER            "distinguisher"
#define ARG_NAME_PROFILER                 "profiler"
#define ARG_NAME_TRACE_COUNT              "traces"
#define ARG_NAME_BYTE_INDEX               "byte"
#define ARG_NAME_TRACE_STEP               "step"
#define ARG_NAME_KNOWN_KEY                "key"
#define ARG_NAME_OUTPUT_FILE              "out"
#define ARG_NAME_ORDER                    "order"
#define ARG_NAME_SIGMA                    "sigma"
#define ARG_NAME_INTEGRATION_UPPER_BOUND  "upper"
#define ARG_NAME_INTEGRATION_LOWER_BOUND  "lower"
#define ARG_NAME_INTEGRATION_SAMPLE_COUNT "samples"
#define ARG_NAME_TRAINING_DATASET         "training"
#define ARG_NAME_TESTING_DATASET          "testing"
#define ARG_NAME_FIXED_DATASET            "fixed"
#define ARG_NAME_RANDOM_DATASET           "random"
#define ARG_NAME_ENUMERATED_KEY_COUNT     "enumerated-key-count"
#define ARG_NAME_SUBKEY                   "subkey"
#define ARG_NAME_BIN_SIZE                 "bin-size"
}