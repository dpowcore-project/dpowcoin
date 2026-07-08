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
#include <version.h>
#include <stdlib.h>
#include <sync.h>

/* argon2id algo */

#include <crypto/sha512.h>
#include <crypto/argon2d/argon2.h>
#include <crypto/argon2d/blake2/blake2.h>

uint256 CBlockHeader::GetHash() const
{
    return (CHashWriter{PROTOCOL_VERSION} << *this).GetHash();
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
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    if (yespower_tls((const uint8_t *)&ss[0], ss.size(), &yespower_1_0_dpowcoin, (yespower_binary_t *)&hash)) {
        tfm::format(std::cerr, "Error: CBlockHeader::GetYespowerPoWHash(): failed to compute PoW hash (out of memory?)\n");
        exit(1);
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
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;

    // Hashing the data using SHA-512 (two rounds)
    std::vector<unsigned char> salt_sha512(CSHA512::OUTPUT_SIZE);
    CSHA512 sha512;
    sha512.Write((unsigned char*)&ss[0], ss.size()).Finalize(salt_sha512.data());
    sha512.Reset().Write(salt_sha512.data(), salt_sha512.size()).Finalize(salt_sha512.data());

    // Preparing data for hashing
    const void* pwd = &ss[0];
    size_t pwdlen = ss.size();
    const void* salt = salt_sha512.data();
    size_t saltlen = salt_sha512.size();

    // Calling the argon2id_hash_raw function for the first round
    int rc = argon2id_hash_raw(2, 4096, 2, pwd, pwdlen, salt, saltlen, &hash, 32);
    if (rc != ARGON2_OK) {
        printf("Error: Failed to compute Argon2id hash for the first round\n");
        exit(1);
    }

    // Using the hash from the first round as the salt for the second round
    salt = &hash;
    saltlen = 32;

    // Calling the argon2id_hash_raw function for the second round
    rc = argon2id_hash_raw(2, 32768, 2, pwd, pwdlen, salt, saltlen, &hash2, 32);
    if (rc != ARGON2_OK) {
        printf("Error: Failed to compute Argon2id hash for the second round\n");
        exit(1);
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
