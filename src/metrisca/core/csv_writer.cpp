/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/core/csv_writer.hpp"

namespace metrisca {

    CSVWriter::CSVWriter(const std::string& filename, char separator) :
        m_Separator(separator)
    {
        this->m_FileOutputStream.open(filename);
    }

    CSVWriter::~CSVWriter()
    {
        this->Flush();
        this->m_FileOutputStream.close();
    }

    void CSVWriter::Flush()
    {
        this->m_FileOutputStream.flush();
    }

    void CSVWriter::EndRow()
    {
        this->m_FileOutputStream << std::endl;
    }

    CSVWriter& CSVWriter::operator<< (CSVWriter& (* func)(CSVWriter&))
    {
        return func(*this);
    }

    CSVWriter& CSVWriter::operator<< (const char* value)
    {
        this->m_FileOutputStream << '"' << value << '"' << this->m_Separator;
        return *this;
    }

    CSVWriter& CSVWriter::operator<< (const std::string& value)
    {
        this->m_FileOutputStream << '"' << value << '"' << this->m_Separator;
        return *this;
    }
}