// Copyright (c) 2026 The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Dpowcoin Dual PoW fuzz harness.
//
// Goal: feed the fuzzer arbitrary 80-byte block-header serializations and
// invoke BOTH Dual PoW legs (Yespower and chained Argon2id) on them. Any
// crash, OOM not handled by the failure sentinel, or assertion violation
// is a real bug — the consensus path must accept arbitrary header bytes
// without UB.
//
// Cost note: Yespower(N=2048,r=8) ≈ 16 MiB per call, chained Argon2id
// (2/4096/2 -> 2/32768/2) ≈ 32 MiB peak. We therefore reject inputs that
// fail to deserialize cheaply and gate the Argon2id leg behind a small
// per-input coin flip so the fuzzer can still cover the Yespower path
// densely while only occasionally paying for the heavier Argon2id call.

#include <primitives/block.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <uint256.h>

#include <cassert>
#include <cstdint>
#include <optional>

FUZZ_TARGET(dpowcoin_pow)
{
    FuzzedDataProvider provider{buffer.data(), buffer.size()};
    const std::optional<CBlockHeader> header = ConsumeDeserializable<CBlockHeader>(provider);
    if (!header) return;

    // Yespower leg: cheap-ish, run on every input. Must be deterministic.
    const uint256 y1 = header->GetYespowerPoWHash();
    const uint256 y2 = header->GetYespowerPoWHash();
    assert(y1 == y2);

    // Argon2id leg is ~32 MiB peak; gate it to ~6% of inputs. The
    // libFuzzer corpus minimisation will still hit it many times per
    // 24h CI run while keeping the median exec/s usable.
    if (provider.remaining_bytes() > 0 && (provider.ConsumeIntegral<uint8_t>() < 16)) {
        const uint256 a1 = header->GetArgon2idPoWHash();
        const uint256 a2 = header->GetArgon2idPoWHash();
        assert(a1 == a2);
        // Independence between the two legs: must not collide for the
        // same 80-byte preimage (the two algorithms are structurally
        // different). A collision would be an extraordinary find — the
        // assert is here to surface it, not to assume it impossible.
        assert(a1 != y1);
    }
}
