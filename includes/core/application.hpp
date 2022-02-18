/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _APPLICATION_HPP
#define _APPLICATION_HPP

#include <cstdint>
#include <string>

#include "model.hpp"
#include "fileloader.hpp"
#include "metric.hpp"
#include "distinguisher.hpp"
#include "profiler.hpp"

namespace metrisca {

    /**
     * \brief Represent the main application.
     */
    class Application {
    public:
        Application() = default;
        virtual ~Application() = default;
        
        /// Start the application
        virtual int Start(int argc, char *argv[]) = 0;
    
        /// Methods used to register various custom functions inside the application
        virtual void RegisterModel(const std::string& name, const ModelFunction& model) = 0;
        virtual void RegisterLoader(const std::string& name, const LoaderFunction& loader) = 0;
        virtual void RegisterMetric(const std::string& name, const MetricFunction& metric) = 0;
        virtual void RegisterDistinguisher(const std::string& name, const DistinguisherFunction& distinguisher) = 0;
        virtual void RegisterProfiler(const std::string& name, const ProfilerFunction& profiler) = 0;
    };

}

#endif