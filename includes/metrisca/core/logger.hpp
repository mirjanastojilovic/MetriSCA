/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#pragma warning(pop)

#include <ostream>
#include <iostream>
#include <vector>

namespace metrisca {

    enum class LogLevel : uint32_t {
        Off = SPDLOG_LEVEL_OFF,
        Trace = SPDLOG_LEVEL_TRACE,
        Info = SPDLOG_LEVEL_INFO,
        Warn = SPDLOG_LEVEL_WARN,
        Error = SPDLOG_LEVEL_ERROR,
        Critical = SPDLOG_LEVEL_CRITICAL
    };

    class Logger {
    public:
        ~Logger() = default;

        static void Init(LogLevel level);
        static void Init(LogLevel level, std::ostream& os);

        static std::shared_ptr<spdlog::logger> The()
        {
            if (!s_Logger)
                Init(LogLevel::Trace);
            return s_Logger;
        }

    private:
        static void Init(LogLevel level, const std::vector<spdlog::sink_ptr>& sinks);

    private:
        static std::shared_ptr<spdlog::logger> s_Logger;
    };

#define METRISCA_TRACE(...)    ::metrisca::Logger::The()->trace(__VA_ARGS__)
#define METRISCA_INFO(...)     ::metrisca::Logger::The()->info(__VA_ARGS__)
#define METRISCA_WARN(...)     ::metrisca::Logger::The()->warn(__VA_ARGS__)
#define METRISCA_ERROR(...)    ::metrisca::Logger::The()->error(__VA_ARGS__)
#define METRISCA_CRITICAL(...) ::metrisca::Logger::The()->critical(__VA_ARGS__)

}