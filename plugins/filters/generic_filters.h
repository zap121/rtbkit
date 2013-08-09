/** generic_filters.h                                 -*- C++ -*-
    Rémi Attab, 07 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Utilities for writting filters.

*/

#pragma once

#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/core/agent_configuration/include_exclude.h"
#include "rtbkit/common/filter.h"

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

    virtual bool filterConfig(FilterState&, const AgentConfig&) const
    {
        ExcAssert(false);
        return false;
    }

protected:
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
    template<typename List>
    bool isEmpty(const List& list) const
    {
        return list.empty();
    }

    template<typename List>
    void addConfig(unsigned configIndex, const List& list)
    {
        for (const auto& value : list)
            addConfig(configIndex, value);
    }

    template<typename List>
    void removeConfig(unsigned configIndex, const List& list)
    {
        for (const auto& value : list)
            removeConfig(configIndex, value);
    }

    ConfigSet filter(const Str& str) const
    {
        ConfigSet matches;

        for (const auto& entry : data)
            matches |= entry.second.filter(str);

        return matches;
    }

private:

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
/* LIST FILTER                                                                */
/******************************************************************************/

template<typename T, typename List = std::vector<T> >
struct ListFilter
{
    bool isEmpty(const List& list) const
    {
        return list.empty();
    }

    void addConfig(unsigned configIndex, const List& list)
    {
        setConfig(configIndex, list, true);
    }

    void removeConfig(unsigned configIndex, const List& list)
    {
        setConfig(configIndex, list, false);
    }

    ConfigSet filter(const T& value) const
    {
        auto it = data.find(value);
        return it == data.end()? ConfigSet() : it->second;
    }

    ConfigSet filter(const List& list) const
    {
        ConfigSet configs;

        for (const auto& entry : list)
            configs |= data[entry];

        return configs;
    }

private:

    void setConfig(unsigned configIndex, const List& list, bool value)
    {
        for (const auto& entry : list)
            data[entry].set(configIndex, value);
    }

    std::unordered_map<T, ConfigSet> data;
};


/******************************************************************************/
/* SEGMENT LIST FILTER                                                        */
/******************************************************************************/

/** Segments have quirks and are best handled seperatly from the list filter.

 */
struct SegmentListFilter
{
    bool isEmpty(const SegmentList& segments) const
    {
        return segments.empty();
    }

    void addConfig(unsigned configIndex, const SegmentList& segments)
    {
        setConfig(configIndex, segments, true);
    }

    void removeConfig(unsigned configIndex, const SegmentList& segments)
    {
        setConfig(configIndex, segments, false);
    }

    ConfigSet filter(int i, const std::string& str, float weights) const
    {
        return i >= 0 ? get(intSet, i) : get(strSet, str);
    }

    ConfigSet filter(const SegmentList& segments) const
    {
        ConfigSet configs;

        segments.forEach([&](int i, std::string str, float weights) {
                    configs |= filter(i, str, weights);
                });

        return configs;
    }

private:

    void setConfig(unsigned configIndex, const SegmentList& segments, bool value)
    {
        segments.forEach([&](int i, std::string str, float weights) {
                    if (i < 0) intSet[i].set(configIndex, value);
                    else strSet[str].set(configIndex, value);
                });
    }

    template<typename K>
    ConfigSet get(const std::unordered_map<K, ConfigSet>& m, K k) const
    {
        auto it = m.find(k);
        return it != m.end() ? it->second : ConfigSet();
    }

    std::unordered_map<int, ConfigSet> intSet;
    std::unordered_map<std::string, ConfigSet> strSet;
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
        if (includes.isEmpty(std::forward<Args>(args)...))
            emptyIncludes.set(configIndex);
        else includes.addConfig(configIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void addExclude(unsigned configIndex, Args&&... args)
    {
        excludes.addConfig(configIndex, std::forward<Args>(args)...);
    }

    template<typename T, typename IE = std::vector<T> >
    void addIncludeExclude(unsigned configIndex, const IncludeExclude<T, IE>& ie)
    {
        addInclude(configIndex, ie.include);
        addExclude(configIndex, ie.exclude);
    }


    template<typename... Args>
    void removeInclude(unsigned configIndex, Args&&... args)
    {
        if (includes.isEmpty(std::forward<Args>(args)...))
            emptyIncludes.reset(configIndex);
        else includes.removeConfig(configIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void removeExclude(unsigned configIndex, Args&&... args)
    {
        excludes.removeConfig(configIndex, std::forward<Args>(args)...);
    }

    template<typename T, typename IE = std::vector<T> >
    void removeIncludeExclude(unsigned configIndex, const IncludeExclude<T, IE>& ie)
    {
        removeInclude(configIndex, ie.include);
        removeExclude(configIndex, ie.exclude);
    }


    template<typename... Args>
    void setInclude(unsigned configIndex, bool value, Args&&... args)
    {
        if (value) addInclude(configIndex, std::forward<Args>(args)...);
        else removeInclude(configIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void setExclude(unsigned configIndex, bool value, Args&&... args)
    {
        if (value) addExclude(configIndex, std::forward<Args>(args)...);
        else removeExclude(configIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void setIncludeExclude(unsigned configIndex, bool value, Args&&... args)
    {
        if (value) addIncludeExclude(configIndex, std::forward<Args>(args)...);
        else removeIncludeExclude(configIndex, std::forward<Args>(args)...);
    }


    template<typename... Args>
    ConfigSet filter(Args&&... args) const
    {
        ConfigSet configs = emptyIncludes;

        configs |= includes.filter(std::forward<Args>(args)...);

        if (configs.empty()) return configs;

        configs &= excludes.filter(std::forward<Args>(args)...).negate();

        return configs;
    }


private:
    ConfigSet emptyIncludes;
    Filter includes;
    Filter excludes;
};


} // namespace RTBKIT
