/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/core/logger.hpp"

#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <vector>

namespace metrisca {

    std::shared_ptr<spdlog::logger> Logger::s_Logger = nullptr;

    void Logger::Init(LogLevel level)
    {
        std::vector<spdlog::sink_ptr> sinks;
        
        sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        sinks[0]->set_pattern("%^[%T] %n: %v%$");
        return Init(level, sinks);
    }

    void Logger::Init(LogLevel level, std::ostream& ostream)
    {
        std::vector<spdlog::sink_ptr> sinks;

        sinks.emplace_back(std::make_shared<spdlog::sinks::ostream_sink_mt>(ostream));
        sinks[0]->set_pattern("[%T] [%l] %n: %v");
        return Init(level, sinks);
    }

    void Logger::Init(LogLevel level, const std::vector<spdlog::sink_ptr>& sinks)
    {
        s_Logger = std::make_shared<spdlog::logger>("METRISCA", sinks.begin(), sinks.end());
        s_Logger->set_level(static_cast<spdlog::level::level_enum>(level));
        s_Logger->flush_on(static_cast<spdlog::level::level_enum>(level));
    }

}