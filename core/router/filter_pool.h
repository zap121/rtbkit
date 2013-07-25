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

    ConfigSet filter(const BidRequest& br, const ExchangeConnector* conn);

    // \todo Need batch interfaces of these to alleviate overhead.
    void addFilter(const std::string& toAdd);
    void removeFilter(const std::string& toRemove);

    // \todo Need batch interfaces to alleviate overhead.
    size_t addConfig(std::shared_ptr<AgentConfig>& config);
    void removeConfig(std::shared_ptr<AgentConfig> configs);
    void removeConfig(size_t configIndex);

    // Temporary until we can get a fully dynamic system going.
    static void initWithDefaultFilters(FilterPool& pool);

private:

    struct Filters : public std::vector<FilterBase*>
    {
        Filters(const Filters& other);
        Filters(const Filters& other, const std::string& toRemove);
        ~Filters();

        ConfigSet activeConfigs;
    };

    void addFilter(std::unique_ptr<FilterBase>& filter);
    void setFilters(std::unique_ptr<Filters>& newFilters);

    std::atomic<Filters*> filters;
    std::vector< std::shared_ptr<AgentConfig> > configs;
    Datacratic::GcLock gc;
};

} // namespace RTBKIT
