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

# ---- dpowcoin start ----
# Dual PoW vendored crypto: yespower (PoW1) + argon2d/argon2id (PoW2).
#
# Built as a separate C static library so we can:
#   * keep aggressive optimization flags scoped to this lib (-O2, -funroll-loops,
#     -fomit-frame-pointer) without leaking into the rest of bitcoin_crypto;
#   * silence vendored-code warnings without polluting the upstream tree;
#   * pick the architecture-appropriate argon2 implementation
#     (opt.c on x86/x86_64, ref.c elsewhere) at configure time.
#
# Linked into bitcoin_consensus from src/CMakeLists.txt so primitives/block.cpp
# (GetYespowerPoWHash / GetArgon2idPoWHash) resolves the symbols.
enable_language(C)

set(_dpow_argon2_sources
  argon2d/argon2.c
  argon2d/core.c
  argon2d/encoding.c
  argon2d/thread.c
  argon2d/blake2/blake2b.c
)

# Pick architecture-specific Argon2 fill_segment implementation.
# opt.c guards itself with #if defined(__x86_64__) || defined(__amd64__)
# || defined(__i386__); on other arches it compiles to nothing, so on those
# arches we must use ref.c instead.
include(CheckCSourceCompiles)
check_c_source_compiles("
  #if !(defined(__x86_64__) || defined(__amd64__) || defined(__i386__))
  #error not x86
  #endif
  int main(void){return 0;}
" DPOW_ARGON2_X86)

if(DPOW_ARGON2_X86)
  list(APPEND _dpow_argon2_sources argon2d/opt.c)
else()
  list(APPEND _dpow_argon2_sources argon2d/ref.c)
endif()

add_library(dpowcoin_crypto_dualpow STATIC EXCLUDE_FROM_ALL
  yespower-1.0.1/yespower-opt.c
  yespower-1.0.1/sha256.c
  ${_dpow_argon2_sources}
)

target_include_directories(dpowcoin_crypto_dualpow
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dpowcoin_crypto_dualpow
  PRIVATE
    core_interface
)

# yespower-opt.c #include's yespower-platform.c (single TU build) and uses
# pthread TLS via yespower_tls() — needs threads on every platform.
find_package(Threads REQUIRED)
target_link_libraries(dpowcoin_crypto_dualpow PUBLIC Threads::Threads)

# Argon2 reference build assumes pthreads available; ARGON2_NO_THREADS=0 is
# the default but we set it explicitly so the intent is clear in the build log.
target_compile_definitions(dpowcoin_crypto_dualpow PRIVATE ARGON2_NO_THREADS=0)

# Performance-critical inner loop. Vendored upstream Makefile uses these flags;
# we mirror them so hashing throughput matches the v26.2 binary.
target_compile_options(dpowcoin_crypto_dualpow PRIVATE
  -O2
  -funroll-loops
  -fomit-frame-pointer
)

# Vendored libraries trip several upstream-clean -W flags. Silence locally so
# the rebase build stays warning-free without touching vendored files.
include(CheckCCompilerFlag)
foreach(_flag
    -Wno-unused-function
    -Wno-unused-but-set-variable
    -Wno-unused-variable
    -Wno-implicit-fallthrough
    -Wno-cast-qual
    -Wno-pointer-sign
    -Wno-redundant-decls
    -Wno-type-limits
    -Wno-cpp
)
  string(MAKE_C_IDENTIFIER "HAS${_flag}" _flag_var)
  check_c_compiler_flag(${_flag} ${_flag_var})
  if(${_flag_var})
    target_compile_options(dpowcoin_crypto_dualpow PRIVATE ${_flag})
  endif()
endforeach()
# ---- dpowcoin end ----
