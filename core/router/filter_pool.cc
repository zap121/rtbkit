/** filter_pool.cc                                 -*- C++ -*-
    Rémi Attab, 24 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Implementation of the filter pool.

*/

#include "filter_pool.h"
#include "rtbkit/common/bid_request.h"
#include "rtbkit/common/exchange_connector.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "jml/utils/call_guard.h"
#include "jml/utils/exc_check.h"


using namespace std;
using namespace ML;


namespace RTBKit {


/******************************************************************************/
/* FILTER POOL                                                                */
/******************************************************************************/

void
FilterPool::
initWithDefaultFilters(FilterPool& pool)
{
    for (const auto& filter : FilterRegistry::listFilters())
        pool.addFilter(filter);
}


void
FilterPool::
setFilters(Filters* newFilters)
{
    Filters* oldFilters = filters.exchange(newFilters.release());
    gc.defer([=] { delete oldFilters; });
}


FilterPool::
~FilterPool()
{
    setFilters(nullptr);
    rcu.deferBarrier();
}


ConfigSet
FilterPool::
filter(const BidRequest& br, const ExchangeConnector* conn)
{
    rcu.lockShared();
    Call_Guard guard([&] { rcu.unlockShared(); });

    Filters* current = filters.load();
    ConfigSet matching = current->activeConfigs;

    for (FilterBase* filter : *current) {
        matching &= filter->filter(br, conn);
        if (matching.empty()) break;
    }

    return matching;
}


void
FilterPool::
addFilter(const string& filterName)
{
    unique_ptr<FilterBase> filter(FilterRegistry::makeFilter(filterName));
    addFilter(filter);
}

void
FilterPool::
addFilter(unique_ptr<FilterBase>& filter)
{
    unique_ptr<Filters> newFilters(new Filters(*filters.load()));

    for (size_t i = 0; i < newFilters->size(); ++i) {
        if ((*newFilters)[i]->name() == filter->name()) return;
    }

    for (size_t i = 0; i < configs.size(); ++i) {
        if (!configs[i]) continue;
        filter->addConfig(i, configs[i]);
    }

    newFilters->push_back(filter.release());

    auto lessFn = [] (FilterBase* lhs, FilterBase* rhs) {
        return lhs->priority() < rhs->priority();
    };
    std::sort(newFilters->begin(), newFilters->end(), lessFn);

    setFilters(newFilters);
}


void
FilterPool::
removeFilter(const string& filterName)
{
    Filters* current = filters.load();

    unique_ptr<Filters> newFilters(new Filters(*current, filterName));
    if (newFilters->size() == current->size()) return;

    newFilters->push_back(filter);
    setFilters(newFilters);

}


size_t
FilterPool::
addConfig(shared_ptr<AgentConfig>& config)
{
    size_t index = 0;
    for (; index < configs.size(); ++index) {
        if (configs[index]) continue;

        configs[i] = config;
        break;
    }

    if (index == configs.size())
        configs.push_back(config);

    unique_ptr<Filters> newFilters(new Filters(*filters.load()));
    newFilters->activeConfigs.set(configIndex);

    for (size_t i = 0; i < current->size(); ++i)
        (*newFilters)[i]->addConfig(index, config);

    setFilters(newFilters);
    return index;
}


void
FilterPool::
removeConfig(shared_ptr<AgentConfig> configs)
{
    size_t index = 0;
    for (; index < configs.size(); ++index) {
        if (configs[index] == config) break;
    }

    removeConfig(index);
}


void
FilterPool::
removeConfig(size_t configIndex)
{
    ExcCheckLess(index, configs.size(), "unknown config: " + configIndex);
    ExcCheck(configs[configIndex], "unknown config: " + configIndex);

    configs[configIndex].reset();

    unique_ptr<Filters> newFilters(new Filters(*filters.load()));
    newFilters->activeConfigs.reset(configIndex);

    for (size_t i = 0; i < newFitlers->size(); ++i)
        (*newFilters)[i]->removeConfig(index, config);

    setFilters(newFilters);
}


/******************************************************************************/
/* FILTER POOL - FILTERS                                                      */
/******************************************************************************/

FilterPool::Filters::
Filters(const Filters& other) : activeConfigs(other.activeConfigs)
{
    reserve(other.size());

    for (size_t i = 0; i < other.size(); ++i)
        (*this)[i] = other[i]->clone();
}


FilterPool::Filters::
Filters(const Filters& other, const string& name) :
    activeConfigs(other.activeConfigs)
{
    reserve(other.size() - 1);

    for (size_t i = 0, j = 0; i < other.size(); ++i) {
        if ((*this)[i]->name() == name) continue;

        (*this)[j] = other[i]->clone();
        ++j;
    }
}


FilterPool::Filters::
~Filters()
{
    for (const auto& filter : *this) delete filter;
}

} // namepsace RTBKit
