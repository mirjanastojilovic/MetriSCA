/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _CSV_WRITER_HPP
#define _CSV_WRITER_HPP

#include "matrix.hpp"

#include <nonstd/span.hpp>

#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <iostream>

namespace metrisca {

    class CSVWriter;

    namespace csv {
    
        inline static CSVWriter& EndRow(CSVWriter& writer);
        inline static CSVWriter& Flush(CSVWriter& writer);
    
    }

    /**
     * \brief Utility to create CSV files
     * Each writes to this stream appends the value followed by the separator.
     * Therefore, every line ends with the separator.
     * This class does not check that each row has the same number of column, this
     * task is left to the programmer.
     */
    class CSVWriter {
    public:
        explicit CSVWriter(const std::string& filename, char separator = ',');
        ~CSVWriter();

        void Flush();
        void EndRow();

        CSVWriter& operator<< (CSVWriter& (* func)(CSVWriter&));

        /// Write a string to the file
        /// The strings are surrounded with double quotes
        CSVWriter& operator<< (const char* value);
        CSVWriter& operator<< (const std::string& value);

        /// Write a generic data type to the file
        template<typename T>
        CSVWriter& operator<< (const T& value)
        {
            this->m_FileOutputStream << value << this->m_Separator;
            return *this;
        }

        /// Write a vector to the file
        template<typename T>
        CSVWriter& operator<< (const std::vector<T>& values)
        {
            for(const auto& value : values)
            {
                this->m_FileOutputStream << value << this->m_Separator;
            }
            return *this;
        }

        template<typename T>
        CSVWriter& operator<< (const nonstd::span<T>& values)
        {
            for(const auto& value : values)
            {
                this->m_FileOutputStream << value << this->m_Separator;
            }
            return *this;
        }

        /// Write a matrix to the file
        template<typename T>
        CSVWriter& operator<< (const Matrix<T>& matrix)
        {
            size_t height = matrix.GetHeight();
            for(size_t r = 0; r < height; ++r)
            {
                *this << matrix.GetRow(r);
                if(r < height-1)
                {
                    this->EndRow();
                }
            }
            return *this;
        }

    private:
        std::ofstream m_FileOutputStream;
        char m_Separator;
    };

    namespace csv {
    
        inline static CSVWriter& EndRow(CSVWriter& writer)
        {
            writer.EndRow();
            return writer;
        }

        inline static CSVWriter& Flush(CSVWriter& writer)
        {
            writer.Flush();
            return writer;
        }
    
    }

}

#endif