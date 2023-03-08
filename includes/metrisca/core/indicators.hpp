/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#pragma warning(push, 0)
#include <indicators/progress_bar.hpp>
#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <indicators/multi_progress.hpp>
#pragma warning(pop)

namespace metrisca {

    class HideCursorGuard
    {
        HideCursorGuard(const HideCursorGuard&) = delete;
        HideCursorGuard(HideCursorGuard&&) = delete;
        HideCursorGuard& operator=(const HideCursorGuard&) = delete;
        HideCursorGuard& operator=(HideCursorGuard&&) = delete;
    
    public:
        inline HideCursorGuard()
        {
            indicators::show_console_cursor(false);
        }

        inline ~HideCursorGuard()
        {
            indicators::show_console_cursor(true);
        }
    };

}
