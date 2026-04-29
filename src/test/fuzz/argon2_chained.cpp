// Copyright (c) 2026 The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Chained Argon2id fuzz harness — exercises the exact two-round
// SHA-512(salt) -> argon2id(2,4096,2) -> argon2id(2,32768,2) sequence
// used by CBlockHeader::GetArgon2idPoWHash(), but on arbitrary input
// bytes (not constrained to a serialized header).
//
// The goal is to find inputs that:
//   * cause argon2id_hash_raw to return non-zero (we must handle it),
//   * trigger any UB inside the SHA-512 -> Argon2 chain,
//   * produce non-deterministic output for the same input.
//
// Cost: ~32 MiB peak per execution. Throttled by the fuzzer's own
// resource limits + a hard input-length cap below.

#include <crypto/sha512.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <uint256.h>

#include <crypto/argon2d/argon2.h>

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

//! Reproduce the exact GetArgon2idPoWHash() chain on an arbitrary
//! byte buffer. Returns std::nullopt on any argon2 failure, which the
//! caller must tolerate gracefully (mirrors the consensus sentinel).
[[nodiscard]] bool ChainedArgon2id(const uint8_t* pwd, size_t pwdlen, uint256& out)
{
    // Two rounds of SHA-512 over the input -> initial salt.
    std::vector<unsigned char> salt(CSHA512::OUTPUT_SIZE);
    CSHA512 sha512;
    sha512.Write(pwd, pwdlen).Finalize(salt.data());
    sha512.Reset().Write(salt.data(), salt.size()).Finalize(salt.data());

    uint256 h1;
    int rc = argon2id_hash_raw(/*t_cost=*/2, /*m_cost=*/4096, /*parallelism=*/2,
                               pwd, pwdlen,
                               salt.data(), salt.size(),
                               &h1, sizeof(h1));
    if (rc != ARGON2_OK) return false;

    rc = argon2id_hash_raw(/*t_cost=*/2, /*m_cost=*/32768, /*parallelism=*/2,
                           pwd, pwdlen,
                           h1.data(), h1.size(),
                           &out, sizeof(out));
    return rc == ARGON2_OK;
}

} // namespace

FUZZ_TARGET(argon2_chained)
{
    // Hard cap on input length: argon2id_hash_raw requires pwdlen > 0.
    // We also cap aggressively to keep median exec/s tolerable; the real
    // consensus call always uses an 80-byte serialized header.
    if (buffer.empty() || buffer.size() > 256) return;

    uint256 a;
    if (!ChainedArgon2id(buffer.data(), buffer.size(), a)) return;

    // Determinism: identical input must produce identical output.
    uint256 a2;
    const bool ok = ChainedArgon2id(buffer.data(), buffer.size(), a2);
    assert(ok);
    assert(a == a2);

    // Non-trivial output: the chain must not collapse to zero for any
    // non-empty input (would indicate a broken salt / parameter passing).
    assert(a != uint256());
}
