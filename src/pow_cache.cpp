// Copyright (c) 2026 The Dpowcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow_cache.h>

#include <primitives/block.h>
#include <sync.h>
#include <uint256.h>

#include <cstring>
#include <list>
#include <unordered_map>
#include <utility>

namespace pow_cache {

namespace {

//! Same sentinel value used by primitives/block.cpp on PoW compute failure.
//! Replicated here (private) so we never cache failures. Kept in lock-step
//! with PoWHashFailureSentinel() in src/primitives/block.cpp.
uint256 FailureSentinel()
{
    uint256 s;
    std::memset(s.begin(), 0xff, 32);
    return s;
}

struct uint256_hasher {
    size_t operator()(const uint256& k) const noexcept
    {
        // GetHash() of a CBlockHeader is already a uniformly distributed
        // SHA-256d, so taking the low 64 bits is a fine hash table key.
        size_t r;
        std::memcpy(&r, k.begin(), sizeof(r));
        return r;
    }
};

//! One LRU instance per algorithm. Encapsulated so capacity / state is
//! not shared between Yespower and Argon2id (different cost & call patterns).
class LruCache
{
public:
    using Entry = std::pair<uint256, uint256>; // key, value
    using ListT = std::list<Entry>;

    void SetCapacity(size_t cap) EXCLUSIVE_LOCKS_REQUIRED(!m_mu)
    {
        LOCK(m_mu);
        m_capacity = cap;
        while (m_list.size() > m_capacity) {
            m_map.erase(m_list.back().first);
            m_list.pop_back();
            ++m_evictions;
        }
    }

    void Clear() EXCLUSIVE_LOCKS_REQUIRED(!m_mu)
    {
        LOCK(m_mu);
        m_list.clear();
        m_map.clear();
        m_hits = m_misses = m_evictions = 0;
    }

    //! Look up `key`. On hit: returns true and writes value to *out, marks LRU.
    bool Lookup(const uint256& key, uint256* out) EXCLUSIVE_LOCKS_REQUIRED(!m_mu)
    {
        LOCK(m_mu);
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            ++m_misses;
            return false;
        }
        m_list.splice(m_list.begin(), m_list, it->second);
        *out = it->second->second;
        ++m_hits;
        return true;
    }

    //! Insert `key -> value`. Evicts LRU tail if over capacity.
    //! No-op if capacity == 0 (cache disabled).
    void Insert(const uint256& key, const uint256& value) EXCLUSIVE_LOCKS_REQUIRED(!m_mu)
    {
        LOCK(m_mu);
        if (m_capacity == 0) return;
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            // Concurrent insert: refresh value and move to front.
            it->second->second = value;
            m_list.splice(m_list.begin(), m_list, it->second);
            return;
        }
        m_list.emplace_front(key, value);
        m_map.emplace(key, m_list.begin());
        while (m_list.size() > m_capacity) {
            m_map.erase(m_list.back().first);
            m_list.pop_back();
            ++m_evictions;
        }
    }

    Stats Snapshot() EXCLUSIVE_LOCKS_REQUIRED(!m_mu)
    {
        LOCK(m_mu);
        return Stats{
            /*hits=*/m_hits,
            /*misses=*/m_misses,
            /*evictions=*/m_evictions,
            /*size=*/m_list.size(),
            /*capacity=*/m_capacity,
        };
    }

private:
    mutable Mutex m_mu;
    size_t m_capacity GUARDED_BY(m_mu){0};
    ListT m_list GUARDED_BY(m_mu);
    std::unordered_map<uint256, ListT::iterator, uint256_hasher> m_map GUARDED_BY(m_mu);
    uint64_t m_hits GUARDED_BY(m_mu){0};
    uint64_t m_misses GUARDED_BY(m_mu){0};
    uint64_t m_evictions GUARDED_BY(m_mu){0};
};

LruCache g_yes;
LruCache g_arg;

//! Generic compute-or-cache helper. `compute` runs OUTSIDE the lock — argon2id
//! takes >50 ms and must never block other threads.
template <typename Compute>
uint256 LookupOrCompute(LruCache& cache, const uint256& key, Compute compute)
{
    uint256 cached;
    if (cache.Lookup(key, &cached)) return cached;
    uint256 fresh = compute();
    if (fresh == FailureSentinel()) return fresh; // never cache transient failures
    cache.Insert(key, fresh);
    return fresh;
}

} // namespace

void Init(size_t capacity)
{
    g_yes.SetCapacity(capacity);
    g_arg.SetCapacity(capacity);
}

void Shutdown()
{
    g_yes.Clear();
    g_arg.Clear();
    g_yes.SetCapacity(0);
    g_arg.SetCapacity(0);
}

uint256 GetYespower(const CBlockHeader& header)
{
    return LookupOrCompute(g_yes, header.GetHash(),
                           [&] { return header.GetYespowerPoWHash(); });
}

uint256 GetArgon2id(const CBlockHeader& header)
{
    return LookupOrCompute(g_arg, header.GetHash(),
                           [&] { return header.GetArgon2idPoWHash(); });
}

Stats GetYespowerStats() { return g_yes.Snapshot(); }
Stats GetArgon2idStats() { return g_arg.Snapshot(); }

} // namespace pow_cache
