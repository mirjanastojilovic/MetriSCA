/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "app/cli_application.hpp"

#include "version.hpp"

#include "core/errors.hpp"

#include "core/csv_writer.hpp"

#include "metrics/rank_metric.hpp"
#include "metrics/score_metric.hpp"
#include "metrics/guess_metric.hpp"
#include "metrics/guessing_entropy_metric.hpp"
#include "metrics/success_rate_metric.hpp"
#include "metrics/ttest_metric.hpp"
#include "metrics/mi_metric.hpp"
#include "metrics/pi_metric.hpp"

#include "models/hamming_distance_model.hpp"
#include "models/hamming_weight_model.hpp"
#include "models/identity_model.hpp"

#include "distinguishers/pearson_distinguisher.hpp"

#include "profilers/standard_profiler.hpp"

#include <sstream>
#include <functional>
#include <iomanip>
#include <fstream>

#define BIND_COMMAND_HANDLER(command) std::bind(&command, this, std::placeholders::_1)

namespace metrisca {

    static std::string getModelFileName(const std::string& alias, const std::string& model, uint32_t byte_index)
    {
        return alias + "_" + model + "_b" + std::to_string(byte_index) + ".model";
    }

    static std::string getProfileFileName(const std::string& alias, const std::string& profiler, uint32_t key, uint32_t byte_index)
    {
        return alias + "_" + profiler + "_k" + std::to_string(key) + "_b" + std::to_string(byte_index) + ".profile";
    }

    CLIApplication::CLIApplication()
    {
        m_View = std::make_unique<CLIView>();
        m_Model = std::make_unique<CLIModel>();
    }

    int CLIApplication::HandleQuit(const InvocationContext& context)
    {
        this->m_Running = false;

        return SCA_OK;
    }

    int CLIApplication::HandleHelp(const InvocationContext& context)
    {
        CLIView& view = *this->m_View;
        Parameter commandParam = context.GetPositionalParameter(0);
        if(commandParam)
        {
            std::string command;
            if(!commandParam.Get(command))
            {
                view.PrintError("Unknown command '" + command + "'. See 'help'.\n");
                return SCA_INVALID_ARGUMENT;
            }
            if(command == "quit")
            {
                std::stringstream message;
                message << "Terminate the application.\n";
                message << "\n";
                message << "Usage: quit\n";
                view.PrintInfo(message.str());
            }
            else if(command == "help")
            {
                std::stringstream message;
                message << "Print information about the usage of the application. It can also be used to obtain detailed information about a specific command.\n";
                message << "\n";
                message << "Usage: help [<command>] [<arguments>...]\n";
                message << "\n";
                message << " command: Optional, the name of the command to display information about.\n";
                message << " arguments: Optional, additional arguments.\n";
                view.PrintInfo(message.str());
            }
            else if(command == "load")
            {
                std::stringstream message;
                message << "Load a dataset of traces. The user can optionally choose which loading method to use depending on the dataset format. The resulting dataset is given an alias for easy referencing in other commands. It can optionally be saved to disk in an optimized format so that it can be loaded faster after a restart.\n";
                message << "\n";
                message << "Usage: load <file> <alias> [-l <loader_name> | --loader=<loader_name>] [-o <output_file> | --out=<output_file>]\n";
                message << "\n";
                message << " file: The name of the file to load.\n";
                message << " alias: The alias of the dataset.\n";
                message << " -l, --loader: Optional, the identifier of the loader method to use. Default: use the optimized file format loader.\n";
                message << " -o, --out: Optional, the name of the optimized output file. If the default loader is used, this parameter is ignored.\n";
                message << "\n";
                message << "Available loaders:\n";
                auto& loaders = this->m_Model->GetLoaders();
                if(loaders.size() == 0)
                {
                    message << " no loaders available.\n";
                }
                else
                {
                    for(auto const& [name, loader] : loaders)
                    {
                        message << " " << name << "\n";
                    }
                }
                view.PrintInfo(message.str());
            }
            else if(command == "unload")
            {
                std::stringstream message;
                message << "Unload a dataset of traces.\n";
                message << "\n";
                message << "Usage: unload <alias>\n";
                message << "\n";
                message << " alias: The alias of the dataset.\n";
                view.PrintInfo(message.str());
            }
            else if(command == "datasets")
            {
                std::stringstream message;
                message << "List all loaded datasets along with basic information about its content.\n";
                message << "\n";
                message << "Usage: datasets\n";
                view.PrintInfo(message.str());
            }
            else if(command == "metric")
            {
                std::stringstream message;
                Parameter metricParam = context.GetPositionalParameter(1);
                if(metricParam)
                {
                    std::string metric;
                    metricParam.Get(metric);
                    if(!this->m_Model->HasMetric(metric))
                    {
                        view.PrintError("Unknown metric '" + metric + "'. See 'help metric'\n");
                        return SCA_INVALID_ARGUMENT;
                    }

                    if(metric == "rank")
                    {
                        message << "Compute the prediction rank of each key for an increasing amount of traces.\n";
                        message << "\n";
                        message << "Usage: metric rank <dataset> " 
                            "[-m <model> | --model=<model>] " 
                            "[-d <distinguisher> | --distinguisher=<distinguisher>] "
                            "[-o <filepath> | --out=<filepath>] "
                            "[-b <index> | --byte=<index>] "
                            "[-t <count> | --traces=<count>] "
                            "[-ts <step> | --step=<step>] "
                            "[-s <index> | --start=<index>] "
                            "[-e <index> | --end=<index>] "
                            "[-c | --use-cache]\n";
                        message << "\n";
                        message << " dataset: The alias of the dataset to use.\n";
                        message << " -m, --model: The name of the power model to use.\n";
                        message << " -d, --distinguisher: The name of the distinguisher to use.\n";
                        message << " -o, --out: The CSV file name to save the result into.\n";
                        message << " -b, --byte: Optional, the index of the byte to attack. Default: 0.\n";
                        message << " -t, --traces: Optional, the number of traces to use during analysis. Default: all the traces in the dataset.\n";
                        message << " -ts, --step: Optional, if greater than zero, will compute the same metric with an increasing number of traces starting at <step> up to the specified trace count. Default: 0.\n";
                        message << " -s, --start: Optional, the index of the first sample to analyse. Default: 0.\n";
                        message << " -e, --end: Optional, the non-inclusive index of the last sample to analyse. Default: number of samples in the dataset.\n";
                        message << " -c, --use-cache: Optional, if set, the metric will try to load precomputed values from disk. Default: false.\n";
                    }
                    else if(metric == "score")
                    {
                        message << "Compute the prediction score of each key for an increasing amount of traces.\n";
                        message << "\n";
                        message << "Usage: metric score <dataset> " 
                            "[-m <model> | --model=<model>] " 
                            "[-d <distinguisher> | --distinguisher=<distinguisher>] "
                            "[-o <filepath> | --out=<filepath>] "
                            "[-b <index> | --byte=<index>] "
                            "[-t <count> | --traces=<count>] "
                            "[-ts <step> | --step=<step>] "
                            "[-s <index> | --start=<index>] "
                            "[-e <index> | --end=<index>] "
                            "[-c | --use-cache]\n";
                        message << "\n";
                        message << " dataset: The alias of the dataset to use.\n";
                        message << " -m, --model: The name of the power model to use.\n";
                        message << " -d, --distinguisher: The name of the distinguisher to use.\n";
                        message << " -o, --out: The CSV file name to save the result into.\n";
                        message << " -b, --byte: Optional, the index of the byte to attack. Default: 0.\n";
                        message << " -t, --traces: Optional, the number of traces to use during analysis. Default: all the traces in the dataset.\n";
                        message << " -ts, --step: Optional, if greater than zero, will compute the same metric with an increasing number of traces starting at <step> up to the specified trace count. Default: 0.\n";
                        message << " -s, --start: Optional, the index of the first sample to analyse. Default: 0.\n";
                        message << " -e, --end: Optional, the non-inclusive index of the last sample to analyse. Default: number of samples in the dataset.\n";
                        message << " -c, --use-cache: Optional, if set, the metric will try to load precomputed values from disk. Default: false.\n";
                    }
                    else if(metric == "guess")
                    {
                        message << "Compute a key guess ordered by score for an increasing amount of traces.\n";
                        message << "\n";
                        message << "Usage: metric guess <dataset> " 
                            "[-m <model> | --model=<model>] " 
                            "[-d <distinguisher> | --distinguisher=<distinguisher>] "
                            "[-o <filepath> | --out=<filepath>] "
                            "[-b <index> | --byte=<index>] "
                            "[-t <count> | --traces=<count>] "
                            "[-ts <step> | --step=<step>] "
                            "[-s <index> | --start=<index>] "
                            "[-e <index> | --end=<index>] "
                            "[-c | --use-cache]\n";
                        message << "\n";
                        message << " dataset: The alias of the dataset to use.\n";
                        message << " -m, --model: The name of the power model to use.\n";
                        message << " -d, --distinguisher: The name of the distinguisher to use.\n";
                        message << " -o, --out: The CSV file name to save the result into.\n";
                        message << " -b, --byte: Optional, the index of the byte to attack. Default: 0.\n";
                        message << " -t, --traces: Optional, the number of traces to use during analysis. Default: all the traces in the dataset.\n";
                        message << " -ts, --step: Optional, if greater than zero, will compute the same metric with an increasing number of traces starting at <step> up to the specified trace count. Default: 0.\n";
                        message << " -s, --start: Optional, the index of the first sample to analyse. Default: 0.\n";
                        message << " -e, --end: Optional, the non-inclusive index of the last sample to analyse. Default: number of samples in the dataset.\n";
                        message << " -c, --use-cache: Optional, if set, the metric will try to load precomputed values from disk. Default: false.\n";
                    }
                    else if(metric == "guessing_entropy")
                    {
                        message << "Compute the guessing entropy of a particular key for an increasing amount of traces.\n";
                        message << "\n";
                        message << "Usage: metric guessing_entropy <dataset> " 
                            "[-m <model> | --model=<model>] " 
                            "[-d <distinguisher> | --distinguisher=<distinguisher>] "
                            "[-o <filepath> | --out=<filepath>] "
                            "[-k <key> | --key=<key>] "
                            "[-b <index> | --byte=<index>] "
                            "[-t <count> | --traces=<count>] "
                            "[-ts <step> | --step=<step>] "
                            "[-s <index> | --start=<index>] "
                            "[-e <index> | --end=<index>] "
                            "[-c | --use-cache]\n";
                        message << "\n";
                        message << " dataset: The alias of the dataset to use.\n";
                        message << " -m, --model: The name of the power model to use.\n";
                        message << " -d, --distinguisher: The name of the distinguisher to use.\n";
                        message << " -o, --out: The CSV file name to save the result into.\n";
                        message << " -k, --key: Optional, the key to compute the guessing entropy for. Default: 0.\n";
                        message << " -b, --byte: Optional, the index of the byte to attack. Default: 0.\n";
                        message << " -t, --traces: Optional, the number of traces to use during analysis. Default: all the traces in the dataset.\n";
                        message << " -ts, --step: Optional, if greater than zero, will compute the same metric with an increasing number of traces starting at <step> up to the specified trace count. Default: 0.\n";
                        message << " -s, --start: Optional, the index of the first sample to analyse. Default: 0.\n";
                        message << " -e, --end: Optional, the non-inclusive index of the last sample to analyse. Default: number of samples in the dataset.\n";
                        message << " -c, --use-cache: Optional, if set, the metric will try to load precomputed values from disk. Default: false.\n";
                    }
                    else if(metric == "success_rate")
                    {
                        message << "Compute the binary success rate of a particular key for an increasing amount of traces.\n";
                        message << "\n";
                        message << "Usage: metric success_rate <dataset> " 
                            "[-m <model> | --model=<model>] " 
                            "[-d <distinguisher> | --distinguisher=<distinguisher>] "
                            "[-o <filepath> | --out=<filepath>] "
                            "[-k <key> | --key=<key>] "
                            "[-or <order> | --order=<order>] "
                            "[-b <index> | --byte=<index>] "
                            "[-t <count> | --traces=<count>] "
                            "[-ts <step> | --step=<step>] "
                            "[-s <index> | --start=<index>] "
                            "[-e <index> | --end=<index>] "
                            "[-c | --use-cache]\n";
                        message << "\n";
                        message << " dataset: The alias of the dataset to use.\n";
                        message << " -m, --model: The name of the power model to use.\n";
                        message << " -d, --distinguisher: The name of the distinguisher to use.\n";
                        message << " -o, --out: The CSV file name to save the result into.\n";
                        message << " -k, --key: Optional, the key to compute the success rate for. Default: 0.\n";
                        message << " -or, --order: Optional, the order of the success rate metric. Default: 1.\n";
                        message << " -b, --byte: Optional, the index of the byte to attack. Default: 0.\n";
                        message << " -t, --traces: Optional, the number of traces to use during analysis. Default: all the traces in the dataset.\n";
                        message << " -ts, --step: Optional, if greater than zero, will compute the same metric with an increasing number of traces starting at <step> up to the specified trace count. Default: 0.\n";
                        message << " -s, --start: Optional, the index of the first sample to analyse. Default: 0.\n";
                        message << " -e, --end: Optional, the non-inclusive index of the last sample to analyse. Default: number of samples in the dataset.\n";
                        message << " -c, --use-cache: Optional, if set, the metric will try to load precomputed values from disk. Default: false.\n";
                    }
                    else if(metric == "ttest")
                    {
                        message << "Compute the Welsch T-Test between two datasets.\n";
                        message << "\n";
                        message << "Usage: metric ttest <dataset1> <dataset2> "
                            "[-o <filepath> | --out=<filepath>] "
                            "[-t <count> | --traces=<count>] "
                            "[-ts <step> | --step=<step>] "
                            "[-s <index> | --start=<index>] "
                            "[-e <index> | --end=<index>]\n"; 
                        message << "\n";
                        message << " dataset1: The alias of the first dataset to compare.\n";
                        message << " dataset2: The alias of the second dataset to compare.\n";
                        message << " -o, --out: The CSV file name to save the result into.\n";
                        message << " -t, --traces: Optional, the number of traces to use during analysis. Default: all the traces in the dataset.\n";
                        message << " -ts, --step: Optional, if greater than zero, will compute the same metric with an increasing number of traces starting at <step> up to the specified trace count. Default: 0.\n";
                        message << " -s, --start: Optional, the index of the first sample to analyse. Default: 0.\n";
                        message << " -e, --end: Optional, the non-inclusive index of the last sample to analyse. Default: number of samples in the dataset.\n";
                    }
                    else if(metric == "mi")
                    {
                        message << "Compute the MI score of a dataset for a particular key.\n";
                        message << "\n";
                        message << "Usage: metric mi <dataset> "
                            "[-p <profiler> | --profiler=<profiler>] "
                            "[-b <byte> | --byte=<byte>] "
                            "[-k <key> | --key=<key>] "
                            "[-o <filepath> | --out=<filepath>] "
                            "[-l <lower_bound> | --lower=<lower_bound>] "
                            "[-u <upper_bound> | --upper=<upper_bound>] "
                            "[-s <samples> | --samples=<samples>] "
                            "[-sg <sigma> | --sigma=<sigma>]";
                        message << "\n";
                        message << " dataset: The alias of the dataset to use.\n";
                        message << " -p, --profiler: The name of the profiler to use.\n";
                        message << " -o, --out: The CSV file name to save the result into.\n";
                        message << " -b, --byte: Optional, the index of the byte to attack. Default: 0\n";
                        message << " -k, --key: Optional, the key to analyse. Default: 0\n";
                        message << " -l, --lower: Optional, the integration lower bound.\n";
                        message << " -u, --upper: Optional, the integration upper bound.\n";
                        message << " -s, --samples: Optional, the integration sample count.\n";
                        message << " -sg, --sigma: Optional, override the profile's standard deviation. This is useful when performing an analysis on simulated traces.\n";
                    }
                    else if(metric == "pi")
                    {
                        message << "Compute the PI score between a training and a testing dataset for a particular key.\n";
                        message << "\n";
                        message << "Usage: metric pi <training> <testing> "
                            "[-p <profiler> | --profiler=<profiler>] "
                            "[-b <byte> | --byte=<byte>] "
                            "[-k <key> | --key=<key>] "
                            "[-o <filepath> | --out=<filepath>] "
                            "[-sg <sigma> | --sigma=<sigma>]";
                        message << "\n";
                        message << " training: The alias of the training dataset to use.\n";
                        message << " testing: The alias of the testing dataset to use.\n";
                        message << " -p, --profiler: The name of the profiler to use.\n";
                        message << " -o, --out: The CSV file name to save the result into.\n";
                        message << " -b, --byte: Optional, the index of the byte to attack. Default: 0\n";
                        message << " -k, --key: Optional, the key to analyse. Default: 0\n";
                        message << " -sg, --sigma: Optional, override the profile's standard deviation. This is useful when performing an analysis on simulated traces.\n";
                    }
                    else
                    {
                        message << "No information is available for this metric.\n";
                    }
                }
                else
                {
                    message << "Compute various metrics on a given dataset.\n";
                    message << "\n";
                    message << "Usage: metric <metric> [metric_args...]\n";
                    message << "\n";
                    message << " metric: The name of the metric to compute.\n";
                    message << " metric_args: Additional metric arguments.\n";
                    message << "\n";
                    message << "Available metrics:\n";
                    for(auto const& [name, metric] : this->m_Model->GetMetrics())
                    {
                        message << " " << name << "\n";
                    }
                    message << "\n";
                    message << "Additional information about built-in metrics can be obtained using 'help metric <metric>'.\n";
                }
                view.PrintInfo(message.str());
            }
            else if(command == "model")
            {
                std::stringstream message;
                message << "Compute a power model on a given dataset. The output is saved on disk as '<dataset_alias>_<model>_b<byte>.model'.\n";
                message << "\n";
                message << "Usage: model <model> <dataset> [-b <byte> | --byte=<byte>]\n";
                message << "\n";
                message << " model: The name of the model to use.\n";
                message << " dataset: The alias of the dataset.\n";
                message << " -b, --byte: Optional, the byte index to model. Default: 0\n";
                message << "\n";
                message << "Available models:\n";
                for(const auto& [name, model] : this->m_Model->GetModels())
                {
                    message << " " << name << "\n";
                }
                view.PrintInfo(message.str());
            }
            else if(command == "profile")
            {
                std::stringstream message;
                message << "Compute a profile on a given dataset and key. The output is saved on disk as '<dataset_alias>_<profiler>_k<key>_b<byte>.profile'.\n";
                message << "\n";
                message << "Usage: profile <dataset> [-p <profiler> | --profiler=<profiler>] [-k <key> | --key=<key>] [-b <byte> | --byte=<byte>]\n";
                message << "\n";
                message << " dataset: The alias of the dataset.\n";
                message << " -p, --profiler: The name of the profiler to use.\n";
                message << " -k, --key: Optional, the key to compute the profile for. Default: 0\n";
                message << " -b, --byte: Optional, the byte index to model. Default: 0\n";
                message << "\n";
                message << "Available profilers:\n";
                for(const auto& [name, profiler] : this->m_Model->GetProfilers())
                {
                    message << " " << name << "\n";
                }
                view.PrintInfo(message.str());
            }
            else if(command == "split")
            {
                std::stringstream message;
                message << "Split a dataset into two new datasets at a specific number of traces.\n";
                message << "\n";
                message << "Usage: split <dataset> <alias1> <alias2> [-s <count> | --split=<count>]\n";
                message << "\n";
                message << " dataset: The alias of the dataset to split.\n";
                message << " alias1: The alias of the first new dataset.\n";
                message << " alias2: The alias of the second new dataset.\n";
                message << " -s, --split: Optional, the trace index midpoint at which to split the dataset. Default: number of traces divided by 2.\n";
                message << "\n";
                view.PrintInfo(message.str());
            }
            else
            {
                view.PrintError("Unknown command '" + command + "'. See 'help'.\n");
                return SCA_INVALID_ARGUMENT;
            }
        }
        else
        {
            std::stringstream message;
            message << "MetriSCA " << METRISCA_VERSION << ", a side-channel analysis library.\n";
            message << "\n";
            message << "Here is a list of the available commands:\n";
            message << "\n";
            message << std::left << std::setw(20) << " load" << "Load a dataset of traces.\n";
            message << std::left << std::setw(20) << " unload" << "Unload a dataset of traces.\n";
            message << std::left << std::setw(20) << " quit" << "Terminate the application.\n";
            message << std::left << std::setw(20) << " datasets" << "List all loaded datasets.\n";
            message << std::left << std::setw(20) << " metric" << "Compute various metrics on datasets.\n";
            message << std::left << std::setw(20) << " model" << "Compute various models on datasets.\n";
            message << std::left << std::setw(20) << " profile" << "Compute profiles on datasets for specific keys.\n";
            message << std::left << std::setw(20) << " split" << "Split a dataset into two.\n";
            message << "\n";
            message << "See 'help <command>' to read about a specific command.\n";
            view.PrintInfo(message.str());
        }

        return SCA_OK;
    }

    int CLIApplication::HandleLoad(const InvocationContext& context)
    {
        CLIView& view = *this->m_View;

        Parameter fileParam = context.GetPositionalParameter(0);
        Parameter aliasParam = context.GetPositionalParameter(1);
        Parameter loaderParam = context.GetNamedParameter({"l", "loader"});
        Parameter outParam = context.GetNamedParameter({"o", "out"});

        if(!fileParam)
        {
            view.PrintError("Missing file parameter. See 'help load'.\n");
            return SCA_INVALID_COMMAND;
        }
        
        if(!aliasParam)
        {
            view.PrintError("Missing alias parameter. See 'help load'.\n");
            return SCA_INVALID_COMMAND;
        }

        std::string inputFile;
        fileParam.Get(inputFile);
        std::string alias;
        aliasParam.Get(alias);
        
        TraceDataset dataset;
        if(loaderParam)
        {
            std::string loaderName;
            loaderParam.Get(loaderName);

            if(!this->m_Model->HasLoader(loaderName))
            {
                view.PrintError("Unknown loader '" + loaderName + "'. See 'help load'.\n");
                return SCA_INVALID_ARGUMENT;
            }
            LoaderFunction loader = this->m_Model->GetLoader(loaderName);

            view.PrintInfo("Loading " + inputFile + "...");
        
            TraceDatasetBuilder builder;
            int errorCode = loader(builder, inputFile);
            if(errorCode != SCA_OK)
            {
                view.PrintError("Loader failed to load dataset and exited with error code " + std::to_string(errorCode) + ": " + ErrorCause(errorCode) + "\n");
                return errorCode;
            }

            errorCode = builder.Build(dataset);
            if(errorCode != SCA_OK)
            {
                view.PrintError("Loader failed to build dataset and exited with error code " + std::to_string(errorCode) + ": " + ErrorCause(errorCode) + "\n");
                return errorCode;
            }
        }
        else
        {
            view.PrintInfo("Loading " + inputFile + "...");

            int errorCode = dataset.LoadFromFile(inputFile);
            if(errorCode != SCA_OK)
            {
                view.PrintError("Loader failed to load dataset and exited with error code " + std::to_string(errorCode) + ": " + ErrorCause(errorCode) + "\n");
                return errorCode;
            }
        }

        view.PrintInfo("Loaded " + std::to_string(dataset.GetHeader().NumberOfTraces) + " traces of " + std::to_string(dataset.GetHeader().NumberOfSamples) + " samples as " + alias);

        if(loaderParam && outParam)
        {
            std::string outputFile;
            outParam.Get(outputFile);

            dataset.SaveToFile(outputFile);
            view.PrintInfo("Saved optimized dataset as " + outputFile);
        }

        this->m_Model->RegisterDataset(alias, dataset);

        view.PrintInfo("");

        return SCA_OK;
    }

    int CLIApplication::HandleUnload(const InvocationContext& context)
    {
        CLIView& view = *this->m_View;

        Parameter datasetParam = context.GetPositionalParameter(0);
        if(!datasetParam)
        {
            view.PrintError("Missing dataset parameter. See 'help unload'.\n");
            return SCA_INVALID_COMMAND;
        }

        std::string datasetAlias;
        datasetParam.Get(datasetAlias);

        if(!this->m_Model->HasDataset(datasetAlias))
        {
            view.PrintError("Unknown dataset '" + datasetAlias + "'. See 'datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        this->m_Model->UnregisterDataset(datasetAlias);

        view.PrintInfo("Unloaded dataset '" + datasetAlias + "'.\n");

        return SCA_OK;
    }

    int CLIApplication::HandleDatasets(const InvocationContext& context)
    {
        CLIView& view = *this->m_View;
        auto infos = this->m_Model->GetDatasetInfos();
        std::stringstream message;
        if(infos.size() > 0)
        {
            message << "Here is the list of loaded datasets.\n\n";
            message << std::left << std::setw(20) << "Alias" << std::left << std::setw(10) << "Traces" << std::left << std::setw(10) << "Samples" << std::left << std::setw(20) << "Algorithm" << std::left << std::setw(16) << "Size in bytes" << "\n";
            message << std::setfill('-') << std::setw(76) << "" << std::setfill(' ') << "\n";
            for(const auto [alias, info] : infos)
            {
                TraceDatasetHeader header = info.Header;
                message << std::left << std::setw(20) << alias << std::left << std::setw(10) << header.NumberOfTraces << std::left << std::setw(10) << header.NumberOfSamples << std::left << std::setw(20) << to_string(header.EncryptionType) << std::left << std::setw(16) << std::to_string(info.Size) << "\n";
            }
        }
        else
        {
            message << "There are no loaded datasets.\n\n" << "See 'help load'.\n";
        }
        view.PrintInfo(message.str());

        return SCA_OK;
    }

    int CLIApplication::HandleMetric(const InvocationContext& context)
    {
        CLIView& view = *this->m_View;
        CLIModel& model = *this->m_Model;

        Parameter metricParam = context.GetPositionalParameter(0);
        if(!metricParam)
        {
            view.PrintError("Missing metric parameter. See 'help metric'.\n");
            return SCA_INVALID_COMMAND;
        }

        std::string metricName;
        metricParam.Get(metricName);

        if(!model.HasMetric(metricName)) 
        {
            view.PrintError("Unknown metric '" + metricName + "'. See 'help metric'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        MetricFunction metric = model.GetMetric(metricName);

        int metricResult = metric(view, model, context);

        return metricResult;
    }

    int CLIApplication::HandleModel(const InvocationContext& context)
    {
        CLIView& view = *this->m_View;

        Parameter modelParam = context.GetPositionalParameter(0);
        Parameter datasetParam = context.GetPositionalParameter(1);
        Parameter byteParam = context.GetNamedParameter({"b", "byte"});

        if(!modelParam)
        {
            view.PrintError("Missing model parameter. See 'help model'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!datasetParam)
        {
            view.PrintError("Missing dataset parameter. See 'help model'.\n");
            return SCA_INVALID_COMMAND;
        }

        std::string modelName;
        modelParam.Get(modelName);
        std::string datasetAlias;
        datasetParam.Get(datasetAlias);

        if(!this->m_Model->HasModel(modelName))
        {
            view.PrintError("Unknown model '" + modelName + "'. See 'help model'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(!this->m_Model->HasDataset(datasetAlias))
        {
            view.PrintError("Unknown dataset '" + datasetAlias + "'. See 'help datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        ModelFunction model = this->m_Model->GetModel(modelName);
        const TraceDataset& dataset = this->m_Model->GetDataset(datasetAlias);
        TraceDatasetHeader header = dataset.GetHeader();
        uint32_t byteIndex = byteParam.GetOrDefault((uint32_t)0);

        if(byteIndex > header.KeySize || byteIndex > header.PlaintextSize)
        {
            view.PrintError("Invalid byte index. Dataset keys or plaintexts are of size " + std::to_string(header.KeySize) + ".\n");
            return SCA_INVALID_ARGUMENT;
        }

        std::string outputFile = getModelFileName(datasetAlias, modelName, byteIndex);

        view.PrintInfo("Computing model for byte " + std::to_string(byteIndex) + " of dataset " + datasetAlias + "...");

        Matrix<int> modelOut;
        int modelResult = model(modelOut, dataset, byteIndex);
        if(modelResult != SCA_OK)
        {
            view.PrintError("Model failed with error code " + std::to_string(modelResult) + ": " + ErrorCause(modelResult) + ".\n");
            return modelResult;
        }

        int saveResult = modelOut.SaveToFile(outputFile);
        if(saveResult != SCA_OK)
        {
            view.PrintError("Failed to save model to file with path '" + outputFile + "'. Error code " + std::to_string(saveResult) + ": " + ErrorCause(saveResult) + ".\n");
            return saveResult;
        }

        view.PrintInfo("Saved model output in file '" + outputFile + "'.\n");

        return SCA_OK;
    }

    int CLIApplication::HandleProfile(const InvocationContext& context)
    {
        CLIView& view = *this->m_View;

        Parameter datasetParam = context.GetPositionalParameter(0);
        Parameter profilerParam = context.GetNamedParameter({"p", "profiler"});
        Parameter byteParam = context.GetNamedParameter({"b", "byte"});
        Parameter correctKeyParam = context.GetNamedParameter({"k", "key"});

        if(!datasetParam)
        {
            view.PrintError("Missing dataset parameter. See 'help profile'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!profilerParam)
        {
            view.PrintError("Missing profiler parameter. See 'help profile'.\n");
            return SCA_INVALID_COMMAND;
        }

        std::string datasetAlias;
        datasetParam.Get(datasetAlias);
        std::string profilerName;
        profilerParam.Get(profilerName);

        if(!this->m_Model->HasDataset(datasetAlias))
        {
            view.PrintError("Unknown dataset '" + datasetAlias + "'. See 'help datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(!this->m_Model->HasProfiler(profilerName))
        {
            view.PrintError("Unknown profiler '" + profilerName + "'. See 'help profile'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        ProfilerFunction profiler = this->m_Model->GetProfiler(profilerName);
        const TraceDataset& dataset = this->m_Model->GetDataset(datasetAlias);
        TraceDatasetHeader header = dataset.GetHeader();

        uint32_t byte_index = byteParam.GetOrDefault(0);
        uint32_t correct_key = correctKeyParam.GetOrDefault(0);

        if(byte_index > header.KeySize || byte_index > header.PlaintextSize)
        {
            view.PrintError("Invalid byte index. Dataset keys or plaintexts are of size " + std::to_string(header.KeySize) + ".\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(correct_key >= 256)
        {
            view.PrintError("Invalid key. Key must be in the range [0,255].\n");
            return SCA_INVALID_ARGUMENT;
        }

        view.PrintInfo("Computing profile for key " + std::to_string(correct_key) + " and byte " + std::to_string(byte_index) + " of dataset " + datasetAlias + "...");

        Matrix<double> profile;
        int profiler_result = profiler(profile, dataset, correct_key, byte_index);

        if(profile.GetWidth() != 256 || profile.GetHeight() != 2)
        {
            view.PrintError("Profiler produced invalid output. Expected width: 256, height: 2 but got width: " + std::to_string(profile.GetWidth()) + ", height: " + std::to_string(profile.GetHeight()) + ".\n");
            return SCA_INVALID_DATA;
        }

        if(profiler_result != SCA_OK)
        {
            view.PrintError("Profiler failed with error code " + std::to_string(profiler_result) + ": " + ErrorCause(profiler_result) + ".\n");
            return profiler_result;
        }

        std::string outputFile = getProfileFileName(datasetAlias, profilerName, correct_key, byte_index);

        int saveResult = profile.SaveToFile(outputFile);
        if(saveResult != SCA_OK)
        {
            view.PrintError("Failed to save profile to file with path '" + outputFile + "'. Error code " + std::to_string(saveResult) + ": " + ErrorCause(saveResult) + ".\n");
            return saveResult;
        }

        view.PrintInfo("Saved profile output in file '" + outputFile + "'.\n");

        return SCA_OK;
    }

    int CLIApplication::HandleSplit(const InvocationContext& context)
    {
        CLIView& view = *this->m_View;

        Parameter datasetParam = context.GetPositionalParameter(0);
        Parameter alias1Param = context.GetPositionalParameter(1);
        Parameter alias2Param = context.GetPositionalParameter(2);
        Parameter splitParam = context.GetNamedParameter({"s", "split"});

        if(!datasetParam)
        {
            view.PrintError("Missing dataset parameter. See 'help split'.\n");
            return SCA_INVALID_COMMAND;
        }
        
        if(!alias1Param)
        {
            view.PrintError("Missing first alias parameter. See 'help split'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!alias2Param)
        {
            view.PrintError("Missing second alias parameter. See 'help split'.\n");
            return SCA_INVALID_COMMAND;
        }

        std::string datasetAlias;
        datasetParam.Get(datasetAlias);

        if(!this->m_Model->HasDataset(datasetAlias))
        {
            view.PrintError("Unknown dataset '" + datasetAlias + "'. See 'help datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        const TraceDataset& dataset = this->m_Model->GetDataset(datasetAlias);
        TraceDatasetHeader header = dataset.GetHeader();

        uint32_t traceSplit = splitParam.GetOrDefault((uint32_t)(header.NumberOfTraces / 2));

        if(traceSplit >= header.NumberOfTraces)
        {
            view.PrintError("Invalid splitting index " + std::to_string(traceSplit) + ". Dataset has " + std::to_string(header.NumberOfTraces) + " traces.\n");
            return SCA_INVALID_ARGUMENT;
        }

        view.PrintInfo("Splitting dataset '" + datasetAlias + "' at index " + std::to_string(traceSplit) + ".");

        TraceDataset dataset1;
        TraceDataset dataset2;
        dataset.SplitDataset(dataset1, dataset2, traceSplit);

        std::string alias1;
        alias1Param.Get(alias1);
        std::string alias2;
        alias2Param.Get(alias2);

        this->m_Model->RegisterDataset(alias1, dataset1);
        this->m_Model->RegisterDataset(alias2, dataset2);

        view.PrintInfo("Created datasets '" + alias1 + "' and '" + alias2 + "'.\n");

        return SCA_OK;
    }

    int CLIApplication::Start(int argc, char *argv[])
    {
        std::vector<std::string> args;
        for(int i = 0; i < argc; ++i)
        {
            args.push_back(std::string(argv[i]));
        }

        this->m_View->RegisterHandler("quit", BIND_COMMAND_HANDLER(CLIApplication::HandleQuit));
        this->m_View->RegisterHandler("help", BIND_COMMAND_HANDLER(CLIApplication::HandleHelp));
        this->m_View->RegisterHandler("load", BIND_COMMAND_HANDLER(CLIApplication::HandleLoad));
        this->m_View->RegisterHandler("unload", BIND_COMMAND_HANDLER(CLIApplication::HandleUnload));
        this->m_View->RegisterHandler("datasets", BIND_COMMAND_HANDLER(CLIApplication::HandleDatasets));
        this->m_View->RegisterHandler("model", BIND_COMMAND_HANDLER(CLIApplication::HandleModel));
        this->m_View->RegisterHandler("metric", BIND_COMMAND_HANDLER(CLIApplication::HandleMetric));
        this->m_View->RegisterHandler("profile", BIND_COMMAND_HANDLER(CLIApplication::HandleProfile));
        this->m_View->RegisterHandler("split", BIND_COMMAND_HANDLER(CLIApplication::HandleSplit));
        
        this->m_Model->RegisterMetric("rank", metrics::RankMetric);
        this->m_Model->RegisterMetric("score", metrics::ScoreMetric);
        this->m_Model->RegisterMetric("guess", metrics::GuessMetric);
        this->m_Model->RegisterMetric("guessing_entropy", metrics::GuessingEntropyMetric);
        this->m_Model->RegisterMetric("success_rate", metrics::SuccessRateMetric);
        this->m_Model->RegisterMetric("ttest", metrics::TTestMetric);
        this->m_Model->RegisterMetric("mi", metrics::MIMetric);
        this->m_Model->RegisterMetric("pi", metrics::PIMetric);

        this->m_Model->RegisterModel("hamming_distance", models::HammingDistanceModel);
        this->m_Model->RegisterModel("hamming_weight", models::HammingWeightModel);
        this->m_Model->RegisterModel("identity", models::IdentityModel);

        this->m_Model->RegisterDistinguisher("pearson", distinguishers::PearsonDistinguisher);

        this->m_Model->RegisterProfiler("standard", profilers::StandardProfiler);

        for(size_t i = 1; i < args.size(); ++i)
        {
            std::string arg = args[i];
            if(arg == "-h" || arg == "--help")
            {
                this->PrintHelpMessage();
                break;
            }
            else if(arg == "-s")
            {
                if(i < args.size() - 1)
                {
                    std::string scriptFile = args[i + 1];
                    this->HandleScript(scriptFile);
                }
                else
                {
                    this->m_View->PrintError("Script file parameter is not defined. See 'scacli -h'.\n");
                }
                break;
            }
            else if(arg.rfind("--script=", 0) == 0)
            {
                std::string scriptFile = arg.substr(arg.find('=') + 1);
                this->HandleScript(scriptFile);

                break;
            }
            else
            {
                this->m_View->PrintError("Unknown parameter '" + arg + "'. See 'scacli -h'.");
                break;
            }
        }

        if(args.size() <= 1)
        {
            this->m_Running = true;
            while(this->m_Running)
            {
                (void)m_View->HandleCommand();
            }
        }

        return 0;
    }

    void CLIApplication::RegisterModel(const std::string& name, const ModelFunction& model)
    {
        this->m_Model->RegisterModel(name, model);
    }

    void CLIApplication::RegisterLoader(const std::string& name, const LoaderFunction& loader)
    {
        this->m_Model->RegisterLoader(name, loader);
    }

    void CLIApplication::RegisterMetric(const std::string& name, const MetricFunction& metric)
    {
        this->m_Model->RegisterMetric(name, metric);
    }

    void CLIApplication::RegisterDistinguisher(const std::string& name, const DistinguisherFunction& distinguisher)
    {
        this->m_Model->RegisterDistinguisher(name, distinguisher);
    }

    void CLIApplication::RegisterProfiler(const std::string& name, const ProfilerFunction& profiler)
    {
        this->m_Model->RegisterProfiler(name, profiler);
    }

    void CLIApplication::PrintHelpMessage() const
    {
        const CLIView& view = *this->m_View;

        std::stringstream message;
        message << "MetriSCA " << METRISCA_VERSION << ", a side-channel analysis library.\n";
        message << "\n";
        message << "Usage: metrisca [ARGUMENT]\n";
        message << "\n";
        message << "ARGUMENT:\n";
        message << std::left << std::setw(40) << " -h, --help" << "Display this message.\n";
        message << std::left << std::setw(40) << " -s <path>, --script=<path>" << "Run a script file containing commands.\n";
        message << "\n";
        message << "If no argument is specified, the program will start in 'prompt mode'.\n";
        message << "\n";
        view.PrintInfo(message.str());
    }

    void CLIApplication::HandleScript(const std::string& filename)
    {
        const CLIView& view = *this->m_View;
        ScriptExecutionResult result = this->RunScriptFile(filename);
        if(result.Line > 0)
        {
            if(result.ErrorCode != SCA_OK)
            {
                view.PrintError("script execution failed at line " + std::to_string(result.Line) + " with error code " + std::to_string(result.ErrorCode) + ": " + ErrorCause(result.ErrorCode) + ".");
            }
            else
            {
                view.PrintInfo("Script execution finished successfully.");
            }
        }
        else
        {
            view.PrintError("failed to start script with error code " + std::to_string(result.ErrorCode) + ": " + ErrorCause(result.ErrorCode) + ".");
        }
        view.PrintInfo("");
    }

    /// Read a text file and execute commands line by line
    /// Lines starting with the character '#' are ignored as comments
    ScriptExecutionResult CLIApplication::RunScriptFile(const std::string& filename)
    {
        ScriptExecutionResult result = { 0, SCA_OK };
        
        std::ifstream file;
        file.open(filename);
        size_t lineIndex = 1;
        std::string line;
        if(file)
        {
            while(std::getline(file, line))
            {
                result.Line = lineIndex;
                if(!line.empty() && line.find('#') != 0)
                {
                    int errorCode = this->m_View->HandleCommand(line);
                    if(errorCode != SCA_OK)
                    {
                        result.ErrorCode = errorCode;
                        return result;
                    }
                }
                ++lineIndex;
            }
        }
        else
        {
            result.ErrorCode = SCA_FILE_NOT_FOUND;
        }
        
        return result;
    }

}