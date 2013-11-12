/* polling_api_test.cc
   Eric Robert, 6 November 2013
   Copyright (c) 2013 Datacratic.  All rights reserved.

   Test for a polling api.
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <poll.h>

#include "rtbkit/testing/rtbkit_api.h"

/*
 * RTBkit sample
 */

void process_bidding_agent_events(rtbkit_object * ba) {
    // wait for the next event
    rtbkit_event * e;
    rtbkit_next_event(ba, &e);

    int type = e->type;
    switch(type) {
    case RTBKIT_EVENT_BID_REQUEST:
        {
        // cast for the actual event structure
        rtbkit_event_bid_request * b = (rtbkit_event_bid_request *) e;

        // create a response
        rtbkit_event_bid_response br;
        br.base.type = RTBKIT_EVENT_BID_RESPONSE;
        br.base.subject = ba;
        br.bid.id = b->request.id;
        br.bid.price = 1234;

        // send back
        rtbkit_send_event(&br.base);
        }

        break;
    case RTBKIT_EVENT_BID_RESULT:
    case RTBKIT_EVENT_WIN:
        break;
    };

    rtbkit_free_event(e);
}

BOOST_AUTO_TEST_CASE( polling_api_test )
{
    // initialize
    rtbkit_handle * rh = rtbkit_initialize("rtbkit/sample.bootstrap.json");

    // create bidding agent
    rtbkit_object * ba = rtbkit_create_bidding_agent(rh, "test-agent");

    // send the config
    rtbkit_bidding_agent_config cfg;
    cfg.base.type = RTBKIT_EVENT_CONFIG;
    cfg.base.subject = ba;
    cfg.config = "...";
    rtbkit_send_event(&cfg.base);

    // easy blocking call
    #ifdef BLOCKING
    for(;;) {
        process_bidding_agent_events(ba);
    }
    #endif

    // easy non blocking call
    #ifdef NON_BLOCKING
    for(;;) {
        int fd = rtbkit_fd(ba);

        pollfd pfd {
            fd, POLLIN, 0
        };

        int n = poll(&pfd, 1, 0);
        if(n == 1) {
            process_bidding_agent_events(ba);
        }

        // do other stuff
    }
    #endif

    // cleanup
    rtbkit_release(ba);

    // shutdown
    rtbkit_shutdown(rh);
}

struct my_bidding_agent : public rtbkit::bidding_agent {
    explicit my_bidding_agent(rtbkit & kit) :
        rtbkit::bidding_agent(kit, "test-agent") {
    }

    void on_bid_request(rtbkit_event_bid_request const & event) {
        rtbkit_bid bid;
        bid.id = event.request.id;
        bid.price = 1234;
        send_bid(bid);
    }
};

BOOST_AUTO_TEST_CASE( polling_api_cpp_test )
{
    rtbkit kit("rtbkit/sample.bootstrap.json");

    my_bidding_agent ba(kit);
    ba.set_config("...");

    // easy blocking call
    #ifdef BLOCKING
    for(;;) {
        ba.process();
    }
    #endif

    // easy non blocking call
    #ifdef NON_BLOCKING
    for(;;) {
        pollfd pfd {
            ba.fd(), POLLIN, 0
        };

        int n = poll(&pfd, 1, 0);
        if(n == 1) {
            ba.process(ba);
        }

        // do other stuff
    }
    #endif
}

