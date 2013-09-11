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
    static constexpr unsigned ExchangePre          = 0x0010;

    static constexpr unsigned FoldPosition         = 0x1100;
    static constexpr unsigned CreativeFormat       = 0x1200;
    static constexpr unsigned CreativeLocation     = 0x1300;
    static constexpr unsigned CreativeExchangeName = 0x1400;
    static constexpr unsigned CreativeLanguage     = 0x1500;

    // Really slow so delay as much as possible.
    static constexpr unsigned CreativeExchange     = 0x1600;

    static constexpr unsigned RequiredIds          = 0x3000;
    static constexpr unsigned HourOfWeek           = 0x4000;
    static constexpr unsigned ExchangeName         = 0x5000;
    static constexpr unsigned Location             = 0x6000;
    static constexpr unsigned Language             = 0x7000;

    static constexpr unsigned Segments             = 0x8000;
    static constexpr unsigned UserPartition        = 0x8010;
    static constexpr unsigned Host                 = 0x8500;
    static constexpr unsigned Url                  = 0x9000;

    static constexpr unsigned ExchangePost         = 0x9900;
};


} // namespace RTBKIT
