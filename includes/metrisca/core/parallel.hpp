/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/core/assert.hpp"

#include <stdexcept>
#include <functional>
#include <thread>
#include <vector>

namespace metrisca
{
    static void parallel_for(size_t start, size_t end, std::function<void(size_t, size_t, bool)> fnCallback)
    {
        METRISCA_ASSERT(start < end);

        const size_t threadCount = std::min<size_t>(std::thread::hardware_concurrency(), end - start) - 1;

        std::vector<std::thread> threads;
        threads.reserve(threadCount);

        size_t begin = start;
        size_t elemPerThreads = (end - start) / (threadCount + 1);

        for (size_t i = 0; i != threadCount; ++i) {
            threads.emplace_back(std::thread(fnCallback, begin, begin + elemPerThreads, false));
            begin += elemPerThreads;
            METRISCA_ASSERT(begin < end);
        }

        fnCallback(begin, end, true);
        
        for (std::thread& thread : threads) {
            thread.join();
        }
    }
}
