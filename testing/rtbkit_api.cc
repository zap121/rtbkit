/* rtbkit_api.cc
   Eric Robert, 12 November 2013
   Copyright (c) 2013 Datacratic.  All rights reserved.
*/

#include "rtbkit/testing/rtbkit_api.h"
#include "rtbkit/plugins/bidding_agent/bidding_agent.h"

using namespace Datacratic;
using namespace RTBKIT;

struct RTBkitHandle {
    std::shared_ptr<ServiceProxies> proxies;

    RTBkitHandle(std::string bootstrap) {
        proxies.reset(new ServiceProxies());
        proxies->bootstrap(bootstrap);
    }

    rtbkit_handle * handle() {
        return reinterpret_cast<rtbkit_handle *>(this);
    }

    static RTBkitHandle * object(rtbkit_handle * handle) {
        return reinterpret_cast<RTBkitHandle *>(handle);
    }
};

struct RTBkitObject {
    virtual ~RTBkitObject() {
    }

    virtual rtbkit_event * next() {
        return 0;
    }

    virtual void send(rtbkit_event * event) {
    }

    virtual void free(rtbkit_event * event) {
    }

    virtual int fd() {
        return -1;
    }

    void process(MessageLoop * ml) {
        bool more = true;
        while(more) {
            more = ml->processOne();
        }
    }

    rtbkit_object * handle() {
        return reinterpret_cast<rtbkit_object *>(this);
    }

    static RTBkitObject * object(rtbkit_object * handle) {
        return reinterpret_cast<RTBkitObject *>(handle);
    }
};

struct RTBkitBiddingAgent : public RTBkitObject {
    std::unique_ptr<BiddingAgent> agent;

    bool hasRequest;
    rtbkit_event_bid_request request;

    RTBkitBiddingAgent(RTBkitHandle * handle, std::string const & name) {
        agent.reset(new BiddingAgent(handle->proxies, name));
        agent->init();

        //agent->onBidRequest = std::bind(&RTBkitBiddingAgent::onBidRequest, this, _1, _2, _3, _4, _5, _6);

        request.base.type = RTBKIT_EVENT_BID_REQUEST;
        request.base.subject = this->handle();
    }

    void onBidRequest(double timestamp,
                      Id id,
                      std::shared_ptr<BidRequest> bidRequest,
                      const Bids & bids,
                      double timeLeftMs,
                      Json::Value augmentations,
                      WinCostModel const & wcm) {
        hasRequest = true;
    }

    rtbkit_event * next() {
        hasRequest = false;
        process(agent.get());

        if(hasRequest) {
            return &request.base;
        }

        return 0;
    }

    void send(rtbkit_event * event) {
        if(event->type == RTBKIT_EVENT_BID_RESPONSE) {
            rtbkit_event_bid_response * e = (rtbkit_event_bid_response *) event;
            Bids bids;
            agent->doBid(Id(e->bid.id), bids);
        }
    }

    int fd() {
        return agent->selectFd();
    }
};

rtbkit_handle * rtbkit_initialize(char const * bootstrap) {
    auto * item = new RTBkitHandle(bootstrap);
    return item->handle();
}

void rtbkit_shutdown(rtbkit_handle * handle) {
    auto * item = RTBkitHandle::object(handle);
    delete item;
}

rtbkit_object * rtbkit_create_bidding_agent(rtbkit_handle * handle, char const * name) {
    auto * item = new RTBkitBiddingAgent(RTBkitHandle::object(handle), name);
    return item->handle();
}

void rtbkit_release(rtbkit_object * handle) {
    auto * item = RTBkitObject::object(handle);
    delete item;
}

void rtbkit_next_event(rtbkit_object * handle, rtbkit_event ** event) {
    for(;;) {
        auto * item = RTBkitObject::object(handle)->next();
        if(item) {
            event[0] = item;
            break;
        }
    }
}

void rtbkit_free_event(rtbkit_event * event) {
    RTBkitObject::object(event->subject)->free(event);
}

void rtbkit_send_event(rtbkit_event * event) {
    RTBkitObject::object(event->subject)->send(event);
}

int rtbkit_fd(rtbkit_object * handle) {
    return RTBkitObject::object(handle)->fd();
}

