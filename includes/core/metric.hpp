/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _METRIC_HPP
#define _METRIC_HPP

#include "forward.hpp"

#include <functional>

namespace metrisca {

    /**
     * Represent a function that compute a metric on a dataset.
     * Due to the wide variety of metrics that can be computed, this functions expose
     * the internal classes of the application to give more freedom to the programmer.
     * The arguments are defined in order as follows:
     * 1) A reference to the view of the application. This can be used to output messages to
     *    the console.
     * 2) A reference to the model of the application. This can be used to query functions and
     *    datasets currently loaded in the application.
     * 3) A reference to the command line arguments that called this function. This can be used to
     *    Retrieve the required user parameters.
     * The return value of this function is an element of the error enumeration.
     */
    typedef std::function<int(const CLIView&, const CLIModel&, const InvocationContext&)> MetricFunction;

}

#endif