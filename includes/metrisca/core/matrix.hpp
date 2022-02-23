/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _MATRIX_HPP
#define _MATRIX_HPP

#include "metrisca/core/errors.hpp"

#include <vector>
#include <cstdint>
#include <cassert>
#include <fstream>

#include <nonstd/span.hpp>

namespace metrisca {

    /**
     * Represent a 2-dimensional matrix of data
     * This class stores its data row-wise in a contiguous array,
     * therefore it does not provide any method to access data by column
     * for performance reasons.
     */
    template<typename T>
    class Matrix {
    public:
        typedef T value_type;
        typedef size_t size_type;
        typedef T* ptr_type;

    public:
        Matrix(): m_Width(0), m_Height(0) {}
        Matrix(size_type width, size_type height): m_Width(width), m_Height(height) { this->m_Data.resize(width * height); }
    
        /// Get the raw pointer of the underlying data storage
        ptr_type Data() { return this->m_Data.data(); }
        size_type Size() const { return this->m_Data.size() * sizeof(value_type); }

        /// Resize the matrix
        void Resize(size_type width, size_type height) 
        {
            this->m_Data.resize(width * height);
            this->m_Width = width;
            this->m_Height = height;
        }

        /// Copy this matrix
        Matrix<value_type> Copy() const
        {
            Matrix<value_type> result(m_Width, m_Height);

            std::copy(this->m_Data.begin(), this->m_Data.end(), result.m_Data.begin());

            return result;
        }

        /// Set a row of the matrix
        void SetRow(size_type row_index, const ptr_type row_data)
        {
            assert(row_index < this->m_Height);

            std::copy(row_data, row_data + this->m_Width, this->m_Data.begin() + (row_index * this->m_Width));
        }

        void SetRow(size_type row_index, const std::vector<value_type>& row)
        {
            assert(row.size() == this->m_Width);

            SetRow(row_index, (const ptr_type)row.data());
        }

        void SetRow(size_type row_index, const nonstd::span<const value_type>& row)
        {
            assert(row.size() == this->m_Width);

            SetRow(row_index, (const ptr_type)row.data());
        }

        /// Fill a row of this matrix with a constant value
        void FillRow(size_type row_index, const value_type& value)
        {
            assert(row_index < this->m_Height);

            std::fill(this->m_Data.begin() + (row_index * this->m_Width), this->m_Data.begin() + ((row_index + 1) * this->m_Width), value);
        }

        /// Get a view of a row of this matrix. This method does not perform
        /// a copy of the data but rather returns a span that provides a 
        /// read-only view of the row.
        nonstd::span<const value_type> GetRow(size_type row_index) const
        {
            assert(row_index < this->m_Height);

            nonstd::span<const value_type> span(this->m_Data);
            return span.subspan(row_index * this->m_Width, this->m_Width);
        }

        /// Extract a submatrix from this matrix. The row and column upper bounds are not inclusive.
        Matrix<value_type> Submatrix(size_type row_start, size_type col_start, size_type row_end, size_type col_end) const
        {
            assert(row_start < this->m_Height);
            assert(row_end <=  this->m_Height);
            assert(row_start < row_end);

            assert(col_start < this->m_Width);
            assert(col_end <= this->m_Width);
            assert(col_start < col_end);

            Matrix<value_type> result(col_end - col_start, row_end - row_start);

            for(size_type r = 0; r < result.GetHeight(); r++)
            {
                nonstd::span<const value_type> row = GetRow(row_start + r);

                result.SetRow(r, row.subspan(col_start, result.GetWidth()));
            }

            return result;
        }

        /// Element-wise accessor
        value_type& operator()(size_type row, size_type col)
        {
            assert(row < m_Height && col < m_Width);
            return this->m_Data[row * m_Width + col];
        }
        const value_type& operator()(size_type row, size_type col) const
        {
            assert(row < m_Height && col < m_Width);
            return this->m_Data[row * m_Width + col];
        }

        size_type GetWidth() const { return this->m_Width; }
        size_type GetHeight() const { return this->m_Height; }

    private:
#define MATRIX_HEADER_MAGIC_VALUE 0x726564616568746d
        struct MatrixFileHeader
        {
            uint64_t _MagicValue;
            size_t ElemSize;
            size_type Width;
            size_type Height;
        };

    public:
        /// Save this matrix to a file.
        int SaveToFile(const std::string& filename)
        {
            MatrixFileHeader header;
            header._MagicValue = MATRIX_HEADER_MAGIC_VALUE;
            header.ElemSize = sizeof(value_type);
            header.Width = this->m_Width;
            header.Height = this->m_Height;

            std::ofstream file;
            file.open(filename, std::ofstream::out | std::ofstream::binary);

            if(file)
            {
                file.write((char*)&header, sizeof(MatrixFileHeader));

                file.write((char*)this->Data(), this->Size());
            }

            return SCA_OK;
        }

        /// Load the matrix data from a file
        int LoadFromFile(const std::string& filename)
        {
            std::ifstream file;
            file.open(filename, std::ifstream::in | std::ifstream::binary);

            if(file)
            {
                MatrixFileHeader header;
                file.read((char*)&header, sizeof(MatrixFileHeader));

                if(header._MagicValue != MATRIX_HEADER_MAGIC_VALUE)
                {
                    return SCA_INVALID_HEADER;
                }
                if(header.ElemSize != sizeof(value_type))
                {
                    return SCA_INVALID_DATA_TYPE;
                }

                this->m_Data.resize(header.Width * header.Height);
                this->m_Width = header.Width;
                this->m_Height = header.Height;
                file.read((char*)this->Data(), this->Size());
            }
            else
            {
                return SCA_FILE_NOT_FOUND;
            }

            return SCA_OK;
        }

    private:
        std::vector<value_type> m_Data;
        size_type m_Width;
        size_type m_Height;
    };

}

#endif