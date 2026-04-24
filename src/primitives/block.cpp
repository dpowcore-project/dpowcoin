// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>

/* yespower algo */

#include <crypto/yespower-1.0.1/yespower.h>
#include <streams.h>
#include <uint256.h>
#include <cstdlib>
#include <cstring>
#include <sync.h>

/* argon2id algo */

#include <crypto/sha512.h>
#include <crypto/argon2d/argon2.h>
#include <crypto/argon2d/blake2/blake2.h>

namespace {
// Consensus-safe sentinel: any value > powLimit. Returned on crypto failure
// so CheckProofOfWork() naturally rejects the header instead of the process
// calling exit()/abort() (CVE-class remote-DoS vector). See SECURITY-FIXES C2.
uint256 PoWHashFailureSentinel()
{
    uint256 s;
    std::memset(s.begin(), 0xff, 32);
    return s;
}
} // namespace

uint256 CBlockHeader::GetHash() const
{
    return (HashWriter{} << *this).GetHash();
}

/* Yespower */
uint256 CBlockHeader::GetYespowerPoWHash() const
{
    static const yespower_params_t yespower_1_0_dpowcoin = {
        .version = YESPOWER_1_0,
        .N = 2048,
        .r = 8,
        .pers = (const uint8_t *)"One POW? Why not two? 17/04/2024",
        .perslen = 32
    };
    uint256 hash;
    DataStream ss{};
    ss << *this;
    if (yespower_tls((const uint8_t *)ss.data(), ss.size(), &yespower_1_0_dpowcoin, (yespower_binary_t *)&hash)) {
        // OOM or yespower internal failure. Must not crash consensus code:
        // return a sentinel that will always fail CheckProofOfWork(). See
        // SECURITY-FIXES.md C2.
        tfm::format(std::cerr, "Warning: CBlockHeader::GetYespowerPoWHash(): failed to compute PoW hash (out of memory?) — rejecting header\n");
        return PoWHashFailureSentinel();
    }
    return hash;
}

// CBlockHeader::GetArgon2idPoWHash() instance
// -> Serialize Block Header using CDataStream
// -> Compute SHA-512 hash of serialized data (Two Rounds)
// -> Use the computed hash as the salt for argon2id_hash_raw function for the first round
// -> Call argon2id_hash_raw function for the first round using the serialized data as password and SHA-512 hash as salt
// -> Use the hash obtained from the first round as the salt for the second round
// -> Call argon2id_hash_raw function for the second round using the serialized data as password and the hash from the first round as salt
// -> Return the hash computed in the second round (hash2)

uint256 CBlockHeader::GetArgon2idPoWHash() const
{
    uint256 hash;
    uint256 hash2;
    DataStream ss{};
    ss << *this;

    // Hashing the data using SHA-512 (two rounds)
    std::vector<unsigned char> salt_sha512(CSHA512::OUTPUT_SIZE);
    CSHA512 sha512;
    sha512.Write((unsigned char*)ss.data(), ss.size()).Finalize(salt_sha512.data());
    sha512.Reset().Write(salt_sha512.data(), salt_sha512.size()).Finalize(salt_sha512.data());

    // Preparing data for hashing
    const void* pwd = ss.data();
    size_t pwdlen = ss.size();
    const void* salt = salt_sha512.data();
    size_t saltlen = salt_sha512.size();

    // Calling the argon2id_hash_raw function for the first round
    int rc = argon2id_hash_raw(2, 4096, 2, pwd, pwdlen, salt, saltlen, &hash, 32);
    if (rc != ARGON2_OK) {
        // Consensus-safe: see SECURITY-FIXES.md C2. Never exit() here.
        tfm::format(std::cerr, "Warning: Failed to compute Argon2id hash for the first round (rc=%d) — rejecting header\n", rc);
        return PoWHashFailureSentinel();
    }

    // Using the hash from the first round as the salt for the second round
    salt = &hash;
    saltlen = 32;

    // Calling the argon2id_hash_raw function for the second round
    rc = argon2id_hash_raw(2, 32768, 2, pwd, pwdlen, salt, saltlen, &hash2, 32);
    if (rc != ARGON2_OK) {
        tfm::format(std::cerr, "Warning: Failed to compute Argon2id hash for the second round (rc=%d) — rejecting header\n", rc);
        return PoWHashFailureSentinel();
    }

    // Return the result of the second round of Argon2id
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
