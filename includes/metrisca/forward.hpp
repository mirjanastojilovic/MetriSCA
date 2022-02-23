/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

namespace metrisca {

    /// Forward declarations
    class TraceDataset;
    class TraceDatasetBuilder;
    class ArgumentList;
    class Plugin;
    class PowerModelPlugin;
    class DistinguisherPlugin;
    class ProfilerPlugin;
    class MetricPlugin;
    class LoaderPlugin;
    
    template<typename T>
    class Matrix;

    template<typename T, typename E>
    class Result;

}
