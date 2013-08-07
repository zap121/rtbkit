/** generic_filters.h                                 -*- C++ -*-
    Rémi Attab, 07 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Utilities for writting filters.

*/

#pragma once

namespace RTBKIT {



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
    void addConfig(unsigned configIndex, const Str& pattern)
    {
        setConfig(configIndex, pattern, true);
    }

    void addConfig(unsigned configIndex, const Regex& regex)
    {
        setConfig(configIndex, regex.str(), true);
    }

    void addConfig(unsigned configIndex, const CachedRegex<Regex, Str>& regex)
    {
        setConfig(configIndex, regex.base.str(), true);
    }


    void removeConfig(unsigned configIndex, const Str& pattern)
    {
        setConfig(configIndex, pattern, false);
    }

    void removeConfig(unsigned configIndex, const Regex& regex)
    {
        setConfig(configIndex, regex.str(), false);
    }

    void removeConfig(unsigned configIndex, const CachedRegex<Regex, Str>& regex)
    {
        setConfig(configIndex, regex.base.str(), false);
    }


    ConfigSet filter(const Str& str) const
    {
        ConfigSet configs;

        for (const auto& entry : data)
            configs |= entry.second.filter(str);

        return configs;
    }

private:

    void setConfig(unsigned configIndex, const Str& pattern, bool value)
    {
        data[pattern].set(configIndex, pattern, value);
    }

    struct RegexData
    {
        Regex regex;
        ConfigSet configs;

        void set(unsigned configIndex, const Str& pattern, bool value)
        {
            if (regex.empty())
                createRegex(regex, pattern);

            configs.set(configIndex, value);
        }

        ConfigSet filter(const Str& str) const
        {
            return RTBKIT::matches(regex, str) ? configs : ConfigSet();
        }
    };

    std::unordered_map<Str, RegexData> data;
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
