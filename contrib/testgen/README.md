### TestGen ###

Utilities to generate test vectors for the data-driven Dpowcoin tests.

To use inside a scripted-diff (or just execute directly):

    ./gen_key_io_test_vectors.py valid 70 > ../../src/test/data/key_io_valid.json
    ./gen_key_io_test_vectors.py invalid 70 > ../../src/test/data/key_io_invalid.json


### gen_validateaddress_vectors.py ###
Regenerates INVALID_DATA and VALID_DATA test vectors in
test/functional/rpc_validateaddress.py for an arbitrary bech32 HRP.

    # Print generated blocks to stdout (inspect before applying):
    chmod +x gen_validateaddress_vectors.py
    ./gen_validateaddress_vectors.py
    ./gen_validateaddress_vectors.py --hrp dpc

    # Patch rpc_validateaddress.py in-place (recommended):
    chmod +x gen_validateaddress_vectors.py
    ./gen_validateaddress_vectors.py --apply
    ./gen_validateaddress_vectors.py --hrp dpc --apply

### regenerate_test_data.py ###
Regenerates dpowcoin-tx utility test data (.hex / .json).
Requires a built binary (cmake --build build -j$(nproc)).

    cd contrib/testgen
    python3 regenerate_test_data.py

### gen_kernel_block_data.py ###
Regenerates src/test/kernel/block_data.h - 206 serialized regtest blocks
used by the kernel library unit tests in src/test/kernel/test_kernel.cpp.
Requires built dpowcoind and dpowcoin-cli binaries (cmake --build build -j$(nproc)).

    python3 contrib/testgen/gen_kernel_block_data.py

The script starts a temporary regtest node, mines 206 blocks using the same
address sequence and mocktime as _initialize_chain() in the functional test
framework, exports the raw block data, and writes the header directly to
src/test/kernel/block_data.h. The temporary datadir is cleaned up on exit.
