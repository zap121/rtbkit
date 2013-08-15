/** generic_filters_test.cc                                 -*- C++ -*-
    Rémi Attab, 15 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Tests for the generic filters

*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include "rtbkit/plugins/filters/generic_filters.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace ML;
using namespace Datacratic;

void check(const ConfigSet& configs, const std::initializer_list<size_t>& expected)
{
    bool err = false;
    size_t cfg = configs.next();

    for (size_t ex : expected) {
        while (cfg < ex) {
            err = true;
            cerr << "\t" << cfg << " -> extra\n";
            cfg = configs.next(cfg+1);
        }

        if (cfg > ex) {
            err = true;
            cerr << "\t" << ex << " -> missing\n";
        }

        if (ex == cfg) cfg = configs.next(cfg+1);
    }

    BOOST_CHECK(!err);
}

BOOST_AUTO_TEST_CASE( listFilterTest)
{
    ListFilter<size_t> filter;

    filter.addConfig(0, { 1, 2, 3 });
    filter.addConfig(1, { 3, 4, 5 });
    filter.addConfig(2, { 1, 5 });
    filter.addConfig(3, { 0, 6 });

    check(filter.filter(0), { 3 });
    check(filter.filter(1), { 0, 2 });
    check(filter.filter(2), { 0 });
    check(filter.filter(3), { 0, 1 });
    check(filter.filter(4), { 1 });
    check(filter.filter(5), { 1, 2 });
    check(filter.filter(6), { 3 });
    check(filter.filter(7), {  });

    filter.removeConfig(3, { 0, 6 });

    check(filter.filter(0), {  });
    check(filter.filter(1), { 0, 2 });
    check(filter.filter(2), { 0 });
    check(filter.filter(3), { 0, 1 });
    check(filter.filter(4), { 1 });
    check(filter.filter(5), { 1, 2 });
    check(filter.filter(6), {  });
    check(filter.filter(7), {  });

    filter.removeConfig(0, { 1, 2, 3 });

    check(filter.filter(0), {  });
    check(filter.filter(1), { 2 });
    check(filter.filter(2), {  });
    check(filter.filter(3), { 1 });
    check(filter.filter(4), { 1 });
    check(filter.filter(5), { 1, 2 });
    check(filter.filter(6), {  });
    check(filter.filter(7), {  });

}
