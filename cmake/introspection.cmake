# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(CheckCXXSourceCompiles)
include(CheckCXXSymbolExists)

check_cxx_symbol_exists(O_CLOEXEC "fcntl.h" HAVE_O_CLOEXEC)
check_cxx_symbol_exists(fdatasync "unistd.h" HAVE_FDATASYNC)
check_cxx_symbol_exists(fork "unistd.h" HAVE_DECL_FORK)
check_cxx_symbol_exists(pipe2 "unistd.h" HAVE_DECL_PIPE2)
check_cxx_symbol_exists(setsid "unistd.h" HAVE_DECL_SETSID)

if(NOT WIN32)
  include(TestAppendRequiredLibraries)
  test_append_socket_library(core_interface)
endif()

include(TestAppendRequiredLibraries)
test_append_atomic_library(core_interface)

# Even though ::system is part of the standard library, we still check
# for it, to support building targets that don't have it, such as iOS.
check_cxx_symbol_exists(std::system "cstdlib" HAVE_STD_SYSTEM)
check_cxx_symbol_exists(::_wsystem "stdlib.h" HAVE__WSYSTEM)
if(HAVE_STD_SYSTEM OR HAVE__WSYSTEM)
  set(HAVE_SYSTEM 1)
endif()

check_cxx_source_compiles("
  #include <string.h>

  int main()
  {
    char buf[100];
    char* p{strerror_r(0, buf, sizeof buf)};
    (void)p;
  }
  " STRERROR_R_CHAR_P
)

# Check for malloc_info (for memory statistics information in getmemoryinfo).
check_cxx_symbol_exists(malloc_info "malloc.h" HAVE_MALLOC_INFO)

# Check for mallopt(M_ARENA_MAX) (to set glibc arenas).
check_cxx_source_compiles("
  #include <malloc.h>

  int main()
  {
    mallopt(M_ARENA_MAX, 1);
  }
  " HAVE_MALLOPT_ARENA_MAX
)

# Check for posix_fallocate().
check_cxx_source_compiles("
  #include <fcntl.h>

  int main()
  {
    return posix_fallocate(0, 0, 0);
  }
  " HAVE_POSIX_FALLOCATE
)

# Check for strong getauxval() support in the system headers.
check_cxx_source_compiles("
  #include <sys/auxv.h>

  int main()
  {
    getauxval(AT_HWCAP);
  }
  " HAVE_STRONG_GETAUXVAL
)

# Check for UNIX sockets.
check_cxx_source_compiles("
  #include <sys/socket.h>
  #include <sys/un.h>

  int main()
  {
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
  }
  " HAVE_SOCKADDR_UN
)

# Check for different ways of gathering OS randomness:
# - Linux getrandom()
check_cxx_source_compiles("
  #include <sys/random.h>

  int main()
  {
    getrandom(nullptr, 32, 0);
  }
  " HAVE_GETRANDOM
)

# - BSD getentropy()
check_cxx_source_compiles("
  #include <sys/random.h>

  int main()
  {
    getentropy(nullptr, 32);
  }
  " HAVE_GETENTROPY_RAND
)


# - BSD sysctl()
check_cxx_source_compiles("
  #include <sys/types.h>
  #include <sys/sysctl.h>

  #ifdef __linux__
  #error Don't use sysctl on Linux, it's deprecated even when it works
  #endif

  int main()
  {
    sysctl(nullptr, 2, nullptr, nullptr, nullptr, 0);
  }
  " HAVE_SYSCTL
)

# - BSD sysctl(KERN_ARND)
check_cxx_source_compiles("
  #include <sys/types.h>
  #include <sys/sysctl.h>

  #ifdef __linux__
  #error Don't use sysctl on Linux, it's deprecated even when it works
  #endif

  int main()
  {
    static int name[2] = {CTL_KERN, KERN_ARND};
    sysctl(name, 2, nullptr, nullptr, nullptr, 0);
  }
  " HAVE_SYSCTL_ARND
)

if(NOT MSVC)
  include(CheckSourceCompilesWithFlags)

  # Check for SSE4.1 intrinsics.
  set(SSE41_CXXFLAGS -msse4.1)
  check_cxx_source_compiles_with_flags("
    #include <immintrin.h>

    int main()
    {
      __m128i a = _mm_set1_epi32(0);
      __m128i b = _mm_set1_epi32(1);
      __m128i r = _mm_blend_epi16(a, b, 0xFF);
      return _mm_extract_epi32(r, 3);
    }
    " HAVE_SSE41
    CXXFLAGS ${SSE41_CXXFLAGS}
  )

  # Check for AVX2 intrinsics.
  set(AVX2_CXXFLAGS -mavx -mavx2)
  check_cxx_source_compiles_with_flags("
    #include <immintrin.h>

    int main()
    {
      __m256i l = _mm256_set1_epi32(0);
      return _mm256_extract_epi32(l, 7);
    }
    " HAVE_AVX2
    CXXFLAGS ${AVX2_CXXFLAGS}
  )

  # Check for x86 SHA-NI intrinsics.
  set(X86_SHANI_CXXFLAGS -msse4 -msha)
  check_cxx_source_compiles_with_flags("
    #include <immintrin.h>

    int main()
    {
      __m128i i = _mm_set1_epi32(0);
      __m128i j = _mm_set1_epi32(1);
      __m128i k = _mm_set1_epi32(2);
      return _mm_extract_epi32(_mm_sha256rnds2_epu32(i, j, k), 0);
    }
    " HAVE_X86_SHANI
    CXXFLAGS ${X86_SHANI_CXXFLAGS}
  )

  # Check for ARMv8 SHA-NI intrinsics.
  set(ARM_SHANI_CXXFLAGS -march=armv8-a+crypto)
  check_cxx_source_compiles_with_flags("
    #include <arm_neon.h>

    int main()
    {
      uint32x4_t a, b, c;
      vsha256h2q_u32(a, b, c);
      vsha256hq_u32(a, b, c);
      vsha256su0q_u32(a, b);
      vsha256su1q_u32(a, b, c);
    }
    " HAVE_ARM_SHANI
    CXXFLAGS ${ARM_SHANI_CXXFLAGS}
  )

  # Dpowcoin Params
  # --------------------------------------------------------------------------
  # Check for ARM NEON intrinsics - portable subset (AArch64 + ARMv7+NEON).
  #
  # Probes only intrinsics available on both 64-bit and 32-bit ARM NEON:
  #   vmovn_u64, vmull_u32, vaddq_u64, veorq_u64 - widening-multiply G step.
  # vqtbl1q_u8 is intentionally NOT probed here (AArch64-only).
  #
  # Step 1: compile without extra flags.
  #   On AArch64, NEON is mandatory - arm_neon.h just works.
  #   On any other baseline-NEON target this also catches it.
  # Step 2: if step 1 fails, try -mfpu=neon (ARMv7 where NEON is optional).
  # --------------------------------------------------------------------------
  check_cxx_source_compiles("
    #include <arm_neon.h>
    #include <stdint.h>
    int main()
    {
      uint64x2_t a = vdupq_n_u64(UINT64_C(0));
      uint64x2_t b = vdupq_n_u64(UINT64_C(1));
      uint32x2_t lo = vmovn_u64(a);
      uint64x2_t ml = vmull_u32(lo, lo);
      uint64x2_t r  = veorq_u64(vaddq_u64(a, b), vaddq_u64(ml, ml));
      (void)r;
      return 0;
    }
    " HAVE_ARGON2_NEON_NATIVE
  )
  if(HAVE_ARGON2_NEON_NATIVE)
    set(HAVE_ARGON2_NEON TRUE)
    set(ARGON2_NEON_CXXFLAGS "")
  else()
    set(ARGON2_NEON_ARMV7_CXXFLAGS -mfpu=neon)
    check_cxx_source_compiles_with_flags("
      #include <arm_neon.h>
      #include <stdint.h>
      int main()
      {
        uint64x2_t a = vdupq_n_u64(UINT64_C(0));
        uint64x2_t b = vdupq_n_u64(UINT64_C(1));
        uint32x2_t lo = vmovn_u64(a);
        uint64x2_t ml = vmull_u32(lo, lo);
        uint64x2_t r  = veorq_u64(vaddq_u64(a, b), vaddq_u64(ml, ml));
        (void)r;
        return 0;
      }
      " HAVE_ARGON2_NEON_ARMV7
      CXXFLAGS ${ARGON2_NEON_ARMV7_CXXFLAGS}
    )
    if(HAVE_ARGON2_NEON_ARMV7)
      set(HAVE_ARGON2_NEON TRUE)
      set(ARGON2_NEON_CXXFLAGS ${ARGON2_NEON_ARMV7_CXXFLAGS})
    endif()
  endif()

  # Check for Argon2 SSE2 intrinsics.
  # SSE2 is mandatory on x86-64 (ABI baseline) so -msse2 is a no-op there.
  # On i686 without SSE2 this check will fail and opt_sse2.cpp is skipped;
  # opt.cpp will fall back to fill_segment_ref (pure-C reference).
  set(ARGON2_SSE2_CXXFLAGS -msse2)
  check_cxx_source_compiles_with_flags("
    #include <emmintrin.h>

    int main()
    {
      __m128i a = _mm_set1_epi32(0);
      __m128i b = _mm_set1_epi32(1);
      __m128i r = _mm_xor_si128(a, b);
      (void)r;
      return 0;
    }
    " HAVE_ARGON2_SSE2
    CXXFLAGS ${ARGON2_SSE2_CXXFLAGS}
  )

  # Check for Argon2 SSSE3 intrinsics.
  # SSSE3 was introduced in Intel Penryn (2007) and AMD Bobcat (2011).
  # On Sandy Bridge / Ivy Bridge machines (SSSE3 but no AVX2) this is the
  # fastest available tier. On Haswell+ it is superseded by AVX2.
  #
  # Key SSSE3 instructions used:
  #   _mm_shuffle_epi8  - faster rotr24 and rotr16 vs. shift+or (SSE2)
  #   _mm_alignr_epi8   - faster diagonalize vs. unpack pair (SSE2)
  set(ARGON2_SSSE3_CXXFLAGS -mssse3)
  check_cxx_source_compiles_with_flags("
    #include <tmmintrin.h>
    int main()
    {
      __m128i a = _mm_set1_epi8(0);
      /* _mm_shuffle_epi8 and _mm_alignr_epi8 are the two key SSSE3 ops */
      static const char tbl[16] = {3,4,5,6,7,0,1,2, 11,12,13,14,15,8,9,10};
      __m128i b = _mm_shuffle_epi8(a, _mm_loadu_si128((const __m128i*)tbl));
      __m128i c = _mm_alignr_epi8(a, b, 8);
      (void)c;
      return 0;
    }
    " HAVE_ARGON2_SSSE3
    CXXFLAGS ${ARGON2_SSSE3_CXXFLAGS}
  )

  # Check for Argon2 AVX2 intrinsics.
  set(ARGON2_AVX2_CXXFLAGS -mavx2)
  check_cxx_source_compiles_with_flags("
    #include <stdint.h>
    #include <immintrin.h>

    int main()
    {
      __m256i l = _mm256_set1_epi32(0);
      return 0;
    }
    " HAVE_ARGON2_AVX2
    CXXFLAGS ${ARGON2_AVX2_CXXFLAGS}
  )

  # Check for Argon2 AVX512F intrinsics.
  set(ARGON2_AVX512F_CXXFLAGS -mavx512f)
  check_cxx_source_compiles_with_flags("
    #include <stdint.h>
    #include <immintrin.h>

    int main()
    {
      __m512i v = _mm512_set1_epi32(0);
      return 0;
    }
    " HAVE_ARGON2_AVX512
    CXXFLAGS ${ARGON2_AVX512F_CXXFLAGS}
  )

endif()
