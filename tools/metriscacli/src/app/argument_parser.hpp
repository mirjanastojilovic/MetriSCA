/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "string_utils.hpp"

#include "metrisca/forward.hpp"

#include <optional>
#include <string>
#include <vector>
#include <exception>
#include <sstream>
#include <cassert>

namespace metrisca {

    class Application;

    enum class ArgumentType {
        Unknown,
        Int32,
        UInt32,
        UInt8,
        Double,
        String,
        Boolean,
        Dataset,
        TupleUInt32,
    };

    enum class ArgumentAction {
        Store,
        StoreConst
    };

    static std::string ArgumentTypeToString(ArgumentType type)
    {
        switch(type)
        {
        case ArgumentType::Unknown: return "unknown";
        case ArgumentType::Int32: return "int32";
        case ArgumentType::UInt32: return "uint32";
        case ArgumentType::UInt8: return "uint8";
        case ArgumentType::Double: return "double";
        case ArgumentType::String: return "string";
        case ArgumentType::Boolean: return "boolean";
        case ArgumentType::Dataset: return "dataset";
        case ArgumentType::TupleUInt32: return "uint32:uint32";
        }
        assert(false);
        return "unknown";
    }

    struct ParserException : public std::exception {
        ParserException() {}
        
        explicit ParserException(const std::string& msg)
            : m_Msg(msg)
        {}

        const char* what() const throw ()
        {
            return m_Msg.c_str();
        }

    protected:
        std::string m_Msg;
    };

    struct MissingValueException : public ParserException {
        explicit MissingValueException(const std::string& name)
        {
            std::stringstream msg;
            msg << "Missing value for argument '" << Upper(name) << "'.";
            m_Msg = msg.str();
        }
    };

    struct MissingArgumentException : public ParserException {
        explicit MissingArgumentException(const std::string& name)
        {
            std::stringstream msg;
            msg << "Missing required argument '" << Upper(name) << "'.";
            m_Msg = msg.str();
        }
    };

    struct BadTypeException : public ParserException {
        BadTypeException(const std::string& name, const std::string& value, ArgumentType type)
        {
            std::stringstream msg;
            msg << "Bad type for argument '" << Upper(name) << "': '" << value << "' is not of type " << ArgumentTypeToString(type) << ".";
            m_Msg = msg.str();
        }
    };

    struct UnknownDatasetException : public ParserException {
        explicit UnknownDatasetException(const std::string& alias)
        {
            std::stringstream msg;
            msg << "Unknown dataset with alias '" << alias << "'.";
            m_Msg = msg.str();
        }
    };

    struct UnknownArgumentException : public ParserException {
        explicit UnknownArgumentException(const std::string& abbrev)
        {
            std::stringstream msg;
            msg << "Unknown argument with abbreviation '" << abbrev << "'.";
            m_Msg = msg.str();
        }
    };

    struct InvalidOptionStringException : public ParserException {
        explicit InvalidOptionStringException(const std::string& option)
        {
            std::stringstream msg;
            msg << "Invalid option string '" << option << "'. Must start with '-' or '--'.";
            m_Msg = msg.str();
        }
    };

    struct BadSpecialCharException : public ParserException {
        explicit BadSpecialCharException(const char ch)
        {
            std::stringstream msg;
            msg << "Unknown special character '" << ch << "'.";
            m_Msg = msg.str();
        }
    };

    struct BadTokenException : public ParserException {
        explicit BadTokenException(const std::string& token)
        {
            std::stringstream msg;
            msg << "Unexpected token '" << token << "'.";
            m_Msg = msg.str();
        }
    };

    class ArgumentParser {
    public:
        ArgumentParser(const std::string& name, const std::string& description, const std::string& parent_parser_name = "");
        ~ArgumentParser() = default;

        ArgumentList Parse(const std::string& command) const;
        ArgumentList Parse(const std::vector<std::string>& args) const;

        void SetTitle(const std::string& title);
        void AddPositionalArgument(const std::string& name, ArgumentType type, const std::string& description, bool required = true);
        void AddFlagArgument(const std::string& name, const std::vector<std::string>& option_strings, const std::string& description);
        void AddOptionArgument(const std::string& name, const std::vector<std::string>& option_strings, ArgumentType type, const std::string& description, const std::string& default_);
        void AddOptionArgument(const std::string& name, const std::vector<std::string>& option_strings, ArgumentType type, const std::string& description, const char* default_);
        void AddOptionArgument(const std::string& name, const std::vector<std::string>& option_strings, ArgumentType type, const std::string& description, bool required = true);

        std::string HelpMessage() const;

        std::string Name() const { return m_Name; }
        std::string FullName() const 
        {
            if (m_ParentParserName.empty())
                return Name();
            return m_ParentParserName + " " + Name();
        }

    private:
        struct Argument {
            Argument() {}

            Argument(const std::string& name, const std::vector<std::string>& option_strings, ArgumentAction action,
                ArgumentType type, const std::string& default_, const std::string& const_val, bool positional, bool required,
                const std::string& description)
                : Name(name)
                , OptionStrings(option_strings)
                , Action(action)
                , Type(type)
                , Default(default_)
                , Const(const_val)
                , IsPositional(positional)
                , IsRequired(required)
                , Description(description)
            {}

            std::string Name{};
            std::vector<std::string> OptionStrings{};
            ArgumentAction Action = ArgumentAction::Store;
            ArgumentType Type = ArgumentType::Unknown;
            std::string Default{};
            std::string Const{};
            bool IsPositional = false;
            bool IsRequired = false;
            std::string Description{};
        };

        enum class TokenType {
            Value,
            Option,
            Eoi,
        };

        struct Token {
            Token(TokenType type, const std::string& value)
                : Type(type)
                , Value(value)
            {}

            TokenType Type;
            std::string Value;
        };

        std::vector<Argument>::const_iterator FindPositionForArgument(const Argument& arg) const;

        std::vector<Token> Tokenize(const std::string& args) const;
        ArgumentType FindBestTypeForValue(const std::string& value) const;
        bool IsOptionString(const std::string& value) const;
        void ConvertAndAddArgument(ArgumentList& list, const std::string& value, const std::string& name, ArgumentType type) const;
        void CheckOptionStrings(const std::vector<std::string>& option_strings) const;
        void CheckRequiredArguments(ArgumentList& list) const;
        std::optional<Argument> FindFromOptionString(const std::string& option_string) const;

        std::string GetUsageString() const;
        std::vector<std::string> GetPositionalsHelp() const;
        std::vector<std::string> GetOptionsHelp() const;

        std::vector<Argument> m_Arguments;
        std::string m_Title{};
        std::string m_Name{};
        std::string m_ParentParserName{};
        std::string m_Description{};
    };

}