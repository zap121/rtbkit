/** priority.h                                 -*- C++ -*-
    Rémi Attab, 09 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Priority of the various filters.

*/

#pragma once

namespace RTBKIT {


/******************************************************************************/
/* FILTER PRIORITY                                                            */
/******************************************************************************/

struct Priority
{
    static constexpr unsigned Segments         = 0x000000;
    static constexpr unsigned HourOfWeek       = 0x000000;
    static constexpr unsigned FoldPosition     = 0x000000;

    static constexpr unsigned Url              = 0x000000;
    static constexpr unsigned Language         = 0x000000;
    static constexpr unsigned Location         = 0x000000;

    static constexpr unsigned ExchangePre      = 0x000000;
    static constexpr unsigned ExchangeName     = 0x000000;
    static constexpr unsigned ExchangePost     = 0x000000;

    static constexpr unsigned CreativeFormat   = 0x000000;
    static constexpr unsigned CreativeLanguage = 0x000000;
    static constexpr unsigned CreativeLocation = 0x000000;
    static constexpr unsigned CreativeExchange = 0x000000;
};


} // namespace RTBKIT
