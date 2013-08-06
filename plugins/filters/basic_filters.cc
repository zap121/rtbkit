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
    for (const auto& segment : config.segments) {
        data[segment.first].set(configIndex, segment.second, value);

        if (segment.second.excludeIfNotPresent)
            excludeIfNotPresent.insert(segment.first);
    }
}

ConfigSet
SegmentsFilter::
filter(const BidRequest& br, const ExchangeConnector*) const
{
    ConfigSet matches(true);

    unordered_set<string> toCheck = excludeIfNotPresent;

    for (const auto& segment : br.segments) {
        toCheck.erase(segment.first);

        auto it = data.find(segment.first);
        if (it == data.end()) continue;

        matches &= it->second.filter(*segment.second);
        if (matches.empty()) return matches;
    }

    for(const auto& segment : toCheck) {
        auto it = data.find(segment);
        if (it == data.end()) continue;

        matches &= it->second.excludeIfNotPresent.negate();
        if (matches.empty()) return matches;
    }

    return matches;
}


void
SegmentsFilter::SegmentInfo::
set(unsigned configIndex, const SegmentList& segments, bool value)
{
    segments.forEach([&](int i, string str, float weights) {
                if (i < 0)
                    intSet[i].set(configIndex, value);
                else strSet[str].set(configIndex, value);
            });
}

void
SegmentsFilter::SegmentFilter::
set(    unsigned configIndex,
        const AgentConfig::SegmentInfo& segments,
        bool value)
{
    if (segments.include.empty())
        emptyInclude.set(configIndex);
    else
        include.set(configIndex, segments.include, value);

    exclude.set(configIndex, segments.exclude, value);

    if (segments.excludeIfNotPresent)
        excludeIfNotPresent.set(configIndex, value);
}

// \todo This is not quite right but will do for now.
// Double check with the segment filter test for all the edge cases.
ConfigSet
SegmentsFilter::SegmentFilter::
filter(const SegmentList& segments) const
{
    ConfigSet includes = emptyInclude;

    segments.forEach([&](int i, string str, float weights) {
                includes |= include.get(i, str);
            });

    segments.forEach([&](int i, string str, float weights) {
                includes &= exclude.get(i, str).negate();
            });

    return includes;
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
    }

} initFilters;


} // namespace anonymous
