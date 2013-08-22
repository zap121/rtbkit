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

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace ML;
using namespace Datacratic;

static string ExchangeName = "rtbkit";

void check(
        const FilterBase& filter,
        const BidRequest& request,
        const std::string exchangeName,
        unsigned numConfigs,
        const initializer_list<size_t>& exp)
{
    FilterExchangeConnector conn(exchangeName);
    vector<unsigned> creativesCount(numConfigs, 1);
    FilterState state(request, &conn, creativesCount);

    filter.filter(state);
    check(state.configs(), exp);
}


BOOST_AUTO_TEST_CASE( segmentsFilter )
{
    auto addc = [] (
            AgentConfig& config,
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
        return config;
    };

    auto addb = [] (
            BidRequest& br,
            const string& name,
            const SegmentList& segments)
    {
        br.segments[name] = make_shared<SegmentList>(segments);
    };

    SegmentsFilter filter;
    auto doCheck = [&] (
            const BidRequest& request,
            const std::string& exchangeName,
            const initializer_list<size_t>& expected)
    {
        check(filter, request, exchangeName, 1, expected);
    };

    AgentConfig c0;
    addc(c0, "seg1", false, segment(), segment(1, "a"), ie<string>());

    BidRequest r0;
    addb(r0, "seg1", segment("b"));

    title("segment-1");
    addConfig(filter, 0, c0);
    doCheck(r0, "ex0", { 0 });

    title("segment-2");
    removeConfig(filter, 0, c0);
    doCheck(r0, "ex0", { 0 });
}
