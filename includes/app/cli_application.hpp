/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _CLI_APPLICATION_HPP
#define _CLI_APPLICATION_HPP

#include "core/application.hpp"
#include "cli_view.hpp"
#include "cli_model.hpp"

#include <string>
#include <memory>

namespace metrisca {

    struct ScriptExecutionResult {
        size_t Line;
        int ErrorCode;
    };

    /**
     * \brief Represent the CLI application
     */
    class CLIApplication : public Application {
    public:
        CLIApplication();
        virtual ~CLIApplication() = default;

        virtual int Start(int argc, char *argv[]) override;

        /// Methods used to register various custom functions inside the application
        virtual void RegisterModel(const std::string& name, const ModelFunction& model) override;
        virtual void RegisterLoader(const std::string& name, const LoaderFunction& loader) override;
        virtual void RegisterMetric(const std::string& name, const MetricFunction& metric) override;
        virtual void RegisterDistinguisher(const std::string& name, const DistinguisherFunction& distinguisher) override;
        virtual void RegisterProfiler(const std::string& name, const ProfilerFunction& profiler) override;

    private:
        /// Handlers for CLI commands
        int HandleQuit(const InvocationContext& context);
        int HandleHelp(const InvocationContext& context);
        int HandleLoad(const InvocationContext& context);
        int HandleUnload(const InvocationContext& context);
        int HandleDatasets(const InvocationContext& context);
        int HandleMetric(const InvocationContext& context);
        int HandleModel(const InvocationContext& context);
        int HandleProfile(const InvocationContext& context);
        int HandleSplit(const InvocationContext& context);

        void PrintHelpMessage() const;
        void HandleScript(const std::string& filename);

        ScriptExecutionResult RunScriptFile(const std::string& filename);

    private:
        std::unique_ptr<CLIView> m_View;
        std::unique_ptr<CLIModel> m_Model;
        bool m_Running = false;
    };

}

#endif