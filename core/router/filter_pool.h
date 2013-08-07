/** filter_pool.h                                 -*- C++ -*-
    Rémi Attab, 23 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    blah

*/

#pragma once

#include "rtbkit/common/filter.h"
#include "soa/gc/gc_lock.h"

#include <atomic>
#include <vector>
#include <memory>
#include <string>


namespace RTBKIT {


struct BidRequest;
struct ExchangeConnector;
struct AgentConfig;


/******************************************************************************/
/* FILTER POOL                                                                */
/******************************************************************************/

struct FilterPool
{
    ~FilterPool();


    typedef std::pair<std::shared_ptr<AgentConfig>, BiddableSpots> ConfigEntry;
    typedef std::vector<ConfigEntry> ConfigList;

    ConfigList filter(const BidRequest& br, const ExchangeConnector* conn);


    // \todo Need batch interfaces of these to alleviate overhead.
    void addFilter(const std::string& name);
    void removeFilter(const std::string& name);

    // \todo Need batch interfaces to alleviate overhead.
    void addConfig(
            const std::string& name, const std::shared_ptr<AgentConfig>& config);
    void removeConfig(const std::string& name);

    static void initWithDefaultFilters(FilterPool& pool);

private:

    struct Data
    {
        Data() {}
        Data(const Data& other);
        ~Data();

        ssize_t findConfig(const std::string& name) const;
        void addConfig(
                const std::string& name,
                const std::shared_ptr<AgentConfig>& config);
        void removeConfig(const std::string& name);

        ssize_t findFilter(const std::string& name) const;
        void addFilter(FilterBase* filter);
        void removeFilter(const std::string& name);

        // \todo Use unique_ptr when moving to gcc 4.7
        std::vector<FilterBase*> filters;

        typedef std::pair<std::string, std::shared_ptr<AgentConfig> > ConfigEntry;
        std::vector<ConfigEntry> configs;

        ConfigSet activeConfigs;
    };

    bool setData(Data*&, std::unique_ptr<Data>&);

    std::atomic<Data*> data;
    std::vector< std::shared_ptr<AgentConfig> > configs;
    Datacratic::GcLock gc;
};

} // namespace RTBKIT
