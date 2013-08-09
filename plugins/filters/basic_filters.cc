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


} // namespace RTBKIT


/******************************************************************************/
/* INIT FILTERS                                                               */
/******************************************************************************/

namespace {

struct InitFilters
{
    InitFilters()
    {
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::SegmentsFilter>();
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::FoldPositionFilter>();
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::HourOfWeekFilter>();

        RTBKIT::FilterRegistry::registerFilter<RTBKIT::UrlFilter>();
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::LanguageFilter>();
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::LocationFilter>();

        RTBKIT::FilterRegistry::registerFilter<RTBKIT::ExchangePreFilter>();
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::ExchangeNameFilter>();
        RTBKIT::FilterRegistry::registerFilter<RTBKIT::ExchangePostFilter>();
    }

} initFilters;


} // namespace anonymous
