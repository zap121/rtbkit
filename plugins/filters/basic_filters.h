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
/* SEGMENTS FILTER                                                            */
/******************************************************************************/

/** \todo weights and exchanges.

 */
struct SegmentsFilter : public FilterBaseT<SegmentsFilter>
{
    static constexpr const char* name = "Segments";
    unsigned priority() const { return Priority::Segments; }

    void setConfig(unsigned configIndex, const AgentConfig& config, bool value);

    void filter(FilterState& state) const;

private:

    struct SegmentData
    {
        IncludeExcludeFilter<SegmentListFilter> ie;
        ConfigSet excludeIfNotPresent;
    };

    std::unordered_map<std::string, SegmentData> data;
    std::unordered_set<std::string> excludeIfNotPresent;
};


/******************************************************************************/
/* HOUR OF WEEK FILTER                                                        */
/******************************************************************************/

struct HourOfWeekFilter : public FilterBaseT<HourOfWeekFilter>
{
    HourOfWeekFilter() { data.fill(ConfigSet()); }

    static constexpr const char* name = "HourOfWeek";
    unsigned priority() const { return Priority::HourOfWeek; }

    void setConfig(unsigned configIndex, const AgentConfig& config, bool value)
    {
        const auto& bitmap = config.hourOfWeekFilter.hourBitmap;
        for (size_t i = 0; i < bitmap.size(); ++i) {
            if (!bitmap[i]) continue;
            data[i].set(configIndex, value);
        }
    }

    void filter(FilterState& state) const
    {
        ExcCheckNotEqual(state.request.timestamp, Date(), "Null auction date");
        state.narrowConfigs(data[state.request.timestamp.hourOfWeek()]);
    }

private:

    std::array<ConfigSet, 24 * 7> data;
};


/******************************************************************************/
/* URL FILTER                                                                 */
/******************************************************************************/

struct UrlFilter : public FilterBaseT<UrlFilter>
{
    static constexpr const char* name = "Url";
    unsigned priority() const { return Priority::Url; }

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

struct LanguageFilter : public FilterBaseT<LanguageFilter>
{
    static constexpr const char* name = "Language";
    unsigned priority() const { return Priority::Language; }

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

struct LocationFilter : public FilterBaseT<LocationFilter>
{
    static constexpr const char* name = "Location";
    unsigned priority() const { return Priority::Location; }

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


/******************************************************************************/
/* FOLD POSITION FILTER                                                       */
/******************************************************************************/

struct FoldPositionFilter : public FilterBaseT<FoldPositionFilter>
{
    static constexpr const char* name = "FoldPosition";
    unsigned priority() const { return Priority::FoldPosition; }

    void addConfig(unsigned cfgIndex, const std::shared_ptr<AgentConfig>& config)
    {
        impl.addIncludeExclude(cfgIndex, config->foldPositionFilter);
    }

    void removeConfig(
            unsigned cfgIndex, const std::shared_ptr<AgentConfig>& config)
    {
        impl.removeIncludeExclude(cfgIndex, config->foldPositionFilter);
    }

    void filter(FilterState& state) const
    {
        for (const auto& imp : state.request.imp) {
            state.narrowConfigs(impl.filter(imp.position));
            if (state.configs().empty()) break;
        }
    }

private:
    IncludeExcludeFilter< ListFilter<OpenRTB::AdPosition> > impl;
};


/******************************************************************************/
/* REQUIRED IDS FILTER                                                        */
/******************************************************************************/

struct RequiredIdsFilter : public FilterBaseT<RequiredIdsFilter>
{
    static constexpr const char* name = "RequireIds";
    unsigned priority() const { return Priority::RequiredIds; }

    void setConfig(unsigned cfgIndex, const AgentConfig& config, bool value)
    {
        for (const auto& domain : config.requiredIds) {
            domains[domain].set(cfgIndex, value);
            required.insert(domain);
        }
    }

    void filter(FilterState& state) const
    {
        std::unordered_set<std::string> missing = required;

        for (const auto& uid : state.request.userIds)
            missing.erase(uid.first);

        ConfigSet mask;
        for (const auto& domain : missing) {
            auto it = domains.find(domain);
            ExcAssert(it != domains.end());
            mask |= it->second;
        }

        state.narrowConfigs(mask);
    }

private:

    std::unordered_map<std::string, ConfigSet> domains;
    std::unordered_set<std::string> required;
};

} // namespace RTBKIT
