/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "app/cli_view.hpp"

#include "core/errors.hpp"

#include <iostream>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace metrisca {

    InvocationContext::InvocationContext(std::string line)
    {
        std::vector<std::string> tokens;
        size_t pos = 0;
        std::string token;
        bool finished = false;
        while(!finished)
        {
            pos = line.find(' ');
            if(pos != std::string::npos) 
            {
                token = line.substr(0, pos);
                line.erase(0, pos + 1);
            }
            else
            {
                token = line;
                finished = true;
            }
            
            if(token.length() > 0)
            {
                tokens.push_back(token);
            }
        }

        for(size_t t = 0; t < tokens.size(); ++t)
        {
            std::string token = tokens[t];
            if(token.rfind("--", 0) == 0)
            {
                size_t delimiter = token.rfind('=');
                std::string name = token.substr(2, delimiter-2);
                std::string value = "true";
                if(delimiter != std::string::npos)
                {
                    value = token.substr(delimiter + 1);
                }
                this->m_NamedParameters[name] = Parameter(value);
            }
            else if(token.rfind("-", 0) == 0)
            {
                std::string name = token.substr(1);
                std::string value = "true";
                if(t < tokens.size()-1)
                {
                    std::string next_token = tokens[t+1];
                    if(next_token.rfind("--", 0) != 0 && next_token.rfind("-", 0) != 0)
                    {
                        value = next_token;
                        t++;
                    }
                }
                this->m_NamedParameters[name] = Parameter(value);
            }
            else
            {
                this->m_PositionalParameters.emplace_back(token);
            }
        }
    }

    Parameter InvocationContext::GetPositionalParameter(uint32_t position) const
    {
        if(position < this->m_PositionalParameters.size())
        {
            return this->m_PositionalParameters[position];
        }
        else
        {
            return Parameter();
        }
    }

    Parameter InvocationContext::GetNamedParameter(const std::string& name) const
    {
        auto it = this->m_NamedParameters.find(name);
        if (it != this->m_NamedParameters.end())
        {
            return it->second;
        }
        else
        {
            return Parameter();
        }
    }

    Parameter InvocationContext::GetNamedParameter(const std::initializer_list<std::string>& names) const
    {
        for(const auto& name : names) 
        {
            auto it = this->m_NamedParameters.find(name);
            if (it != this->m_NamedParameters.end())
            {
                return it->second;
            }
        }
        return Parameter();
    }

    void CLIView::RegisterHandler(const std::string& command, const CommandHandler& handler)
    {
        this->m_Handlers[command] = handler;
    }

    void CLIView::PrintInfo(const std::string& text) const
    {
        std::cout << text << std::endl;
    }

    void CLIView::PrintWarning(const std::string& warning) const
    {
        std::cout << "Warning: " << warning << std::endl;
    }

    void CLIView::PrintError(const std::string& error) const
    {
        std::cout << "Error: " << error << std::endl;
    }

    int CLIView::HandleCommand(const std::string& input)
    {
        std::string line;
        if(input.length() == 0)
        {
            std::cout << "metrisca>";

            std::getline(std::cin, line);
        }
        else
        {
            line = input;
        }

        for(size_t c = 0; c < line.size(); ++c)
        {
            if(std::isspace((unsigned char)line[c])) { line[c] = ' '; }
        }

        size_t delimiter = line.find(' ');
        std::string command = line.substr(0, delimiter);
        if(command.length() > 0)
        {
            std::string args = line.substr(command.length());
            InvocationContext context(args);
            auto handler = this->m_Handlers.find(command);
            if(handler != this->m_Handlers.end())
            {
                int errorCode = handler->second(context);
                if(errorCode != SCA_OK)
                {
                    return errorCode;
                }
            }
            else
            {
                std::cout << "metrisca: '" << command << "' is not a valid command. See 'help'." << std::endl << std::endl;
                return SCA_INVALID_COMMAND;
            }
        }

        return SCA_OK;
    }

}