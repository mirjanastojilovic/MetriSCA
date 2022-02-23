/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include <optional>

namespace metrisca {

    template<typename T, typename E>
    class Result {
    public:
        using value_type = T;
        using error_type = E;

        Result(const value_type& value)
            : m_Value(value)
        {}

        Result(const error_type& error)
            : m_Error(error)
        {}

        ~Result() = default;

        bool IsError()
        {
            return m_Error.has_value();
        }

        value_type& Value()
        {
            return m_Value.value();
        }

        error_type& Error()
        {
            return m_Error.value();
        }

    private:
        std::optional<value_type> m_Value;
        std::optional<error_type> m_Error;
    };

    template<typename E>
    class Result<void, E> {
    public:
        using value_type = void;
        using error_type = E;

        Result(const error_type& error)
            : m_Error(error)
        {}

        Result() = default;
        ~Result() = default;

        bool IsError()
        {
            return m_Error.has_value();
        }

        error_type& Error()
        {
            return m_Error.value();
        }

    private:
        std::optional<error_type> m_Error;
    };

}