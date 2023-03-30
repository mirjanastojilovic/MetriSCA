/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "application.hpp"
#include "argument_parser.hpp"
#include "string_utils.hpp"

#include "metrisca.hpp"

#include <sstream>
#include <functional>
#include <iomanip>
#include <fstream>
#include <iostream>

#include <stdlib.h> // for system(...)


#define BIND_COMMAND_HANDLER(command) std::bind(&command, this, std::placeholders::_1)

namespace metrisca {

    void Application::InitCommands()
    {
        // Quit command
        RegisterCommand({"quit", "Terminate the application."}, BIND_COMMAND_HANDLER(Application::HandleQuit), "Terminate the application.");

        // Clear command
        // Each time I type clear on the console and I get an error I feel bad
        {
            ArgumentParser parser("clear", "Are you tired of cluttered screens and messy command histories? Fear not, for the \"clear\" command is here to save the day!");
            RegisterCommand(parser, [](const ArgumentList& arguments) -> Result<void, Error> {
#if defined(_WIN32)
                system("cls");
#elif defined (__LINUX__) || defined(__gnu_linux__) || defined(__linux__) || defined(__APPLE__)
                system("clear");
#endif
                return {};
            }, "Clear the text on the console so that your screen may rest");
        }

        // Help command
        {
            ArgumentParser parser("help", "Print information about the usage of the application. Can also be used to obtain detailed information about a specific command.");
            parser.AddPositionalArgument("command", ArgumentType::String, "The name of the command to display information about.", false);
            parser.AddPositionalArgument("arg", ArgumentType::String, "Additional argument.", false);
            RegisterCommand(parser, BIND_COMMAND_HANDLER(Application::HandleHelp), "Print information about the usage of the application.");
        }

        // Load command
        {
            ArgumentParser parser("load", "Load a dataset of traces. The user can optionally choose which loading method to use depending on the dataset format. "
                                          "The resulting dataset is given an alias for easy referencing in other commands. It can optionally be saved to disk in "
                                          "an optimized format so that it can be loaded faster after a restart.");
            parser.AddPositionalArgument("file", ArgumentType::String, "The path of the trace file to load.");
            parser.AddPositionalArgument("alias", ArgumentType::String, "An alias name for the dataset. This alias is used in other commands to refer to the dataset.");
            parser.AddOptionArgument("loader", { "-l", "--loader" }, ArgumentType::String, "The identifier of the loader method to use. Default: use the optimized file format loader.", false);
            parser.AddOptionArgument("out", { "-o", "--out" }, ArgumentType::String, "The path of the optimized output file. If the default loader is used, this parameter is ignored.", false);
            RegisterCommand(parser, BIND_COMMAND_HANDLER(Application::HandleLoad), "Load a dataset of traces.");
        }

        // Unload command
        {
            ArgumentParser parser("unload", "Unload a dataset of traces from memory.");
            parser.AddPositionalArgument("alias", ArgumentType::String, "The alias of the dataset to unload.");
            RegisterCommand(parser, BIND_COMMAND_HANDLER(Application::HandleUnload), "Unload a dataset of traces.");
        }

        // Datasets command
        {
            ArgumentParser parser("datasets", "List all loaded datasets along with basic information about their content.");
            RegisterCommand(parser, BIND_COMMAND_HANDLER(Application::HandleDatasets), "List loaded datasets.");
        }

        // Split command
        {
            ArgumentParser parser("split", "Split a dataset into two new datasets with specific number of traces.");
            parser.AddPositionalArgument("dataset", ArgumentType::Dataset, "The alias of the dataset to split.");
            parser.AddPositionalArgument("alias1", ArgumentType::String, "The alias of the first new dataset.");
            parser.AddPositionalArgument("alias2", ArgumentType::String, "The alias of the second new dataset.");
            parser.AddOptionArgument("split", { "-s", "--split" }, ArgumentType::UInt32, "The trace index midpoint at which to split the dataset. Default: #traces / 2 of the original dataset.", false);
            RegisterCommand(parser, BIND_COMMAND_HANDLER(Application::HandleSplit), "Split a dataset in two.");
        }

        // Metric command
        {
            ArgumentParser parser("metric", "Compute various metrics on a given dataset. See 'help metric <METRIC>' for more information about a specific metric.");
            parser.AddPositionalArgument("metric", ArgumentType::String, "The name of the metric to compute");
            // FIXME: This is a bit of a hack in order to display a hint for other arguments that should be handled by
            //        subparsers. It would be nice to have a more integrated way of doing that.
            parser.AddPositionalArgument("args...", ArgumentType::String, "Additional metric arguments.", false);
            auto command = RegisterCommand(parser, BIND_COMMAND_HANDLER(Application::HandleMetric), "Compute metrics on a dataset.");

            // Rank estimation subcommand
            {
                ArgumentParser parser("rank_estimation", "Compute the rank estimation of each byte for a key for an increasing number of traces.", "metric");
                parser.AddPositionalArgument(ARG_NAME_DATASET, ArgumentType::Dataset, "The alias of the dataset to use.");
                parser.AddOptionArgument(ARG_NAME_MODEL, { "-m", "--model" }, ArgumentType::String, "The identifier of the power model to use.");
                parser.AddOptionArgument(ARG_NAME_DISTINGUISHER, { "-d", "--distinguisher" }, ArgumentType::String, "The identifier of the distinguisher to use.");
                parser.AddOptionArgument(ARG_NAME_OUTPUT_FILE, { "-o", "--out" }, ArgumentType::String, "The path of the output CSV file to save the result into.");
                parser.AddOptionArgument(ARG_NAME_TRACE_COUNT, { "-t", "--traces" }, ArgumentType::UInt32, "The maximum number of traces to use during analysis. Default: #traces in the dataset.", false);
                parser.AddOptionArgument(ARG_NAME_TRACE_STEP, { "-ts", "--step" }, ArgumentType::UInt32, "If greater than zero, computes the same metric with an increasing number of traces starting at <STEP> up to <TRACES>", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_START, { "-s", "--start" }, ArgumentType::UInt32, "The index of the first sample to analyse.", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_END, { "-e", "--end" }, ArgumentType::UInt32, "The non-inclusive index of the last sample to analyse. Default: #samples in the dataset.", false);
                parser.AddOptionArgument(ARG_NAME_BIN_COUNT, { "-b", "--bin-count" }, ArgumentType::UInt32, "Number of bin when building the histogram according to the enumeration algorithm", "10000");
                command->AddSubParser(parser);
            }

            // Rank subcommand
            {
                ArgumentParser parser("rank", "Compute the prediction rank of each key for an increasing number of traces.", "metric");
                parser.AddPositionalArgument(ARG_NAME_DATASET, ArgumentType::Dataset, "The alias of the dataset to use.");
                parser.AddOptionArgument(ARG_NAME_MODEL, { "-m", "--model" }, ArgumentType::String, "The identifier of the power model to use.");
                parser.AddOptionArgument(ARG_NAME_DISTINGUISHER, { "-d", "--distinguisher" }, ArgumentType::String, "The identifier of the distinguisher to use.");
                parser.AddOptionArgument(ARG_NAME_OUTPUT_FILE, { "-o", "--out" }, ArgumentType::String, "The path of the output CSV file to save the result into.");
                parser.AddOptionArgument(ARG_NAME_BYTE_INDEX, { "-b", "--byte" }, ArgumentType::UInt32, "The index of the byte to attack.", "0");
                parser.AddOptionArgument(ARG_NAME_TRACE_COUNT, { "-t", "--traces" }, ArgumentType::UInt32, "The maximum number of traces to use during analysis. Default: #traces in the dataset.", false);
                parser.AddOptionArgument(ARG_NAME_TRACE_STEP, { "-ts", "--step" }, ArgumentType::UInt32, "If greater than zero, computes the same metric with an increasing number of traces starting at <STEP> up to <TRACES>", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_START, { "-s", "--start" }, ArgumentType::UInt32, "The index of the first sample to analyse.", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_END, { "-e", "--end" }, ArgumentType::UInt32, "The non-inclusive index of the last sample to analyse. Default: #samples in the dataset.", false);
                command->AddSubParser(parser);
            }

            // Score subcommand
            {
                ArgumentParser parser("score", "Compute the prediction score of each key for an increasing number of traces.", "metric");
                parser.AddPositionalArgument(ARG_NAME_DATASET, ArgumentType::Dataset, "The alias of the dataset to use.");
                parser.AddOptionArgument(ARG_NAME_MODEL, { "-m", "--model" }, ArgumentType::String, "The identifier of the power model to use.");
                parser.AddOptionArgument(ARG_NAME_DISTINGUISHER, { "-d", "--distinguisher" }, ArgumentType::String, "The identifier of the distinguisher to use.");
                parser.AddOptionArgument(ARG_NAME_OUTPUT_FILE, { "-o", "--out" }, ArgumentType::String, "The path of the output CSV file to save the result into.");
                parser.AddOptionArgument(ARG_NAME_BYTE_INDEX, { "-b", "--byte" }, ArgumentType::UInt32, "The index of the byte to attack.", "0");
                parser.AddOptionArgument(ARG_NAME_TRACE_COUNT, { "-t", "--traces" }, ArgumentType::UInt32, "The maximum number of traces to use during analysis. Default: #traces in the dataset.", false);
                parser.AddOptionArgument(ARG_NAME_TRACE_STEP, { "-ts", "--step" }, ArgumentType::UInt32, "If greater than zero, computes the same metric with an increasing number of traces starting at <STEP> up to <TRACES>", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_START, { "-s", "--start" }, ArgumentType::UInt32, "The index of the first sample to analyse.", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_END, { "-e", "--end" }, ArgumentType::UInt32, "The non-inclusive index of the last sample to analyse. Default: #samples in the dataset.", false);
                command->AddSubParser(parser);
            }

            // Guess subcommand
            {
                ArgumentParser parser("guess", "Compute an confidence ordered key guess for an increasing number of traces.", "metric");
                parser.AddPositionalArgument(ARG_NAME_DATASET, ArgumentType::Dataset, "The alias of the dataset to use.");
                parser.AddOptionArgument(ARG_NAME_MODEL, { "-m", "--model" }, ArgumentType::String, "The identifier of the power model to use.");
                parser.AddOptionArgument(ARG_NAME_DISTINGUISHER, { "-d", "--distinguisher" }, ArgumentType::String, "The identifier of the distinguisher to use.");
                parser.AddOptionArgument(ARG_NAME_OUTPUT_FILE, { "-o", "--out" }, ArgumentType::String, "The path of the output CSV file to save the result into.");
                parser.AddOptionArgument(ARG_NAME_BYTE_INDEX, { "-b", "--byte" }, ArgumentType::UInt32, "The index of the byte to attack.", "0");
                parser.AddOptionArgument(ARG_NAME_TRACE_COUNT, { "-t", "--traces" }, ArgumentType::UInt32, "The maximum number of traces to use during analysis. Default: #traces in the dataset.", false);
                parser.AddOptionArgument(ARG_NAME_TRACE_STEP, { "-ts", "--step" }, ArgumentType::UInt32, "If greater than zero, computes the same metric with an increasing number of traces starting at <STEP> up to <TRACES>", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_START, { "-s", "--start" }, ArgumentType::UInt32, "The index of the first sample to analyse.", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_END, { "-e", "--end" }, ArgumentType::UInt32, "The non-inclusive index of the last sample to analyse. Default: #samples in the dataset.", false);
                command->AddSubParser(parser);
            }

            // Guessing entropy subcommand
            {
                ArgumentParser parser("guessing_entropy", "Compute the guessing entropy of a particular key for an increasing number of traces.", "metric");
                parser.AddPositionalArgument(ARG_NAME_DATASET, ArgumentType::Dataset, "The alias of the dataset to use.");
                parser.AddOptionArgument(ARG_NAME_MODEL, { "-m", "--model" }, ArgumentType::String, "The identifier of the power model to use.");
                parser.AddOptionArgument(ARG_NAME_DISTINGUISHER, { "-d", "--distinguisher" }, ArgumentType::String, "The identifier of the distinguisher to use.");
                parser.AddOptionArgument(ARG_NAME_OUTPUT_FILE, { "-o", "--out" }, ArgumentType::String, "The path of the output CSV file to save the result into.");
                parser.AddOptionArgument(ARG_NAME_KNOWN_KEY, { "-k", "--key" }, ArgumentType::UInt8, "The key to compute the guessing entropy for.", "0");
                parser.AddOptionArgument(ARG_NAME_BYTE_INDEX, { "-b", "--byte" }, ArgumentType::UInt32, "The index of the byte to attack.", "0");
                parser.AddOptionArgument(ARG_NAME_TRACE_COUNT, { "-t", "--traces" }, ArgumentType::UInt32, "The maximum number of traces to use during analysis. Default: #traces in the dataset.", false);
                parser.AddOptionArgument(ARG_NAME_TRACE_STEP, { "-ts", "--step" }, ArgumentType::UInt32, "If greater than zero, computes the same metric with an increasing number of traces starting at <STEP> up to <TRACES>", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_START, { "-s", "--start" }, ArgumentType::UInt32, "The index of the first sample to analyse.", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_END, { "-e", "--end" }, ArgumentType::UInt32, "The non-inclusive index of the last sample to analyse. Default: #samples in the dataset.", false);
                command->AddSubParser(parser);
            }

            // Success rate subcommand
            {
                ArgumentParser parser("success_rate", "Compute the binary success rate of order o of recovering a particular key for an increasing number of traces.", "metric");
                parser.AddPositionalArgument(ARG_NAME_DATASET, ArgumentType::Dataset, "The alias of the dataset to use.");
                parser.AddOptionArgument(ARG_NAME_MODEL, { "-m", "--model" }, ArgumentType::String, "The identifier of the power model to use.");
                parser.AddOptionArgument(ARG_NAME_DISTINGUISHER, { "-d", "--distinguisher" }, ArgumentType::String, "The identifier of the distinguisher to use.");
                parser.AddOptionArgument(ARG_NAME_OUTPUT_FILE, { "-o", "--out" }, ArgumentType::String, "The path of the output CSV file to save the result into.");
                parser.AddOptionArgument(ARG_NAME_KNOWN_KEY, { "-k", "--key" }, ArgumentType::UInt8, "The key to compute the success rate for.", "0");
                parser.AddOptionArgument(ARG_NAME_ORDER, { "-or", "--order" }, ArgumentType::UInt32, "The order of the success rate metric.", "1");
                parser.AddOptionArgument(ARG_NAME_BYTE_INDEX, { "-b", "--byte" }, ArgumentType::UInt32, "The index of the byte to attack.", "0");
                parser.AddOptionArgument(ARG_NAME_TRACE_COUNT, { "-t", "--traces" }, ArgumentType::UInt32, "The maximum number of traces to use during analysis. Default: #traces in the dataset.", false);
                parser.AddOptionArgument(ARG_NAME_TRACE_STEP, { "-ts", "--step" }, ArgumentType::UInt32, "If greater than zero, computes the same metric with an increasing number of traces starting at <STEP> up to <TRACES>", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_START, { "-s", "--start" }, ArgumentType::UInt32, "The index of the first sample to analyse.", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_END, { "-e", "--end" }, ArgumentType::UInt32, "The non-inclusive index of the last sample to analyse. Default: #samples in the dataset.", false);
                command->AddSubParser(parser);
            }

            // TTest subcommand
            {
                ArgumentParser parser("ttest", "Compute the Welsch T-Test between two datasets.", "metric");
                parser.AddPositionalArgument(ARG_NAME_FIXED_DATASET, ArgumentType::Dataset, "The alias of a dataset recorded with fixed plaintexts");
                parser.AddPositionalArgument(ARG_NAME_RANDOM_DATASET, ArgumentType::Dataset, "The alias of a dataset recorded with random plaintexts");
                parser.AddOptionArgument(ARG_NAME_OUTPUT_FILE, { "-o", "--out" }, ArgumentType::String, "The path of the output CSV file to save the result into.");
                parser.AddOptionArgument(ARG_NAME_TRACE_COUNT, { "-t", "--traces" }, ArgumentType::UInt32, "The maximum number of traces to use during analysis. Default: #traces in the dataset.", false);
                parser.AddOptionArgument(ARG_NAME_TRACE_STEP, { "-ts", "--step" }, ArgumentType::UInt32, "If greater than zero, computes the same metric with an increasing number of traces starting at <STEP> up to <TRACES>", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_START, { "-s", "--start" }, ArgumentType::UInt32, "The index of the first sample to analyse.", "0");
                parser.AddOptionArgument(ARG_NAME_SAMPLE_END, { "-e", "--end" }, ArgumentType::UInt32, "The non-inclusive index of the last sample to analyse. Default: #samples in the dataset.", false);
                command->AddSubParser(parser);
            }

            // MI subcommand
            {
                ArgumentParser parser("mi", "Compute the MI leakage score of a dataset for a particular key.", "metric");
                parser.AddPositionalArgument(ARG_NAME_DATASET, ArgumentType::Dataset, "The alias of the dataset to use.");
                parser.AddOptionArgument(ARG_NAME_PROFILER, { "-p", "--profiler" }, ArgumentType::String, "The identifier of the profiler to use.");
                parser.AddOptionArgument(ARG_NAME_OUTPUT_FILE, { "-o", "--out" }, ArgumentType::String, "The path of the output CSV file to save the result into.");
                parser.AddOptionArgument(ARG_NAME_KNOWN_KEY, { "-k", "--key" }, ArgumentType::UInt8, "The key to analyse.", "0");
                parser.AddOptionArgument(ARG_NAME_BYTE_INDEX, { "-b", "--byte" }, ArgumentType::UInt32, "The index of the byte to attack.", "0");
                parser.AddOptionArgument(ARG_NAME_INTEGRATION_LOWER_BOUND, { "-l", "--lower" }, ArgumentType::Double, "The integration interval lower bound.", false);
                parser.AddOptionArgument(ARG_NAME_INTEGRATION_UPPER_BOUND, { "-u", "--upper" }, ArgumentType::Double, "The integration interval upper bound.", false);
                parser.AddOptionArgument(ARG_NAME_INTEGRATION_SAMPLE_COUNT, { "-s", "--samples" }, ArgumentType::UInt32, "The integration sample count.", false);
                parser.AddOptionArgument(ARG_NAME_SIGMA, { "-sg", "--sigma" }, ArgumentType::Double, "Overrides the profile's standard deviation. This is useful when performing an analysis on simulated traces.", false);
                command->AddSubParser(parser);
            }

            // PI subcommand
            {
                ArgumentParser parser("pi", "Compute the PI leakage score between a training and a testing dataset for a particular key.", "metric");
                parser.AddPositionalArgument(ARG_NAME_TRAINING_DATASET, ArgumentType::Dataset, "The alias of the training dataset to use.");
                parser.AddPositionalArgument(ARG_NAME_TESTING_DATASET, ArgumentType::Dataset, "The alias of the testing dataset to use.");
                parser.AddOptionArgument(ARG_NAME_PROFILER, { "-p", "--profiler" }, ArgumentType::String, "The identifier of the profiler to use.");
                parser.AddOptionArgument(ARG_NAME_OUTPUT_FILE, { "-o", "--out" }, ArgumentType::String, "The path of the output CSV file to save the result into.");
                parser.AddOptionArgument(ARG_NAME_KNOWN_KEY, { "-k", "--key" }, ArgumentType::UInt8, "The key to analyse.", "0");
                parser.AddOptionArgument(ARG_NAME_BYTE_INDEX, { "-b", "--byte" }, ArgumentType::UInt32, "The index of the byte to attack.", "0");
                parser.AddOptionArgument(ARG_NAME_SIGMA, { "-sg", "--sigma" }, ArgumentType::Double, "Overrides the profile's standard deviation. This is useful when performing an analysis on simulated traces.", false);
                command->AddSubParser(parser);
            }
        }
    }

    std::shared_ptr<TraceDataset> Application::GetDataset(const std::string& alias) const
    {
        auto it = m_Datasets.find(alias);
        if(it == m_Datasets.end()) return nullptr;
        return it->second;
    }

    void Application::RemoveDataset(const std::string& alias)
    {
        auto dataset = GetDataset(alias);
        if (dataset)
        {
            // Force deletion of the underlying memory
            dataset.reset();
            m_Datasets.erase(alias);
        }
    }

    void Application::RegisterDataset(const std::string& alias, std::shared_ptr<TraceDataset> dataset)
    {
        RemoveDataset(alias);
        m_Datasets[alias] = dataset;
    }

    std::shared_ptr<Application::Command> Application::RegisterCommand(ArgumentParser parser, CommandHandler handler, const std::string& short_description)
    {
        auto command = std::make_shared<Command>(parser, handler, short_description);
        m_Commands[parser.Name()] = command;
        return command;
    }

    std::shared_ptr<Application::Command> Application::GetCommand(const std::string& name) const
    {
        auto it = m_Commands.find(name);
        if (it == m_Commands.end()) return nullptr;
        return it->second;
    }

    Result<void, Error> Application::HandleQuit(const ArgumentList& arguments)
    {
        this->m_Running = false;

        return {};
    }

    Result<void, Error> Application::HandleHelp(const ArgumentList& arguments)
    {
        if (arguments.HasArgument("command"))
        {
            auto command_name = arguments.GetString("command").value();
            auto command = GetCommand(command_name);
            if (command)
            {
                auto parser = command->Parser;
                if (arguments.HasArgument("arg") && command->SubParsers.size() > 0)
                {
                    auto subcommand_name = arguments.GetString("arg").value();
                    auto subparser = command->GetSubParser(subcommand_name);
                    if (subparser.has_value())
                        parser = subparser.value();
                    else
                    {
                        std::cout << "Unknown subcommand '" << subcommand_name << "'. See 'help " << command_name << "' for a list of available subcommands." << std::endl << std::endl;
                        return Error::INVALID_ARGUMENT;
                    }
                }
                std::cout << parser.HelpMessage() << std::endl;
            }
            else
            {
                std::cout << "Unknown command '" << command_name << "'. See 'help' for a list of available commands." << std::endl << std::endl;
                return Error::INVALID_COMMAND;
            }

            // Some commands can display some additional information. This is handled here.
            if (command_name == "load")
            {
                std::cout << "List of available loaders:" << std::endl;
                auto loader_names = PluginFactory::The().PluginNamesWithType(PluginType::Loader);
                if (loader_names.size() == 0)
                {
                    std::cout << " no loaders available." << std::endl;
                }
                else
                {
                    for (const auto& name : loader_names)
                    {
                        std::cout << " " << name << std::endl;
                    }
                }
                std::cout << std::endl;
            }
            else if (command_name == "metric" && !arguments.HasArgument("arg"))
            {
                std::cout << "List of available metrics:" << std::endl;
                auto metric_names = PluginFactory::The().PluginNamesWithType(PluginType::Metric);
                if (metric_names.size() == 0)
                {
                    std::cout << " no metrics available." << std::endl;
                }
                else
                {
                    for (const auto& name : metric_names)
                    {
                        std::cout << " " << name << std::endl;
                    }
                }
                std::cout << std::endl;
            }
        }
        else
        {
            std::cout << "MetriSCA " << METRISCA_VERSION << ", a side-channel analysis library." << std::endl << std::endl;
            std::cout << "List of available commands:" << std::endl << std::endl;
            for (const auto& command : m_Commands)
            {
                std::cout << std::left << std::setw(20) << " " + command.first << command.second->ShortDescription << std::endl;
            }
            std::cout << std::endl;
            std::cout << "See 'help <command>' to read about a specific command." << std::endl << std::endl;
        }

        return {};
    }

    Result<void, Error> Application::HandleLoad(const ArgumentList& arguments)
    {
        std::shared_ptr<TraceDataset> dataset;

        if (arguments.HasArgument("loader"))
        {
            auto loader_name = arguments.GetString("loader").value();
            auto loader_or_error = PluginFactory::The().ConstructAs<LoaderPlugin>(PluginType::Loader, loader_name, arguments);
            if (loader_or_error.IsError())
            {
                METRISCA_ERROR("Failed to initialize loader '{}' with error code {}: {}. See 'help load'.", loader_name, loader_or_error.Error(), ErrorCause(loader_or_error.Error()));
                return Error::INVALID_ARGUMENT;
            }

            METRISCA_INFO("Loading file '{}'...", arguments.GetString("file").value());
        
            auto loader = loader_or_error.Value();
            TraceDatasetBuilder builder;
            auto result = loader->Load(builder);
            if(result.IsError())
            {
                METRISCA_ERROR("Loader failed to load dataset and exited with error code {}: {}", result.Error(), ErrorCause(result.Error()));
                return result.Error();
            }

            auto dataset_or_error = builder.Build();
            if(dataset_or_error.IsError())
            {
                METRISCA_ERROR("Loader failed to build dataset and exited with error code {}: {}", dataset_or_error.Error(), ErrorCause(dataset_or_error.Error()));
                return dataset_or_error.Error();
            }

            dataset = dataset_or_error.Value();
        }
        else
        {
            METRISCA_INFO("Loading file '{}'...", arguments.GetString("file").value());

            auto dataset_or_error = TraceDataset::LoadFromFile(arguments.GetString("file").value());
            if(dataset_or_error.IsError())
            {
                METRISCA_ERROR("Loader failed to load dataset and exited with error code {}: {}", dataset_or_error.Error(), ErrorCause(dataset_or_error.Error()));
                return dataset_or_error.Error();
            }

            dataset = dataset_or_error.Value();
        }

        METRISCA_INFO("Loaded {} traces of {} samples as '{}'", dataset->GetHeader().NumberOfTraces, dataset->GetHeader().NumberOfSamples, arguments.GetString("alias").value());

        if(arguments.HasArgument("loader") && arguments.HasArgument("out"))
        {
            auto out_path = arguments.GetString("out").value();
            dataset->SaveToFile(out_path);
            METRISCA_INFO("Saved optimized dataset as '{}'", out_path);
        }

        RegisterDataset(arguments.GetString("alias").value(), dataset);

        return {};
    }

    Result<void, Error> Application::HandleUnload(const ArgumentList& arguments)
    {
        auto alias = arguments.GetString("alias").value();
        if(!GetDataset(alias))
        {
            METRISCA_ERROR("Unknown dataset '{}'. See 'datasets'.", alias);
            return Error::INVALID_ARGUMENT;
        }

        RemoveDataset(alias);

        METRISCA_INFO("Unloaded dataset '{}'.", alias);

        return {};
    }

    Result<void, Error> Application::HandleDatasets(const ArgumentList& arguments)
    {
        if(m_Datasets.size() > 0)
        {
            std::cout << "List of loaded datasets:" << std::endl << std::endl;
            std::cout << std::left << std::setw(20) << "Alias" << std::left << std::setw(10) << "#Traces" << std::left << std::setw(10) << "#Samples" 
                << std::left << std::setw(20) << "Algorithm" << std::left << std::setw(16) << "Size in bytes" << std::endl;
            std::cout << std::setfill('-') << std::setw(76) << "" << std::setfill(' ') << std::endl;
            for(const auto [alias, dataset] : m_Datasets)
            {
                TraceDatasetHeader header = dataset->GetHeader();
                std::cout << std::left << std::setw(20) << alias << std::left << std::setw(10) << header.NumberOfTraces << std::left << std::setw(10) 
                    << header.NumberOfSamples << std::left << std::setw(20) << to_string(header.EncryptionType) << std::left << std::setw(16) << std::to_string(dataset->Size()) << std::endl;
            }
        }
        else
        {
            std::cout << "There are no loaded datasets. See 'help load'." << std::endl << std::endl;
        }

        return {};
    }

    Result<void, Error> Application::HandleMetric(const ArgumentList& arguments)
    {
        METRISCA_ASSERT(arguments.HasArgument("subcommand"));

        auto metric_name = arguments.GetString("subcommand").value();
        auto metric_or_error = PluginFactory::The().ConstructAs<MetricPlugin>(PluginType::Metric, metric_name, arguments);
        if (metric_or_error.IsError())
        {
            METRISCA_ERROR("Failed to initialize metric '{0}' with error code {1}: {2}. See 'help metric {0}'.", metric_name, metric_or_error.Error(), ErrorCause(metric_or_error.Error()));
            return Error::INVALID_ARGUMENT;
        }

        auto metric = metric_or_error.Value();
        auto result = metric->Compute();
        if (result.IsError())
        {
            METRISCA_ERROR("Metric failed to compute and exited with error code {}: {}", result.Error(), ErrorCause(result.Error()));
            return result.Error();
        }

        return {};
    }

    Result<void, Error> Application::HandleSplit(const ArgumentList& arguments)
    {
        auto dataset = arguments.GetDataset("dataset").value();
        TraceDatasetHeader header = dataset->GetHeader();

        uint32_t traceSplit = arguments.GetUInt32("split").value_or((uint32_t)(header.NumberOfTraces / 2));

        if(traceSplit >= header.NumberOfTraces)
        {
            METRISCA_ERROR("Invalid splitting index {}. Dataset has {} traces.", traceSplit, header.NumberOfTraces);
            return Error::INVALID_ARGUMENT;
        }

        METRISCA_INFO("Splitting dataset at index {}.", traceSplit);

        std::shared_ptr<TraceDataset> dataset1 = std::make_shared<TraceDataset>();
        std::shared_ptr<TraceDataset> dataset2 = std::make_shared<TraceDataset>();
        dataset->SplitDataset(*dataset1, *dataset2, traceSplit);

        std::string alias1 = arguments.GetString("alias1").value();
        std::string alias2 = arguments.GetString("alias2").value();

        RegisterDataset(alias1, dataset1);
        RegisterDataset(alias2, dataset2);

        METRISCA_INFO("Created datasets '{}' and '{}'.", alias1, alias2);

        return {};
    }

    Result<void, Error> Application::Start(int argc, char *argv[])
    {
#if defined(DEBUG)
        Logger::Init(LogLevel::Trace);
#else
        Logger::Init(LogLevel::Warn);
#endif
        PluginFactory::Init();
        InitCommands();

        ArgumentParser parser("metrisca", "If no argument is specified, the program will start in 'prompt mode'.");
        parser.SetTitle("MetriSCA " + std::string(METRISCA_VERSION) + ", a side-channel analysis library.");
        parser.AddFlagArgument("help", { "-h", "--help" }, "Print this message.");
        parser.AddOptionArgument("script", { "-s", "--script" }, ArgumentType::String, "Path of a script file to execute.", false);

        std::vector<std::string> args;
        for(int i = 1; i < argc; ++i)
        {
            args.push_back(std::string(argv[i]));
        }

        ArgumentList arguments;
        try 
        {
            arguments = parser.Parse(args);
        }
        catch (ParserException& e)
        {
            std::cout << e.what() << std::endl;
            return Error::INVALID_COMMAND;
        }

        if (arguments.HasArgument("help") && arguments.GetBool("help").value())
        {
            std::cout << parser.HelpMessage();
            return {};
        }

        if (arguments.HasArgument("script"))
        {
            std::string script_path = arguments.GetString("script").value();
            return HandleScript(script_path);
        }

        if(args.size() == 0)
        {
            m_Running = true;
            while(m_Running)
            {
                (void)HandleCommand();
            }
        }

        return {};
    }

    static std::pair<std::string, std::string> ParseLine(const std::string& line)
    {
        std::string trimmed_line = Trim(line);
        std::string command = trimmed_line;
        std::string args;
        for (size_t i = 0; i < trimmed_line.size(); ++i)
        {
            char current = trimmed_line[i];
            if (std::isspace((unsigned char)current))
            {
                command = trimmed_line.substr(0, i);
                args = Trim(trimmed_line.substr(i));
                break;
            }
        }
        return std::make_pair(command, args);
    }

    Result<void, Error> Application::HandleCommand(const std::string& input)
    {
        std::string line;
        if (input.length() == 0)
        {
            std::cout << "metrisca $ ";
            bool hasNextLine;

            do {
                std::string input;
                std::getline(std::cin, input);

                // Check that the line is not empty
                size_t last_idx = input.find_last_not_of(" \t");
                if (last_idx == std::string::npos) {
                    return {};
                }

                // If the line terminates with \ then continue on the next line
                hasNextLine = input[last_idx] == '\\';
                if (hasNextLine)
                {
                    line += input.substr(0, last_idx);
                    std::cout << "...          " << std::flush;
                }
                else
                {
                    line += input;
                }

            } while(hasNextLine);
        }
        else
            line = input;

        auto args = Split(NormalizeWhitespace(Trim(line)));
        if (args.size() > 0)
        {
            auto name = args[0];
            auto command = GetCommand(name);
            if (command)
            {
                // We use the default parser for that command unless we find a matching subparser.
                auto parser = command->Parser;
                std::string subparser_name;
                std::vector<std::string> command_args(args.begin() + 1, args.end());
                if (command->SubParsers.size() > 0 && args.size() > 1)
                {
                    auto subcommand_name = args[1];
                    auto subparser = command->GetSubParser(subcommand_name);
                    if (subparser.has_value())
                    {
                        parser = subparser.value();
                        subparser_name = subcommand_name;
                        command_args = std::vector<std::string>(args.begin() + 2, args.end());
                    }
                    else
                    {
                        std::cout << "Unknown subcommand '" << subcommand_name << "'. See 'help " << parser.Name() << "'." << std::endl << std::endl;
                        return Error::INVALID_COMMAND;
                    }
                }

                ArgumentList arguments;
                try 
                {
                    arguments = parser.Parse(command_args);
                }
                catch (UnknownDatasetException& ude)
                {
                    std::cout << ude.what() << " See 'datasets' for a list of loaded datasets." << std::endl << std::endl;
                    return Error::INVALID_COMMAND;
                }
                catch (ParserException& pe)
                {
                    std::cout << pe.what() << " See 'help " << parser.FullName() << "'." << std::endl << std::endl;
                    return Error::INVALID_COMMAND;
                }

                // If we found a matching subparser, we pass the name of that subparser
                // as an argument in the argument list. This let's us keep track of which subcommand
                // is being used in the command handler.
                if (!subparser_name.empty())
                    arguments.SetString("subcommand", subparser_name);

                auto handler_result = command->Handler(arguments);
                if (handler_result.IsError())
                    return handler_result.Error();
            }
            else
            {
                std::cout << "Invalid command: '" << name << "'. See 'help' for a list of valid commands." << std::endl << std::endl;
                return Error::INVALID_COMMAND;
            }
        }

        return {};
    }

    Result<void, Error> Application::HandleScript(const std::string& filename)
    {
        auto script_result = this->RunScriptFile(filename);
        if(script_result.IsError())
        {
            auto [line, error] = script_result.Error();
            if(line > 0)
            {
                METRISCA_ERROR("Script execution failed at line {} with error code {}: {}.", line, error, ErrorCause(error));
            }
            else
            {
                METRISCA_ERROR("Failed to start script with error code {}: {}.", error, ErrorCause(error));
            }
        }
        else
        {
            METRISCA_INFO("Script execution finished successfully.");
        }

        if (script_result.IsError())
            return script_result.Error().Code;

        return {};
    }

    /// Read a text file and execute commands line by line
    /// Lines starting with the character '#' are ignored as comments
    Result<void, Application::ScriptExecutionError> Application::RunScriptFile(const std::string& filename)
    {
        std::ifstream file;
        file.open(filename);
        size_t lineIndex = 1;
        std::string line;

        if(!file)
            return { { 0, Error::FILE_NOT_FOUND } };

        while(std::getline(file, line))
        {
            if(!line.empty() && line.find('#') != 0)
            {
                auto command_result = HandleCommand(line);
                if(command_result.IsError())
                    return { { lineIndex, command_result.Error() } };
            }
            ++lineIndex;
        }
        
        return {};
    }

}