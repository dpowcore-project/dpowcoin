// Copyright (c) 2022-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/context.h>

/* Dpowcoin Params */
#include <crypto/argon2d/argon2.h>
/* Dpowcoin Params */
#include <crypto/sha256.h>
#include <random.h>
#include <util/log.h>

#include <mutex>
#include <string>

namespace kernel {
Context::Context()
{
    static std::once_flag globals_initialized{};
    std::call_once(globals_initialized, []() {
        std::string sha256_algo = SHA256AutoDetect();
        LogInfo("Using the '%s' SHA256 implementation\n", sha256_algo);
        /* Dpowcoin Params */
        // Dpowcoin: detect best SIMD implementation for Argon2id PoW hashing
        std::string argon2_algo = Argon2AutoDetect();
        LogInfo("Using the '%s' Argon2id implementation\n", argon2_algo);
        /* Dpowcoin Params */
        RandomInit();
    });
}


} // namespace kernel
