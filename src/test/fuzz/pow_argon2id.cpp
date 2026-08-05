// Copyright (c) 2026-present The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <uint256.h>

FUZZ_TARGET(pow_argon2id)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());

    // Construct a block header from arbitrary fuzzer-provided bytes.
    CBlockHeader header;
    header.nVersion = fuzzed_data_provider.ConsumeIntegral<int32_t>();
    header.nTime    = fuzzed_data_provider.ConsumeIntegral<uint32_t>();
    header.nBits    = fuzzed_data_provider.ConsumeIntegral<uint32_t>();
    header.nNonce   = fuzzed_data_provider.ConsumeIntegral<uint32_t>();

    // Fill hashPrevBlock and hashMerkleRoot with arbitrary 32-byte values.
    const auto prev  = fuzzed_data_provider.ConsumeBytes<uint8_t>(32);
    const auto mroot = fuzzed_data_provider.ConsumeBytes<uint8_t>(32);
    if (prev.size()  == 32) std::copy(prev.begin(),  prev.end(),  header.hashPrevBlock.begin());
    if (mroot.size() == 32) std::copy(mroot.begin(), mroot.end(), header.hashMerkleRoot.begin());

    // Primary check: GetArgon2idPoWHash() must never crash or assert-fail
    // regardless of input. The internal assert(rc == ARGON2_OK) will catch
    // any failure in the Argon2id library itself.
    const uint256 hash1 = header.GetArgon2idPoWHash();

    // Determinism check: identical header must always produce identical hash.
    // A non-deterministic result would be a critical consensus bug.
    const uint256 hash2 = header.GetArgon2idPoWHash();
    Assert(hash1 == hash2);

    // Argon2id must always return a non-zero 32-byte digest.
    Assert(!hash1.IsNull());
}
