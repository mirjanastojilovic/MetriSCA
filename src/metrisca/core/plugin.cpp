/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/core/plugin.hpp"

#include "metrisca/distinguishers/pearson_distinguisher.hpp"

#include "metrisca/metrics/guess_metric.hpp"
#include "metrisca/metrics/guessing_entropy_metric.hpp"
#include "metrisca/metrics/mi_metric.hpp"
#include "metrisca/metrics/pi_metric.hpp"
#include "metrisca/metrics/rank_metric.hpp"
#include "metrisca/metrics/score_metric.hpp"
#include "metrisca/metrics/success_rate_metric.hpp"
#include "metrisca/metrics/ttest_metric.hpp"
#include "metrisca/metrics/rank_estimation_metric.hpp"
#include "metrisca/metrics/key_enumeration_metric.hpp"

#include "metrisca/models/hamming_distance_model.hpp"
#include "metrisca/models/hamming_weight_model.hpp"
#include "metrisca/models/identity_model.hpp"

#include "metrisca/scores/score.hpp"
#include "metrisca/scores/CPA.hpp"
#include "metrisca/scores/bayesian.hpp"
#include "metrisca/scores/old_bayesian.hpp"

#include "metrisca/profilers/standard_profiler.hpp"

namespace metrisca {

    void PluginFactory::Init()
    {
        auto& factory = PluginFactory::The();

        METRISCA_REGISTER_PLUGIN(PearsonDistinguisher, "pearson");

        METRISCA_REGISTER_PLUGIN(GuessMetric, "guess");
        METRISCA_REGISTER_PLUGIN(GuessingEntropyMetric, "guessing_entropy");
        METRISCA_REGISTER_PLUGIN(MIMetric, "mi");
        METRISCA_REGISTER_PLUGIN(PIMetric, "pi");
        METRISCA_REGISTER_PLUGIN(RankMetric, "rank");
        METRISCA_REGISTER_PLUGIN(ScoreMetric, "score");
        METRISCA_REGISTER_PLUGIN(SuccessRateMetric, "success_rate");
        METRISCA_REGISTER_PLUGIN(TTestMetric, "ttest");
        METRISCA_REGISTER_PLUGIN(RankEstimationMetric, "rank_estimation");
        METRISCA_REGISTER_PLUGIN(KeyEnumerationMetric, "key_enumeration");

        METRISCA_REGISTER_PLUGIN(HammingDistanceModel, "hamming_distance");
        METRISCA_REGISTER_PLUGIN(HammingWeightModel, "hamming_weight");
        METRISCA_REGISTER_PLUGIN(IdentityModel, "identity");

        METRISCA_REGISTER_PLUGIN(CPAPlugin, "cpa");
        METRISCA_REGISTER_PLUGIN(BayesianPlugin, "bayesian");
        METRISCA_REGISTER_PLUGIN(OldBayesianPlugin, "old_bayesian");

        METRISCA_REGISTER_PLUGIN(StandardProfiler, "standard");
    }

    std::vector<std::string> PluginFactory::PluginNamesWithType(PluginType type) const
    {
        std::vector<std::string> result;
        auto it = m_Registry.find(type);
        if (it != m_Registry.end())
        {
            for (const auto& [name, constructor] : it->second)
                result.push_back(name);
        }
        return result;
    }

}