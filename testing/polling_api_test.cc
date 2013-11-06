/* polling_api_test.cc
   Eric Robert, 6 November 2013
   Copyright (c) 2013 Datacratic.  All rights reserved.

   Test for a polling api.
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <poll.h>

struct rtbkit;
struct rtbkit_object;

/*
 * RTBkit factory
 */

rtbkit * rtbkit_initialize(char const * bootstrap);
void rtbkit_shutdown(rtbkit * handle);

enum rtbkit_object_types {
    RTBKIT_UNKNOWN,
    RTBKIT_AUGMENTER,
    RTBKIT_BIDDING_AGENT
};

rtbkit_object * rtbkit_create(rtbkit * handle, rtbkit_object_types type, char const * name);
void rtbkit_release(rtbkit_object * handle);

/*
 * RTBkit events polling
 */

enum rtbkit_events {
    RTBKIT_EVENT_CONFIG,
    RTBKIT_EVENT_BID_REQUEST,
    RTBKIT_EVENT_BID_RESULT,
    RTBKIT_EVENT_WIN,
    RTBKIT_EVENT_ERROR
};

struct rtbkit_event {
    rtbkit_object * handle;
    rtbkit_events type;
};

void rtbkit_next_event(rtbkit_object * handle, rtbkit_event ** event);
void rtbkit_send_event(rtbkit_event * event);

/*
 * RTBkit non blocking
 */

int rtbkit_fd(rtbkit_object * handle);

/*
 * RTBkit structures
 */

struct rtbkit_bid_request {
    int id;
};

struct rtbkit_bid {
    int price;
};

/*
 * RTBkit bidding agent
 */

struct rtbkit_bidding_agent_config {
    rtbkit_event header;
    char const * config;
};

struct rtbkit_bidding_agent_bid_request {
    rtbkit_event header;
    rtbkit_bid_request request;
    rtbkit_bid bid;
};

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
        rtbkit_bidding_agent_bid_request * br = (rtbkit_bidding_agent_bid_request *) e;
        br->bid.price = 1234;

        // send back
        rtbkit_send_event(&br->header);
        }

        break;
    case RTBKIT_EVENT_BID_RESULT:
    case RTBKIT_EVENT_WIN:
        break;
    };
}

BOOST_AUTO_TEST_CASE( polling_api_test )
{
    // initialize
    rtbkit * rk = rtbkit_initialize("bootstrap.json");

    // create bidding agent
    rtbkit_object * ba = rtbkit_create(rk, RTBKIT_BIDDING_AGENT, "test-agent");

    // send the config
    rtbkit_bidding_agent_config cfg;
    cfg.header.handle = ba;
    cfg.header.type = RTBKIT_EVENT_CONFIG;
    cfg.config = "...";
    rtbkit_send_event(&cfg.header);

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
    rtbkit_shutdown(rk);
}

