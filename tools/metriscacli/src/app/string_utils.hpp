/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include <cctype>
#include <string>
#include <locale>
#include <sstream>
#include <vector>

namespace metrisca {

    static std::string Trim(const std::string& value)
    {
        if (value.size() == 0) return "";

        size_t startIndex = 0;
        for (size_t i = 0; i < value.size(); ++i)
        {
            if (!std::isspace((unsigned char)value[i]))
                break;
            ++startIndex;
        }

        if (startIndex == value.size()) return "";

        size_t endIndex = value.size() - 1;
        for (size_t i = value.size() - 1; i > startIndex; --i)
        {
            if (!std::isspace((unsigned char)value[i]))
                break;
            --endIndex;
        }

        return value.substr(startIndex, endIndex - startIndex + 1);
    }

    static std::string Upper(const std::string& value)
    {
        std::locale loc;
        std::string result = value;
        for (size_t i = 0; i < result.size(); ++i)
        {
            result[i] = std::toupper(result[i], loc);
        }
        return result;
    }

    static std::string Lower(const std::string& value)
    {
        std::locale loc;
        std::string result = value;
        for (size_t i = 0; i < result.size(); ++i)
        {
            result[i] = std::tolower(result[i], loc);
        }
        return result;
    }

    static std::string Join(const std::vector<std::string>& values, const std::string& separator)
    {
        std::stringstream out;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0)
                out << separator;
            out << values[i];
        }
        return out.str();
    }

    static std::vector<std::string> Split(const std::string& value, const char separator = ' ')
    {
        std::vector<std::string> result;
        std::stringstream current_block;
        for (size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == separator)
            {
                if (current_block.str().size() > 0)
                {
                    result.push_back(current_block.str());
                    current_block.str("");
                }
            }
            else
            {
                current_block << value[i];
            }
        }

        if (current_block.str().size() > 0)
            result.push_back(current_block.str());

        return result;
    }

    static std::string NormalizeWhitespace(const std::string& value)
    {
        std::stringstream result;
        bool in_whitespace = false;
        for (size_t i = 0; i < value.size(); ++i)
        {
            bool is_current_whitespace = std::isspace((unsigned char)value[i]);

            if (!is_current_whitespace)
            {
                result << value[i];
                in_whitespace = false;
            }

            if (!in_whitespace && is_current_whitespace)
            {
                result << " ";
                in_whitespace = true;
            }
        }
        return result.str();
    }

}