/** basic_filters.cc                                 -*- C++ -*-
    Rémi Attab, 24 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Default pool of filters for an BidRequest object.

*/

#include "basic_filters.h"


using namespace std;
using namespace ML;

namespace RTBKIT {

/******************************************************************************/
/* SEGMENT FILTER                                                             */
/******************************************************************************/

void
SegmentsFilter::
setConfig(unsigned configIndex, const AgentConfig& config, bool value)
{
    for (const auto& entry : config.segments) {
        auto& segment = data[entry.first];

        segment.ie.setInclude(configIndex, value, entry.second.include);
        segment.ie.setExclude(configIndex, value, entry.second.exclude);

        if (entry.second.excludeIfNotPresent) {
            segment.excludeIfNotPresent.set(configIndex, value);
            excludeIfNotPresent.insert(entry.first);
        }
    }
}

void
SegmentsFilter::
filter(FilterState& state) const
{
    unordered_set<string> toCheck = excludeIfNotPresent;

    for (const auto& segment : state.request.segments) {
        toCheck.erase(segment.first);

        auto it = data.find(segment.first);
        if (it == data.end()) continue;

        state.narrowConfigs(it->second.ie.filter(*segment.second));
        if (state.configs().empty()) return;
    }

    for (const auto& segment : toCheck) {
        auto it = data.find(segment);
        if (it == data.end()) continue;

        state.narrowConfigs(it->second.excludeIfNotPresent.negate());
        if (state.configs().empty()) return;
    }
}

/******************************************************************************/
/* CREATIVE FILTER                                                            */
/******************************************************************************/

void
CreativeFilter::
setConfig(unsigned configIndex, const AgentConfig& config, bool value)
{
    for (size_t i = 0; i < config.creatives.size(); ++i) {
        const Creative& creative = config.creatives[i];

        auto formatKey = makeKey(creative.format);
        formatFilter[formatKey].set(i, configIndex, value);
    }
}


void
CreativeFilter::
filter(FilterState& state) const
{
    ConfigSet configs;

    for (unsigned impId = 0; impId < state.request.imp.size(); ++impId) {

        CreativeMatrix matrix;
        const AdSpot& imp = state.request.imp[impId];

        for (const auto& format : imp.formats) {
            auto it = formatFilter.find(makeKey(format));
            if (it == formatFilter.end()) continue;

            matrix |= it->second;
        }


        if (matrix.empty()) continue;
        processMatrix(state, matrix, impId);
        configs |= matrix.aggregate();
    }

    state.narrowConfigs(configs);
}

void
CreativeFilter::
processMatrix(FilterState& state, CreativeMatrix& matrix, size_t impId) const
{
    std::unordered_map<unsigned, SmallIntVector> biddableCreatives;

    for (unsigned creativeId = 0; creativeId < matrix.size(); ++creativeId) {
        const auto& configs = matrix[creativeId];
        if (configs.empty()) continue;

        for (size_t config = configs.next();
             config < configs.size();
             config = configs.next(config + 1))
        {
            biddableCreatives[config].push_back(creativeId);
        }
    }

    for (const auto& entry : biddableCreatives)
        state.addBiddableSpot(entry.first, impId, entry.second);
}


} // namespace RTBKIT


/******************************************************************************/
/* INIT FILTERS                                                               */
/******************************************************************************/

namespace {

struct InitFilters
{
    InitFilters()
    {
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::HourOfWeekFilter>();
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::SegmentsFilter>();
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::LanguageRegexFilter>();

        RTBKIT::FilterRegistry::registerFilter<RTBKIT::ExchangePreFilter>();
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::ExchangePostFilter>();
    }

} initFilters;


} // namespace anonymous
