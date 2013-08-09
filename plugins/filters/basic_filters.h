/** basic_filters.h                                 -*- C++ -*-
    Rémi Attab, 26 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Default pool of filters for a bid request object.

*/

#pragma once

#include "generic_filters.h"
#include "priority.h"
#include "rtbkit/common/exchange_connector.h"
#include "jml/utils/compact_vector.h"

#include <array>
#include <unordered_map>
#include <unordered_set>
#include <mutex>


namespace RTBKIT {



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

    void filter(FilterState& state) const
    {
        ExcCheckNotEqual(state.request.timestamp, Date(), "Null auction date");
        state.narrowConfigs(data[state.request.timestamp.hourOfWeek()]);
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

    void filter(FilterState& state) const;


private:

    void setConfig(unsigned configIndex, const AgentConfig& config, bool value);

    struct SegmentData
    {
        IncludeExcludeFilter<SegmentListFilter> ie;
        ConfigSet emptyInclude;
        ConfigSet excludeIfNotPresent;
    };

    std::unordered_map<std::string, SegmentData> data;
    std::unordered_set<std::string> excludeIfNotPresent;
};


/******************************************************************************/
/* URL FILTER                                                                 */
/******************************************************************************/

struct UrlRegexFilter : public FilterBaseT<UrlRegexFilter>
{
    static constexpr const char* name = "UrlRegex";
    unsigned priority() const { return Priority::UrlRegex; }

    void addConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        impl.addIncludeExclude(configIndex, config->urlFilter);
    }

    void removeConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        impl.removeIncludeExclude(configIndex, config->urlFilter);
    }

    void filter(FilterState& state) const
    {
        state.narrowConfigs(impl.filter(state.request.url.toString()));
    }

private:
    typedef RegexFilter<boost::regex, std::string> BaseFilter;
    IncludeExcludeFilter<BaseFilter> impl;
};


/******************************************************************************/
/* LANGUAGE FILTER                                                            */
/******************************************************************************/

struct LanguageRegexFilter : public FilterBaseT<LanguageRegexFilter>
{
    static constexpr const char* name = "LanguageRegex";
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

    void filter(FilterState& state) const
    {
        state.narrowConfigs(impl.filter(state.request.language.rawString()));
    }

private:
    typedef RegexFilter<boost::regex, std::string> BaseFilter;
    IncludeExcludeFilter<BaseFilter> impl;
};


/******************************************************************************/
/* LOCATION FILTER                                                            */
/******************************************************************************/

struct LocationRegexFilter : public FilterBaseT<LocationRegexFilter>
{
    static constexpr const char* name = "LocationRegex";
    unsigned priority() const { return Priority::LocationRegex; }

    void addConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        impl.addIncludeExclude(configIndex, config->locationFilter);
    }

    void removeConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        impl.removeIncludeExclude(configIndex, config->locationFilter);
    }

    void filter(FilterState& state) const
    {
        Utf8String location = state.request.location.fullLocationString();
        state.narrowConfigs(impl.filter(location));
    }

private:
    typedef RegexFilter<boost::u32regex, Utf8String> BaseFilter;
    IncludeExcludeFilter<BaseFilter> impl;
};


/******************************************************************************/
/* EXCHANGE PRE/POST FILTER                                                   */
/******************************************************************************/

/** The lock makes it next to impossible to do any kind of pre-processing. */
struct ExchangePreFilter : public IterativeFilter<ExchangePreFilter>
{
    static constexpr const char* name = "ExchangePre";
    unsigned priority() const { return Priority::ExchangePre; }

    bool filterConfig(FilterState& state, const AgentConfig& config) const
    {
        if (!state.exchange) return true;

        const void * exchangeInfo = nullptr;

        {
            std::lock_guard<ML::Spinlock> guard(config.lock);
            auto it = config.providerData.find(state.exchange->exchangeName());
            if (it == config.providerData.end()) return false;

            exchangeInfo = it->second.get();
        }

        return state.exchange->bidRequestPreFilter(
                state.request, config, exchangeInfo);
    }
};

struct ExchangePostFilter : public IterativeFilter<ExchangePostFilter>
{
    static constexpr const char* name = "ExchangePost";
    unsigned priority() const { return Priority::ExchangePost; }

    bool filterConfig(FilterState& state, const AgentConfig& config) const
    {
        if (!state.exchange) return true;

        const void * exchangeInfo = nullptr;

        {
            std::lock_guard<ML::Spinlock> guard(config.lock);
            auto it = config.providerData.find(state.exchange->exchangeName());
            if (it == config.providerData.end()) return false;

            exchangeInfo = it->second.get();
        }

        return state.exchange->bidRequestPostFilter(
                state.request, config, exchangeInfo);
    }
};


/******************************************************************************/
/* EXCHANGE NAME FILTER                                                       */
/******************************************************************************/

struct ExchangeNameFilter : public FilterBaseT<ExchangeNameFilter>
{
    static constexpr const char* name = "ExchangeName";
    unsigned priority() const { return Priority::ExchangeName; }


    void addConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        data.addIncludeExclude(configIndex, config->exchangeFilter);
    }

    void removeConfig(
            unsigned configIndex, const std::shared_ptr<AgentConfig>& config)
    {
        data.removeIncludeExclude(configIndex, config->exchangeFilter);
    }

    void filter(FilterState& state) const
    {
        data.filter(state.request.exchange);
    }

private:
    IncludeExcludeFilter< ListFilter<std::string> > data;
};


} // namespace RTBKIT
