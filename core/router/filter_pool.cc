/** filter_pool.cc                                 -*- C++ -*-
    Rémi Attab, 24 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Implementation of the filter pool.

*/

#include "filter_pool.h"
#include "rtbkit/common/bid_request.h"
#include "rtbkit/common/exchange_connector.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "jml/utils/exc_check.h"


using namespace std;
using namespace ML;


namespace RTBKIT {


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


bool
FilterPool::
setData(Data*& oldData, unique_ptr<Data>& newData)
{
    if (!data.compare_exchange_strong(oldData, newData.get()))
        return false;

    newData.release();
    gc.defer([=] { delete oldData; });

    return true;
}


FilterPool::
~FilterPool()
{
    {
        GcLockBase::SharedGuard guard(gc);

        unique_ptr<Data> nil;
        Data* current = data.load();
        while (!setData(current, nil));
    }

    gc.deferBarrier();
}


FilterPool::ConfigList
FilterPool::
filter(const BidRequest& br, const ExchangeConnector* conn)
{
    GcLockBase::SharedGuard guard(gc, false);

    Data* current = data.load();
    ConfigSet matching = current->activeConfigs;

    for (FilterBase* filter : current->filters) {
        matching &= filter->filter(br, conn);
        if (matching.empty()) break;
    }

    ConfigList configs;
    for (size_t i = matching.next();
         i < matching.size();
         i = matching.next(i + 1))
    {
        configs.push_back(current->configs[i].second);
    }

    return configs;
}


void
FilterPool::
addFilter(const string& name)
{
    GcLockBase::SharedGuard guard(gc);

    Data* oldData = data.load();
    unique_ptr<Data> newData;

    do {
        newData.reset(new Data(*oldData));
        newData->addFilter(FilterRegistry::makeFilter(name));
    } while (!setData(oldData, newData));
}


void
FilterPool::
removeFilter(const string& name)
{
    GcLockBase::SharedGuard guard(gc);

    unique_ptr<Data> newData;
    Data* oldData = data.load();

    do {
        newData.reset(new Data(*oldData));
        newData->removeFilter(name);
    } while (!setData(oldData, newData));
}


void
FilterPool::
addConfig(const string& name, const shared_ptr<AgentConfig>& config)
{
    GcLockBase::SharedGuard guard(gc);

    unique_ptr<Data> newData;
    Data* oldData = data.load();

    do {
        newData.reset(new Data(*oldData));
        newData->addConfig(name, config);
    } while (!setData(oldData, newData));
}


void
FilterPool::
removeConfig(const string& name)
{
    GcLockBase::SharedGuard guard(gc);

    unique_ptr<Data> newData;
    Data* oldData = data.load();

    do {
        newData.reset(new Data(*oldData));
        newData->removeConfig(name);
    } while (!setData(oldData, newData));
}


/******************************************************************************/
/* FILTER POOL - DATA                                                         */
/******************************************************************************/

FilterPool::Data::
Data(const Data& other) :
    configs(other.configs),
    activeConfigs(other.activeConfigs)
{
    filters.reserve(other.filters.size());
    for (FilterBase* filter : other.filters)
        filters.push_back(filter->clone());
}


FilterPool::Data::
~Data()
{
    for (FilterBase* filter : filters) delete filter;
}

ssize_t
FilterPool::Data::
findConfig(const string& name) const
{
    for (size_t i = 0; i < configs.size(); ++i) {
        if (configs[i].first == name) return i;
    }
    return -1;
}

void
FilterPool::Data::
addConfig(const string& name, const shared_ptr<AgentConfig>& config)
{
    ssize_t index = findConfig("");

    if (index >= 0)
        configs[index] = make_pair(name, config);
    else {
        index = configs.size();
        configs.emplace_back(make_pair(name, config));
    }

    activeConfigs.set(index);
    for (FilterBase* filter : filters)
        filter->addConfig(index, config);
}

void
FilterPool::Data::
removeConfig(const string& name)
{
    ssize_t index = findConfig(name);
    if (index < 0) return;

    activeConfigs.reset(index);
    for (FilterBase* filter : filters)
        filter->removeConfig(index, configs[index].second);

    configs[index].first = "";
    configs[index].second.reset();
}


ssize_t
FilterPool::Data::
findFilter(const string& name) const
{
    for (size_t i = 0; i < filters.size(); ++i) {
        if (filters[i]->name() == name) return i;
    }
    return -1;
}

void
FilterPool::Data::
addFilter(FilterBase* filter)
{
    filters.push_back(filter);
    sort(filters.begin(), filters.end(), [] (FilterBase* lhs, FilterBase* rhs) {
                return lhs->priority() < rhs->priority();
            });
}

void
FilterPool::Data::
removeFilter(const string& name)
{
    ssize_t index = findFilter(name);
    if (index < 0) return;

    delete filters[index];

    for (size_t i = index; i < filters.size() - 1; ++i)
        filters[i] = filters[i+1];

    filters.pop_back();
}

} // namepsace RTBKit
