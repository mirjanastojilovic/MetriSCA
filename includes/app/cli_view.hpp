/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _CLI_VIEW_HPP
#define _CLI_VIEW_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <ostream>
#include <initializer_list>
#include <sstream>

namespace metrisca {

    /**
     * \brief Represent the value of a command line argument
     *
     * This class can be used to convert this value into various useful types
     */
    class Parameter {
    public:
        Parameter(): m_Exist(false) {}
        explicit Parameter(const std::string& value): m_Value(value), m_Exist(true) {}
        ~Parameter() = default;

        /**
         * \brief Convert the value of the parameter as a generic type
         * This method cannot fail if the parameter exists.
         */
        template<typename T>
        bool Get(T& out)
        {
            if(this->m_Exist)
            {
                std::stringstream stream(this->m_Value);
                stream >> out;
                return true;
            }
            return false;
        }
        
        /**
         * \brief Convert the value of the parameter as a generic type
         * This method returns a default value if the parameter does not exist.
         */
        template<typename T>
        T GetOrDefault(const T& def)
        {
            if(this->m_Exist)
            {
                std::stringstream stream(this->m_Value);
                T result;
                stream >> result;
                return result;
            }
            return def;
        }

        /**
         * \brief Convert the value of the parameter as a string
         * This method cannot fail if the parameter exists.
         */
        bool Get(std::string& out)
        {
            if(this->m_Exist)
            {
                out = this->m_Value;
                return true;
            }
            return false;
        }

        /**
         * \brief Convert the value of the parameter as a string
         * This method returns a default value if the parameter does not exist.
         */
        std::string GetOrDefault(const std::string& def)
        {
            if(this->m_Exist)
            {
                return this->m_Value;
            }
            return def;
        }

        /**
         * \brief Convert the value of the parameter as an int
         */
        bool Get(int& out)
        {
            if(this->m_Exist)
            {
                try
                {
                    int result = std::stoi(this->m_Value);
                    out = result;
                }
                catch(...)
                {
                    return false;
                }
                return true;
            }
            return false;
        }

        /**
         * \brief Convert the value of the parameter as an int
         * This method returns a default value if the parameter does not exist.
         */
        int GetOrDefault(int def)
        {
            if(this->m_Exist)
            {
                try
                {
                    int result = std::stoi(this->m_Value);
                    return result;
                }
                catch(...)
                {
                    return def;
                }
            }
            return def;
        }

        /**
         * \brief Convert the value of the parameter as an double
         */
        bool Get(double& out)
        {
            if(this->m_Exist)
            {
                try
                {
                    double result = std::stod(this->m_Value);
                    out = result;
                }
                catch(...)
                {
                    return false;
                }
                return true;
            }
            return false;
        }

        /**
         * \brief Convert the value of the parameter as an double
         * This method returns a default value if the parameter does not exist.
         */
        double GetOrDefault(double def)
        {
            if(this->m_Exist)
            {
                try
                {
                    double result = std::stod(this->m_Value);
                    return result;
                }
                catch(...)
                {
                    return def;
                }
            }
            return def;
        }

        /**
         * \brief Convert the value of the parameter as an unsigned int
         */
        bool Get(unsigned int& out)
        {
            if(this->m_Exist)
            {
                try
                {
                    unsigned int result = std::stoul(this->m_Value);
                    out = result;
                }
                catch(...)
                {
                    return false;
                }
                return true;
            }
            return false;
        }

        /**
         * \brief Convert the value of the parameter as an unsigned int
         * This method returns a default value if the parameter does not exist.
         */
        unsigned int GetOrDefault(unsigned int def)
        {
            if(this->m_Exist)
            {
                try
                {
                    unsigned int result = std::stoul(this->m_Value);
                    return result;
                }
                catch(...)
                {
                    return def;
                }
            }
            return def;
        }

        /**
         * \brief Convert the value of the parameter as a boolean
         */
        bool Get(bool& out)
        {
            if(this->m_Exist)
            {
                if(this->m_Value == "true" || this->m_Value == "True") out = true;
                else if(this->m_Value == "false" || this->m_Value == "False") out = false;
                else return false;

                return true;
            }
            return false;
        }

        /**
         * \brief Convert the value of the parameter as a boolean
         * This method returns a default value if the parameter does not exist.
         */
        bool GetOrDefault(bool def)
        {
            if(this->m_Exist)
            {
                if(this->m_Value == "true" || this->m_Value == "True") return true;
                else if(this->m_Value == "false" || this->m_Value == "False") return false;
            }
            return def;
        }
        
        /**
         * \brief Check if the parameter exists
         */
        bool Exist() { return m_Exist; }

        operator bool() const { return m_Exist; }

    private:
        std::string m_Value;
        bool m_Exist;
    };

    /**
     * \brief Represent a list of parameter that where parsed from the command line
     */
    class InvocationContext {
    public:
        InvocationContext(std::string line);
        ~InvocationContext() = default;

        /// Query either positional arguments or named arguments
        Parameter GetPositionalParameter(uint32_t position) const;
        Parameter GetNamedParameter(const std::string& name) const;
        Parameter GetNamedParameter(const std::initializer_list<std::string>& names) const;

    private:
        std::vector<Parameter> m_PositionalParameters;
        std::unordered_map<std::string, Parameter> m_NamedParameters;
    };

    /**
     * \brief Represent the view of the application
     * This class handles the user input in the command line and dispatches
     * the parsed commands to the rest of the application.
     */
    class CLIView {
    public:
        CLIView() = default;
        ~CLIView() = default;
    
        /// Delete the copy constructor and copy assignment
        CLIView(const CLIView&) = delete;
        CLIView& operator=(const CLIView&) = delete;

        /// Handle a command. If the input is the empty string,
        /// then this functions prompts the user to enter some text
        /// in the console
        int HandleCommand(const std::string& input = "");

        void PrintInfo(const std::string& text) const;
        void PrintWarning(const std::string& warning) const;
        void PrintError(const std::string& error) const;

        typedef std::function<int(const InvocationContext&)> CommandHandler;
        void RegisterHandler(const std::string& command, const CommandHandler& handler);

    private:
        std::unordered_map<std::string, CommandHandler> m_Handlers;
    };

}

#endif