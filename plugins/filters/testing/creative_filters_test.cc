/** creative_filters_test.cc                                 -*- C++ -*-
    Rémi Attab, 28 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Tests for the creative filters

*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include "utils.h"
#include "rtbkit/plugins/filters/creative_filters.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/common/bid_request.h"
#include "rtbkit/common/exchange_connector.h"
#include "jml/utils/vector_utils.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace ML;
using namespace Datacratic;


void addImp(
        BidRequest& request,
        OpenRTB::AdPosition::Vals pos,
        const initializer_list<Format>& formats)
{
    AdSpot imp;
    for (const auto& format : formats) imp.formats.push_back(format);
    imp.position.val = pos;
    request.imp.push_back(imp);
};

void addConfig(
        FilterBase& filter,
        unsigned cfgIndex, AgentConfig& cfg,
        vector<unsigned>& counts)
{
    addConfig(filter, cfgIndex, cfg);

    if (counts.size() <= cfgIndex)
        counts.resize(cfgIndex + 1, 0);
    counts[cfgIndex] = cfg.creatives.size();
}

void removeConfig(
        FilterBase& filter,
        unsigned cfgIndex, AgentConfig& cfg,
        vector<unsigned>& counts)
{
    removeConfig(filter, cfgIndex, cfg);
    counts[cfgIndex] = 0;
}


BOOST_AUTO_TEST_CASE( testFormatFilter )
{
    CreativeFormatFilter filter;

    vector<unsigned> creatives;

    AgentConfig c0;
    c0.creatives.push_back(Creative());
    c0.creatives.push_back(Creative(100, 100));
    c0.creatives.push_back(Creative(300, 300));

    AgentConfig c1;
    c1.creatives.push_back(Creative(100, 100));
    c1.creatives.push_back(Creative(200, 200));

    BidRequest r0;
    addImp(r0, OpenRTB::AdPosition::ABOVE, { {100, 100} });
    addImp(r0, OpenRTB::AdPosition::ABOVE, { {200, 200}, {100, 100} });
    addImp(r0, OpenRTB::AdPosition::ABOVE, { {200, 200}, {300, 300} });
    addImp(r0, OpenRTB::AdPosition::ABOVE, { {300, 300} });
    addImp(r0, OpenRTB::AdPosition::ABOVE, { {400, 400} });


    FilterExchangeConnector conn("bob");

    {
        title("formatFilter-1");
        addConfig(filter, 0, c0, creatives);
        addConfig(filter, 1, c1, creatives);

        FilterState state(r0, &conn, creatives);
        filter.filter(state);

        check(state.creatives(0), { {0, 1}, {0}        });
        check(state.creatives(1), { {0, 1}, {0, 1}     });
        check(state.creatives(2), { {0},    {1},   {0} });
        check(state.creatives(3), { {0},    {},    {0} });
        check(state.creatives(4), { {0}                });
    }

    {
        title("formatFilter-2");
        removeConfig(filter, 0, c0, creatives);

        FilterState state(r0, &conn, creatives);
        filter.filter(state);

        check(state.creatives(0), { {1},     });
        check(state.creatives(1), { {1}, {1} });
        check(state.creatives(2), { {},  {1} });
        check(state.creatives(3), {          });
        check(state.creatives(4), {          });
    }
}
