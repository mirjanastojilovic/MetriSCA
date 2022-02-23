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
    enum {
        SCA_INVALID_HEADER = 0,
        SCA_FILE_NOT_FOUND,
        SCA_INVALID_DATA_TYPE,
        SCA_INVALID_COMMAND,
        SCA_INVALID_ARGUMENT,
        SCA_UNSUPPORTED_OPERATION,
        SCA_INVALID_DATA,
        SCA_UNKNOWN_PLUGIN,
        SCA_MISSING_ARGUMENT,
    };

    /// Utility method to convert an error into a human-readable string
    static std::string ErrorCause(int error)
    {
        switch(error)
        {
        case SCA_INVALID_HEADER:        return "invalid file header";
        case SCA_FILE_NOT_FOUND:        return "file not found";
        case SCA_INVALID_DATA_TYPE:     return "invalid data type";
        case SCA_INVALID_COMMAND:       return "invalid command";
        case SCA_INVALID_ARGUMENT:      return "invalid argument";
        case SCA_UNSUPPORTED_OPERATION: return "unsupported operation";
        case SCA_INVALID_DATA:          return "invalid data";
        case SCA_UNKNOWN_PLUGIN:        return "unknown plugin";
        case SCA_MISSING_ARGUMENT:      return "missing argument";
        default:                        return "unknown error";
        }
    }

}

#endif