/** filter_test.cc                                 -*- C++ -*-
    Rémi Attab, 16 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Tests the for common filter utilities.

*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include "rtbkit/common/filter.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace RTBKIT;
using namespace Datacratic;

BOOST_AUTO_TEST_CASE(configSetTest)
{
    enum { n = 200 };

    {
        ConfigSet set;

        BOOST_CHECK_EQUAL(set.size(), 0);
        BOOST_CHECK(set.empty());
        BOOST_CHECK_EQUAL(set.count(), 0);

        for (size_t i = 0; i < n; ++i) {
            set.set(i);
            BOOST_CHECK(set.test(i));
            BOOST_CHECK_LE(i, set.size());
            BOOST_CHECK_EQUAL(set.count(), 1);
            set.reset(i);
        }

        BOOST_CHECK(set.empty());
        BOOST_CHECK_EQUAL(set.count(), 0);
    }

    {
        ConfigSet set;
        for (size_t i = 0; i < n; ++i) {
            ConfigSet mask;
            mask.set(i);
            set |= mask;

            for (size_t j = 0; j <= i; ++j)
                BOOST_CHECK(set.test(j));

            for (size_t j = i + 1; j < n; ++j)
                BOOST_CHECK(!set.test(j));
        }
    }

    {
        ConfigSet set(true);

        for (size_t i = 0; i < n; ++i) {
            ConfigSet mask;
            mask.set(i);
            set &= mask.negate();

            for (size_t j = 0; j <= i; ++j)
                BOOST_CHECK(!set.test(j));

            for (size_t j = i + 1; j < n; ++j)
                BOOST_CHECK(set.test(j));

        }
    }

    {
        ConfigSet setA, setB;

        for (size_t i = 0; i < n; ++i) {
            setA.set(i);
            setB.set(i+n);
        }

        ConfigSet resultA = setA;
        resultA |= setB;

        ConfigSet resultB = setB;
        resultB |= setA;

        BOOST_CHECK_EQUAL(resultA.size(), resultB.size());
        BOOST_CHECK_EQUAL(resultA.count(), resultB.count());

        for (size_t i = 0; i < n*2; ++i) {
            BOOST_CHECK(resultA.test(i));
            BOOST_CHECK(resultB.test(i));
        }
    }

    {
        for (size_t i = 1; i < 64; ++i) {
            ConfigSet set;

            for (size_t j = 0; j < n; j += i)
                set.set(j);

            for (size_t j = set.next(), k = 0;
                 j < set.size();
                 j = set.next(j+1), k++)
            {
                BOOST_CHECK_EQUAL(j, k*i);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(creativeMatrixTest)
{
    enum { n = 10, m = 100 };


    {
        CreativeMatrix matrix;

        BOOST_CHECK(matrix.empty());
        BOOST_CHECK_EQUAL(matrix.size(), 0);

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < m; ++j) {
                BOOST_CHECK(!matrix.test(i,j));
                matrix.set(i,j);
                BOOST_CHECK(matrix.test(i,j));
                matrix.reset(i,j);
                BOOST_CHECK(!matrix.test(i,j));
                BOOST_CHECK(matrix.empty());
            }
        }

        BOOST_CHECK(matrix.empty());
    }

    {
        CreativeMatrix matrix;

        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < m; ++j)
                BOOST_CHECK(!matrix.test(i,j));

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < m; ++j) {
                CreativeMatrix mask;
                mask.set(i,j);
                matrix |= mask;
                BOOST_CHECK(matrix.test(i,j));
            }
        }

        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < m; ++j)
                BOOST_CHECK(matrix.test(i,j));
    }

    {
        CreativeMatrix matrix(true);

        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < m; ++j)
                BOOST_CHECK(matrix.test(i,j));

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < m; ++j) {
                CreativeMatrix mask;
                mask.set(i,j);
                matrix &= mask.negate();
                BOOST_CHECK(!matrix.test(i,j));
            }
        }

        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < m; ++j)
                BOOST_CHECK(!matrix.test(i,j));
    }
}
