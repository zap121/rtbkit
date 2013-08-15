/** generic_filters.h                                 -*- C++ -*-
    Rémi Attab, 07 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Utilities for writting filters.

*/

#pragma once

#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/core/agent_configuration/include_exclude.h"
#include "rtbkit/common/filter.h"

#include <boost/range/adaptor/reversed.hpp>

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

    void addConfig(unsigned cfgIndex, const std::shared_ptr<AgentConfig>& config)
    {
        setConfig(cfgIndex, *config, true);
    }

    void removeConfig(
            unsigned cfgIndex, const std::shared_ptr<AgentConfig>& config)
    {
        setConfig(cfgIndex, *config, false);
    }

    virtual void setConfig(unsigned cfgIndex, const AgentConfig& config, bool value)
    {
        ExcAssert(false);
    }

};


/******************************************************************************/
/* ITERATIVE FILTER                                                           */
/******************************************************************************/

/** These filters are discouraged because they do not scale well. */
template<typename Filter>
struct IterativeFilter : public FilterBaseT<Filter>
{
    virtual void addConfig(
            unsigned cfgIndex,
            const std::shared_ptr<AgentConfig>& config)
    {
        if (cfgIndex >= configs.size())
            configs.resize(cfgIndex + 1);

        configs[cfgIndex] = config;
    }

    virtual void removeConfig(
            unsigned cfgIndex,
            const std::shared_ptr<AgentConfig>& config)
    {
        configs[cfgIndex].reset();
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
/* INTERVAL FILTER                                                            */
/******************************************************************************/

template<typename T>
struct IntervalFilter
{
    template<typename List>
    bool isEmpty(const List& list) const
    {
        return list.empty();
    }

    template<typename List>
    void addConfig(unsigned cfgIndex, const List& list)
    {
        for (const auto& value : list)
            addConfig(cfgIndex, value);
    }

    template<typename List>
    void removeConfig(unsigned cfgIndex, const List& list)
    {
        for (const auto& value : list)
            removeConfig(cfgIndex, value);
    }

    ConfigSet filter(const T& value) const
    {
        ConfigSet matches;

        for (const auto& ub : upperBounds) {
            if (value >= ub.first) break;
            matches |= ub.second;
        }

        for (const auto& lb : lowerBounds) {
            if (value >= lb.first) break;
            matches &= lb.second.negate();
        }

        return matches;
    }

private:

    void addConfig(unsigned cfgIndex, const std::pair<T, T>& interval)
    {
        insertBound(cfgIndex, lowerBounds, interval.first);
        insertBound(cfgIndex, upperBounds, interval.second);
    }

    void removeConfig(unsigned cfgIndex, const std::pair<T, T>& interval)
    {
        removeBound(cfgIndex, lowerBounds, interval.first);
        removeBound(cfgIndex, upperBounds, interval.second);
    }

    void addConfig(unsigned cfgIndex, const UserPartition::Interval& interval)
    {
        insertBound(cfgIndex, lowerBounds, interval.first);
        insertBound(cfgIndex, upperBounds, interval.last);
    }

    void removeConfig(unsigned cfgIndex, const UserPartition::Interval& interval)
    {
        removeBound(cfgIndex, lowerBounds, interval.first);
        removeBound(cfgIndex, upperBounds, interval.last);
    }


    typedef std::vector< std::pair<T, ConfigSet> > BoundList;

    void insertBound(unsigned cfgIndex, BoundList& list, T bound)
    {
        ssize_t index = findBound(list, bound);

        if (index < 0) {
            index = list.size();
            list.emplace_back(bound, ConfigSet());
        }

        if (list[index].first != bound)
            list.insert(list.begin() + index, make_pair(bound, ConfigSet()));

        ExcAssertEqual(list[index].first, bound);

        list[index].second.set(cfgIndex);
    }

    void removeBound(unsigned cfgIndex, BoundList& list, T bound)
    {
        ssize_t index = findBound(list, bound);
        if (index < 0 || list[index].first != bound) return;

        list[index].second.reset(cfgIndex);
        if (!list[index].second.empty()) return;

        list.erase(list.begin() + index);
    }

    ssize_t findBound(const BoundList& list, T bound)
    {
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].first >= bound) return i;
        }
        return -1;
    }

    BoundList lowerBounds;
    BoundList upperBounds;
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
    void addConfig(unsigned cfgIndex, const List& list)
    {
        for (const auto& value : list)
            addConfig(cfgIndex, value);
    }

    template<typename List>
    void removeConfig(unsigned cfgIndex, const List& list)
    {
        for (const auto& value : list)
            removeConfig(cfgIndex, value);
    }

    ConfigSet filter(const Str& str) const
    {
        ConfigSet matches;

        for (const auto& entry : data)
            matches |= entry.second.filter(str);

        return matches;
    }

private:

    void addConfig(unsigned cfgIndex, const Regex& regex)
    {
        auto& entry = data[regex.str()];
        if (entry.regex.empty()) entry.regex = regex;
        entry.configs.set(cfgIndex);
    }

    void addConfig(unsigned cfgIndex, const CachedRegex<Regex, Str>& regex)
    {
        addConfig(cfgIndex, regex.base);
    }


    void removeConfig(unsigned cfgIndex, const Regex& regex)
    {
        auto it = data.find(regex.str());
        if (it == data.end()) return;

        it->second.configs.reset(cfgIndex);
        if (it->second.configs.empty()) data.erase(it);
    }

    void removeConfig(unsigned cfgIndex, const CachedRegex<Regex, Str>& regex)
    {
        removeConfig(cfgIndex, regex.base);
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

    void addConfig(unsigned cfgIndex, const List& list)
    {
        setConfig(cfgIndex, list, true);
    }

    void removeConfig(unsigned cfgIndex, const List& list)
    {
        setConfig(cfgIndex, list, false);
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

    void setConfig(unsigned cfgIndex, const List& list, bool value)
    {
        for (const auto& entry : list)
            data[entry].set(cfgIndex, value);
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

    void addConfig(unsigned cfgIndex, const SegmentList& segments)
    {
        setConfig(cfgIndex, segments, true);
    }

    void removeConfig(unsigned cfgIndex, const SegmentList& segments)
    {
        setConfig(cfgIndex, segments, false);
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

    void setConfig(unsigned cfgIndex, const SegmentList& segments, bool value)
    {
        segments.forEach([&](int i, std::string str, float weights) {
                    if (i < 0) intSet[i].set(cfgIndex, value);
                    else strSet[str].set(cfgIndex, value);
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
    void addInclude(unsigned cfgIndex, Args&&... args)
    {
        if (includes.isEmpty(std::forward<Args>(args)...))
            emptyIncludes.set(cfgIndex);
        else includes.addConfig(cfgIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void addExclude(unsigned cfgIndex, Args&&... args)
    {
        excludes.addConfig(cfgIndex, std::forward<Args>(args)...);
    }

    template<typename T, typename IE = std::vector<T> >
    void addIncludeExclude(unsigned cfgIndex, const IncludeExclude<T, IE>& ie)
    {
        addInclude(cfgIndex, ie.include);
        addExclude(cfgIndex, ie.exclude);
    }


    template<typename... Args>
    void removeInclude(unsigned cfgIndex, Args&&... args)
    {
        if (includes.isEmpty(std::forward<Args>(args)...))
            emptyIncludes.reset(cfgIndex);
        else includes.removeConfig(cfgIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void removeExclude(unsigned cfgIndex, Args&&... args)
    {
        excludes.removeConfig(cfgIndex, std::forward<Args>(args)...);
    }

    template<typename T, typename IE = std::vector<T> >
    void removeIncludeExclude(unsigned cfgIndex, const IncludeExclude<T, IE>& ie)
    {
        removeInclude(cfgIndex, ie.include);
        removeExclude(cfgIndex, ie.exclude);
    }


    template<typename... Args>
    void setInclude(unsigned cfgIndex, bool value, Args&&... args)
    {
        if (value) addInclude(cfgIndex, std::forward<Args>(args)...);
        else removeInclude(cfgIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void setExclude(unsigned cfgIndex, bool value, Args&&... args)
    {
        if (value) addExclude(cfgIndex, std::forward<Args>(args)...);
        else removeExclude(cfgIndex, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void setIncludeExclude(unsigned cfgIndex, bool value, Args&&... args)
    {
        if (value) addIncludeExclude(cfgIndex, std::forward<Args>(args)...);
        else removeIncludeExclude(cfgIndex, std::forward<Args>(args)...);
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
