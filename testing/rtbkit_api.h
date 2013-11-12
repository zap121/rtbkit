/* rtbkit_api.cc
   Eric Robert, 12 November 2013
   Copyright (c) 2013 Datacratic.  All rights reserved.
*/

#ifndef RTBKIT_API
#define RTBKIT_API

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}

/*
 * C++ wrappers
 */

struct rtbkit {
    explicit rtbkit(char const * bootstrap) {
        handle = rtbkit_initialize(bootstrap);
    }

    rtbkit(rtbkit const &) = delete;
    rtbkit(rtbkit && item) {
        handle = item.handle;
        item.handle = 0;
    }

    rtbkit & operator=(rtbkit const &) = delete;

    ~rtbkit() {
        rtbkit_shutdown(handle);
    }

    struct object {
        object(object const &) = delete;
        object(object && item) {
            handle = item.handle;
            item.handle = 0;
        }

        object & operator=(object const &) = delete;

        ~object() {
            rtbkit_release(handle);
        }

        int fd() const {
            return rtbkit_fd(handle);
        }

    protected:
        object() : handle(0) {
        }

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

#endif
#endif

