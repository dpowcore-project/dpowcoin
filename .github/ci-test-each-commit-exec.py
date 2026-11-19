#!/usr/bin/env python3
# Copyright (c) The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

import re
import subprocess
import sys
import shlex


def run(cmd, **kwargs):
    print("+ " + shlex.join(cmd), flush=True)
    kwargs.setdefault("check", True)
    try:
        return subprocess.run(cmd, **kwargs)
    except Exception as e:
        sys.exit(str(e))


def cmake_supports_non_target_directive():
    """CMake >= 4.5 knows the `non-target-directive` warning category.

    On older CMake the flag -Wno-error=non-target-directive is itself a fatal
    error ("The warning category ... is not known"), so it must be added
    only when the running CMake is new enough.
    """
    out = subprocess.run(["cmake", "--version"], capture_output=True, text=True, check=True).stdout
    m = re.search(r"cmake version (\d+)\.(\d+)", out)
    return bool(m) and (int(m.group(1)), int(m.group(2))) >= (4, 5)


def main():
    print("Running tests on commit ...")
    run(["git", "log", "-1"])

    num_procs = int(run(["nproc"], stdout=subprocess.PIPE).stdout)
    build_dir = "ci_build"

    # Must come AFTER -Werror=dev, otherwise -Werror=dev overrides it.
    extra_warning_flags = ["-Wno-error=non-target-directive"] if cmake_supports_non_target_directive() else []

    run([
        "cmake",
        "-B",
        build_dir,
        "-Werror=dev",
        *extra_warning_flags,
        # Use clang++, because it is a bit faster and uses less memory than g++
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_CXX_COMPILER=clang++",
        # Use mold, because it is faster than the default linker
        "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=mold",
        # Use Debug build type for more debug checks, but enable optimizations
        "-DAPPEND_CXXFLAGS='-O3 -g2'",
        "-DAPPEND_CFLAGS='-O3 -g2'",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_COMPILE_WARNING_AS_ERROR=ON",
        "--preset=dev-mode",
        # Tolerate unused member functions in intermediate commits in a pull request
        "-DCMAKE_CXX_FLAGS=-Wno-error=unused-member-function",
    ])

    if run(["cmake", "--build", build_dir, "-j", str(num_procs)], check=False).returncode != 0:
        print("Build failure. Verbose build follows.")
        run(["cmake", "--build", build_dir, "-j1", "--verbose"])

    run([
        "ctest",
        "--output-on-failure",
        "--stop-on-failure",
        "--test-dir",
        build_dir,
        "-j",
        str(num_procs),
    ])
    run([
        sys.executable,
        f"./{build_dir}/test/functional/test_runner.py",
        "-j",
        str(num_procs * 2),
        "--failfast",
        "--combinedlogslen=99999999",
    ])


if __name__ == "__main__":
    main()
