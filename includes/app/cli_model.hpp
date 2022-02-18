/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _CLI_MODEL_HPP
#define _CLI_MODEL_HPP

#include "core/trace_dataset.hpp"

#include "core/model.hpp"
#include "core/fileloader.hpp"
#include "core/metric.hpp"
#include "core/distinguisher.hpp"
#include "core/profiler.hpp"

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace metrisca {

    struct DatasetInfo {
        TraceDatasetHeader Header;
        size_t Size;
    };

    /**
     * \brief Represent the main CLI application model.
     *
     * This class stores the data of the application, such as the currently
     * loaded datasets and the various functions that can be performed on
     * those datasets.
     */
    class CLIModel {
    public:
        CLIModel() = default;
        ~CLIModel() = default;

        /// Delete the copy constructor and copy assignment
        CLIModel(const CLIModel&) = delete;
        CLIModel& operator=(const CLIModel&) = delete;

        /// Manage the loaded datasets
        /// The dataset alias is an identifier that persists through the execution of the
        /// application, e.g., it is used when providing a dataset parameter to a function.
        void RegisterDataset(const std::string& alias, const TraceDataset& dataset);
        void UnregisterDataset(const std::string& alias);
        const TraceDataset& GetDataset(const std::string& alias) const;
        DatasetInfo GetDatasetInfo(const std::string& alias) const;
        std::unordered_map<std::string, DatasetInfo> GetDatasetInfos() const;
        bool HasDataset(const std::string& alias) const;

        /// Register various functions into the application
        void RegisterModel(const std::string& name, const ModelFunction& model);
        void RegisterLoader(const std::string& name, const LoaderFunction& loader);
        void RegisterMetric(const std::string& name, const MetricFunction& metric);
        void RegisterDistinguisher(const std::string& name, const DistinguisherFunction& distinguisher);
        void RegisterProfiler(const std::string& name, const ProfilerFunction& profiler);
        
        /// Check for the availabilty of a function by name
        bool HasModel(const std::string& name) const;
        bool HasLoader(const std::string& name) const;
        bool HasMetric(const std::string& name) const;
        bool HasDistinguisher(const std::string& name) const;
        bool HasProfiler(const std::string& name) const;

        /// Access a function using its name
        ModelFunction GetModel(const std::string& name) const;
        LoaderFunction GetLoader(const std::string& name) const;
        MetricFunction GetMetric(const std::string& name) const;
        DistinguisherFunction GetDistinguisher(const std::string& name) const;
        ProfilerFunction GetProfiler(const std::string& name) const;

        /// Query the list of all currently registered functions
        const std::unordered_map<std::string, ModelFunction> GetModels() const;
        const std::unordered_map<std::string, LoaderFunction> GetLoaders() const;
        const std::unordered_map<std::string, MetricFunction> GetMetrics() const;
        const std::unordered_map<std::string, DistinguisherFunction> GetDistinguishers() const;
        const std::unordered_map<std::string, ProfilerFunction> GetProfilers() const;

    private:
        std::unordered_map<std::string, TraceDataset> m_Datasets;

        std::unordered_map<std::string, LoaderFunction> m_Loaders;
        std::unordered_map<std::string, ModelFunction> m_Models;
        std::unordered_map<std::string, MetricFunction> m_Metrics;
        std::unordered_map<std::string, DistinguisherFunction> m_Distinguishers;
        std::unordered_map<std::string, ProfilerFunction> m_Profilers;
    };

}

#endif