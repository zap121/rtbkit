/** static_filters_test.cc                                 -*- C++ -*-
    Rémi Attab, 22 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Tests for the static filters.

    Note that these tests assume that the generic filters work properly so we
    don't waste time constructing complicated set of include/exclude statements.

    \todo UserParitionFilter
    \todo HourOfTheWeek
    \todo Exchange Pre/Post filter ???
    \todo RequiredIdsFilter
 */

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include "utils.h"
#include "rtbkit/plugins/filters/static_filters.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/common/bid_request.h"
#include "rtbkit/common/exchange_connector.h"
#include "jml/utils/vector_utils.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace ML;
using namespace Datacratic;

void check(
        const FilterBase& filter,
        BidRequest& request,
        const std::string exchangeName,
        const ConfigSet& mask,
        const initializer_list<size_t>& exp)
{
    FilterExchangeConnector conn(exchangeName);

    /* Note that some filters depends on the bid request's exchange field while
       others depend on the exchange connector's name. Now you might think that
       they're always the same but you'd be wrong. To alleviate my endless pain
       on this subject, let's just fudge it here and call it a day.
     */
    request.exchange = exchangeName;

    vector<unsigned> creativesCount;
    for (size_t i = mask.next(); i < mask.size(); i = mask.next(i+1)) {
        if (creativesCount.size() <= i)
            creativesCount.resize(i+1, 0);
        creativesCount[i] = 1;
    }

    FilterState state(request, &conn, creativesCount);

    filter.filter(state);
    check(state.configs() & mask, exp);
}

void
add(    AgentConfig& config,
        const string& name,
        bool excludeIfNotPresent,
        const SegmentList& includes,
        const SegmentList& excludes,
        const IncludeExclude<string>& exchangeIE)
{
    AgentConfig::SegmentInfo seg;

    seg.excludeIfNotPresent = excludeIfNotPresent;
    seg.include = includes;
    seg.exclude = excludes;
    seg.applyToExchanges = exchangeIE;

    config.segments[name] = seg;
};


void
add(    BidRequest& br,
        const string& name,
        const SegmentList& segment)
{
    br.segments[name] = make_shared<SegmentList>(segment);
};


/** Simple test to check that all a config will fail if one of its segment
    fails. Nothing to interesting really.
 */
BOOST_AUTO_TEST_CASE( segmentFilter_simple )
{
    SegmentsFilter filter;
    ConfigSet mask;

    auto doCheck = [&] (
            BidRequest& request,
            const std::string& exchangeName,
            const initializer_list<size_t>& expected)
    {
        check(filter, request, exchangeName, mask, expected);
    };

    AgentConfig c0;
    add(c0, "seg1", false, segment(1), segment(), ie<string>());
    add(c0, "seg2", false, segment(2), segment(), ie<string>());
    add(c0, "seg3", false, segment(3), segment(), ie<string>());

    BidRequest r0;
    add(r0, "seg1", segment(1));
    add(r0, "seg2", segment(2));
    add(r0, "seg3", segment(3));
    add(r0, "seg4", segment(4));

    BidRequest r1;
    add(r1, "seg1", segment(0));
    add(r1, "seg2", segment(2));
    add(r1, "seg3", segment(3));
    add(r1, "seg4", segment(4));

    BidRequest r2;
    add(r2, "seg1", segment(1));
    add(r2, "seg2", segment(0));
    add(r2, "seg3", segment(3));
    add(r2, "seg4", segment(4));

    BidRequest r3;
    add(r3, "seg1", segment(1));
    add(r3, "seg2", segment(2));
    add(r3, "seg3", segment(0));
    add(r3, "seg4", segment(4));

    title("segment-simple-1");
    addConfig(filter, 0, c0); mask.set(0);

    doCheck(r0, "ex0", { 0 });
    doCheck(r1, "ex0", { });
    doCheck(r2, "ex0", { });
    doCheck(r3, "ex0", { });
}

/** Tests for the endlessly confusing excludeIfNotPresent attribute. */
BOOST_AUTO_TEST_CASE( segmentFilter_excludeIfNotPresent )
{
    SegmentsFilter filter;
    ConfigSet mask;

    auto doCheck = [&] (
            BidRequest& request,
            const std::string& exchangeName,
            const initializer_list<size_t>& expected)
    {
        check(filter, request, exchangeName, mask, expected);
    };

    AgentConfig c0;

    AgentConfig c1;
    add(c1, "seg1", false, segment(), segment(), ie<string>());

    AgentConfig c2;
    add(c2, "seg1", true, segment(), segment(), ie<string>());

    BidRequest r0;

    BidRequest r1;
    add(r1, "seg1", segment("a"));

    BidRequest r2;
    add(r2, "seg1", segment("a"));
    add(r2, "seg2", segment("b"));

    BidRequest r3;
    add(r3, "seg2", segment("b"));
    add(r3, "seg3", segment("c"));

    title("segment-excludeIfNotPresent-1");
    addConfig(filter, 0, c0); mask.set(0);
    addConfig(filter, 1, c1); mask.set(1);
    addConfig(filter, 2, c2); mask.set(2);

    doCheck(r0, "ex0", { 0, 1 });
    doCheck(r1, "ex0", { 0, 1, 2 });
    doCheck(r2, "ex0", { 0, 1, 2 });
    doCheck(r3, "ex0", { 0, 1 });

    title("segment-excludeIfNotPresent-2");
    removeConfig(filter, 0, c0); mask.reset(0);

    doCheck(r0, "ex0", { 1 });
    doCheck(r1, "ex0", { 1, 2 });
    doCheck(r2, "ex0", { 1, 2 });
    doCheck(r3, "ex0", { 1 });

    title("segment-excludeIfNotPresent-3");
    removeConfig(filter, 2, c2); mask.reset(2);

    doCheck(r0, "ex0", { 1 });
    doCheck(r1, "ex0", { 1 });
    doCheck(r2, "ex0", { 1 });
    doCheck(r3, "ex0", { 1 });
}

/** The logic being tested here is a little wonky.

    Short version, the result of a single segment filter should be ignored
    (whether it succeeded or not) if the exchange filter failed for that
    segment.

    This applies on both regular inclue/exclude segment filtering as well as
    excludeIfNotPresent filtering (just to keep you on your toes).
 */
BOOST_AUTO_TEST_CASE( segmentFilter_exchange )
{
    SegmentsFilter filter;
    ConfigSet mask;

    auto doCheck = [&] (
            BidRequest& request,
            const std::string& exchangeName,
            const initializer_list<size_t>& expected)
    {
        check(filter, request, exchangeName, mask, expected);
    };

    AgentConfig c0;
    add(c0, "seg1", false, segment(1), segment(), ie<string>({ "ex1" }, {}));

    AgentConfig c1;
    add(c1, "seg1", false, segment(1), segment(), ie<string>({ "ex2" }, {}));

    AgentConfig c2;
    add(c2, "seg1", false, segment(1), segment(), ie<string>({ "ex1" }, {}));
    add(c2, "seg2", false, segment(2), segment(), ie<string>({ "ex2" }, {}));

    // Control case with no exchange filters.
    AgentConfig c3;
    add(c3, "seg1", false, segment(1), segment(), ie<string>());

    // excludeIfNotPresent = true && segment does exist
    AgentConfig c4;
    add(c4, "seg1", true, segment(), segment(), ie<string>({ "ex1" }, {}));
    add(c4, "seg2", true, segment(), segment(), ie<string>({ "ex3" }, {}));

    // excludeIfNotPresent = true && segment doesn't exist.
    AgentConfig c5;
    add(c5, "seg3", true, segment(), segment(), ie<string>({ "ex1" }, {}));
    add(c5, "seg4", true, segment(), segment(), ie<string>({ "ex3" }, {}));

    BidRequest r0;
    add(r0, "seg1", segment(1));
    add(r0, "seg2", segment(1));

    BidRequest r1;
    add(r1, "seg1", segment(2));
    add(r1, "seg2", segment(2));

    title("segment-exchange-1");
    addConfig(filter, 0, c0); mask.set(0);
    addConfig(filter, 1, c1); mask.set(1);
    addConfig(filter, 2, c2); mask.set(2);
    addConfig(filter, 3, c3); mask.set(3);
    addConfig(filter, 4, c4); mask.set(4);
    addConfig(filter, 5, c5); mask.set(5);

    doCheck(r0, "ex1", { 0, 1, 2, 3, 4 });
    doCheck(r1, "ex1", { 1, 4 });

    doCheck(r0, "ex2", { 0, 1, 3, 4, 5 });
    doCheck(r1, "ex2", { 0, 2, 4, 5 });

    doCheck(r0, "ex3", { 0, 1, 2, 3, 4 });
    doCheck(r1, "ex3", { 0, 1, 2, 4 });

    title("segment-exchange-2");
    removeConfig(filter, 2, c2); mask.reset(2);

    doCheck(r0, "ex1", { 0, 1, 3, 4 });
    doCheck(r1, "ex1", { 1, 4 });

    doCheck(r0, "ex2", { 0, 1, 3, 4, 5 });
    doCheck(r1, "ex2", { 0, 4, 5 });

    doCheck(r0, "ex3", { 0, 1, 3, 4 });
    doCheck(r1, "ex3", { 0, 1, 4 });

    title("segment-exchange-3");
    removeConfig(filter, 5, c5); mask.reset(5);

    doCheck(r0, "ex1", { 0, 1, 3, 4 });
    doCheck(r1, "ex1", { 1, 4 });

    doCheck(r0, "ex2", { 0, 1, 3, 4 });
    doCheck(r1, "ex2", { 0, 4 });

    doCheck(r0, "ex3", { 0, 1, 3, 4 });
    doCheck(r1, "ex3", { 0, 1, 4 });

    title("segment-exchange-4");
    removeConfig(filter, 0, c0); mask.reset(0);

    doCheck(r0, "ex1", { 1, 3, 4 });
    doCheck(r1, "ex1", { 1, 4 });

    doCheck(r0, "ex2", { 1, 3, 4 });
    doCheck(r1, "ex2", { 4 });

    doCheck(r0, "ex3", { 1, 3, 4 });
    doCheck(r1, "ex3", { 1, 4 });
}
