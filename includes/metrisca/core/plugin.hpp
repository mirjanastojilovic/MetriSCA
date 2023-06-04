/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/forward.hpp"

#include "metrisca/core/result.hpp"
#include "metrisca/core/errors.hpp"
#include "metrisca/core/arg_list.hpp"

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>

namespace metrisca {

    enum class PluginType : uint32_t {
        Loader = 0,
        PowerModel,
        Profiler,
        Distinguisher,
        Metric,
        Score
    };

    class Plugin {
    public:
        Plugin(PluginType type)
            : m_Type(type)
        {}

        virtual ~Plugin() = default;

        virtual Result<void, Error> Init(const ArgumentList& args) = 0;

        PluginType GetType() { return m_Type; }

    public:
        PluginType m_Type;
    };

    class PluginFactory {
    public:
        ~PluginFactory() = default;

        using Constructor = std::function<std::shared_ptr<Plugin>()>;

        static PluginFactory& The()
        {
            static PluginFactory instance;
            return instance;
        }

        static void Init();

        static void RegisterPlugin(const std::string& name, const Constructor& plugin)
        {
            auto plugin_instance = plugin();
            PluginFactory::The().m_Registry[plugin_instance->GetType()][name] = plugin;
        }

        template<typename T>
        Result<std::shared_ptr<T>, Error> ConstructAs(PluginType type, const std::string& name, const ArgumentList& args) const
        {
            auto plugin = ConstructPlugin<T>(type, name);
            if(!plugin)
                return Error::UNKNOWN_PLUGIN;
            auto init_result = plugin->Init(args);
            if(init_result.IsError())
                return init_result.Error();
            return plugin;
        }

        std::vector<std::string> PluginNamesWithType(PluginType) const;

    private:
        PluginFactory() = default;

        template<typename T>
        std::shared_ptr<T> ConstructPlugin(PluginType type, const std::string& name) const
        {
            auto type_it = m_Registry.find(type);
            if(type_it == m_Registry.end())
                return nullptr;
            auto name_it = type_it->second.find(name);
            if(name_it == type_it->second.end())
                return nullptr;
            return std::dynamic_pointer_cast<T>(name_it->second());
        }

        std::unordered_map<PluginType, std::unordered_map<std::string, Constructor>> m_Registry;
    };

#define METRISCA_REGISTER_PLUGIN(cls, name) \
    ::metrisca::PluginFactory::The().RegisterPlugin(name, []() { return std::make_shared<cls>(); })

}