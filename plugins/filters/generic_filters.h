/** generic_filters.h                                 -*- C++ -*-
    Rémi Attab, 07 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Utilities for writting filters.

*/

#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/core/agent_configuration/include_exclude.h"
#include "rtbkit/common/filter.h"

#pragma once

namespace RTBKIT {


/******************************************************************************/
/* FILTER BASE T                                                              */
/******************************************************************************/

template<typename Filter>
struct FilterBaseT : public FilterBase
{
    std::string name() const { return Filter::name; }

    FilterBase* clone() const
    {
        return new Filter(*static_cast<const Filter*>(this));
    }
};


/******************************************************************************/
/* CONFIG FILTER                                                              */
/******************************************************************************/

/** These filters are discouraged because they do not scale well. */
template<typename Filter>
struct IterativeFilter : public FilterBaseT<Filter>
{
    virtual void addConfig(
            unsigned configIndex,
            const std::shared_ptr<AgentConfig>& config)
    {
        if (configIndex >= configs.size())
            configs.resize(configIndex + 1);

        configs[configIndex] = config;
    }

    virtual void removeConfig(
            unsigned configIndex,
            const std::shared_ptr<AgentConfig>& config)
    {
        configs[configIndex].reset();
    }

    virtual void filter(FilterState& state) const
    {
        ConfigSet matches = state.configs();

        for (size_t i = matches.next();
             i < matches.size();
             i = matches.next(i+1))
        {
            ExcAssert(configs[i]);

            if (!filterConfig(state, *configs[i])) continue;
            matches.reset(i);
        }

        state.narrowConfigs(matches);
    }

    virtual bool filterConfig(FilterState&, const AgentConfig&) const = 0;

private:
    std::vector< std::shared_ptr<AgentConfig> > configs;
};


/******************************************************************************/
/* REGEX FILTER                                                               */
/******************************************************************************/

/** Generic include filter for regexes.

    \todo We could add a TLS cache of all seen values such that we can avoid the
    regex entirely.
 */
template<typename Regex, typename Str>
struct RegexFilter
{
    void addConfig(unsigned configIndex, const Regex& regex)
    {
        auto& entry = data[regex.str()];
        if (entry.regex.empty()) entry.regex = regex;
        entry.configs.set(configIndex);
    }

    void addConfig(unsigned configIndex, const CachedRegex<Regex, Str>& regex)
    {
        addConfig(configIndex, regex.base);
    }

    void removeConfig(unsigned configIndex, const Regex& regex)
    {
        auto it = data.find(regex.str());
        if (it == data.end()) return;

        it->second.configs.reset(configIndex);
        if (it->second.configs.empty()) data.erase(it);
    }

    void removeConfig(unsigned configIndex, const CachedRegex<Regex, Str>& regex)
    {
        removeConfig(configIndex, regex.base);
    }


    ConfigSet filter(const Str& str) const
    {
        ConfigSet matches;

        for (const auto& entry : data)
            matches |= entry.second.filter(str);

        return matches;
    }

private:

    struct RegexData
    {
        Regex regex;
        ConfigSet configs;

        ConfigSet filter(const Str& str) const
        {
            return RTBKIT::matches(regex, str) ? configs : ConfigSet();
        }
    };

    typedef std::basic_string<typename Regex::value_type> KeyT;
    std::unordered_map<KeyT, RegexData> data;
};


/******************************************************************************/
/* INCLUDE EXCLUDE FILTER                                                     */
/******************************************************************************/

template<typename Filter>
struct IncludeExcludeFilter
{

    template<typename... Args>
    void addInclude(unsigned configIndex, Args&&... args)
    {
        includes.addConfig(configIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void addExclude(unsigned configIndex, Args&&... args)
    {
        excludes.addConfig(configIndex, std::forward<Args>(args)...);
    }

    template<typename T, typename IE = std::vector<T> >
    void addIncludeExclude(unsigned configIndex, const IncludeExclude<T, IE>& ie)
    {
        for (const auto& value : ie.include)
            addInclude(configIndex, value);

        for (const auto& value : ie.exclude)
            addExclude(configIndex, value);
    }


    template<typename... Args>
    void removeInclude(unsigned configIndex, Args&&... args)
    {
        includes.removeConfig(configIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void removeExclude(unsigned configIndex, Args&&... args)
    {
        excludes.removeConfig(configIndex, std::forward<Args>(args)...);
    }

    template<typename T, typename IE = std::vector<T> >
    void removeIncludeExclude(unsigned configIndex, const IncludeExclude<T, IE>& ie)
    {
        for (const auto& value : ie.include)
            removeInclude(configIndex, value);

        for (const auto& value : ie.exclude)
            removeExclude(configIndex, value);
    }


    template<typename... Args>
    ConfigSet filter(Args&&... args) const
    {
        ConfigSet configs;

        configs |= includes.filter(std::forward<Args>(args)...);
        if (configs.empty()) return configs;

        configs &= excludes.filter(std::forward<Args>(args)...).negate();
        return configs;
    }


private:
    Filter includes;
    Filter excludes;
};



} // namespace RTBKIT
