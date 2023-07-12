/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _NUMERICS_HPP
#define _NUMERICS_HPP

#include "math.hpp"
#include "metrisca/core/assert.hpp"

#include <vector>
#include <cstdint>
#include <cmath>
#include <limits>
#include <array>

#include <nonstd/span.hpp>

#define METRISCA_SQRT_2 1.4142135623730950488

namespace metrisca { namespace numerics {

    /// Generate linearly a range of values between two end points
    static std::vector<double> Linspace(double from, double to, size_t samples, bool endpoint = true)
    {
        std::vector<double> result;
        result.reserve(samples);

        double step = 0.0;
        if(endpoint)
        {
            step = (to - from) / ((double)samples - 1.0);
        }
        else
        {
            step = (to - from) / (double)samples;
        }
        for(size_t i = 0; i < samples; ++i)
        {
            result.push_back(from + i * step);
        }
        return result;
    }

    /// Generate a range of values between two endpoints with a constant step
    template<typename T>
    static std::vector<T> ARange(T from, T to, T step)
    {
        if(from > to)
        {
            T old_to = to;
            to = from;
            from = old_to;
        }
        size_t count = (size_t)std::floor(((double)(to - from) / (double)step));
        std::vector<T> result;
        result.reserve(count);

        for(T val = from; val < to; val += step)
        {
            result.push_back(val);
        }

        return result;
    }

    /// Compute the maximum value of an array
    template<typename T>
    static T Max(T const * values, const size_t count)
    {
        T max = std::numeric_limits<T>::lowest();
        for(size_t i = 0; i < count; ++i)
        {
            T val = *(values + i);
            if(val > max) max = val;
        }

        return max;
    }

    template<typename T>
    static T Max(const std::vector<T>& values)
    {
        return Max(values.data(), values.size());
    }

    template<typename T>
    static T Max(const nonstd::span<T>& values)
    {
        return Max(values.data(), values.size());
    }

    /// Compute the index of the maximum value of an array
    template<typename T>
    static size_t ArgMax(T const * values, const size_t count)
    {
        T max = std::numeric_limits<T>::lowest();
        size_t max_index = 0;
        for(size_t i = 0; i < count; ++i)
        {
            T val = *(values + i);
            if(val > max) 
            {
                max = val;
                max_index = i;
            }
        }

        return max_index;
    }

    template<typename T>
    static size_t ArgMax(const std::vector<T>& values)
    {
        return ArgMax(values.data(), values.size());
    }

    template<typename T>
    static size_t ArgMax(const nonstd::span<T>& values)
    {
        return ArgMax(values.data(), values.size());
    }

    /// Compute the minimum value of an array
    template<typename T>
    static T Min(T const * values, const size_t count)
    {
        T min = std::numeric_limits<T>::max();
        for(size_t i = 0; i < count; ++i)
        {
            T val = *(values + i);
            if(val < min) min = val;
        }

        return min;
    }

    template<typename T>
    static T Min(const std::vector<T>& values)
    {
        return Min(values.data(), values.size());
    }

    template<typename T>
    static T Min(const nonstd::span<T>& values)
    {
        return Min(values.data(), values.size());
    }

    /// Compute the index of the minimum value in an array
    template<typename T>
    static size_t ArgMin(T const * values, const size_t count)
    {
        T min = std::numeric_limits<T>::max();
        size_t min_index = 0;
        for(size_t i = 0; i < count; ++i)
        {
            T val = *(values + i);
            if(val < min) 
            {
                min = val;
                min_index = i;
            }
        }

        return min_index;
    }

    template<typename T>
    static size_t ArgMin(const std::vector<T>& values)
    {
        return ArgMin(values.data(), values.size());
    }

    template<typename T>
    static size_t ArgMin(const nonstd::span<T>& values)
    {
        return ArgMin(values.data(), values.size());
    }

    /// Compute the maximum and minimum value of an array
    template<typename T>
    static std::pair<T, T> MinMax(T const * values, const size_t count)
    {
        T min = std::numeric_limits<T>::max();
        T max = std::numeric_limits<T>::lowest();
        for(size_t i = 0; i < count; ++i)
        {
            T val = *(values + i);
            if(val < min) min = val;
            if(val > max) max = val;
        }

        return std::make_pair(min, max);
    }

    template<typename T>
    static std::pair<T, T> MinMax(const std::vector<T>& values)
    {
        return MinMax(values.data(), values.size());
    }

    template<typename T>
    static std::pair<T, T> MinMax(const nonstd::span<T>& values)
    {
        return MinMax(values.data(), values.size());
    }

    /// Compute the sum of all values in an array
    template<typename T>
    static T Sum(T const * values, const size_t count)
    {
        T sum = (T)0;
        for(size_t i = 0; i < count; ++i)
        {
            T val = *(values + i);
            sum += val;
        }
        return sum;
    }

    template<typename T>
    static T Sum(const std::vector<T>& values)
    {
        return Sum(values.data(), values.size());
    }

    template<typename T>
    static T Sum(const nonstd::span<T>& values)
    {
        return Sum(values.data(), values.size());
    }

    /// Compute the mean value of an array
    template<typename T>
    static double Mean(T const * values, const size_t count)
    {
        if(count == 0) return 0.0;
        double mean = 0.0;
        T max = std::numeric_limits<T>::max();
        T half_max = max / (T)2;
        T sum = (T)0;
        double n = (double)count;
        for(size_t i = 0; i < count; ++i)
        {
            T val = *(values + i);
            sum += val;
            if(std::abs(sum) > half_max)
            {
                mean += sum / n;
                sum = (T)0;
            }
        }
        mean += sum / n;
        return mean;
    }

    template<typename T>
    static double Mean(const std::vector<T>& values)
    {
        return Mean(values.data(), values.size());
    }

    template<typename T>
    static double Mean(const nonstd::span<T>& values)
    {
        return Mean(values.data(), values.size());
    }

    /// Compute the variance value of an array
    template<typename T>
    static double Variance(T const * values, const size_t count, const double mean)
    {
        if(count == 0) return 0.0;
        double max = std::numeric_limits<double>::max();
        double half_max = max / 2.0;
        double sum = 0.0;
        double var = 0.0;
        double n = (double)count;
        for(size_t i = 0; i < count; ++i)
        {
            T val = *(values + i);
            double centered = (double)val - mean;
            sum += centered * centered;
            if(sum > half_max)
            {
                var += sum / n;
                sum = 0.0;
            }
        }
        var += sum / n;
        return var;
    }

    template<typename T>
    static double Variance(const std::vector<T>& values, const double mean)
    {
        return Variance(values.data(), values.size(), mean);
    }

    template<typename T>
    static double Variance(const nonstd::span<T>& values, const double mean)
    {
        return Variance(values.data(), values.size(), mean);
    }

    template<typename T>
    static double Variance(const std::vector<T>& values)
    {
        return Variance(values.data(), values.size(), Mean(values));
    }

    template<typename T>
    static double Variance(const nonstd::span<T>& values)
    {
        return Variance(values.data(), values.size(), Mean(values));
    }

    /// Compute the standard deviation value of an array
    template<typename T>
    static double Std(T const * values, const size_t count, const double mean)
    {
        return std::sqrt(Variance(values, count, mean));
    }

    template<typename T>
    static double Std(const std::vector<T>& values, const double mean)
    {
        return Std(values.data(), values.size(), mean);
    }

    template<typename T>
    static double Std(const nonstd::span<T>& values, const double mean)
    {
        return Std(values.data(), values.size(), mean);
    }

    template<typename T>
    static double Std(const std::vector<T>& values)
    {
        return Std(values.data(), values.size(), Mean(values));
    }

    template<typename T>
    static double Std(const nonstd::span<T>& values)
    {
        return Std(values.data(), values.size(), Mean(values));
    }

    /// Perform the Welch TTest on two arrays
    template<typename T>
    static double WelchTTest(T const * values1, const size_t count1, T const * values2, const size_t count2)
    {
        if(count1 == 0 || count2 == 0) return 0.0;
        double mean1 = Mean(values1, count1);
        double mean2 = Mean(values2, count2);
        double variance1 = Variance(values1, count1, mean1);
        double variance2 = Variance(values2, count2, mean2);

        double ttest = (mean1 - mean2) / std::sqrt(variance1 / (double)count1 + variance2 / (double)count2);
        return ttest;
    }

    template<typename T>
    static double WelchTTest(const std::vector<T>& values1, const std::vector<T>& values2)
    {
        return WelchTTest(values1.data(), values1.size(), values2.data(), values2.size());
    }

    template<typename T>
    static double WelchTTest(const nonstd::span<T>& values1, const nonstd::span<T>& values2)
    {
        return WelchTTest(values1.data(), values1.size(), values2.data(), values2.size());
    }

    template<typename T>
    static double WelchTTest(const nonstd::span<T>& values1, const std::vector<T>& values2)
    {
        return WelchTTest(values1.data(), values1.size(), values2.data(), values2.size());
    }

    template<typename T>
    static double WelchTTest(const std::vector<T>& values1, const nonstd::span<T>& values2)
    {
        return WelchTTest(values1.data(), values1.size(), values2.data(), values2.size());
    }

    /// Compute the Pearson correlation coefficent of two arrays
    template <typename L, typename R>
    static double PearsonCorrelation(L const * values1, const size_t count1, R const * values2, const size_t count2)
    {
        size_t count = std::min(count1, count2);
        std::array<double, 5> sums = std::array<double, 5>();
        for(size_t i = 0; i < count; i++)
        {
            double x = static_cast<double>(*(values1 + i));
            double y = static_cast<double>(*(values2 + i));

            sums[0] += x * y;
            sums[1] += y;
            sums[2] += x;
            sums[3] += y * y;
            sums[4] += x * x;
        }
        double divisor = std::sqrt(count * sums[4] - sums[2] * sums[2]) * std::sqrt(count * sums[3] - sums[1] * sums[1]);
        double pearson = (count * sums[0] - sums[1] * sums[2]) / divisor;
        return pearson;
    }

    template<typename L, typename R>
    static double PearsonCorrelation(const std::vector<L>& values1, const std::vector<R>& values2)
    {
        return PearsonCorrelation(values1.data(), values1.size(), values2.data(), values2.size());
    }

    template<typename L, typename R>
    static double PearsonCorrelation(const nonstd::span<L>& values1, const nonstd::span<R>& values2)
    {
        return PearsonCorrelation(values1.data(), values1.size(), values2.data(), values2.size());
    }

    template<typename L, typename R>
    static double PearsonCorrelation(const nonstd::span<L>& values1, const std::vector<R>& values2)
    {
        return PearsonCorrelation(values1.data(), values1.size(), values2.data(), values2.size());
    }

    template<typename L, typename R>
    static double PearsonCorrelation(const std::vector<L>& values1, const nonstd::span<R>& values2)
    {
        return PearsonCorrelation(values1.data(), values1.size(), values2.data(), values2.size());
    }

    /// Generate n equidistant samples of the gaussian function from a to b
    static double SampleGaussian(std::vector<double>& out, double mean, double std, double a, double b, uint32_t n)
    {
        if(n == 0) return 0.0;
        if(n % 2 == 0) n += 1;
        out.reserve(n);

        double invstd = 1.0 / std;
        double delta = (b - a) / (double)(n - 1);
        for(uint32_t i = 0; i < n; i++)
        {
            double val = math::Gaussian(a + i * delta, mean, invstd);
            out.push_back(val);
        }
        return delta;
    }

    /// Perform the Simpson quadrature on a vector of samples
    static double Simpson(const std::vector<double>& samples, double delta)
    {
        double result = 0.0;
        result += samples[0];
        result += samples[samples.size() - 1];
        for(size_t i = 1; i < samples.size() - 1; i++)
        {
            double factor = i % 2 == 1 ? 4.0 : 2.0;
            result += factor * samples[i];
        }
        result *= delta / 3.0;
        return result;
    }

    /// Find bin utility for histogram
    static size_t FindBin(double value, double min, double max, size_t binCount)
    {
        value -= min;
        value /= (max - min);
        uint64_t result = static_cast<uint64_t>(std::floor(value * (binCount - 1)));
        if (std::abs(min - max) < 1e5) return 0;
        else if (result < 0) return 0;
        else if (result >= binCount) return binCount - 1;
        else return  result;
    }

    /// Convoluting two list together (result is of length N + M - 1)
    template<typename T, typename R>
    static auto Convolve(const nonstd::span<T>& A, const nonstd::span<R>& B) -> std::vector<std::decay_t<decltype(A[0] * B[0])>>
    {
        using RType = typename std::decay_t<decltype(A[0] * B[0])>;
        std::vector<RType> result;
        const int N = (int) A.size();
        const int M = (int) B.size();

        // Without loss of generality we assume that N >= M
        if (M > N) {
            return Convolve(B, A);
        }

        result.resize(N + M - 1, (RType) 0);

        // Compute each component individually, notice that this algorithm
        // is not the most efficient one, because FFT exists
        for (int i = 0; i != result.size(); ++i)
        {
            // Compute last index of the sumation corresponding respectively
            // to the A array and to the B array. (Excluded)
            int last1 = std::min(N, i + 1);
            int last2 = std::min(M, N + M - i - 1);

            // Compute first index of the sumation corresponding respectively
            // to the A array and to the B array. (Included)
            int first1 = std::max(std::max(0, i - N), (last1 - last2));
            int first2 = std::max(0, M - (i + 1));

            int range = last2 - first2;

            // Finally perform the summation operation
            for (int k = 0; k != range; ++k) {
                result[i] += A[first1 + k] * B[M - (first2 + k) - 1];
            }
        }

        return result;
    }
}}

#endif