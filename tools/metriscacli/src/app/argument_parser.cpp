/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "argument_parser.hpp"
#include "application.hpp"

#include "metrisca.hpp"

#include <cctype>
#include <cstdint>
#include <limits>
#include <iomanip>
#include <algorithm>

namespace metrisca {

    static std::optional<std::string> GetNameFromOptionString(const std::string& option)
    {
        if (option.rfind("--", 0) != 0)
            return {};
        return option.substr(2);
    }

    static std::optional<bool> ConvertToBool(const std::string& value)
    {
        if (value == "true") return true;
        if (value == "false") return false;
        return {};
    }

    static std::optional<double> ConvertToDouble(const std::string& value)
    {
        try
        {
            size_t processed_character_count{};
            double double_value = std::stod(value, &processed_character_count);
            if (processed_character_count != value.size())
                return {};
            return double_value;
        }
        catch (...)
        {
            return {};
        }
    }

    static std::optional<int32_t> ConvertToInt32(const std::string& value)
    {
        try 
        {
            size_t processed_character_count{};
            int32_t int_value = std::stoi(value, &processed_character_count, 0);
            if (processed_character_count != value.size())
                return {};
            return int_value;
        }
        catch (...)
        {
            return {};
        }
    }

    static std::optional<uint32_t> ConvertToUInt32(const std::string& value)
    {
        try
        {
            size_t processed_character_count{};
            uint32_t uint_value = std::stoul(value, &processed_character_count, 0);
            if (processed_character_count != value.size())
                return {};
            return uint_value;
        }
        catch (...)
        {
            return {};
        }
    }

    static std::optional<uint8_t> ConvertToUInt8(const std::string& value)
    {
        try
        {
            size_t processed_character_count{};
            uint32_t uint_value = std::stoul(value, &processed_character_count, 0);
            if (processed_character_count != value.size() || uint_value > std::numeric_limits<uint8_t>::max())
                return {};
            return uint_value;
        }
        catch (...)
        {
            return {};
        }
    }

    ArgumentParser::ArgumentParser(const std::string& name, const std::string& description, const std::string& parent_parser_name)
        : m_Name(name)
        , m_Description(description)
        , m_ParentParserName(parent_parser_name)
    {}

    void ArgumentParser::SetTitle(const std::string& title)
    {
        m_Title = title;
    }

    void ArgumentParser::AddPositionalArgument(
        const std::string& name, 
        ArgumentType type, 
        const std::string& description,
        bool required)
    {
        Argument arg(
            name,
            std::vector<std::string> {},
            ArgumentAction::Store,
            type,
            std::string{},
            std::string{},
            true,
            required,
            description
        );
        m_Arguments.insert(FindPositionForArgument(arg), arg);
    }

    void ArgumentParser::AddFlagArgument(
        const std::string& name, 
        const std::vector<std::string>& option_strings, 
        const std::string& description)
    {
        CheckOptionStrings(option_strings);
        Argument arg(
            name,
            option_strings,
            ArgumentAction::StoreConst,
            ArgumentType::Boolean,
            "false",
            "true",
            false,
            false,
            description
        );
        m_Arguments.insert(FindPositionForArgument(arg), arg);
    }

    void ArgumentParser::AddOptionArgument(
        const std::string& name, 
        const std::vector<std::string>& option_strings, 
        ArgumentType type, 
        const std::string& description, 
        const std::string& default_)
    {
        CheckOptionStrings(option_strings);
        Argument arg(
            name,
            option_strings,
            ArgumentAction::Store,
            type,
            default_,
            std::string{},
            false,
            false,
            description
        );
        m_Arguments.insert(FindPositionForArgument(arg), arg);
    }

    void ArgumentParser::AddOptionArgument(
        const std::string& name,
        const std::vector<std::string>& option_strings,
        ArgumentType type,
        const std::string& description,
        const char* default_)
    {
        this->AddOptionArgument(name, option_strings, type, description, std::string(default_));
    }

    void ArgumentParser::AddOptionArgument(
        const std::string& name,
        const std::vector<std::string>& option_strings,
        ArgumentType type,
        const std::string& description,
        bool required)
    {
        CheckOptionStrings(option_strings);
        Argument arg(
            name,
            option_strings,
            ArgumentAction::Store,
            type,
            std::string{},
            std::string{},
            false,
            required,
            description
        );
        m_Arguments.insert(FindPositionForArgument(arg), arg);
    }

    ArgumentType ArgumentParser::FindBestTypeForValue(const std::string& value) const
    {
        auto b = ConvertToBool(value);
        if(b.has_value()) return ArgumentType::Boolean;

        auto i32 = ConvertToInt32(value);
        if(i32.has_value()) return ArgumentType::Int32;

        auto u32 = ConvertToUInt32(value);
        if(u32.has_value()) return ArgumentType::UInt32;

        auto d = ConvertToDouble(value);
        if(d.has_value()) return ArgumentType::Double;

        auto dataset = Application::The().GetDataset(value);
        if(dataset) return ArgumentType::Dataset;

        return ArgumentType::String;
    }

    void ArgumentParser::ConvertAndAddArgument(ArgumentList& list, const std::string& value, const std::string& name, ArgumentType type) const
    {
        switch(type)
        {
        case ArgumentType::Unknown:
        {
            auto new_type = FindBestTypeForValue(value);
            return ConvertAndAddArgument(list, value, name, new_type);
        } break;
        case ArgumentType::Boolean:
        {
            auto v = ConvertToBool(value);
            if(!v.has_value()) throw BadTypeException(name, value, type);
            list.SetBool(name, v.value());
        } break;
        case ArgumentType::Double:
        {
            auto v = ConvertToDouble(value);
            if(!v.has_value()) throw BadTypeException(name, value, type);
            list.SetDouble(name, v.value());
        } break;
        case ArgumentType::Int32:
        {
            auto v = ConvertToInt32(value);
            if(!v.has_value()) throw BadTypeException(name, value, type);
            list.SetInt32(name, v.value());
        } break;
        case ArgumentType::UInt32:
        {
            auto v = ConvertToUInt32(value);
            if(!v.has_value()) throw BadTypeException(name, value, type);
            list.SetUInt32(name, v.value());
        } break;
        case ArgumentType::UInt8:
        {
            auto v = ConvertToUInt8(value);
            if(!v.has_value()) throw BadTypeException(name, value, type);
            list.SetUInt8(name, v.value());
        } break;
        case ArgumentType::String:
        {
            list.SetString(name, value);
        } break;
        case ArgumentType::Dataset:
        {
            auto dataset = Application::The().GetDataset(value);
            if(!dataset) throw UnknownDatasetException(value);
            list.SetDataset(name, dataset);
        } break;
        }
    }

    std::vector<ArgumentParser::Argument>::const_iterator ArgumentParser::FindPositionForArgument(const Argument& arg) const
    {
        if (arg.Action == ArgumentAction::StoreConst)
            return m_Arguments.cend();

        if (m_Arguments.size() == 0)
            return m_Arguments.cbegin();

        for (auto it = m_Arguments.cbegin(); it != m_Arguments.cend(); ++it)
        {
            const auto& other = *it;

            if (arg.IsPositional)
            {
                if (arg.IsRequired)
                {
                    if (!other.IsPositional || !other.IsRequired)
                        return it;
                }
                else
                {
                    if (!other.IsPositional)
                        return it;
                }
            }
            else if (arg.IsRequired)
            {
                if (!other.IsPositional && !other.IsRequired)
                    return it;
            }
            else
            {
                if (other.Action == ArgumentAction::StoreConst)
                    return it;
            }
        }
        
        return m_Arguments.cend();
    }

    std::vector<ArgumentParser::Token> ArgumentParser::Tokenize(const std::string& args) const
    {
        std::vector<Token> tokens;

        bool isInToken = true;
        size_t tokenStartIndex = 0;
        std::string trimmed_args = Trim(args);

        if (!trimmed_args.empty()) {
            std::vector<std::string> string_tokens;
            for (size_t c = 0; c < trimmed_args.size(); ++c)
            {
                char current = trimmed_args[c];
                if (isInToken && std::isspace(static_cast<unsigned char>(current)))
                {
                    string_tokens.push_back(trimmed_args.substr(tokenStartIndex, (c - tokenStartIndex)));
                    isInToken = false;
                }
                else if (!isInToken && !std::isspace(static_cast<unsigned char>(current)))
                {
                    tokenStartIndex = c;
                    isInToken = true;
                }
            }
            if (isInToken)
                string_tokens.push_back(args.substr(tokenStartIndex));

            for (const auto& string_token : string_tokens)
            {
                if (IsOptionString(string_token))
                    tokens.emplace_back(TokenType::Option, string_token);
                else
                    tokens.emplace_back(TokenType::Value, string_token);
            }
        }


        tokens.emplace_back(TokenType::Eoi, "");

        return tokens;
    }

    ArgumentList ArgumentParser::Parse(const std::vector<std::string>& arguments) const 
    {
        return Parse(Join(arguments, " "));
    }

    ArgumentList ArgumentParser::Parse(const std::string& command) const
    {
        ArgumentList result;

        auto tokens = Tokenize(command);

        if (m_Arguments.size() == 0)
            return result;

        bool should_expect_positional_arg = m_Arguments[0].IsPositional;
        bool should_expect_option_value = false;
        Argument current_option_argument{};

        for (size_t t = 0; t < tokens.size(); ++t)
        {
            auto token = tokens[t];

            if (should_expect_positional_arg) {
                if (t >= m_Arguments.size())
                    break;

                auto arg = m_Arguments[t];
                
                if (!arg.IsPositional) {
                    should_expect_positional_arg = false;
                    --t;
                    continue;
                }

                if (token.Type != TokenType::Value)
                {
                    if (arg.IsRequired)
                    {
                        throw MissingArgumentException(arg.Name);
                    }
                    else
                    {
                        should_expect_positional_arg = false;
                        --t;
                        continue;
                    }
                }

                ConvertAndAddArgument(result, token.Value, arg.Name, arg.Type);
            }
            else
            {
                if (should_expect_option_value)
                {
                    should_expect_option_value = false;
                    if (token.Type == TokenType::Value)
                    {
                        ConvertAndAddArgument(result, token.Value, current_option_argument.Name, current_option_argument.Type);
                    }
                    else
                    {
                        if (current_option_argument.Type == ArgumentType::Boolean ||
                            current_option_argument.Type == ArgumentType::Unknown)
                            ConvertAndAddArgument(result, "true", current_option_argument.Name, ArgumentType::Boolean);
                        else
                            throw MissingValueException(current_option_argument.Name);
                    }
                }
                else
                {
                    if (token.Type == TokenType::Value)
                        throw BadTokenException(token.Value);

                    if (token.Type == TokenType::Option)
                    {
                        auto arg = FindFromOptionString(token.Value);
                        if (arg.has_value())
                        {
                            if (arg.value().Action == ArgumentAction::StoreConst)
                            {
                                ConvertAndAddArgument(result, arg.value().Const, arg.value().Name, arg.value().Type);
                            }
                            else
                            {
                                current_option_argument = arg.value();
                                should_expect_option_value = true;
                            }
                        }
                        else
                        {
                            auto name = GetNameFromOptionString(token.Value);
                            if (!name.has_value())
                                throw UnknownArgumentException(token.Value);
                            current_option_argument = Argument {
                                name.value(),
                                { token.Value },
                                ArgumentAction::Store,
                                ArgumentType::Unknown,
                                std::string {},
                                std::string {},
                                false,
                                false,
                                std::string {}
                            };
                            should_expect_option_value = true;
                        }
                    }
                }
            }

            if (token.Type == TokenType::Eoi)
                break;
        }

        CheckRequiredArguments(result);

        return result;
    }

    std::string ArgumentParser::HelpMessage() const
    {
        std::stringstream msg;

        if (m_Title.size() > 0)
        {
            msg << m_Title << "\n\n";
        }

        msg << "Usage: ";
        if (m_ParentParserName.size() > 0)
            msg << m_ParentParserName << " ";
        msg << m_Name << " " << GetUsageString() << "\n";
        msg << "\n";
        msg << m_Description << "\n";

        auto positionals_help = GetPositionalsHelp();
        if (positionals_help.size() > 0)
        {
            msg << "\n";
            msg << "Positional arguments:\n";
            msg << Join(positionals_help, "\n") << "\n";
        }

        auto options_help = GetOptionsHelp();
        if (options_help.size() > 0)
        {
            msg << "\n";
            msg << "Options:\n";
            msg << Join(options_help, "\n") << "\n";
        }
        
        return msg.str();
    }

    void ArgumentParser::CheckRequiredArguments(ArgumentList& list) const
    {
        for (const auto& arg : m_Arguments) 
        {
            if (!list.HasArgument(arg.Name))
            {
                if (!arg.Default.empty())
                    ConvertAndAddArgument(list, arg.Default, arg.Name, arg.Type);
                else if (arg.IsRequired)
                    throw MissingArgumentException(arg.Name);
            }
        }
    }

    std::optional<ArgumentParser::Argument> ArgumentParser::FindFromOptionString(const std::string& option_string) const
    {
        for (const auto& arg : m_Arguments)
        {
            for (const auto& option : arg.OptionStrings) 
            {
                if (option == option_string)
                    return arg;
            }
        }
        return {};
    }

    bool ArgumentParser::IsOptionString(const std::string& value) const
    {
        // True if starts with "--" prefix or starts with "-" prefix and is 
        // not a parameter type that can be converted to a useful value 
        // (e.g., uint, double, dataset)
        return value.rfind("--", 0) == 0 || 
            (value.rfind("-", 0) == 0 && FindBestTypeForValue(value) == ArgumentType::String);
    }

    void ArgumentParser::CheckOptionStrings(const std::vector<std::string>& option_strings) const
    {
        for (const auto& option : option_strings)
        {
            if (!IsOptionString(option))
                throw InvalidOptionStringException(option);
        }
    }

    std::string ArgumentParser::GetUsageString() const
    {
        std::stringstream usage;
        for (size_t i = 0; i < m_Arguments.size(); ++i)
        {
            auto& arg = m_Arguments[i];

            if (i > 0)
                usage << " ";

            if (arg.IsPositional)
            {
                if (arg.IsRequired)
                    usage << "<" << Upper(arg.Name) << ">";
                else
                    usage << "(<" << Upper(arg.Name) << ">)";
            }
            else if (arg.IsRequired)
            {
                std::vector<std::string> options_with_name;
                for (const auto& option : arg.OptionStrings)
                {
                    options_with_name.emplace_back(option + " " + Upper(arg.Name));
                }
                usage << "[" << Join(options_with_name, " | ") << "]";
            }
            else if (arg.Action == ArgumentAction::StoreConst)
            {
                usage << "(" << Join(arg.OptionStrings, " | ") << ")";
            }
            else
            {
                std::vector<std::string> options_with_name;
                for (const auto& option : arg.OptionStrings)
                {
                    options_with_name.emplace_back(option + " " + Upper(arg.Name));
                }
                usage << "(" << Join(options_with_name, " | ") << ")";
            }
        }
        return usage.str();
    }

    std::vector<std::string> ArgumentParser::GetPositionalsHelp() const
    {
        std::vector<std::string> result;
        std::stringstream arg_string;

        size_t indent_size = 0;
        for (const auto& arg : m_Arguments)
        {
            if (!arg.IsPositional)
                continue;

            if (arg.Name.size() > indent_size)
                indent_size = arg.Name.size();
        }

        // Add some extra padding
        indent_size += 4;

        for (const auto& arg : m_Arguments)
        {
            if (!arg.IsPositional)
                continue;

            arg_string << " " << std::left << std::setw(indent_size) << arg.Name;
            
            if (!arg.IsRequired)
                arg_string << "Optional. ";

            arg_string << arg.Description;

            result.emplace_back(arg_string.str());

            arg_string.str("");
            arg_string.clear();
        }

        return result;
    }

    std::vector<std::string> ArgumentParser::GetOptionsHelp() const
    {
        std::vector<std::string> result;
        std::stringstream arg_string;

        int32_t indent_size = 0;
        for (const auto& arg : m_Arguments)
        {
            if (arg.IsPositional)
                continue;

            // option_size is set to -2 to compensate for the extra time we count +2
            // in the inner for loop
            int32_t option_size = -2;
            for (const auto& option : arg.OptionStrings)
            {
                // Add the size of the option plus two characters corresponding to ", "
                option_size += static_cast<int32_t>(option.size()) + 2;
            }
            if (option_size > indent_size)
                indent_size = option_size;
        }

        // Add some extra padding
        indent_size += 4;

        for (const auto& arg : m_Arguments)
        {
            if (arg.IsPositional)
                continue;
            
            arg_string << " " << std::left << std::setw(indent_size) << Join(arg.OptionStrings, ", ");

            if (!arg.IsRequired)
                arg_string << "Optional. ";

            arg_string << arg.Description;

            if (!arg.Default.empty() && arg.Action != ArgumentAction::StoreConst)
                arg_string << " Default: " << arg.Default;

            result.emplace_back(arg_string.str());

            arg_string.str("");
            arg_string.clear();
        }

        return result;
    }

}
