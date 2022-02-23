/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "argument_parser.hpp"

#include "metrisca/forward.hpp"

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

namespace metrisca {

    /**
     * \brief Represent the CLI application
     */
    class Application {
    public:
        virtual ~Application() = default;

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        static Application& The()
        {
            static Application instance;
            return instance;
        }

        Result<void, Error> Start(int argc, char *argv[]);

        std::shared_ptr<TraceDataset> GetDataset(const std::string& alias) const;

    private:
        Application() = default;

        /// Handlers for CLI commands
        Result<void, Error> HandleQuit(const ArgumentList&);
        Result<void, Error> HandleHelp(const ArgumentList&);
        Result<void, Error> HandleLoad(const ArgumentList&);
        Result<void, Error> HandleUnload(const ArgumentList&);
        Result<void, Error> HandleDatasets(const ArgumentList&);
        Result<void, Error> HandleMetric(const ArgumentList&);
        Result<void, Error> HandleSplit(const ArgumentList&);

        Result<void, Error> HandleCommand(const std::string& input = "");
        Result<void, Error> HandleScript(const std::string& filename);

        struct ScriptExecutionError {
            size_t Line;
            Error Code;
        };
        Result<void, ScriptExecutionError> RunScriptFile(const std::string& filename);

        void RegisterDataset(const std::string& alias, std::shared_ptr<TraceDataset>);
        void RemoveDataset(const std::string& alias);

        using CommandHandler = std::function<Result<void, Error>(const ArgumentList& arguments)>;

        struct Command {
            Command(const ArgumentParser& parser, const CommandHandler& handler, const std::string& short_description)
                : Parser(parser)
                , Handler(handler)
                , ShortDescription(short_description)
            {}

            void AddSubParser(const ArgumentParser& parser) { SubParsers.push_back(parser); }

            std::optional<ArgumentParser> GetSubParser(const std::string& name)
            {
                for (const auto& subparser : SubParsers)
                {
                    if (subparser.Name() == name)
                        return subparser;
                }
                return {};
            }

            CommandHandler Handler;
            ArgumentParser Parser;
            std::vector<ArgumentParser> SubParsers;
            std::string ShortDescription;
        };

        std::shared_ptr<Command> RegisterCommand(ArgumentParser, CommandHandler, const std::string& short_description);
        std::shared_ptr<Command> GetCommand(const std::string& name) const;
        void InitCommands();

    private:
        bool m_Running = false;
        std::unordered_map<std::string, std::shared_ptr<TraceDataset>> m_Datasets;
        std::unordered_map<std::string, std::shared_ptr<Command>> m_Commands;
    };

}
