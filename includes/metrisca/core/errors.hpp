/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _ERRORS_HPP
#define _ERRORS_HPP

#include <string>

namespace metrisca {

    /// Enumeration of the errors that can occur in the application
    enum class Error : uint32_t {
        INVALID_HEADER = 0,
        FILE_NOT_FOUND,
        INVALID_DATA_TYPE,
        INVALID_COMMAND,
        INVALID_ARGUMENT,
        UNSUPPORTED_OPERATION,
        INVALID_DATA,
        UNKNOWN_PLUGIN,
        MISSING_ARGUMENT,
    };

    /// Utility method to convert an error into a human-readable string
    static std::string ErrorCause(Error error)
    {
        switch(error)
        {
        case Error::INVALID_HEADER:        return "invalid file header";
        case Error::FILE_NOT_FOUND:        return "file not found";
        case Error::INVALID_DATA_TYPE:     return "invalid data type";
        case Error::INVALID_COMMAND:       return "invalid command";
        case Error::INVALID_ARGUMENT:      return "invalid argument";
        case Error::UNSUPPORTED_OPERATION: return "unsupported operation";
        case Error::INVALID_DATA:          return "invalid data";
        case Error::UNKNOWN_PLUGIN:        return "unknown plugin";
        case Error::MISSING_ARGUMENT:      return "missing argument";
        default:                           return "unknown error";
        }
    }

}

#endif