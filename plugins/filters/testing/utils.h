/** utils.h                                 -*- C++ -*-
    Rémi Attab, 22 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Random utilities to write filter tests.

*/

#pragma once

#include "rtbkit/core/agent_configuration/include_exclude.h"
#include "rtbkit/common/segments.h"

#include <string>
#include <iostream>

namespace RTBKIT {


/******************************************************************************/
/* TITLE                                                                      */
/******************************************************************************/

void title(const std::string& title)
{
    using namespace std;

    size_t padding = 80 - 4 - title.size();
    cerr << "[ " << title << " ]" << string(padding, '-') << endl;
}


/******************************************************************************/
/* IE                                                                         */
/******************************************************************************/

template<typename T, typename List = std::vector<T> >
IncludeExclude<T, List>
ie(     const std::initializer_list<T>& includes,
        const std::initializer_list<T>& excludes)
{
    IncludeExclude<T, List> ie;
    for (const auto& v : includes) ie.include.push_back(v);
    for (const auto& v : excludes) ie.exclude.push_back(v);
    return ie;
}


/******************************************************************************/
/* SEGMENT                                                                    */
/******************************************************************************/

void segmentImpl(SegmentList& seg) {}

template<typename Arg>
void segmentImpl(SegmentList& seg, Arg&& arg)
{
    seg.add(std::forward<Arg>(arg));
}

template<typename Arg, typename... Args>
void segmentImpl(SegmentList& seg, Arg&& arg, Args&&... rest)
{
    segmentImpl(seg, std::forward<Arg>(arg));
    segmentImpl(seg, std::forward<Args>(rest)...);
}

template<typename... Args>
SegmentList
segment(Args&&... args)
{
    SegmentList seg;
    segmentImpl(seg, std::forward<Args>(args)...);
    seg.sort();
    return seg;
}

} // namespace RTBKIT
