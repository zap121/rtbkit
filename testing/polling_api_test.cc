/* polling_api_test.cc
   Eric Robert, 6 November 2013
   Copyright (c) 2013 Datacratic.  All rights reserved.

   Test for a polling api.
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <poll.h>

struct rtbkit_handle;
struct rtbkit_object;

/*
 * RTBkit factory
 */

rtbkit_handle * rtbkit_initialize(char const * bootstrap);
void rtbkit_shutdown(rtbkit_handle * handle);

rtbkit_object * rtbkit_create_bidding_agent(rtbkit_handle * handle, char const * name);
void rtbkit_release(rtbkit_object * handle);

/*
 * RTBkit events polling
 */

enum rtbkit_events {
    RTBKIT_EVENT_CONFIG,
    RTBKIT_EVENT_BID_REQUEST,
    RTBKIT_EVENT_BID_RESPONSE,
    RTBKIT_EVENT_BID_RESULT,
    RTBKIT_EVENT_WIN,
    RTBKIT_EVENT_ERROR
};

struct rtbkit_event {
    rtbkit_object * subject;
    rtbkit_events type;
};

void rtbkit_next_event(rtbkit_object * handle, rtbkit_event ** event);
void rtbkit_free_event(rtbkit_event * event);
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
    // more stuff
};

struct rtbkit_bid {
    int id;
    int price;
    // more stuff
};

/*
 * RTBkit bidding agent
 */

struct rtbkit_bidding_agent_config {
    rtbkit_event base;
    char const * config;
};

struct rtbkit_event_bid_request {
    rtbkit_event base;
    rtbkit_bid_request request;
};

struct rtbkit_event_bid_response {
    rtbkit_event base;
    rtbkit_bid bid;
};

/*
 * C++ wrappers
 */

struct rtbkit {
    explicit rtbkit(char const * bootstrap) {
        handle = rtbkit_initialize(bootstrap);
    }

    rtbkit(rtbkit const &) = delete;
    rtbkit(rtbkit && item) {
        item.handle = handle;
        handle = 0;
    }

    rtbkit & operator=(rtbkit const &) = delete;

    ~rtbkit() {
        rtbkit_shutdown(handle);
    }

    struct object {
        object(object const &) = delete;
        object(object && item) {
            item.handle = handle;
            handle = 0;
        }

        object & operator=(object const &) = delete;

        ~object() {
            rtbkit_release(handle);
        }

        int fd() const {
            return rtbkit_fd(handle);
        }

    protected:
        object();

    protected:
        rtbkit_object * handle;
    };

    struct bidding_agent : public object {
        explicit bidding_agent(rtbkit & kit, char const * name = "agent") {
            rtbkit_handle * h = kit.handle;
            handle = rtbkit_create_bidding_agent(h, name);
        }

        void set_config(char const * config) {
            rtbkit_bidding_agent_config cfg;
            cfg.base.type = RTBKIT_EVENT_CONFIG;
            cfg.base.subject = handle;
            cfg.config = config;
            rtbkit_send_event(&cfg.base);
        }

        void send_bid(rtbkit_bid & bid) {
            rtbkit_event_bid_response r;
            r.base.type = RTBKIT_EVENT_BID_RESPONSE;
            r.base.subject = handle;
            r.bid = bid;
            rtbkit_send_event(&r.base);
        }

        virtual void on_bid_request(rtbkit_event_bid_request const & event) {
        }

        void process() {
            rtbkit_event * e;
            rtbkit_next_event(handle, &e);
            int type = e->type;
            switch(type) {
            case RTBKIT_EVENT_BID_REQUEST:
                on_bid_request(*(rtbkit_event_bid_request *) e);
                break;
            case RTBKIT_EVENT_BID_RESULT:
            case RTBKIT_EVENT_WIN:
                break;
            };

            rtbkit_free_event(e);
        }
    };

private:
    rtbkit_handle * handle;
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
    rtbkit_handle * rh = rtbkit_initialize("bootstrap.json");

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
    rtbkit kit("bootstrap.json");

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

