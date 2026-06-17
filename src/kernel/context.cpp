// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/context.h>

#include <crypto/sha256.h>
#include <crypto/argon2d/argon2.h>
#include <key.h>
#include <logging.h>
#include <pubkey.h>
#include <random.h>

#include <string>


namespace kernel {
Context* g_context;

Context::Context()
{
    assert(!g_context);
    g_context = this;
    std::string sha256_algo = SHA256AutoDetect();
    LogPrintf("Using the '%s' SHA256 implementation\n", sha256_algo);
    std::string argon2_algo = Argon2AutoDetect();
    LogPrintf("Using the '%s' Argon2id implementation\n", argon2_algo);
    RandomInit();
    ECC_Start();
}

Context::~Context()
{
    ECC_Stop();
    assert(g_context);
    g_context = nullptr;
}

} // namespace kernel
