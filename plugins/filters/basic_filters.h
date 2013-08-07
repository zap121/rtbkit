/** basic_filters.h                                 -*- C++ -*-
    Rémi Attab, 26 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Default pool of filters for a bid request object.

*/

#pragma once

#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/core/agent_configuration/include_exclude.h"
#include "rtbkit/common/filter.h"
#include "generic_filters.h"

#include <array>
#include <unordered_map>
#include <unordered_set>


namespace RTBKIT {


/******************************************************************************/
/* FILTER PRIORITY                                                            */
/******************************************************************************/

struct Priority
{
    static constexpr unsigned HourOfWeek    = 0x10000;
    static constexpr unsigned Segment       = 0x20000;
    static constexpr unsigned LanguageRegex = 0x30000;
};


/******************************************************************************/
/* HOUR OF WEEK FILTER                                                        */
/******************************************************************************/

struct HourOfWeekFilter : public FilterBaseT<HourOfWeekFilter>
{
    HourOfWeekFilter() { data.fill(ConfigSet()); }

    static constexpr const char* name = "hourOfWeek";
    unsigned priority() const { return Priority::HourOfWeek; }

    void addConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        setConfig(configIndex, *config, true);
    }

    void removeConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        setConfig(configIndex, *config, false);
    }

    ConfigSet filter(const BidRequest& br, const ExchangeConnector*) const
    {
        ExcCheckNotEqual(br.timestamp, Date(), "Null auction date");
        return data[br.timestamp.hourOfWeek()];
    }

private:

    void setConfig(unsigned configIndex, const AgentConfig& config, bool value)
    {
        const auto& bitmap = config.hourOfWeekFilter.hourBitmap;
        for (size_t i = 0; i < bitmap.size(); ++i) {
            if (!bitmap[i]) continue;
            data[i].set(configIndex, value);
        }
    }

    std::array<ConfigSet, 24 * 7> data;
};


/******************************************************************************/
/* SEGMENTS FILTER                                                            */
/******************************************************************************/

/** \todo weights and exchanges.
    \todo build on top of the IncludeExclude filter.

 */
struct SegmentsFilter : public FilterBaseT<SegmentsFilter>
{
    static constexpr const char* name = "segments";
    unsigned priority() const { return Priority::Segment; }

    void addConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        setConfig(configIndex, *config, true);
    }

    void removeConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        setConfig(configIndex, *config, false);
    }

    ConfigSet filter(const BidRequest& br, const ExchangeConnector*) const;


private:

    struct SegmentInfo
    {
        std::unordered_map<int, ConfigSet> intSet;
        std::unordered_map<std::string, ConfigSet> strSet;

        void set(unsigned configIndex, const SegmentList& segments, bool value);

        template<typename K>
        ConfigSet get(const std::unordered_map<K, ConfigSet>& m, K k) const
        {
            auto it = m.find(k);
            return it != m.end() ? it->second : ConfigSet();
        }

        ConfigSet get(int i, std::string str) const
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

        void set(
                unsigned configIndex,
                const AgentConfig::SegmentInfo& segments,
                bool value);

        // \todo This is not quite right but will do for now.
        // Double check with the segment filter test for all the edge cases.
        ConfigSet filter(const SegmentList& segments) const;
    };

    void setConfig(unsigned configIndex, const AgentConfig& config, bool value);

    std::unordered_map<std::string, SegmentFilter> data;
    std::unordered_set<std::string> excludeIfNotPresent;
};


/******************************************************************************/
/* LANGUAGE FILTER                                                            */
/******************************************************************************/

struct LanguageRegexFilter : public FilterBaseT<LanguageRegexFilter>
{
    static constexpr const char* name = "languageRegex";
    unsigned priority() const { return Priority::LanguageRegex; }

    void addConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        impl.addIncludeExclude(configIndex, config->languageFilter);
    }

    void removeConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        impl.removeIncludeExclude(configIndex, config->languageFilter);
    }

    ConfigSet filter(const BidRequest& br, const ExchangeConnector*) const
    {
        return impl.filter(br.language.rawString());
    }

private:
    typedef RegexFilter<boost::regex, std::string> BaseFilter;
    IncludeExcludeFilter<BaseFilter> impl;
};


} // namespace RTBKIT
