// Copyright (c) The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TEST_UTIL_TIME_H
#define BITCOIN_TEST_UTIL_TIME_H

#include <util/check.h>
#include <util/time.h>


/// Helper to initialize the global MockableSteadyClock, let a duration elapse,
/// and reset it after use in a test.
class SteadyClockContext
{
    MockableSteadyClock::mock_time_point::duration t{MockableSteadyClock::INITIAL_MOCK_TIME};

public:
    /** Initialize with INITIAL_MOCK_TIME. */
    explicit SteadyClockContext() { (*this) += 0s; }

    /** Unset mocktime */
    ~SteadyClockContext() { MockableSteadyClock::ClearMockTime(); }

    SteadyClockContext(const SteadyClockContext&) = delete;
    SteadyClockContext& operator=(const SteadyClockContext&) = delete;

    /** Change mocktime by the given duration delta */
    void operator+=(std::chrono::milliseconds d)
    {
        Assert(d >= 0s); // Steady time can only increase monotonically.
        t += d;
        MockableSteadyClock::SetMockTime(t);
    }
};

/// Helper to initialize the global NodeClock mocktime for the duration of a test,
/// and reset it afterwards.
// BACKPORT (adapted from upstream bitcoin/bitcoin's FakeNodeClock/NodeClockContext; that
// class isn't in 31.x this reimplements just what our fuzz harness needs, using 31.x's
// own SetMockTime()/NodeSeconds, without master's later LimitOne<> CRTP wrapper).
// DO NOT DROP ON NEXT UPSTREAM MERGE/REBASE.
class FakeNodeClock
{
    NodeSeconds m_t{std::chrono::seconds::max()};

public:
    explicit FakeNodeClock(NodeSeconds init_time) { set(init_time); }
    explicit FakeNodeClock(std::chrono::seconds init_time) { set(NodeSeconds{init_time}); }

    /** Unset mocktime. */
    ~FakeNodeClock() { set(std::chrono::seconds{0}); }

    FakeNodeClock(const FakeNodeClock&) = delete;
    FakeNodeClock& operator=(const FakeNodeClock&) = delete;

    /** Set mocktime. */
    void set(NodeSeconds t) { SetMockTime(m_t = t); }
    void set(std::chrono::seconds t) { set(NodeSeconds{t}); }
};

#endif // BITCOIN_TEST_UTIL_TIME_H
