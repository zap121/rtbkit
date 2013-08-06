/** filter.h                                 -*- C++ -*-
    Rémi Attab, 23 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Interface definition for bid request filters used by the filter pool
    mechanism.

*/

#pragma once

#include "jml/arch/bitops.h"

#include <vector>
#include <string>
#include <memory>
#include <functional>


namespace RTBKIT {


struct BidRequest;
struct ExchangeConnector;
struct AgentConfig;


/******************************************************************************/
/* CONFIG SET                                                                 */
/******************************************************************************/

struct ConfigSet
{
    typedef uint64_t Word;
    static constexpr size_t Div = sizeof(Word) * 8;

    ConfigSet(bool defaultValue = false) :
        defaultValue(defaultValue ? ~Word(0) : 0)
    {}


    size_t size() const
    {
        return bitfield.size() * Div;
    }

    void expand(size_t newSize)
    {
        newSize = (newSize - 1) / Div + 1; // ceilDiv(newSize Div)
        if (newSize <= size()) return;

        bitfield.resize(newSize, defaultValue);
    }


    void set(size_t index)
    {
        expand(index);
        bitfield[index / Div] |= 1ULL << (index % Div);
    }

    void set(size_t index, bool value)
    {
        if (value) set(index);
        else reset(index);
    }

    void reset(size_t index)
    {
        expand(index);
        bitfield[index / Div] &= ~(1ULL << (index % Div));
    }

    bool test(size_t index) const
    {
        return bitfield[index / Div] & (1ULL << (index %Div));
    }

    size_t count() const
    {
        size_t total = 0;

        for (size_t i = 0; i < size(); ++i) {
            if (!bitfield[i]) continue;
            total += ML::num_bits_set(bitfield[i]);
        }

        return total;
    }

    size_t empty() const
    {
        for (size_t i = 0; i < size(); ++i) {
            if (bitfield[i]) return false;
        }
        return true;
    }


    ConfigSet& operator &= (const ConfigSet& other)
    {
        expand(other.size());

        for (size_t i = 0; i < other.size(); ++i)
            bitfield[i] &= other.bitfield[i];

        for (size_t i = other.size(); i < bitfield.size(); ++i)
            bitfield[i] &= other.defaultValue;

        return *this;
    }

    ConfigSet& operator |= (const ConfigSet& other)
    {
        expand(other.size());

        for (size_t i = 0; i < other.size(); ++i)
            bitfield[i] |= other.bitfield[i];

        for (size_t i = other.size(); i < bitfield.size(); ++i)
            bitfield[i] |= other.defaultValue;

        return *this;
    }

    ConfigSet& operator ^= (const ConfigSet& other)
    {
        expand(other.size());

        for (size_t i = 0; i < other.size(); ++i)
            bitfield[i] ^= other.bitfield[i];

        for (size_t i = other.size(); i < bitfield.size(); ++i)
            bitfield[i] ^= other.defaultValue;

        return *this;
    }

    ConfigSet& negate()
    {
        for (size_t i = 0; i < size(); ++i)
            bitfield[i] = ~bitfield[i];
        return *this;
    }

    ConfigSet negate() const
    {
        return ConfigSet(*this).negate();
    }


    /** Usage example:

        for (size_t i = set.next(); i < set.size(); i = set.next(i + 1)) {
            // ...
        }

     */
    size_t next(size_t start = 0) const
    {
        size_t topIndex = start / Div;
        size_t subIndex = start % Div;
        Word mask = -1ULL & ~((1ULL << subIndex) - 1);

        for (size_t i = topIndex; i < bitfield.size(); ++i) {
            Word value = bitfield[i] & mask;
            mask = -1ULL;

            if (!value) continue;

            return (i * Div) + ML::lowest_bit(value);
        }

        return size();
    }


private:

    std::vector<Word> bitfield;
    Word defaultValue;
};


/******************************************************************************/
/* FILTER BASE                                                                */
/******************************************************************************/

struct FilterBase
{
    /**

     */
    virtual std::string name() const = 0;

    /**

     */
    virtual FilterBase* clone() const = 0;

    /** Filters the given bid request such and a return the set of agent
        configuration that matches the given bid request.
     */
    virtual ConfigSet
    filter(const BidRequest&, const ExchangeConnector*) const = 0;


    /** Indicates that a new agent configuration is available and that it is
        associated with the given index. The configIndex should be used to
        return the ConfigSet object returned by the filter function.
     */
    virtual void addConfig(
            unsigned configIndex,
            const std::shared_ptr<AgentConfig>& config) = 0;

    /**

     */
    virtual void removeConfig(
            unsigned configIndex,
            const std::shared_ptr<AgentConfig>& config) = 0;

    /**
       \todo This will eventually need to be dynamic or something.
     */
    virtual unsigned priority() const { return 0; }

};


template<typename Filter>
struct FilterBaseT : public FilterBase
{
    std::string name() const { return Filter::name; }

    FilterBase* clone() const
    {
        return new Filter(*static_cast<const Filter*>(this));
    }
};


/******************************************************************************/
/* FILTER REGISTRY                                                            */
/******************************************************************************/

struct FilterRegistry
{
    typedef std::function<FilterBase* ()> ConstructFn;

    template<typename Filter>
    static void registerFilter()
    {
        registerFilter(Filter::name, [] () -> FilterBase* { return new Filter(); });
    }

    static void registerFilter(const std::string& name, ConstructFn fn);
    static FilterBase* makeFilter(const std::string& name);

    static std::vector<std::string> listFilters();
};


} // namespace RTBKIT
