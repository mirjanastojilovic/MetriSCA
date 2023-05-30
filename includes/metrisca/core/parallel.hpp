/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/core/assert.hpp"
#include "metrisca/core/indicators.hpp"
#include "metrisca/core/logger.hpp"

#include <stdexcept>
#include <functional>
#include <thread>
#include <string>
#include <vector>
#include <memory>

namespace metrisca
{
    namespace impl
    {
        static void parallel_for_thread(std::function<void(size_t)> fnCallback,
            size_t end,
            std::atomic_size_t& nextFree,
            std::shared_ptr<indicators::ProgressBar> progress_bar,
            std::mutex& lock)
        {
            for (size_t curIndex = nextFree.fetch_add(1, std::memory_order_seq_cst);
                 curIndex < end;
                 curIndex = nextFree.fetch_add(1, std::memory_order_seq_cst))
            {
                fnCallback(curIndex);

                if (progress_bar != nullptr) {
                    std::lock_guard<std::mutex> guard(lock);
                    progress_bar->set_option(indicators::option::PostfixText(
                        std::to_string(curIndex) + "/" + std::to_string(end)
                    ));
                    progress_bar->tick();
                }
            }
        }
    }

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

    static void parallel_for(const std::string& progress_name, size_t start, size_t end, std::function<void(size_t)> fnCallback)
    {
        METRISCA_ASSERT(start < end);

        const size_t threadCount = std::min<size_t>(std::thread::hardware_concurrency(), end - start) - 1;
        std::shared_ptr<indicators::ProgressBar> progress_bar = nullptr;
        std::mutex lock;

        if (!progress_name.empty()) {
            progress_bar = std::make_shared<indicators::ProgressBar>(
                indicators::option::BarWidth{50},
                indicators::option::MaxProgress{ end - start },
                indicators::option::PrefixText{ progress_name.c_str() },
                indicators::option::ShowElapsedTime{ true },
                indicators::option::ShowRemainingTime{ true },
                indicators::option::ShowPercentage{ true }
            );
        }

        std::vector<std::thread> threads;
        threads.reserve(threadCount);

        std::atomic_size_t nextFree = start;

        METRISCA_INFO("Creating {} threads for {} iterations", threadCount, end - start);
        for (size_t i = 0; i != threadCount; ++i) {
            threads.emplace_back(impl::parallel_for_thread, fnCallback, end, std::ref(nextFree), progress_bar, std::ref(lock));
        }

        impl::parallel_for_thread(fnCallback, end, nextFree, progress_bar, lock);

        // Await all threads
        for (std::thread& thread : threads) {
            thread.join();
        }

        // Finally terminate the progress bar
        if (progress_bar != nullptr) {
            progress_bar->mark_as_completed();
            progress_bar->set_option(indicators::option::PostfixText{ "  Completed  " });
        }
    }

    static void parallel_for(size_t start, size_t end, std::function<void(size_t)> fnCallback)
    {
        parallel_for("", start, end, std::move(fnCallback));
    }
}
