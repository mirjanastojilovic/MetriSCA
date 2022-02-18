/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "app/cli_model.hpp"

#include <cassert>

namespace metrisca {

    void CLIModel::RegisterDataset(const std::string& alias, const TraceDataset& dataset)
    {
        this->m_Datasets[alias] = dataset;
    }

    void CLIModel::UnregisterDataset(const std::string& alias)
    {
        this->m_Datasets.erase(alias);
    }

    const TraceDataset& CLIModel::GetDataset(const std::string& alias) const
    {
        auto dataset = this->m_Datasets.find(alias);
        assert(dataset != this->m_Datasets.end());
        return dataset->second;
    }

    DatasetInfo CLIModel::GetDatasetInfo(const std::string& alias) const
    {
        auto dataset = this->GetDataset(alias);
        return DatasetInfo{ dataset.GetHeader(), dataset.Size() };
    }

    std::unordered_map<std::string, DatasetInfo> CLIModel::GetDatasetInfos() const
    {
        std::unordered_map<std::string, DatasetInfo> infos;
        for(const auto& [alias, dataset] : this->m_Datasets)
        {
            infos[alias] = DatasetInfo { dataset.GetHeader(), dataset.Size() };
        }
        return infos;
    }

    bool CLIModel::HasDataset(const std::string& alias) const
    {
        return this->m_Datasets.find(alias) != this->m_Datasets.end();
    }

    void CLIModel::RegisterModel(const std::string& name, const ModelFunction& model)
    {
        this->m_Models[name] = model;
    }

    void CLIModel::RegisterLoader(const std::string& name, const LoaderFunction& loader)
    {
        this->m_Loaders[name] = loader;
    }

    void CLIModel::RegisterMetric(const std::string& name, const MetricFunction& metric)
    {
        this->m_Metrics[name] = metric;
    }

    void CLIModel::RegisterDistinguisher(const std::string& name, const DistinguisherFunction& distinguisher)
    {
        this->m_Distinguishers[name] = distinguisher;
    }

    void CLIModel::RegisterProfiler(const std::string& name, const ProfilerFunction& profiler)
    {
        this->m_Profilers[name] = profiler;
    }

    bool CLIModel::HasModel(const std::string& name) const
    {
        return this->m_Models.find(name) != this->m_Models.end();
    }

    bool CLIModel::HasLoader(const std::string& name) const
    {
        return this->m_Loaders.find(name) != this->m_Loaders.end();
    }

    bool CLIModel::HasMetric(const std::string& name) const
    {
        return this->m_Metrics.find(name) != this->m_Metrics.end();
    }

    bool CLIModel::HasDistinguisher(const std::string& name) const
    {
        return this->m_Distinguishers.find(name) != this->m_Distinguishers.end();
    }

    bool CLIModel::HasProfiler(const std::string& name) const
    {
        return this->m_Profilers.find(name) != this->m_Profilers.end();
    }

    ModelFunction CLIModel::GetModel(const std::string& name) const
    {
        auto model = this->m_Models.find(name);
        assert(model != this->m_Models.end());
        return model->second;
    }

    LoaderFunction CLIModel::GetLoader(const std::string& name) const
    {
        auto loader = this->m_Loaders.find(name);
        assert(loader != this->m_Loaders.end());
        return loader->second;
    }

    MetricFunction CLIModel::GetMetric(const std::string& name) const
    {
        auto metric = this->m_Metrics.find(name);
        assert(metric != this->m_Metrics.end());
        return metric->second;
    }

    DistinguisherFunction CLIModel::GetDistinguisher(const std::string& name) const
    {
        auto distinguisher = this->m_Distinguishers.find(name);
        assert(distinguisher != this->m_Distinguishers.end());
        return distinguisher->second;
    }

    ProfilerFunction CLIModel::GetProfiler(const std::string& name) const
    {
        auto profiler = this->m_Profilers.find(name);
        assert(profiler != this->m_Profilers.end());
        return profiler->second;
    }

    const std::unordered_map<std::string, ModelFunction> CLIModel::GetModels() const
    {
        return this->m_Models;
    }

    const std::unordered_map<std::string, LoaderFunction> CLIModel::GetLoaders() const
    {
        return this->m_Loaders;
    }

    const std::unordered_map<std::string, MetricFunction> CLIModel::GetMetrics() const
    {
        return this->m_Metrics;
    }

    const std::unordered_map<std::string, DistinguisherFunction> CLIModel::GetDistinguishers() const
    {
        return this->m_Distinguishers;
    }

    const std::unordered_map<std::string, ProfilerFunction> CLIModel::GetProfilers() const
    {
        return this->m_Profilers;
    }

}