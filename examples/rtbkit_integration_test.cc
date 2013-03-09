/* router_integration_test.cc
   Jeremy Barnes, 21 November 2012
   Copyright (c) 2012 Datacratic.  All rights reserved.

   Overall integration test for the router stack.
*/

#include "augmentor_ex.h"

#include "rtbkit/core/router/router.h"
#include "rtbkit/core/post_auction/post_auction_loop.h"
#include "rtbkit/core/agent_configuration/agent_configuration_service.h"
#include "rtbkit/core/banker/master_banker.h"
#include "rtbkit/core/banker/slave_banker.h"
#include "jml/utils/rng.h"
#include "rtbkit/core/monitor/monitor_endpoint.h"
#include "jml/utils/pair_utils.h"
#include "jml/utils/environment.h"
#include "jml/arch/timers.h"
#include "soa/service/testing/redis_temporary_server.h"
#include "testing/generic_exchange_connector.h"
#include "testing/mock_exchange.h"
#include "rtbkit/testing/test_agent.h"
#include <boost/thread.hpp>
#include <netdb.h>
#include <memory>


using namespace std;
using namespace ML;
using namespace Redis;
using namespace Datacratic;
using namespace RTBKIT;


/******************************************************************************/
/* COMPONENTS                                                                 */
/******************************************************************************/

/** Initializes the various components of the RTBKit stack. */
struct Components
{

    std::shared_ptr<ServiceProxies> proxies;

    // See init for an inline description of the various components.

    RedisTemporaryServer redis;
    Router router1, router2;
    PostAuctionLoop postAuctionLoop;
    MasterBanker masterBanker;
    SlaveBudgetController budgetController;
    AgentConfigurationService agentConfig;
    MonitorEndpoint monitor;
    TestAgent agent;
    FrequencyCapAugmentor augmentor;

    // \todo Add a PAL event subscriber.

    vector<unique_ptr<GenericExchangeConnector> > exchangeConnectors;
    vector<int> exchangePorts;


    Components(std::shared_ptr<ServiceProxies> proxies)
        : proxies(proxies),
          router1(proxies, "router1"),
          router2(proxies, "router2"),
          postAuctionLoop(proxies, "pas1"),
          masterBanker(proxies, "masterBanker"),
          agentConfig(proxies, "agentConfigurationService"),
          monitor(proxies, "monitor"),
          agent(proxies, "agent1"),
          augmentor(proxies, "frequency-cap-ex")
    {
    }

    void shutdown()
    {
        router1.shutdown();
        router2.shutdown();
        postAuctionLoop.shutdown();

        budgetController.shutdown();
        masterBanker.shutdown();

        agent.shutdown();
        augmentor.shutdown();
        agentConfig.shutdown();

        monitor.shutdown();

        cerr << "done shutdown" << endl;
    }

    void init()
    {
        const string agentUri = "tcp://127.0.0.1:1234";

        // Setup a monitor which ensures that any instability in the system will
        // throttle the bid request stream. In other words, it ensures you won't
        // go bankrupt.
        monitor.init({"router1", "router2", "pas1", "masterBanker"});
        monitor.bindTcp();
        monitor.start();

        // Setup and agent configuration service which is used to notify all
        // interested services of changes to the agent configuration.
        agentConfig.init();
        agentConfig.bindTcp();
        agentConfig.start();

        // Setup a master banker used to keep the canonical budget of the
        // various bidding agent accounts. The data contained in this service is
        // periodically persisted to redis.
        masterBanker.init(std::make_shared<RedisBankerPersistence>(redis));
        masterBanker.bindTcp();
        masterBanker.start();

        // Setup a slave banker that we can use to manipulate and peak at the
        // budgets during the test.
        budgetController.init(proxies->config);
        budgetController.start();

        // Each router contains a slave masterBanker which is periodically
        // synced with the master banker.
        auto makeSlaveBanker = [=] (const std::string & name)
            {
                auto res = std::make_shared<SlaveBanker>
                (proxies->zmqContext, proxies->config, name);
                res->start();
                return res;
            };

        // Setup a post auction loop (PAL) which handles all exchange events
        // that don't need to be processed in real-time (wins, loss, etc).
        postAuctionLoop.init();
        postAuctionLoop.setBanker(makeSlaveBanker("pas1"));
        postAuctionLoop.bindTcp();
        postAuctionLoop.start();

        // Setup two routers which will manage the bid request stream coming
        // from the exchange, the augmentations coming from the augmentors (to
        // be added to the test) and the bids coming from the agents. Along the
        // way it also applies various filters based on agent configuration
        // while ensuring that all the real-time constraints are respected.

        router1.init();
        router1.setBanker(makeSlaveBanker("router1"));
        router1.bindTcp();
        router1.start();

        router2.init();
        router2.setBanker(makeSlaveBanker("router2"));
        router2.bindTcp();
        router2.start();


        // Setup an exchange connector for each router which will act as the
        // middle men between the exchange and the router.

        exchangeConnectors.emplace_back(
                new GenericExchangeConnector(&router1, Json::Value()));

        exchangeConnectors.emplace_back(
                new GenericExchangeConnector(&router2, Json::Value()));

        for (auto& connector : exchangeConnectors) {
            connector->enableUntil(Date::positiveInfinity());

            int port = connector->init(-1, "localhost", 2 /* threads */);
            exchangePorts.push_back(port);
        }

        // Our bidding agent which listens to the bid request stream from all
        // available routers and decide who gets to see your awesome pictures of
        // kittens.
        agent.init();
        agent.start();
        agent.configure();

        // Our augmentor which does frequency capping for our agent.
        augmentor.init();
        augmentor.start();
    }
};


/******************************************************************************/
/* SETUP                                                                      */
/******************************************************************************/

void setupAgent(TestAgent& agent)
{
    // Set our frequency cap to 42. This has two effects: 1) it instructs the
    // router that we want bid requests destined for our agent to first be
    // augmented with frequency capping information and 2) it instructs our
    // augmentor to place the pass-frequency-cap-ex tag on our bid request if
    // our agent has seen a given user less then 42 times.
    agent.config.addAugmentation("frequency-cap-ex", Json::Value(42));

    // Instructs the router to only keep bid requests that have this tag. In
    // other words keep only the bid requests that haven't reached our frequency
    // cap limit.
    agent.config.augmentationFilter.include.push_back("pass-frequency-cap-ex");

    // This lambda implements our incredibly sophisticated bidding strategy.
    agent.onBidRequest = [&] (
            double timestamp,
            const Id & id,
            std::shared_ptr<BidRequest> br,
            Bids bids,
            double timeLeftMs,
            const Json::Value & augmentations)
        {
            ExcAssertGreater(bids.size(), 0);

            Bid& bid = bids[0];
            ExcAssertGreater(bid.availableCreatives.size(), 0);

            bid.bid(bid.availableCreatives[0], MicroUSD_CPM(10000));

            agent.doBid(id, bids, Json::Value());
            ML::atomic_inc(agent.numBidRequests);
        };
}


/** Transfer a given budget to each router for a given account. */
void allocateBudget(
        SlaveBudgetController& budgetController,
        const AccountKey& account,
        Amount budget)
{
    budgetController.addAccountSync(account);
    budgetController.setBudgetSync(account[0], budget);
    budgetController.topupTransferSync(account, USD(10));

    cerr << budgetController.getAccountSummarySync(account[0], -1) << endl;

    // Syncing is done periodically so we have to wait a bit before the router
    // will have a budget available. Necessary because the bid request stream
    // for this test isn't infinit.
    cerr << "sleeping so that the slave accounts can sync up" << endl;
    ML::sleep(2.1);

    auto summary = budgetController.getAccountSummarySync(account[0], -1);
    cerr << summary << endl;

    ExcAssertEqual(
            summary.subAccounts["testStrategy"].subAccounts["router1"].budget,
            USD(0.10));

    ExcAssertEqual(
            summary.subAccounts["testStrategy"].subAccounts["router2"].budget,
            USD(0.10));
}

/** Some debugging output for the banker. */
void dumpAccounts(
        SlaveBudgetController& budgetController,
        const AccountKey & name, const AccountSummary & a)
{
    cerr << name << ": " << endl;
    cerr << budgetController.getAccountSync(name) << endl;

    for (auto & sub: a.subAccounts) {
        dumpAccounts(budgetController, name.childKey(sub.first), sub.second);
    }
};


/******************************************************************************/
/* MAIN                                                                       */
/******************************************************************************/

/** Small example problem that spins up the various components of the RTBKit
    stack, sends a bunch of bid requests through it then print some stats.

 */
int main(int argc, char ** argv)
{
    // Controls the length of the test.
    enum {
        nExchangeThreads = 10,
        nBidRequestsPerThread = 200
    };

    auto proxies = std::make_shared<ServiceProxies>();

    // If we had a zookeeper instance running, we could use it to do service
    // discovery. Since we don't, ServiceProxies will just default to using a
    // local service map.
    if (false) proxies->useZookeeper("zookeeper.datacratic.com", "stats");

    // If we had a carbon instance running, we could use it to log events. Since
    // we don't, ServiceProxies will just default to using a local equivalent.
    if (false) proxies->logToCarbon("carbon.datacratic.com", "stats");


    // Setups up the various component of the RTBKit stack. See Components::init
    // for more details.
    Components components(proxies);
    components.init();

    // Some extra customization for our agent to make it extra special. See
    // setupAgent for more details.
    setupAgent(components.agent);

    // Setup an initial budgeting for the test.
    allocateBudget(
            components.budgetController,
            {"testCampaign", "testStrategy"},
            USD(1000));


    boost::thread_group threads;


    int numDone = 0;

    // Uses MockExchange to simulates a very basic exchange for the test.
    auto doExchangeThread = [&] (int threadId)
        {
            string exchangeName = string("exchange-") + to_string(threadId);

            MockExchange exchange(proxies, exchangeName);
            exchange.init(threadId, components.exchangePorts);
            exchange.start(nBidRequestsPerThread);

            ML::atomic_inc(numDone);
        };


    // Start up the exchange threads which should let bid requests flow through
    // our stack.
    for (unsigned i = 0;  i < nExchangeThreads;  ++i)
        threads.create_thread(std::bind(doExchangeThread, i));


    // Dump the budget stats while we wait for the test to finish.
    while (numDone < nExchangeThreads) {
        auto summary = components.budgetController.getAccountSummarySync(
                {"testCampaign"}, -1);
        cerr <<  summary << endl;

        dumpAccounts(components.budgetController, {"testCampaign"}, summary);
        ML::sleep(1.0);
    }

    // Test is done; clean up time.

    threads.join_all();
    components.shutdown();

    components.proxies->events->dump(cerr);
}
