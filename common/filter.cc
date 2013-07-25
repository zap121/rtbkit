/** filter.cc                                 -*- C++ -*-
    Rémi Attab, 24 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Implementation of the filter registry.

*/

#include "filter.h"
#include "jml/arch/spinlock.h"
#include "jml/utils/exc_check.h"

#include <unordered_map>
#include <thread>


using namespace std;
using namespace ML;


namespace RTBKIT {


/******************************************************************************/
/* FILTER REGISTRY                                                            */
/******************************************************************************/

namespace {

unordered_map<string, FilterRegistry::ConstructFn> filterRegister;
Spinlock filterRegisterLock;

} // namespace anonymous


void
FilterRegistry::
registerFilter(const string& name, ConstructFn fn)
{
    lock_guard<Spinlock> guard(filterRegisterLock);
    filterRegister.insert(make_pair(name, fn));
}


FilterBase*
FilterRegistry::
makeFilter(const string& name)
{
    ConstructFn fn;

    {
        lock_guard<Spinlock> guard(filterRegisterLock);

        auto it = filterRegister.find(name);
        ExcCheck(it != filterRegister.end(), "Unknown filter: " + name);
        fn = it->second;
    }

    return fn();
}

vector<string>
FilterRegistry::
listFilters()
{
    vector<string> filters;
    {
        lock_guard<Spinlock> guard(filterRegisterLock);

        filters.reserve(filterRegister.size());
        for (const auto& filter : filterRegister)
            filters.push_back(filter.first);
    }

    return filters;
}



} // namepsace RTBKIT
