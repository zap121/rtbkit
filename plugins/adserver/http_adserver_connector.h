/* http_adserver_connector.h                                       -*- C++ -*-
   Wolfgang Sourdeau, March 2013
   Copyright (c) 2013 Datacratic.  All rights reserved.
*/


#pragma once

#include <string>
#include <vector>

#include "soa/service/http_endpoint.h"
#include "soa/service/json_endpoint.h"

#include "adserver_connector.h"


namespace RTBKIT {

struct HttpAdServerConnectionHandler
    : public Datacratic::JsonConnectionHandler {
    HttpAdServerConnectionHandler(const AdServerRequestCb & requestCb);

    virtual void handleJson(const HttpHeader & header,
                            const Json::Value & json,
                            const std::string & jsonStr);

private:
    const AdServerRequestCb & requestCb_;
};

struct HttpAdServerHttpEndpoint : public Datacratic::HttpEndpoint {
    HttpAdServerHttpEndpoint(int port, const AdServerRequestCb & requestCb);
    HttpAdServerHttpEndpoint(HttpAdServerHttpEndpoint && otherEndpoint);

    ~HttpAdServerHttpEndpoint();

private:
    virtual std::shared_ptr<ConnectionHandler> makeNewHandler();

    int port_;
    AdServerRequestCb requestCb_;
};
        
/****************************************************************************/
/* HTTP ADSERVER CONNECTOR                                                  */
/****************************************************************************/

struct HttpAdServerConnector : public AdServerConnector {
    HttpAdServerConnector(std::shared_ptr<Datacratic::ServiceProxies> & proxy,
                          const std::string & serviceName
                          = "AdServerConnector");

    void registerEndpoint(int port, const AdServerRequestCb & requestCb);

private:
    std::vector<HttpAdServerHttpEndpoint> endpoints_;
};

} //namespace RTBKIT
