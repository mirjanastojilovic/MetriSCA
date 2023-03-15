/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */
#pragma once

#include "metrisca/core/assert.hpp"

#include <tuple>
#include <unordered_map>
#include <functional>

namespace std
{
    namespace
    {
        // Code from boost
        // Reciprocal of the golden ratio helps spread entropy
        //     and handles duplicates.
        // See Mike Seymour in magic-numbers-in-boosthash-combine:
        //     http://stackoverflow.com/questions/4948780
        template <class T>
        inline void hash_combine(std::size_t& seed, T const& v)
        {
            seed ^= std::hash<T>{}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        // Recursive template code derived from Matthieu M.
        template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
        struct HashValueImpl
        {
            static void apply(size_t& seed, Tuple const& tuple)
            {
                HashValueImpl<Tuple, Index - 1>::apply(seed, tuple);
                hash_combine(seed, std::get<Index>(tuple));
            }
        };

        template <class Tuple>
        struct HashValueImpl<Tuple, 0>
        {
            static void apply(size_t& seed, Tuple const& tuple)
            {
                hash_combine(seed, std::get<0>(tuple));
            }
        };
    }

    template <typename ... TT>
    struct hash<std::tuple<TT...>>
    {
        size_t
            operator()(std::tuple<TT...> const& tt) const
        {
            size_t seed = 0;
            HashValueImpl<std::tuple<TT...> >::apply(seed, tt);
            return seed;
        }

    };
}

namespace metrisca {

    template<typename Output, typename... Args>
    class LazyFunction
    {
    public:
        inline LazyFunction(std::function<Output(const Args&...)> callback)
        : m_Callback(callback)
        , m_ComputedValues{}
        {
            METRISCA_ASSERT(m_Callback != nullptr);   
        }

        template<typename Functor>
        inline LazyFunction(Functor arg)
        : m_Callback(arg)
        , m_ComputedValues{}
        {
            METRISCA_ASSERT(m_Callback != nullptr);
        }

        inline Output get(const Args&... arguments)
        {
            m_RequestCount += 1;
            std::tuple<Args...> argumentsPacked = std::tuple<Args...>(arguments...);
            typename std::unordered_map<std::tuple<Args...>, Output>::const_iterator it = m_ComputedValues.find(argumentsPacked);
            if (it != m_ComputedValues.end()) {
                return it->second;
            }

            m_CacheMiss += 1;
            Output result = m_Callback(arguments...);
            m_ComputedValues[argumentsPacked] = result;
            return result;
        }

        inline Output operator()(const Args&... arguments)
        {
            return get(arguments...);
        }

        inline double missRate() const
        {
            return (((double)m_CacheMiss)) / ((double)m_RequestCount);
        }

    private:
        std::unordered_map<std::tuple<Args...>, Output> m_ComputedValues{};
        std::function<Output(const Args&...)> m_Callback;
        uint64_t m_CacheMiss{ 0 };
        uint64_t m_RequestCount{ 0 };
    };

}
