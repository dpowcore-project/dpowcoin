// Copyright (c) 2026-present The Bitweb Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Scope of this file: CHeaderPoWCheck's CCheckQueue plumbing and
// HasValidProofOfWork()'s sequential/parallel dispatch (validation.h/.cpp).
//
// Deliberately OUT of scope here (covered elsewhere, not re-tested):
//   - Argon2id PoW correctness itself         -> crypto_argon2id_tests.cpp
//   - CheckProofOfWork()/target math (compact
//     encoding, negative/overflow/zero target) -> pow_tests.cpp
//   - HeaderPoWCache hit/miss/eviction         -> pow_cache.h/.cpp (own tests, if any)
//   - HeadersSyncState chainwork/commitments   -> headers_sync_chainwork_tests.cpp
//
// Every header built below is either "guaranteed valid" (regtest powLimit +
// nonce search, same trick as headers_sync_chainwork_tests.cpp) or
// "guaranteed invalid" (negative-target nBits, same trick as pow_tests.cpp's
// CheckProofOfWork_test_negative_target). Neither depends on genuinely
// exercising Argon2id correctness -- they're only the vehicle to get a
// deterministic true/false result flowing through the queue and through
// HasValidProofOfWork()'s threshold branch.

#include <algorithm>
#include <arith_uint256.h>
#include <chainparams.h>
#include <checkqueue.h>
#include <consensus/params.h>
#include <pow.h>
#include <primitives/block.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <validation.h>

#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include <boost/test/unit_test.hpp>

namespace {

struct HeaderPoWCheckQueueTest : RegTestingSetup {
    const Consensus::Params& m_consensus{Params().GetConsensus()};

    //! nBits that CheckProofOfWork() rejects unconditionally, for any hash:
    //! DeriveTarget() bails out on the sign bit before ever comparing
    //! against a hash. No nonce search, no dependence on Argon2id's actual
    //! output -- deterministic and free. Same trick as pow_tests.cpp's
    //! CheckProofOfWork_test_negative_target.
    const uint32_t m_always_invalid_nbits{UintToArith256(m_consensus.powLimit).GetCompact(/*fNegative=*/true)};

    //! Build a header guaranteed to pass CHeaderPoWCheck. Regtest's powLimit
    //! (nBits 0x207fffff, see headers_sync_chainwork_tests.cpp) accepts
    //! roughly half of all hashes, so this loop is expected to run once or
    //! twice, never more than a handful of iterations.
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

    //! Wrap a header vector into CHeaderPoWCheck objects. m_consensus and
    //! `headers` must outlive every queue/control that consumes the result,
    //! since CHeaderPoWCheck only stores pointers into them.
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
//    CHeaderPoWCheck work items instead of FakeCheck. If the queue dropped a
//    check or a worker thread hung/crashed, nTodo would never reach zero and
//    Complete() below would block forever instead of returning.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(local_queue_no_losses_all_valid)
{
    // Mirrors production's batch_size (see m_header_pow_check_queue's
    // construction in ChainstateManager's constructor, validation.cpp);
    // worker count kept small and fixed, independent of
    // this machine's hardware_concurrency(), so the test is deterministic
    // about how many threads are actually racing on the queue.
    CCheckQueue<CHeaderPoWCheck> queue{/*batch_size=*/64, /*worker_threads_num=*/3};

    for (const size_t count : {size_t{0}, size_t{1}, size_t{2}, size_t{31}, size_t{32}, size_t{33}, size_t{64}, size_t{257}}) {
        const auto headers{MakeValidHeaders(count, /*base=*/static_cast<uint32_t>(count * 1000))};

        CCheckQueueControl<CHeaderPoWCheck> control(queue);
        size_t added = 0;
        while (added < headers.size()) {
            const size_t batch = std::min(headers.size() - added, size_t{1} + m_rng.randrange(9));
            std::vector<CHeaderPoWCheck> vAdd;
            vAdd.reserve(batch);
            for (size_t i = 0; i < batch; ++i) {
                vAdd.emplace_back(headers[added + i], m_consensus);
            }
            control.Add(std::move(vAdd));
            added += batch;
        }
        BOOST_REQUIRE(!control.Complete().has_value());
    }
}

// ---------------------------------------------------------------------------
// 2. A single invalid header anywhere in the batch is never silently
//    absorbed by the queue, regardless of which worker's sub-batch happened
//    to land on it (front, middle, back, or a boundary position).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(local_queue_detects_invalid_header_anywhere)
{
    CCheckQueue<CHeaderPoWCheck> queue{/*batch_size=*/64, /*worker_threads_num=*/3};
    constexpr size_t kBatch = 200;

    for (const size_t bad_pos : {size_t{0}, size_t{1}, kBatch / 2, kBatch - 2, kBatch - 1}) {
        auto headers{MakeValidHeaders(kBatch, /*base=*/static_cast<uint32_t>(bad_pos * 10000))};
        headers[bad_pos] = MakeInvalidHeader(static_cast<uint32_t>(bad_pos));

        CCheckQueueControl<CHeaderPoWCheck> control(queue);
        size_t added = 0;
        while (added < headers.size()) {
            const size_t batch = std::min(headers.size() - added, size_t{1} + m_rng.randrange(9));
            std::vector<CHeaderPoWCheck> vAdd;
            vAdd.reserve(batch);
            for (size_t i = 0; i < batch; ++i) {
                vAdd.emplace_back(headers[added + i], m_consensus);
            }
            control.Add(std::move(vAdd));
            added += batch;
        }
        BOOST_REQUIRE(control.Complete().has_value());
    }
}

// ---------------------------------------------------------------------------
// 3. A failing batch never poisons the queue for the next, independent
//    CCheckQueueControl on the same shared queue (m_result must be reset).
//    Same intent as checkqueue_tests.cpp's test_CheckQueue_Recovers_From_Failure,
//    with real CHeaderPoWCheck items.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(local_queue_recovers_between_batches)
{
    CCheckQueue<CHeaderPoWCheck> queue{/*batch_size=*/64, /*worker_threads_num=*/3};

    for (int round = 0; round < 10; ++round) {
        for (const bool inject_failure : {true, false}) {
            auto headers{MakeValidHeaders(50, /*base=*/static_cast<uint32_t>(round * 100))};
            if (inject_failure) {
                headers[25] = MakeInvalidHeader(static_cast<uint32_t>(round));
            }

            CCheckQueueControl<CHeaderPoWCheck> control(queue);
            control.Add(MakeChecks(headers));
            const bool failed = control.Complete().has_value();
            BOOST_REQUIRE_EQUAL(failed, inject_failure);
        }
    }
}

// ---------------------------------------------------------------------------
// 4. Several threads drive independent CCheckQueueControl instances against
//    the SAME shared local queue concurrently. CCheckQueueControl's
//    m_control_mutex is supposed to serialize them; this proves that under
//    genuine concurrent use with real work items, one thread's failing
//    batch never bleeds into another thread's result, nothing hangs (join()
//    below would simply never return if it did), and nothing crashes.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(concurrent_controls_do_not_cross_contaminate)
{
    CCheckQueue<CHeaderPoWCheck> queue{/*batch_size=*/64, /*worker_threads_num=*/3};
    constexpr int kThreads = 6;

    std::vector<std::thread> threads;
    std::vector<int> observed(kThreads, -1); // 1 = failure detected, 0 = success
    std::vector<int> expected(kThreads, -1);

    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        expected[t] = (t % 2 == 0) ? 1 : 0; // even threads inject a failing header
        threads.emplace_back([&, t] {
            auto headers{MakeValidHeaders(80, /*base=*/static_cast<uint32_t>(t * 1000))};
            if (expected[t] == 1) {
                headers[40] = MakeInvalidHeader(static_cast<uint32_t>(t));
            }

            CCheckQueueControl<CHeaderPoWCheck> control(queue);
            control.Add(MakeChecks(headers));
            observed[t] = control.Complete().has_value() ? 1 : 0;
        });
    }
    for (auto& th : threads) th.join();

    for (int t = 0; t < kThreads; ++t) {
        BOOST_CHECK_EQUAL(observed[t], expected[t]);
    }
}

// ---------------------------------------------------------------------------
// 5. HasValidProofOfWork()'s sequential branch, strictly below
//    HEADER_POW_PARALLEL_THRESHOLD: both all-valid and one-invalid-header
//    input.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_sequential_path_below_threshold)
{
    auto& queue{m_node.chainman->GetHeaderCheckQueue()};

    auto headers{MakeValidHeaders(HEADER_POW_PARALLEL_THRESHOLD - 1)};
    BOOST_CHECK(HasValidProofOfWork(headers, m_consensus, queue));
    headers.back() = MakeInvalidHeader(999);
    BOOST_CHECK(!HasValidProofOfWork(headers, m_consensus, queue));
}

// ---------------------------------------------------------------------------
// 6. HasValidProofOfWork() at exactly HEADER_POW_PARALLEL_THRESHOLD -- the
//    first size that takes the queue path. Both all-valid and
//    one-invalid-header input.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_queue_path_at_threshold)
{
    auto& queue{m_node.chainman->GetHeaderCheckQueue()};

    auto headers{MakeValidHeaders(HEADER_POW_PARALLEL_THRESHOLD, /*base=*/10000)};
    BOOST_CHECK(HasValidProofOfWork(headers, m_consensus, queue));
    headers.front() = MakeInvalidHeader(1000);
    BOOST_CHECK(!HasValidProofOfWork(headers, m_consensus, queue));
}

// ---------------------------------------------------------------------------
// 7. HasValidProofOfWork() comfortably past HEADER_POW_PARALLEL_THRESHOLD,
//    spanning several queue batches. Both all-valid and
//    one-invalid-header (injected in the middle) input.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_queue_path_above_threshold)
{
    auto& queue{m_node.chainman->GetHeaderCheckQueue()};

    auto headers{MakeValidHeaders(HEADER_POW_PARALLEL_THRESHOLD + 300, /*base=*/20000)};
    BOOST_CHECK(HasValidProofOfWork(headers, m_consensus, queue));
    headers[headers.size() / 2] = MakeInvalidHeader(2000);
    BOOST_CHECK(!HasValidProofOfWork(headers, m_consensus, queue));
}

// ---------------------------------------------------------------------------
// 8. The same ChainstateManager-owned queue must be safe to reuse across
//    repeated, sequential HasValidProofOfWork() calls, mixing valid and
//    invalid batches, the way real header-sync batches would arrive from a
//    single peer over time.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_queue_reusable_across_calls)
{
    auto& queue{m_node.chainman->GetHeaderCheckQueue()};

    for (int round = 0; round < 5; ++round) {
        auto headers{MakeValidHeaders(HEADER_POW_PARALLEL_THRESHOLD + 10, /*base=*/static_cast<uint32_t>(round * 1000))};
        BOOST_CHECK(HasValidProofOfWork(headers, m_consensus, queue));

        headers[5] = MakeInvalidHeader(static_cast<uint32_t>(round));
        BOOST_CHECK(!HasValidProofOfWork(headers, m_consensus, queue));
    }
}

// ---------------------------------------------------------------------------
// 9. Multiple threads call the real HasValidProofOfWork() (and therefore the
//    ChainstateManager-owned m_header_pow_check_queue) concurrently, each
//    above HEADER_POW_PARALLEL_THRESHOLD to force the queue path. This is
//    the closest thing to the production call pattern from
//    net_processing.cpp's CheckHeadersPoW() under concurrent peers. Verifies
//    no hang, no crash, and no cross-thread contamination of results.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(has_valid_pow_concurrent_calls_no_cross_contamination)
{
    constexpr int kThreads = 6;
    const size_t per_thread_count = HEADER_POW_PARALLEL_THRESHOLD + 50;

    auto& queue{m_node.chainman->GetHeaderCheckQueue()};
    std::vector<std::thread> threads;
    std::vector<int> observed(kThreads, -1); // 1 = true (valid), 0 = false (invalid)
    std::vector<int> expected(kThreads, -1);

    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        expected[t] = (t % 2 == 0) ? 0 : 1; // even threads inject a failing header
        threads.emplace_back([&, t] {
            auto headers{MakeValidHeaders(per_thread_count, /*base=*/static_cast<uint32_t>(t * 100000))};
            if (expected[t] == 0) {
                headers[per_thread_count / 2] = MakeInvalidHeader(static_cast<uint32_t>(t));
            }
            observed[t] = HasValidProofOfWork(headers, m_consensus, queue) ? 1 : 0;
        });
    }
    for (auto& th : threads) th.join();

    for (int t = 0; t < kThreads; ++t) {
        BOOST_CHECK_EQUAL(observed[t], expected[t]);
    }
}

BOOST_AUTO_TEST_SUITE_END()
