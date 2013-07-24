/** basic_filters.cc                                 -*- C++ -*-
    Rémi Attab, 24 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Default pool of filters for an BidRequest object.

*/

#include "rtbkit/agent_configuration/agent_config.h"
#include "rtbkit/common/filter.h"

using namespace std;
using namespace ML;
using namespace RTBKIT;

namespace {


/******************************************************************************/
/* FILTER PRIORITY                                                            */
/******************************************************************************/

enum struct Priority : unsigned
{
    HourOfWeek = 0x10000,
    Segment    = 0x20000,
};


/******************************************************************************/
/* HOUR OF WEEK FILTER                                                        */
/******************************************************************************/

struct HourOfWeekFilter : public FilterBase<HourOfWeekFilter>
{
    HourOfWeekFilter() { data.fill(ConfigSet()); }

    static constexpr char* name = "hourOfWeek";

    unsigned priority() const { return Prioirty::HourOfWeek; }

    void setConfig(unsigned configIndex, const AgentConfig& config, bool value)
    {
        const auto& bitmap = config.hourOfWeekFilter.hourBitmap;
        for (size_t i = 0; i < bitmap.size(); ++i) {
            if (!bitmap[i]) continue;
            data[i].set(configIndex, value);
        }
    }

    void addConfig(unsigned configIndex, const AgentConfig& config)
    {
        setConfig(configIndex, config, true);
    }

    void removeConfig(unsigned configIndex, const AgentConfig& config)
    {
        setConfig(configIndex, config, false);
    }

    ConfigSet filter(const BidRequest& br, const ExchangeConnector*)
    {
        ExcCheckNotEqual(br.timestamp, Date(), "Null auction date");
        return return data[br.timestamp.hourOfWeek()];
    }

private:
    std::array<ConfigSet, 24 * 7> data;
};




/******************************************************************************/
/* SEGMENT FILTER                                                             */
/******************************************************************************/


/** \todo weights and exchanges.

 */
struct SegmentsFilter : public FilterBase<SegmentsFilter>
{
    SegmentsFilter() { data.fill(ConfigSet()); }

    static constexpr char* name = "segments";

    unsigned priority() const { return Prioirty::Segment; }

    void setConfig(unsigned configIndex, const AgentConfig& config, bool value)
    {
        for (const auto& segment : config.segments)
            data[segment.first].set(configIndex, segment.second, value);
    }

    void addConfig(unsigned configIndex, const AgentConfig& config)
    {
        setConfig(configIndex, configIndex, true);
    }

    void removeConfig(unsigned configIndex, const AgentConfig& config)
    {
        setConfig(configIndex, configIndex, false);
    }

    ConfigSet filter(const BidRequest& br, const ExchangeConnector*)
    {
        ConfigSet matches(true);

        for (const auto& segment : br.segments) {
            matches &= data[segment.first].filter(*segment.second);
            if (matches.empty()) return matches;
        }

        return matches;
    }


private:

    struct SegmentInfo
    {
        std::unordered_map<int, ConfigSet> intSet;
        std::unordered_map<string, ConfigSet> strSet;

        void
        set(unsigned configIndex, const SegmentList& segments, bool value) const
        {
            segments.forEach([&](int i, string str, float weights) {
                        if (i < 0)
                            intSet[i].set(configIndex, value);
                        else strSet[str].set(configIndex, value);
                    });
        }

        const ConfigSet& get(int i, string str) const
        {
            return i >= 0 ? intSet[i] : strSet[str];
        }
    };

    struct SegmentFilter
    {
        SegmentInfo include;
        SegmentInfo exclude;
        ConfigSet excludeIfNotPresent;

        void
        set(    unsigned configIndex,
                const AgentConfig::SegmentInfo& segments,
                bool value)
        {
            include.set(configIndex, segments.include, value);
            exclude.set(configIndex, segments.exclude, value);

            if (segments.excludeIfNotPresent)
                excludeIfNotPresent.set(configIndex, value);
        }

        // \todo This is not quite right but will do for now.
        // Double check with the segment filter test for all the edge cases.
        ConfigSet filter(const SegmentList& segments) const
        {
            ConfigSet includes;

            segments.forEach([&](int i, string str, float weights) {
                        includes |= include.get(i, str);
                    });

            segments.forEach([&](int i, string str, float weights) {
                        includes &= excludes.get(i, str).negate();
                    });

            return includes;
        }
    };

    unordered_map<string, SegmentFilter> data;
};



struct InitFilters
{
    InitFilters()
    {
        FilterRegistry::registerFilter<HourOfWeekFilter>(HourOfWeekFilter);
        FilterRegistry::registerFilter<SegmentsFilter>(SegmentsFilter);
    }

} initFilters;


} // namespace anonymous
