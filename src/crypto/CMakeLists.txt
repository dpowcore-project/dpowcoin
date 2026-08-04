# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

add_library(bitcoin_crypto STATIC EXCLUDE_FROM_ALL
  aes.cpp
  chacha20.cpp
  chacha20poly1305.cpp
  hex_base.cpp
  hkdf_sha256_32.cpp
  hmac_sha256.cpp
  hmac_sha512.cpp
  muhash.cpp
  poly1305.cpp
  ripemd160.cpp
  sha1.cpp
  sha256.cpp
  sha256_sse4.cpp
  sha3.cpp
  sha512.cpp
  siphash.cpp
  ../support/cleanse.cpp
  # Dpowcoin Params
  # Argon2id proof-of-work
  #   ref.cpp    - pure-C++ reference; always compiled; exports fill_segment_ref()
  #                (used as safe default on all platforms, including i686 w/o SSE2)
  #   opt.cpp    - x86 CPUID dispatcher; no SIMD code
  #   opt_sse2/avx2/avx512 - ISA variants, each compiled with their own flags
  argon2d/argon2.cpp
  argon2d/argon2_core.cpp
  argon2d/argon2_encoding.cpp
  argon2d/argon2_thread.cpp
  argon2d/argon2_ref.cpp
  argon2d/argon2_opt.cpp
  argon2d/blake2/blake2b.cpp
)

target_link_libraries(bitcoin_crypto
  PRIVATE
    core_interface
)

if(HAVE_SSE41)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_SSE41)
  target_sources(bitcoin_crypto PRIVATE sha256_sse41.cpp)
  set_property(SOURCE sha256_sse41.cpp PROPERTY
    COMPILE_OPTIONS ${SSE41_CXXFLAGS}
  )
endif()

if(HAVE_AVX2)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_AVX2)
  target_sources(bitcoin_crypto PRIVATE sha256_avx2.cpp)
  set_property(SOURCE sha256_avx2.cpp PROPERTY
    COMPILE_OPTIONS ${AVX2_CXXFLAGS}
  )
endif()

if(HAVE_SSE41 AND HAVE_X86_SHANI)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_SSE41 ENABLE_X86_SHANI)
  target_sources(bitcoin_crypto PRIVATE sha256_x86_shani.cpp)
  set_property(SOURCE sha256_x86_shani.cpp PROPERTY
    COMPILE_OPTIONS ${SSE41_CXXFLAGS} ${X86_SHANI_CXXFLAGS}
  )
endif()

if(HAVE_ARM_SHANI)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_ARM_SHANI)
  target_sources(bitcoin_crypto PRIVATE sha256_arm_shani.cpp)
  set_property(SOURCE sha256_arm_shani.cpp PROPERTY
    COMPILE_OPTIONS ${ARM_SHANI_CXXFLAGS}
  )
endif()

# Dpowcoin Params
# Argon2id SSE2 baseline (x86).
# Compiled separately so -msse2 applies only to this translation unit.
# On i686 without SSE2, HAVE_ARGON2_SSE2 is false; opt.cpp falls back to
# fill_segment_ref (pure-C++ reference from ref.cpp).
if(HAVE_ARGON2_SSE2)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_ARGON2_SSE2)
  target_sources(bitcoin_crypto PRIVATE argon2d/argon2_opt_sse2.cpp)
  set_property(SOURCE argon2d/argon2_opt_sse2.cpp PROPERTY
    COMPILE_OPTIONS ${ARGON2_SSE2_CXXFLAGS}
  )
endif()

# Argon2id SSSE3 optimised implementation (Sandy Bridge / Ivy Bridge).
# Compiled separately so -mssse3 applies only to this translation unit.
# Selected only on SSSE3 machines without AVX2 (Sandy Bridge / Ivy Bridge).
if(HAVE_ARGON2_SSSE3)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_ARGON2_SSSE3)
  target_sources(bitcoin_crypto PRIVATE argon2d/argon2_opt_ssse3.cpp)
  set_property(SOURCE argon2d/argon2_opt_ssse3.cpp PROPERTY
    COMPILE_OPTIONS ${ARGON2_SSSE3_CXXFLAGS}
  )
endif()

# Argon2id AVX2 optimised implementation.
# Compiled separately so -mavx2 applies only to this translation unit.
if(HAVE_ARGON2_AVX2)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_ARGON2_AVX2)
  target_sources(bitcoin_crypto PRIVATE argon2d/argon2_opt_avx2.cpp)
  set_property(SOURCE argon2d/argon2_opt_avx2.cpp PROPERTY
    COMPILE_OPTIONS ${ARGON2_AVX2_CXXFLAGS}
  )
endif()

# Argon2id AVX-512F optimised implementation.
# Compiled separately so -mavx512f applies only to this translation unit.
if(HAVE_ARGON2_AVX512)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_ARGON2_AVX512)
  target_sources(bitcoin_crypto PRIVATE argon2d/argon2_opt_avx512.cpp)
  set_property(SOURCE argon2d/argon2_opt_avx512.cpp PROPERTY
    COMPILE_OPTIONS ${ARGON2_AVX512F_CXXFLAGS}
  )
endif()

# Argon2id NEON optimised implementation (AArch64 and ARMv7+NEON).
# Compiled separately so NEON flags apply only to this translation unit.
# On AArch64, ARGON2_NEON_CXXFLAGS is empty - NEON is the mandatory baseline.
# On ARMv7+NEON, ARGON2_NEON_CXXFLAGS is -mfpu=neon.
# On x86 and all other architectures, HAVE_ARGON2_NEON is false and this
# block is skipped entirely; opt.cpp remains the sole dispatcher on x86.
if(HAVE_ARGON2_NEON)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_ARGON2_NEON)
  target_sources(bitcoin_crypto PRIVATE argon2d/argon2_opt_neon.cpp)
  if(ARGON2_NEON_CXXFLAGS)
    set_property(SOURCE argon2d/argon2_opt_neon.cpp PROPERTY
      COMPILE_OPTIONS ${ARGON2_NEON_CXXFLAGS}
    )
  endif()
endif()
