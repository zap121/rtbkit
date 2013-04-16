#include <memory>

#include "http_adserver_connector.h"


using namespace std;
using namespace RTBKIT;


/*
  HTTPADSERVERCONNECTIONHANDLER
*/

HttpAdServerConnectionHandler::
HttpAdServerConnectionHandler(const AdServerRequestCb & requestCb)
    : requestCb_(requestCb)
{
}

void
HttpAdServerConnectionHandler::
handleJson(const HttpHeader & header,
           const Json::Value & json, const std::string & jsonStr)
{
    requestCb_(json, jsonStr);
}


/*
  HTTPADSERVERHTTPENDPOINT
 */

HttpAdServerHttpEndpoint::
HttpAdServerHttpEndpoint(int port, const AdServerRequestCb & requestCb)
    : HttpEndpoint("adserver-ep-" + to_string(port)),
      port_(port), requestCb_(requestCb)
{
}

HttpAdServerHttpEndpoint::
HttpAdServerHttpEndpoint(HttpAdServerHttpEndpoint && otherEndpoint)
: HttpEndpoint("adserver-ep-" + std::to_string(otherEndpoint.port_))
{
    port_ = otherEndpoint.port_;
    requestCb_ = std::move(otherEndpoint.requestCb_);
}

HttpAdServerHttpEndpoint::
~HttpAdServerHttpEndpoint()
{
    shutdown();
}

shared_ptr<ConnectionHandler>
HttpAdServerHttpEndpoint::
makeNewHandler()
{
    return std::make_shared<HttpAdServerConnectionHandler>(requestCb_);
}


/*
  HTTPADSERVERCONNECTOR
 */

HttpAdServerConnector::
HttpAdServerConnector(shared_ptr<Datacratic::ServiceProxies> & proxy,
                      const string & serviceName)
    : AdServerConnector(proxy, serviceName)
{
}

void
HttpAdServerConnector::
registerEndpoint(int port, const AdServerRequestCb & requestCb)
{
    endpoints_.emplace_back(port, requestCb);
}
