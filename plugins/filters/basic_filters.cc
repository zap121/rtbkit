/** basic_filters.cc                                 -*- C++ -*-
    Rémi Attab, 24 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Default pool of filters for an BidRequest object.

*/

#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/common/filter.h"

#include <array>
#include <unordered_map>
#include <unordered_set>


using namespace std;
using namespace ML;
using namespace RTBKIT;

namespace {


/******************************************************************************/
/* FILTER PRIORITY                                                            */
/******************************************************************************/

struct Priority
{
    static constexpr unsigned HourOfWeek = 0x10000;
    static constexpr unsigned Segment    = 0x20000;
};


/******************************************************************************/
/* HOUR OF WEEK FILTER                                                        */
/******************************************************************************/

struct HourOfWeekFilter : public FilterBaseT<HourOfWeekFilter>
{
    HourOfWeekFilter() { data.fill(ConfigSet()); }

    static constexpr const char* name = "hourOfWeek";

    unsigned priority() const { return Priority::HourOfWeek; }

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

    ConfigSet filter(const BidRequest& br, const ExchangeConnector*) const
    {
        ExcCheckNotEqual(br.timestamp, Date(), "Null auction date");
        return data[br.timestamp.hourOfWeek()];
    }

private:
    array<ConfigSet, 24 * 7> data;
};


/******************************************************************************/
/* SEGMENT FILTER                                                             */
/******************************************************************************/


/** \todo weights and exchanges.

 */
struct SegmentsFilter : public FilterBaseT<SegmentsFilter>
{
    static constexpr const char* name = "segments";

    unsigned priority() const { return Priority::Segment; }

    void setConfig(unsigned configIndex, const AgentConfig& config, bool value)
    {
        for (const auto& segment : config.segments) {
            data[segment.first].set(configIndex, segment.second, value);

            if (segment.second.excludeIfNotPresent)
                excludeIfNotPresent.insert(segment.first);
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

    ConfigSet filter(const BidRequest& br, const ExchangeConnector*) const
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


private:

    struct SegmentInfo
    {
        unordered_map<int, ConfigSet> intSet;
        unordered_map<string, ConfigSet> strSet;

        void
        set(unsigned configIndex, const SegmentList& segments, bool value)
        {
            segments.forEach([&](int i, string str, float weights) {
                        if (i < 0)
                            intSet[i].set(configIndex, value);
                        else strSet[str].set(configIndex, value);
                    });
        }

        template<typename K>
        ConfigSet get(const unordered_map<K, ConfigSet>& m, K k) const
        {
            auto it = m.find(k);
            return it != m.end() ? it->second : ConfigSet();
        }

        ConfigSet get(int i, string str) const
        {
            return i >= 0 ? get(intSet, i) : get(strSet, str);
        }
    };

    struct SegmentFilter
    {
        SegmentInfo include;
        SegmentInfo exclude;
        ConfigSet emptyInclude;
        ConfigSet excludeIfNotPresent;

        void
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
        ConfigSet filter(const SegmentList& segments) const
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
    };

    unordered_map<string, SegmentFilter> data;
    unordered_set<string> excludeIfNotPresent;
};


/******************************************************************************/
/* INIT FILTERS                                                               */
/******************************************************************************/

struct InitFilters
{
    InitFilters()
    {
        FilterRegistry::registerFilter<HourOfWeekFilter>();
        FilterRegistry::registerFilter<SegmentsFilter>();
    }

} initFilters;


} // namespace anonymous
