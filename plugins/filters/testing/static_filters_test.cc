/** static_filters_test.cc                                 -*- C++ -*-
    Rémi Attab, 22 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Tests for the static filters.

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
        const BidRequest& request,
        const std::string exchangeName,
        const ConfigSet& mask,
        const initializer_list<size_t>& exp)
{
    FilterExchangeConnector conn(exchangeName);

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


BOOST_AUTO_TEST_CASE( segmentFilter_excludeIfNotPresent )
{

    SegmentsFilter filter;
    ConfigSet mask;

    auto doCheck = [&] (
            const BidRequest& request,
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
