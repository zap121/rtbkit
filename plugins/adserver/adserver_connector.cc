/* adserver_connector.cc
   Jeremy Barnes, 19 December 2012
   Copyright (c) 2012 Datacratic Inc.  All rights reserved.

   Base class to connect to an ad server.  We also have an http ad server
   connector that builds on top of this.
*/


#include "adserver_connector.h"

#include "rtbkit/common/auction_events.h"


using namespace std;

namespace RTBKIT {


/*****************************************************************************/
/* POST AUCTION PROXY                                                        */
/*****************************************************************************/

AdServerConnector::
AdServerConnector(std::shared_ptr<Datacratic::ServiceProxies> & proxy,
                  const std::string & serviceName)
    : ServiceBase(serviceName, proxy),
      toPostAuctionService(proxy->zmqContext)
{
}

AdServerConnector::
~AdServerConnector()
{
}

void
AdServerConnector::
init(shared_ptr<ConfigurationService> config)
{
    toPostAuctionService.init(config, ZMQ_XREQ);
    toPostAuctionService.connectToServiceClass("rtbPostAuctionService",
                                               "events");

#if 0 // later, for when we have multiple

    toPostAuctionServices.init(getServices()->config, name);
    toPostAuctionServices.connectHandler = [=] (const string & connectedTo) {
        cerr << "AdServerConnector is connected to post auction service "
        << connectedTo << endl;
    };
    toPostAuctionServices.connectAllServiceProviders("rtbPostAuctionService",
                                                     "agents");
    toPostAuctionServices.connectToServiceClass();
#endif
}

void
AdServerConnector::
start()
{
}

void
AdServerConnector::
shutdown()
{
}

void
AdServerConnector::
publishWin(const Id & auctionId,
           const Id & adSpotId,
           Amount winPrice,
           Date timestamp,
           const JsonHolder & winMeta,
           const UserIds & ids,
           const AccountKey & account,
           Date bidTimestamp)
{
    PostAuctionEvent event;
    event.type = PAE_WIN;
    event.auctionId = auctionId;
    event.adSpotId = adSpotId;
    event.winPrice = winPrice;
    event.timestamp = timestamp;
    event.metadata = winMeta;
    event.uids = ids;
    event.account = account;
    event.bidTimestamp = bidTimestamp;

    string str = ML::DB::serializeToString(event);
    toPostAuctionService.sendMessage("WIN", str);
}

void
AdServerConnector::
publishLoss(const Id & auctionId,
            const Id & adSpotId,
            Date timestamp,
            const JsonHolder & lossMeta,
            const AccountKey & account,
            Date bidTimestamp)
{
    PostAuctionEvent event;
    event.type = PAE_LOSS;
    event.auctionId = auctionId;
    event.adSpotId = adSpotId;
    event.timestamp = timestamp;
    event.metadata = lossMeta;
    event.account = account;
    event.bidTimestamp = bidTimestamp;

    string str = ML::DB::serializeToString(event);
    toPostAuctionService.sendMessage("LOSS", str);
}

void
AdServerConnector::
publishCampaignEvent(const string & label,
                     const Id & auctionId,
                     const Id & adSpotId,
                     Date timestamp,
                     const JsonHolder & impressionMeta,
                     const UserIds & ids)
{
    PostAuctionEvent event;
    event.type = PAE_CAMPAIGN_EVENT;
    event.label = label;
    event.auctionId = auctionId;
    event.adSpotId = adSpotId;
    event.timestamp = timestamp;
    event.uids = ids;
    event.metadata = impressionMeta;

    string str = ML::DB::serializeToString(event);
    toPostAuctionService.sendMessage("EVENT", str);
}

} // namespace RTBKIT
