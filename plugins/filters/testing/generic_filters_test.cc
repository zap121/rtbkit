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
using namespace RTBKIT;
using namespace Datacratic;


template<typename T>
vector<T> makeList(const initializer_list<T>& list)
{
    return vector<T>(list.begin(), list.end());
};

void title(const string& title)
{
    size_t padding = 80 - 4 - title.size();
    cerr << "[ " << title << " ]" << string(padding, '-') << endl;
}

void check(const ConfigSet& configs, const initializer_list<size_t>& expected)
{
    ConfigSet ex;
    for (size_t cfg : expected) ex.set(cfg);

    ConfigSet diff = configs;
    diff ^= ex;

    if (diff.empty()) return;

    cerr << "val=" << configs.print() << endl
        << "exp=" << ex.print() << endl
        << "dif=" << diff.print() << endl;
    BOOST_CHECK(false);
}

BOOST_AUTO_TEST_CASE(listFilterTest)
{
    ListFilter<size_t> filter;

    BOOST_CHECK(filter.isEmpty({ }));
    BOOST_CHECK(!filter.isEmpty({ 1 }));

    title("list-1");
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

    title("list-2");
    filter.removeConfig(3, { 0, 6 });

    check(filter.filter(0), {  });
    check(filter.filter(1), { 0, 2 });
    check(filter.filter(2), { 0 });
    check(filter.filter(3), { 0, 1 });
    check(filter.filter(4), { 1 });
    check(filter.filter(5), { 1, 2 });
    check(filter.filter(6), {  });
    check(filter.filter(7), {  });

    title("list-3");
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

BOOST_AUTO_TEST_CASE(intervalFilterTest)
{
    auto range = [] (size_t first, size_t last) {
        return make_pair(first, last);
    };

    IntervalFilter<size_t> filter;

    title("interval-1");
    filter.addConfig(0, makeList({ range(1, 2) }));
    filter.addConfig(1, makeList({ range(1, 3), range(2, 4) }));
    filter.addConfig(2, makeList({ range(2, 4), range(5, 6) }));
    filter.addConfig(3, makeList({ range(3, 4), range(4, 5) }));
    filter.addConfig(4, makeList({ range(1, 2) }));

    check(filter.filter(0), { });
    check(filter.filter(1), { 0, 1, 4 });
    check(filter.filter(2), { 1, 2 });
    check(filter.filter(3), { 1, 2, 3 });
    check(filter.filter(4), { 3 });
    check(filter.filter(5), { 2 });
    check(filter.filter(6), {  });
    check(filter.filter(7), {  });

    title("interval-2");
    filter.removeConfig(3, makeList({ range(3, 4), range(4, 5) }));

    check(filter.filter(0), { });
    check(filter.filter(1), { 0, 1, 4 });
    check(filter.filter(2), { 1, 2 });
    check(filter.filter(3), { 1, 2 });
    check(filter.filter(4), {  });
    check(filter.filter(5), { 2 });
    check(filter.filter(6), {  });
    check(filter.filter(7), {  });

    title("interval-3");
    filter.removeConfig(1, makeList({ range(1, 3), range(2, 4) }));

    check(filter.filter(0), { });
    check(filter.filter(1), { 0, 4 });
    check(filter.filter(2), { 2 });
    check(filter.filter(3), { 2 });
    check(filter.filter(4), {  });
    check(filter.filter(5), { 2 });
    check(filter.filter(6), {  });
    check(filter.filter(7), {  });

    title("interval-4");
    filter.removeConfig(4, makeList({ range(1, 2) }));

    check(filter.filter(0), { });
    check(filter.filter(1), { 0 });
    check(filter.filter(2), { 2 });
    check(filter.filter(3), { 2 });
    check(filter.filter(4), {  });
    check(filter.filter(5), { 2 });
    check(filter.filter(6), {  });
    check(filter.filter(7), {  });

}

BOOST_AUTO_TEST_CASE(domainFilterTest)
{
    DomainFilter<std::string> filter;

    filter.addConfig(0, makeList<string>({ "com" }));
    filter.addConfig(1, makeList<string>({ "google.com" }));
    filter.addConfig(2, makeList<string>({ "bob.org" }));

    title("domain-1");
    check(filter.filter(Url("site.google.com")), { 0, 1 });
    check(filter.filter(Url("google.com")),      { 0, 1 });
    check(filter.filter(Url("com")),             { 0 });
    check(filter.filter(Url("blah.com")),        { 0 });
    check(filter.filter(Url("blooh.org")),       { });
    check(filter.filter(Url("bob.org")),         { 2 });
    check(filter.filter(Url("random.net")),      { });

    title("domain-2");
    filter.removeConfig(0, makeList<string>({ "com" }));

    check(filter.filter(Url("site.google.com")), { 1 });
    check(filter.filter(Url("google.com")),      { 1 });
    check(filter.filter(Url("com")),             { });
    check(filter.filter(Url("blah.com")),        { });
    check(filter.filter(Url("blooh.org")),       { });
    check(filter.filter(Url("bob.org")),         { 2 });
    check(filter.filter(Url("random.net")),      { });

    title("domain-3");
    filter.removeConfig(2, makeList<string>({ "bob.org" }));

    check(filter.filter(Url("site.google.com")), { 1 });
    check(filter.filter(Url("google.com")),      { 1 });
    check(filter.filter(Url("com")),             { });
    check(filter.filter(Url("blah.com")),        { });
    check(filter.filter(Url("blooh.org")),       { });
    check(filter.filter(Url("bob.org")),         { });
    check(filter.filter(Url("random.net")),      { });
}

BOOST_AUTO_TEST_CASE(regexFilterTest)
{
    using boost::regex;
    RegexFilter<regex, string> filter;

    title("regex-1");
    filter.addConfig(0, makeList({ regex("a"), regex("b")}));
    filter.addConfig(1, makeList({ regex("a|b") }));
    filter.addConfig(2, makeList({ regex("a"), regex("c")}));
    filter.addConfig(3, makeList({ regex("^ab+")}));

    check(filter.filter("a"),   { 0, 1, 2});
    check(filter.filter("b"),   { 0, 1 });
    check(filter.filter("c"),   { 2 });
    check(filter.filter("abb"), { 0, 1, 2, 3 });
    check(filter.filter("d"),   { });

    title("regex-2");
    filter.removeConfig(3, makeList({ regex("^ab+")}));

    check(filter.filter("a"),   { 0, 1, 2});
    check(filter.filter("b"),   { 0, 1 });
    check(filter.filter("c"),   { 2 });
    check(filter.filter("abb"), { 0, 1, 2 });
    check(filter.filter("d"),   { });

    title("regex-3");
    filter.removeConfig(0, makeList({ regex("a"), regex("b")}));

    check(filter.filter("a"),   { 1, 2});
    check(filter.filter("b"),   { 1 });
    check(filter.filter("c"),   { 2 });
    check(filter.filter("abb"), { 1, 2 });
    check(filter.filter("d"),   { });

    filter.removeConfig(1, makeList({ regex("a|b") }));

    check(filter.filter("a"),   { 2});
    check(filter.filter("b"),   { });
    check(filter.filter("c"),   { 2 });
    check(filter.filter("abb"), { 2 });
    check(filter.filter("d"),   { });
}
