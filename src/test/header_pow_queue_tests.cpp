// Copyright (c) 2026-present The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Scope of this file: CHeaderPoWCheck's CCheckQueue plumbing
// (headerpowcheckqueue, StartHeaderPoWCheckWorkerThreads()/
// StopHeaderPoWCheckWorkerThreads()) and HasValidProofOfWork()'s
// sequential/parallel threshold dispatch (validation.h/.cpp).
//
// Deliberately OUT of scope here (covered elsewhere, not re-tested):
//   - Argon2id PoW correctness itself      -> crypto_argon2id_tests.cpp
//   - CheckProofOfWork()/target math
//     (compact encoding, negative/overflow/
//     zero target)                         -> pow_tests.cpp
//   - HeaderPoWCache hit/miss/eviction     -> pow_cache_tests.cpp
//
// Every header built below is either "guaranteed valid" (regtest powLimit,
// i.e. the genesis block's nBits, plus a nonce search -- same trick as
// headers_sync_chainwork_tests.cpp) or "guaranteed invalid" (negative-target
// nBits, same trick as pow_tests.cpp's CheckProofOfWork_test_negative_target).
// Neither depends on genuinely exercising Argon2id correctness -- they're
// only the vehicle to get a deterministic true/false result flowing through
// the queue and through HasValidProofOfWork()'s threshold branch.
//
// headerpowcheckqueue itself has no accessor (by design, see the
// implementation notes in validation.cpp) and is not started by
// BasicTestingSetup. Every test in this file therefore runs under
// RegTestingSetup (a TestingSetup, which is in turn a ChainTestingSetup),
// so that:
//   - ChainTestingSetup's constructor/destructor has already called
//     StartHeaderPoWCheckWorkerThreads()/StopHeaderPoWCheckWorkerThreads()
//     around headerpowcheckqueue, letting the HasValidProofOfWork() tests
//     below exercise the real production queue by calling the public
//     function directly, with no getter needed;
//   - Params()/Params().GetConsensus() resolve to regtest, whose easy
//     powLimit keeps the nonce search in MakeValidHeader() fast.
// The number of worker threads behind headerpowcheckqueue is therefore
// whatever ChainTestingSetup happens to start it with -- fixed for the
// whole file, not parameterized per test case. That's fine: the "0 workers"
// master-thread-only code path (relevant for small machines, see
// init.cpp's header_pow_threads calculation) is instead covered directly
// by local_queue_zero_workers_master_processes_all below, against a
// separate, local queue that is simply never started.

#include <chainparams.h>
#include <checkqueue.h>
#include <consensus/params.h>
#include <pow.h>
#include <primitives/block.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <validation.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include <boost/test/unit_test.hpp>

namespace {

//! Local (non-production) worker-thread count for the bare-queue-mechanics
//! tests below. Deliberately independent of whatever ChainTestingSetup
//! starts headerpowcheckqueue's own thread count at.
constexpr int LOCAL_QUEUE_WORKER_THREADS = 3;

//! Matches headerpowcheckqueue's own construction (validation.cpp).
constexpr unsigned int LOCAL_QUEUE_BATCH_SIZE = 64;

struct HeaderPoWCheckQueueTest : RegTestingSetup {
    const Consensus::Params& m_consensus{Params().GetConsensus()};

    //! nBits that CheckProofOfWork() rejects unconditionally, for any hash:
    //! the target it decodes to has the sign bit set, so DeriveTarget's
    //! range check fails before any hash is even compared against it. No
    //! nonce search, no dependence on what Argon2id actually outputs --
    //! deterministic and free. Same trick as pow_tests.cpp's
    //! CheckProofOfWork_test_negative_target.
    const uint32_t m_always_invalid_nbits{UintToArith256(m_consensus.powLimit).GetCompact(/*fNegative=*/true)};

    //! Build a header guaranteed to pass CHeaderPoWCheck. Regtest's genesis
    //! nBits (0x207fffff, see kernel/chainparams.cpp's CRegTestParams and
    //! headers_sync_chainwork_tests.cpp) accepts roughly half of all
    //! hashes, so this loop is expected to run once or twice, never more
    //! than a handful of iterations. distinguisher only needs to keep
    //! GetHash() unique across headers built in the same test run --
    //! HeaderPoWCache (pow_cache.cpp) is keyed on GetHash() and is a
    //! process-lifetime singleton, so a repeated header would short-circuit
    //! straight to "valid" via the cache instead of exercising anything.
    CBlockHeader MakeValidHeader(uint32_t distinguisher) const
    {
        CBlockHeader header;
        header.nVersion = 1;
        header.hashPrevBlock = uint256::ZERO;
        header.hashMerkleRoot = uint256::ZERO;
        header.nTime = 1'700'000'000 + distinguisher;
        header.nBits = Params().GenesisBlock().nBits;
        header.nNonce = 0;
        while (!CheckProofOfWork(header.GetArgon2idPoWHash(), header.nBits, m_consensus)) {
            ++header.nNonce;
        }
        return header;
    }

    //! Build a header guaranteed to fail CHeaderPoWCheck. No mining loop.
    //! Uses a separate time range from MakeValidHeader() so a valid and an
    //! invalid header can never accidentally collide on GetHash() (their
    //! nBits already differ, which alone would prevent it, but keeping the
    //! ranges apart too makes that guarantee obvious on inspection).
    CBlockHeader MakeInvalidHeader(uint32_t distinguisher) const
    {
        CBlockHeader header;
        header.nVersion = 1;
        header.hashPrevBlock = uint256::ZERO;
        header.hashMerkleRoot = uint256::ZERO;
        header.nTime = 1'800'000'000 + distinguisher;
        header.nBits = m_always_invalid_nbits;
        header.nNonce = 0;
        return header;
    }

    std::vector<CBlockHeader> MakeValidHeaders(size_t count, uint32_t base = 0) const
    {
        std::vector<CBlockHeader> headers;
        headers.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            headers.push_back(MakeValidHeader(base + static_cast<uint32_t>(i)));
        }
        return headers;
    }

    //! Wrap a header vector into CHeaderPoWCheck objects. Since
    //! CHeaderPoWCheck only stores pointers into the header and params it's
    //! given, `headers` and m_consensus must both outlive every queue/
    //! control that consumes the result.
    std::vector<CHeaderPoWCheck> MakeChecks(const std::vector<CBlockHeader>& headers) const
    {
        std::vector<CHeaderPoWCheck> checks;
        checks.reserve(headers.size());
        for (const auto& header : headers) {
            checks.emplace_back(header, m_consensus);
        }
        return checks;
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(header_pow_queue_tests, HeaderPoWCheckQueueTest)

// ---------------------------------------------------------------------------
// 1. A local CCheckQueue<CHeaderPoWCheck> loses no work and reports the
//    correct aggregate result across many different Add()-batching
//    patterns, for all-valid input. Queue-plumbing analogue of
//    checkqueue_tests.cpp's test_CheckQueue_Correct_Random, using real
//    CHeaderPoWCheck work items instead of FakeCheck. Only this (single,
//    master) thread ever calls InsecureRandRange() here -- worker threads
//    only ever run CHeaderPoWCheck::operator(), never touch the RNG, so
//    this stays within test/util/random.h's documented single-thread-only
//    contract for g_insecure_rand_ctx.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(local_queue_all_valid)
{
    CCheckQueue<CHeaderPoWCheck> queue(LOCAL_QUEUE_BATCH_SIZE);
    queue.StartWorkerThreads(LOCAL_QUEUE_WORKER_THREADS);

    for (const size_t count : {size_t{0}, size_t{1}, size_t{2}, size_t{31}, size_t{32}, size_t{33}, size_t{64}, size_t{257}}) {
        const auto headers{MakeValidHeaders(count, /*base=*/static_cast<uint32_t>(count * 1000))};

        CCheckQueueControl<CHeaderPoWCheck> control(&queue);
        size_t added = 0;
        while (added < headers.size()) {
            const size_t batch = std::min<size_t>(headers.size() - added, 1 + InsecureRandRange(9));
            std::vector<CHeaderPoWCheck> vAdd;
            vAdd.reserve(batch);
            for (size_t i = 0; i < batch; ++i) {
                vAdd.emplace_back(headers[added + i], m_consensus);
            }
            control.Add(std::move(vAdd));
            added += batch;
        }
        BOOST_REQUIRE(control.Wait());
    }
    queue.StopWorkerThreads();
}

// ---------------------------------------------------------------------------
// 2. A single invalid header anywhere in the batch is never silently
//    absorbed by the queue, regardless of which worker's sub-batch happened
//    to land on it (front, middle, back, or a boundary position).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(local_queue_detects_invalid_header_anywhere)
{
    CCheckQueue<CHeaderPoWCheck> queue(LOCAL_QUEUE_BATCH_SIZE);
    queue.StartWorkerThreads(LOCAL_QUEUE_WORKER_THREADS);
    constexpr size_t kBatch = 200;

    for (const size_t bad_pos : {size_t{0}, size_t{1}, kBatch / 2, kBatch - 2, kBatch - 1}) {
        auto headers{MakeValidHeaders(kBatch, /*base=*/static_cast<uint32_t>(bad_pos * 10000))};
        headers[bad_pos] = MakeInvalidHeader(static_cast<uint32_t>(bad_pos));

        CCheckQueueControl<CHeaderPoWCheck> control(&queue);
        control.Add(MakeChecks(headers));
        BOOST_REQUIRE(!control.Wait());
    }
    queue.StopWorkerThreads();
}

// ---------------------------------------------------------------------------
// 3. A failing batch never poisons the queue for the next, independent
//    CCheckQueueControl on the same shared queue (fAllOk must be reset).
//    Same intent as checkqueue_tests.cpp's
//    test_CheckQueue_Recovers_From_Failure, with real CHeaderPoWCheck items.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(local_queue_recovers_between_batches)
{
    CCheckQueue<CHeaderPoWCheck> queue(LOCAL_QUEUE_BATCH_SIZE);
    queue.StartWorkerThreads(LOCAL_QUEUE_WORKER_THREADS);

    for (int round = 0; round < 10; ++round) {
        for (const bool inject_failure : {true, false}) {
            auto headers{MakeValidHeaders(50, /*base=*/static_cast<uint32_t>(round * 100))};
            if (inject_failure) {
                headers[25] = MakeInvalidHeader(static_cast<uint32_t>(round));
            }

            CCheckQueueControl<CHeaderPoWCheck> control(&queue);
            control.Add(MakeChecks(headers));
            const bool ok = control.Wait();
            BOOST_REQUIRE_EQUAL(ok, !inject_failure);
        }
    }
    queue.StopWorkerThreads();
}

// ---------------------------------------------------------------------------
// 4. With zero worker threads ever started (StartWorkerThreads() simply
//    never called), the calling thread must still process the whole batch
//    itself via CCheckQueue::Loop()'s master-thread path and produce the
//    correct result -- the same path production relies on for small
//    machines (see StartHeaderPoWCheckWorkerThreads()'s call site in
//    init.cpp, skipped outright when header_pow_threads == 0).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(local_queue_zero_workers_master_processes_all)
{
    CCheckQueue<CHeaderPoWCheck> queue(LOCAL_QUEUE_BATCH_SIZE);
    BOOST_REQUIRE(!queue.HasThreads());

    auto headers{MakeValidHeaders(100)};
    {
        CCheckQueueControl<CHeaderPoWCheck> control(&queue);
        control.Add(MakeChecks(headers));
        BOOST_REQUIRE(control.Wait());
    }

    headers[50] = MakeInvalidHeader(999);
    {
        CCheckQueueControl<CHeaderPoWCheck> control(&queue);
        control.Add(MakeChecks(headers));
        BOOST_REQUIRE(!control.Wait());
    }
    // No StopWorkerThreads() call: none were started, so
    // ~CCheckQueue()'s assert(m_worker_threads.empty()) holds trivially.
}

// ---------------------------------------------------------------------------
// 5. Several threads drive independent CCheckQueueControl instances against
//    the SAME shared local queue concurrently. CCheckQueueControl's
//    m_control_mutex is supposed to serialize them; this proves that under
//    genuine concurrent use with real work items, one thread's failing
//    batch never bleeds into another thread's result, nothing hangs (join()
//    below would simply never return if it did), and nothing crashes.
//    All headers are built up front on this (single) test thread before any
//    std::thread is spawned, and never touched again from inside a worker
//    lambda: MakeValidHeader()'s nonce search is the only place that could
//    otherwise reach for the RNG, and test/util/random.h is explicit that
//    g_insecure_rand_ctx is not thread-safe.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(local_queue_concurrent_controls_no_cross_contamination)
{
    CCheckQueue<CHeaderPoWCheck> queue(LOCAL_QUEUE_BATCH_SIZE);
    queue.StartWorkerThreads(LOCAL_QUEUE_WORKER_THREADS);
    constexpr int kThreads = 6;

    std::vector<std::vector<CBlockHeader>> per_thread_headers;
    std::vector<bool> expected_ok(kThreads);
    per_thread_headers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        auto headers{MakeValidHeaders(80, /*base=*/static_cast<uint32_t>(t * 1000))};
        const bool inject_failure = (t % 2 == 0);
        if (inject_failure) {
            headers[40] = MakeInvalidHeader(static_cast<uint32_t>(t));
        }
        expected_ok[t] = !inject_failure;
        per_thread_headers.push_back(std::move(headers));
    }

    std::vector<std::thread> threads;
    std::vector<bool> observed_ok(kThreads, false);
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            CCheckQueueControl<CHeaderPoWCheck> control(&queue);
            control.Add(MakeChecks(per_thread_headers[t]));
            observed_ok[t] = control.Wait();
        });
    }
    for (auto& th : threads) th.join();

    for (int t = 0; t < kThreads; ++t) {
        BOOST_CHECK_EQUAL(observed_ok[t], expected_ok[t]);
    }
    queue.StopWorkerThreads();
}

// ---------------------------------------------------------------------------
// 6. HasValidProofOfWork()'s sequential branch, strictly below
//    HEADER_POW_PARALLEL_THRESHOLD: both all-valid and one-invalid-header
//    input. Doesn't touch headerpowcheckqueue at all (see the size check at
//    the top of HasValidProofOfWork()).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_sequential_path_below_threshold)
{
    auto headers{MakeValidHeaders(HEADER_POW_PARALLEL_THRESHOLD - 1)};
    BOOST_CHECK(HasValidProofOfWork(headers, m_consensus));

    headers.back() = MakeInvalidHeader(999);
    BOOST_CHECK(!HasValidProofOfWork(headers, m_consensus));
}

// ---------------------------------------------------------------------------
// 7. The trivial edge of the sequential branch: an empty header vector.
//    std::all_of over an empty range is vacuously true.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_empty_headers)
{
    std::vector<CBlockHeader> headers;
    BOOST_CHECK(HasValidProofOfWork(headers, m_consensus));
}

// ---------------------------------------------------------------------------
// 8. HasValidProofOfWork() at exactly HEADER_POW_PARALLEL_THRESHOLD -- the
//    first size that takes the headerpowcheckqueue path. Both all-valid and
//    one-invalid-header input.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_queue_path_at_threshold)
{
    auto headers{MakeValidHeaders(HEADER_POW_PARALLEL_THRESHOLD, /*base=*/10000)};
    BOOST_CHECK(HasValidProofOfWork(headers, m_consensus));

    headers.front() = MakeInvalidHeader(1000);
    BOOST_CHECK(!HasValidProofOfWork(headers, m_consensus));
}

// ---------------------------------------------------------------------------
// 9. HasValidProofOfWork() comfortably past HEADER_POW_PARALLEL_THRESHOLD,
//    spanning several of headerpowcheckqueue's internal batches (batch size
//    64, see its construction in validation.cpp). Both all-valid and
//    one-invalid-header (injected in the middle) input.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_queue_path_spans_batches)
{
    auto headers{MakeValidHeaders(HEADER_POW_PARALLEL_THRESHOLD + 300, /*base=*/20000)};
    BOOST_CHECK(HasValidProofOfWork(headers, m_consensus));

    headers[headers.size() / 2] = MakeInvalidHeader(2000);
    BOOST_CHECK(!HasValidProofOfWork(headers, m_consensus));
}

// ---------------------------------------------------------------------------
// 10. The production headerpowcheckqueue (started once, by
//     ChainTestingSetup's constructor) must be safe to reuse across
//     repeated, sequential HasValidProofOfWork() calls, mixing valid and
//     invalid batches, the way real header-sync batches would arrive from a
//     single peer over time.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_reusable_across_calls)
{
    for (int round = 0; round < 5; ++round) {
        auto headers{MakeValidHeaders(HEADER_POW_PARALLEL_THRESHOLD + 10, /*base=*/static_cast<uint32_t>(round * 1000))};
        BOOST_CHECK(HasValidProofOfWork(headers, m_consensus));

        headers[5] = MakeInvalidHeader(static_cast<uint32_t>(round));
        BOOST_CHECK(!HasValidProofOfWork(headers, m_consensus));
    }
}

// ---------------------------------------------------------------------------
// 11. Multiple threads call the real HasValidProofOfWork() (and therefore
//     the same production headerpowcheckqueue) concurrently, each above
//     HEADER_POW_PARALLEL_THRESHOLD to force the queue path. Closest
//     analogue to concurrent peers during headers presync (see
//     net_processing.cpp's CheckHeadersPoW()). Verifies no hang, no crash,
//     and no cross-thread contamination of results. As in test 5, all
//     headers are built up front, before any thread is spawned.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_concurrent_calls_no_cross_contamination)
{
    constexpr int kThreads = 6;
    const size_t per_thread_count = HEADER_POW_PARALLEL_THRESHOLD + 50;

    std::vector<std::vector<CBlockHeader>> per_thread_headers;
    std::vector<bool> expected_ok(kThreads);
    per_thread_headers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        auto headers{MakeValidHeaders(per_thread_count, /*base=*/static_cast<uint32_t>(t * 100000))};
        const bool inject_failure = (t % 2 == 0);
        if (inject_failure) {
            headers[per_thread_count / 2] = MakeInvalidHeader(static_cast<uint32_t>(t));
        }
        expected_ok[t] = !inject_failure;
        per_thread_headers.push_back(std::move(headers));
    }

    std::vector<std::thread> threads;
    std::vector<bool> observed_ok(kThreads, false);
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            observed_ok[t] = HasValidProofOfWork(per_thread_headers[t], m_consensus);
        });
    }
    for (auto& th : threads) th.join();

    for (int t = 0; t < kThreads; ++t) {
        BOOST_CHECK_EQUAL(observed_ok[t], expected_ok[t]);
    }
}

BOOST_AUTO_TEST_SUITE_END()
