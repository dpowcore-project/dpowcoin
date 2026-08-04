// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Copyright (c) 2021-2026 The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>
/* Dpowcoin Params */
#include <crypto/argon2d/argon2.h>
/* Dpowcoin Params */
#include <hash.h>
/* Dpowcoin Params */
#include <streams.h>
/* Dpowcoin Params */
#include <crypto/argon2d/argon2_types.h>
/* Dpowcoin Params */
#include <crypto/sha512.h>
#include <tinyformat.h>

#include <cassert>
#include <memory>
#include <span>
#include <sstream>

uint256 CBlockHeader::GetHash() const
{
    return (HashWriter{} << *this).GetHash();
}

/* Dpowcoin Params */
/*
 * Compute the two-round Argon2id proof-of-work hash of this block header.
 *
 * Salt derivation
 * ---------------
 * The 64-byte salt fed into round 1 is obtained by applying SHA-512 twice
 * to the serialized header:
 *   salt = SHA-512( SHA-512( header ) )
 *
 * Round 1  (consensus-critical parameters, must not be changed)
 * -------
 *   password    = serialized 80-byte block header
 *   salt        = SHA-512 x2 (header)  [64 bytes]
 *   t_cost      = 2
 *   m_cost      = 4096 KiB
 *   lanes = 2
 *   output      = 32 bytes  > used as salt for round 2
 *
 * Round 2  (consensus-critical parameters, must not be changed)
 * -------
 *   password    = serialized 80-byte block header  (same as round 1)
 *   salt        = output of round 1  [32 bytes]
 *   t_cost      = 2
 *   m_cost      = 32768 KiB
 *   lanes = 2
 *   output      = 32 bytes  > final PoW hash
 */
uint256 CBlockHeader::GetArgon2idPoWHash() const
{
    uint256 hash;
    uint256 hash2;
    DataStream ss{};
    ss << *this;
    assert(ss.size() == 80);

    // Hashing the data using SHA-512 (two rounds)
    std::vector<unsigned char> salt_sha512(CSHA512::OUTPUT_SIZE);
    CSHA512 sha512;
    sha512.Write((unsigned char*)ss.data(), ss.size()).Finalize(salt_sha512.data());
    sha512.Reset().Write(salt_sha512.data(), salt_sha512.size()).Finalize(salt_sha512.data());

    // Preparing data for hashing
    uint8_t* const pwd    = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(ss.data()));
    const uint32_t pwdlen = static_cast<uint32_t>(ss.size());  // 80

    // Round 1: t=2, m=4096 KiB, lanes=2; salt = SHA-512 x2 (header).
    {
        argon2_context ctx;
        ctx.out     = hash.begin();
        ctx.outlen  = 32;
        ctx.pwd     = pwd;
        ctx.pwdlen  = pwdlen;
        ctx.salt    = salt_sha512.data();
        ctx.saltlen = static_cast<uint32_t>(salt_sha512.size());  // 64
        ctx.secret  = nullptr; ctx.secretlen = 0;
        ctx.ad      = nullptr; ctx.adlen     = 0;
        ctx.allocate_cbk = nullptr;
        ctx.free_cbk     = nullptr;
        ctx.flags   = ARGON2_DEFAULT_FLAGS;
        ctx.t_cost  = 2;
        ctx.m_cost  = 4096;
        ctx.lanes   = 2;
        ctx.threads = 1;
        ctx.version = ARGON2_VERSION_NUMBER;
        const int rc = argon2_ctx(&ctx, Argon2_id);
        assert(rc == ARGON2_OK);
    }

    // Round 2: t=2, m=32768 KiB, lanes=2; salt = output of round 1.
    {
        argon2_context ctx;
        ctx.out     = hash2.begin();
        ctx.outlen  = 32;
        ctx.pwd     = pwd;
        ctx.pwdlen  = pwdlen;
        ctx.salt    = hash.begin();
        ctx.saltlen = 32;
        ctx.secret  = nullptr; ctx.secretlen = 0;
        ctx.ad      = nullptr; ctx.adlen     = 0;
        ctx.allocate_cbk = nullptr;
        ctx.free_cbk     = nullptr;
        ctx.flags   = ARGON2_DEFAULT_FLAGS;
        ctx.t_cost  = 2;
        ctx.m_cost  = 32768;
        ctx.lanes   = 2;
        ctx.threads = 1;
        ctx.version = ARGON2_VERSION_NUMBER;
        const int rc = argon2_ctx(&ctx, Argon2_id);
        assert(rc == ARGON2_OK);
    }

    return hash2;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
