/** creative_filter.h                                 -*- C++ -*-
    Rémi Attab, 09 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Filters related to creatives.

*/

#pragma once

#include "generic_creative_filters.h"
#include "priority.h"

namespace RTBKIT {


/******************************************************************************/
/* CREATIVE FORTMAT FILTER                                                    */
/******************************************************************************/

struct CreativeFormatFilter : public CreativeFilter<CreativeFormatFilter>
{
    static constexpr const char* name = "CreativeFormat";
    unsigned priority() const { return Priority::CreativeFormat; }

    void addCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        auto formatKey = makeKey(creative.format);
        formatFilter[formatKey].set(crIndex, cfgIndex);
    }

    void removeCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        auto formatKey = makeKey(creative.format);
        formatFilter[formatKey].reset(crIndex, cfgIndex);
    }

    void filterImpression(
            FilterState& state, unsigned impIndex, const AdSpot& imp) const
    {
        CreativeMatrix creatives;

        for (const auto& format : imp.formats) {
            auto it = formatFilter.find(makeKey(format));
            if (it == formatFilter.end()) continue;

            creatives |= it->second;
        }

        state.narrowCreatives(impIndex, creatives);
    }


private:

    typedef uint32_t FormatKey;
    static_assert(sizeof(FormatKey) == sizeof(Format),
            "Conversion of FormatKey depends on size of Format");

    FormatKey makeKey(const Format& format) const
    {
        return uint32_t(format.width << 16 | format.height);
    }

    std::unordered_map<uint32_t, CreativeMatrix> formatFilter;
};


/******************************************************************************/
/* CREATIVE LANGUAGE FILTER                                                   */
/******************************************************************************/

struct CreativeLanguageFilter : public CreativeFilter<CreativeLanguageFilter>
{
    static constexpr const char* name = "CreativeLanguage";
    unsigned priority() const { return Priority::CreativeLanguage; }

    void addCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.addIncludeExclude(cfgIndex, crIndex, creative.languageFilter);
    }

    void removeCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.removeIncludeExclude(cfgIndex, crIndex, creative.languageFilter);
    }

    void filterImpression(
            FilterState& state, unsigned impIndex, const AdSpot& imp) const
    {
        state.narrowCreatives(
                impIndex, impl.filter(state.request.language.rawString()));
    }

private:
    CreativeIncludeExcludeFilter< CreativeListFilter<std::string> > impl;
};


/******************************************************************************/
/* CREATIVE LOCATION FILTER                                                   */
/******************************************************************************/

struct CreativeLocationFilter : public CreativeFilter<CreativeLocationFilter>
{
    static constexpr const char* name = "CreativeLanguage";
    unsigned priority() const { return Priority::CreativeLanguage; }

    void addCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.addIncludeExclude(cfgIndex, crIndex, creative.locationFilter);
    }

    void removeCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.removeIncludeExclude(cfgIndex, crIndex, creative.locationFilter);
    }

    void filterImpression(
            FilterState& state, unsigned impIndex, const AdSpot& imp) const
    {
        state.narrowCreatives(
                impIndex, impl.filter(state.request.location.fullLocationString()));
    }

private:
    typedef CreativeRegexFilter<boost::u32regex, Utf8String> BaseFilter;
    CreativeIncludeExcludeFilter<BaseFilter> impl;
};


} // namespace RTBKIT
