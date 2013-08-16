 /** filter.h                                 -*- C++ -*-
    Rémi Attab, 23 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Interface definition for bid request filters used by the filter pool
    mechanism.

*/

#pragma once

#include "rtbkit/core/router/router_types.h"
#include "jml/utils/compact_vector.h"
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
        if (newSize) newSize = (newSize - 1) / Div + 1; // ceilDiv(newSize, Div)
        if (newSize <= bitfield.size()) return;
        bitfield.resize(newSize, defaultValue);
    }


    void set(size_t index)
    {
        expand(index + 1);
        bitfield[index / Div] |= 1ULL << (index % Div);
    }

    void set(size_t index, bool value)
    {
        if (value) set(index);
        else reset(index);
    }

    void reset(size_t index)
    {
        expand(index + 1);
        bitfield[index / Div] &= ~(1ULL << (index % Div));
    }

    bool operator[] (size_t index) const { return test(index); }

    bool test(size_t index) const
    {
        if (index >= size()) return defaultValue;
        return bitfield[index / Div] & (1ULL << (index %Div));
    }

    size_t count() const
    {
        size_t total = 0;

        for (size_t i = 0; i < bitfield.size(); ++i) {
            if (!bitfield[i]) continue;
            total += ML::num_bits_set(bitfield[i]);
        }

        return total;
    }

    size_t empty() const
    {
        for (size_t i = 0; i < bitfield.size(); ++i) {
            if (bitfield[i]) return false;
        }
        return true;
    }


#define RTBKIT_CONFIG_SET_OP(_op_)                                      \
    ConfigSet& operator _op_ (const ConfigSet& other)                   \
    {                                                                   \
        expand(other.size());                                           \
                                                                        \
        for (size_t i = 0; i < other.bitfield.size(); ++i)              \
            bitfield[i] _op_ other.bitfield[i];                         \
                                                                        \
        for (size_t i = other.bitfield.size(); i < bitfield.size(); ++i) \
            bitfield[i] _op_ other.defaultValue;                        \
                                                                        \
        return *this;                                                   \
    }

    RTBKIT_CONFIG_SET_OP(&=)
    RTBKIT_CONFIG_SET_OP(|=)
    RTBKIT_CONFIG_SET_OP(^=)

#undef RTBKIT_CONFIG_SET_OP

    ConfigSet& negate()
    {
        for (size_t i = 0; i < bitfield.size(); ++i)
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

    ML::compact_vector<Word, 2> bitfield;
    Word defaultValue;
};


/******************************************************************************/
/* CREATIVE MATRIX                                                            */
/******************************************************************************/

struct CreativeMatrix
{
    CreativeMatrix(bool defaultValue = false) :
        defaultValue(ConfigSet(defaultValue))
    {}

    CreativeMatrix(ConfigSet defaultValue) :
        defaultValue(defaultValue)
    {}

    size_t size() const { return matrix.size(); }

    bool empty() const
    {
        for (const ConfigSet& set : matrix) {
            if (!set.empty()) return false;
        }
        return true;
    }

    void expand(size_t newSize)
    {
        if (newSize <= matrix.size()) return;
        matrix.resize(newSize, defaultValue);
    }


    const ConfigSet& operator[] (size_t creative) const
    {
        return matrix[creative];
    }

    bool test(size_t creative, size_t config) const
    {
        if (creative >= matrix.size()) return defaultValue.test(config);
        return matrix[creative].test(config);
    }

    void set(size_t creative, size_t config, bool value = true)
    {
        expand(creative + 1);
        matrix[creative].set(config, value);
    }

    void reset(size_t creative, size_t config)
    {
        expand(creative + 1);
        matrix[creative].reset(config);
    }

#define RTBKIT_CREATIVE_MATRIX_OP(_op_)                                 \
    CreativeMatrix& operator _op_ (const CreativeMatrix& other)         \
    {                                                                   \
        expand(other.matrix.size());                                    \
                                                                        \
        for (size_t i = 0; i < other.matrix.size(); ++i)                \
            matrix[i] _op_ other.matrix[i];                             \
                                                                        \
        for (size_t i = other.matrix.size(); i < matrix.size(); ++i)    \
            matrix[i] _op_ other.defaultValue;                          \
                                                                        \
        return *this;                                                   \
    }

    RTBKIT_CREATIVE_MATRIX_OP(&=)
    RTBKIT_CREATIVE_MATRIX_OP(|=)
    RTBKIT_CREATIVE_MATRIX_OP(^=)

#undef RTBKIT_CREATIVE_MATRIX_OP

    CreativeMatrix& negate()
    {
        for (ConfigSet& set : matrix) set.negate();
        return *this;
    }

    CreativeMatrix negate() const
    {
        return CreativeMatrix(*this).negate();
    }

    ConfigSet aggregate() const
    {
        ConfigSet configs;

        for (const ConfigSet& set : matrix)
            configs |= set;

        return configs;
    }


private:
    ML::compact_vector<ConfigSet, 8> matrix;
    ConfigSet defaultValue;
};


/******************************************************************************/
/* FILTER STATE                                                               */
/******************************************************************************/

struct FilterState
{
    FilterState(const BidRequest& br, const ExchangeConnector* ex, ConfigSet configs) :
        request(br),
        exchange(ex),
        configs_(std::move(configs)),
        creatives_()
    {}

    const BidRequest& request;
    const ExchangeConnector * const exchange;

    const ConfigSet& configs() const { return configs_; }
    void narrowConfigs(const ConfigSet& mask) { configs_ &= mask; }

    CreativeMatrix creatives(unsigned impId) const
    {
        CreativeMatrix mask(configs_);
        if (impId >= creatives_.size()) return mask;

        CreativeMatrix ret = creatives_[impId];
        ret &= mask;
        return ret;
    }

    void narrowCreativesForImp(unsigned impId, const CreativeMatrix& mask)
    {
        if (impId >= creatives_.size())
            creatives_.resize(impId + 1, CreativeMatrix(configs_));

        creatives_[impId] &= mask;
        narrowConfigs(creatives_[impId].aggregate());
    }

    void narrowAllCreatives(const CreativeMatrix& mask)
    {
        for (CreativeMatrix& matrix : creatives_)
            matrix &= mask;
    }


    /** Returns a map of configIndex to BiddableSpots object based on the
        creative matrix.

        \todo Would be nice if we could remove the temp map in the inner loop.
     */
    std::unordered_map<unsigned, BiddableSpots> biddableSpots()
    {
        // Used to remove creatives for configs that have been filtered out.
        CreativeMatrix mask(configs_);

        std::unordered_map<unsigned, BiddableSpots> biddable;

        for (size_t impId = 0; impId < creatives_.size(); ++impId) {
            std::unordered_map<unsigned, SmallIntVector> biddableCreatives;

            creatives_[impId] &= mask;

            for (unsigned crId = 0; crId < creatives_[impId].size(); ++crId) {
                const auto& configs = creatives_[impId][crId];

                for (size_t config = configs.next();
                     config < configs.size();
                     config = configs.next(config + 1))
                {
                    biddableCreatives[config].push_back(crId);
                }
            }

            for (const auto& entry : biddableCreatives)
                biddable[entry.first].emplace_back(impId, entry.second);
        }

        return biddable;
    }

private:
    ConfigSet configs_;
    ML::compact_vector<CreativeMatrix, 4> creatives_;
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
    virtual void filter(FilterState& state) const = 0;


    /** Indicates that a new agent configuration is available and that it is
        associated with the given index. The configIndex should be used to
        return the ConfigSet object returned by the filter function.
     */
    virtual void
    addConfig(unsigned configIndex, const std::shared_ptr<AgentConfig>& config) = 0;

    /**

     */
    virtual void
    removeConfig(unsigned configIndex, const std::shared_ptr<AgentConfig>& config) = 0;

    /**
       \todo This will eventually need to be dynamic or something.
     */
    virtual unsigned priority() const { return 0; }

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
        registerFilter(Filter::name, [] () -> FilterBase* {
                    return new Filter();
                });
    }

    static void registerFilter(const std::string& name, ConstructFn fn);
    static FilterBase* makeFilter(const std::string& name);

    static std::vector<std::string> listFilters();
};


} // namespace RTBKIT
