#!/usr/bin/env python3
"""
Converts rpc_validateaddress.py test vectors to an arbitrary HRP.

Usage:
    cd contrib/testgen

    # Print generated blocks to stdout (inspect before applying):
    python3 gen_validateaddress_vectors.py
    python3 gen_validateaddress_vectors.py --hrp dpc

    # Apply directly to rpc_validateaddress.py (replaces INVALID_DATA and VALID_DATA in-place):
    python3 gen_validateaddress_vectors.py --apply
    python3 gen_validateaddress_vectors.py --hrp dpc --apply

    The script can also be run from test/functional/ - it locates
    test_framework and rpc_validateaddress.py automatically in both cases.

────────────────────────────────────────────────────────────────────────────────
HOW IT WORKS
────────────────────────────────────────────────────────────────────────────────
A bech32/bech32m address has three parts:
    <hrp> '1' <data+checksum>

The data+checksum section encodes:
    - witness version (1 character)
    - witness program (arbitrary bytes)
    - checksum (6 characters, computed from hrp)

When the HRP changes, only the checksum changes. The witness version and program
stay identical. So conversion is simply: decode(src_hrp) -> encode(dst_hrp).

Addresses with a "wrong" HRP (tc1...) test rejection due to HRP mismatch -
they do not need conversion and are kept as-is.

error_locations are character positions in the address string that the node
considers erroneous. They are determined by the node, not by us. For addresses
that carry error_locations we therefore construct the address such that the bad
character lands at exactly the required position:

    [41] - bad checksum: take a valid dst_hrp p2wpkh address, corrupt char at 41
    [40] - mixed case:   take an UPPERCASE dst_hrp p2wpkh address, lowercase char 40
    [59] - invalid char: take a valid dst_hrp p2tr address, inject 'b' at position 59
           ('b' is absent from the bech32 charset, so the node will report that position)
────────────────────────────────────────────────────────────────────────────────
"""

import sys
import os
import re
import argparse

# Locate test_framework/segwit_addr.py regardless of where the script is invoked from.
# Candidates are tried in order:
#   1. contrib/testgen/ -> ../../test/functional/test_framework  (primary location)
#   2. test/functional/ -> ./test_framework                      (fallback if run in-place)
_here = os.path.dirname(os.path.abspath(__file__))
for _candidate in [
    os.path.join(_here, "..", "..", "test", "functional", "test_framework"),
    os.path.join(_here, "test_framework"),
]:
    if os.path.isdir(_candidate):
        sys.path.insert(0, _candidate)
        break
else:
    sys.exit("ERROR: cannot locate test_framework/segwit_addr.py")

from segwit_addr import bech32_encode, bech32_decode  # noqa: E402

# Path to the target test file, resolved from either invocation location.
# Candidates mirror the same logic as test_framework above.
_TARGET_CANDIDATES = [
    os.path.join(_here, "..", "..", "test", "functional", "rpc_validateaddress.py"),
    os.path.join(_here, "rpc_validateaddress.py"),
]

DEFAULT_HRP = "dpc"

BECH32_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l"


# ── Address helpers ───────────────────────────────────────────────────────────

def reencode(addr: str, dst_hrp: str, force_upper: bool = False) -> str:
    """
    Decode a bech32/bech32m address with any HRP and re-encode it with dst_hrp.
    Witness version and program are preserved; only the checksum is recomputed.
    Returns None if the address cannot be decoded.
    """
    encoding, _hrp, data = bech32_decode(addr.lower())
    if encoding is None:
        return None
    result = bech32_encode(encoding, dst_hrp, data)
    return result.upper() if force_upper else result


def corrupt_at(addr: str, pos: int) -> str:
    """
    Replace the character at position pos with the first different character
    from the bech32 alphabet. Used to produce an address with a known-bad
    checksum at a specific position.
    """
    chars = list(addr)
    original = chars[pos]
    for c in BECH32_CHARSET:
        if c != original:
            chars[pos] = c
            break
    return "".join(chars)


def inject_invalid_char_at(addr: str, pos: int, char: str = "b") -> str:
    """
    Place a character that is not in the bech32 alphabet at position pos.
    'b' is absent from BECH32_CHARSET, so the node will report an error at
    exactly that position.
    """
    chars = list(addr)
    chars[pos] = char
    return "".join(chars)


# ── Block generator ───────────────────────────────────────────────────────────

def generate_blocks(hrp: str) -> str:
    """
    Generate INVALID_DATA and VALID_DATA Python source blocks for the given HRP.
    Returns a single string ready to be printed or patched into the target file.
    """
    lines = []
    w = lines.append  # shorthand

    # ── Special addresses with error_locations ────────────────────────────────

    # [41] "Invalid Bech32 checksum"
    # Valid p2wpkh address for dst_hrp, 43 characters long.
    # Corrupt the character at position 41; the node will locate the error there.
    valid_p2wpkh = reencode("BC1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T4", hrp)
    bad_checksum_41 = corrupt_at(valid_p2wpkh, 41)

    # [40] "Invalid character or mixed case"
    # Same p2wpkh address in UPPERCASE; lowercase the character at position 40.
    # The node treats mixed case as an error and reports the first lowercase character.
    valid_p2wpkh_uc = reencode("BC1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T4", hrp, force_upper=True)
    chars = list(valid_p2wpkh_uc)
    chars[40] = chars[40].lower()
    mixed_case_40 = "".join(chars)

    # [59] "Invalid Base 32 character"
    # Valid p2tr address for dst_hrp, 63 characters long.
    # Inject 'b' at position 59 - a character outside the bech32 alphabet.
    valid_p2tr = reencode("bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqzk5jj0", hrp)
    bad_char_59 = inject_invalid_char_at(valid_p2tr, 59)

    # ── INVALID_DATA ──────────────────────────────────────────────────────────

    w("INVALID_DATA = [")
    w("    # BIP 173")

    def row(addr, error, locs, comment=""):
        cmt = f"  {comment}" if comment else ""
        if locs == "[]":
            w("    (")
            w(f"        \"{addr}\",")
            w(f"        \"{error}\",{cmt}")
            w("        [],")
            w("    ),")
        else:
            w(f"    (\"{addr}\", \"{error}\", {locs}),{cmt}")

    # tc1 addresses test wrong HRP rejection; leave them unchanged.
    row("tc1qw508d6qejxtdg4y5r3zarvary0c5xw7kg3g4ty",
        "Invalid or unsupported Segwit (Bech32) or Base58 encoding.",
        "[]", "# Invalid hrp")

    row(bad_checksum_41, "Invalid Bech32 checksum", "[41]")

    row(reencode("BC13W508D6QEJXTDG4Y5R3ZARVARY0C5XW7KN40WF2", hrp, force_upper=True),
        "Version 1+ witness address must use Bech32m checksum", "[]")

    row(reencode("bc1rw5uspcuh", hrp),
        "Version 1+ witness address must use Bech32m checksum",
        "[]", "# Invalid program length")

    row(reencode("bc10w508d6qejxtdg4y5r3zarvary0c5xw7kw508d6qejxtdg4y5r3zarvary0c5xw7kw5rljs90", hrp),
        "Version 1+ witness address must use Bech32m checksum",
        "[]", "# Invalid program length")

    row(reencode("BC1QR508D6QEJXTDG4Y5R3ZARVARYV98GJ9P", hrp, force_upper=True),
        "Invalid Bech32 v0 address program size (16 bytes), per BIP141", "[]")

    # tc1 - wrong HRP + mixed case; leave unchanged.
    row("tc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3q0sL5k7",
        "Invalid or unsupported Segwit (Bech32) or Base58 encoding.",
        "[]", "# tb1, Mixed case")

    row(mixed_case_40, "Invalid character or mixed case",
        "[40]", f"# {hrp}1, Mixed case, not in BIP 173 test vectors")

    row(reencode("bc1zw508d6qejxtdg4y5r3zarvaryvqyzf3du", hrp),
        "Version 1+ witness address must use Bech32m checksum",
        "[]", "# Wrong padding")

    # tc1 - wrong HRP; leave unchanged.
    row("tc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3pjxtptv",
        "Invalid or unsupported Segwit (Bech32) or Base58 encoding.",
        "[]", "# tb1, Non-zero padding in 8-to-5 conversion")

    row(reencode("bc1gmk9yu", hrp), "Empty Bech32 data section", "[]")

    w("    # BIP 350")

    # tc1 - wrong HRP; leave unchanged.
    row("tc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vq5zuyut",
        "Invalid or unsupported Segwit (Bech32) or Base58 encoding.",
        "[]", "# Invalid human-readable part")

    row(reencode("bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqh2y7hd", hrp),
        "Version 1+ witness address must use Bech32m checksum",
        "[]", "# Invalid checksum (Bech32 instead of Bech32m)")

    # tc1 - wrong HRP; leave unchanged.
    row("tc1z0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqglt7rf",
        "Invalid or unsupported Segwit (Bech32) or Base58 encoding.",
        "[]", "# tb1, Invalid checksum (Bech32 instead of Bech32m)")

    row(reencode("BC1S0XLXVLHEMJA6C4DQV22UAPCTQUPFHLXM9H8Z3K2E72Q4K9HCZ7VQ54WELL", hrp, force_upper=True),
        "Version 1+ witness address must use Bech32m checksum",
        "[]", "# Invalid checksum (Bech32 instead of Bech32m)")

    row(reencode("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kemeawh", hrp),
        "Version 0 witness address must use Bech32 checksum",
        "[]", "# Invalid checksum (Bech32m instead of Bech32)")

    # tc1 - wrong HRP; leave unchanged.
    row("tc1q0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vq24jc47",
        "Invalid or unsupported Segwit (Bech32) or Base58 encoding.",
        "[]", "# tb1, Invalid checksum (Bech32m instead of Bech32)")

    row(bad_char_59, "Invalid Base 32 character",
        "[59]", "# Invalid character in checksum")

    row(reencode("BC130XLXVLHEMJA6C4DQV22UAPCTQUPFHLXM9H8Z3K2E72Q4K9HCZ7VQ7ZWS8R", hrp, force_upper=True),
        "Invalid Bech32 address witness version", "[]")

    row(reencode("bc1pw5dgrnzv", hrp),
        "Invalid Bech32 address program size (1 byte)", "[]")

    row(reencode("bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7v8n0nx0muaewav253zgeav", hrp),
        "Invalid Bech32 address program size (41 bytes)", "[]")

    row(reencode("BC1QR508D6QEJXTDG4Y5R3ZARVARYV98GJ9P", hrp, force_upper=True),
        "Invalid Bech32 v0 address program size (16 bytes), per BIP141", "[]")

    # tc1 - wrong HRP + mixed case; leave unchanged.
    row("tc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vq47Zagq",
        "Invalid or unsupported Segwit (Bech32) or Base58 encoding.",
        "[]", "# tb1, Mixed case")

    row(reencode("bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7v07qwwzcrf", hrp),
        "Invalid padding in Bech32 data section",
        "[]", "# zero padding of more than 4 bits")

    # tc1 - wrong HRP; leave unchanged.
    row("tc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vpggkg4j",
        "Invalid or unsupported Segwit (Bech32) or Base58 encoding.",
        "[]", "# tb1, Non-zero padding in 8-to-5 conversion")

    row(reencode("bc1gmk9yu", hrp), "Empty Bech32 data section", "[]")

    w("]")

    # ── VALID_DATA ────────────────────────────────────────────────────────────

    w("VALID_DATA = [")
    w("    # BIP 350")

    def valid_entry(bc_addr, spk, force_upper=False, pre_comment=None):
        if pre_comment:
            w(pre_comment)
        new = reencode(bc_addr, hrp, force_upper=force_upper)
        w("    (")
        w(f"        \"{new}\",")
        w(f"        \"{spk}\",")
        w("    ),")

    valid_entry("BC1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T4",
                "0014751e76e8199196d454941c45d1b3a323f1433bd6",
                force_upper=True)

    # The tb1 variant is commented out in the original - preserve that block.
    valid_entry("bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3",
                "00201863143c14c5166804bd19203356da136c985678cd4d27a1b8c6329604903262",
                pre_comment=(
                    "    # (\n"
                    "    #   \"tb1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3q0sl5k7\",\n"
                    "    #   \"00201863143c14c5166804bd19203356da136c985678cd4d27a1b8c6329604903262\",\n"
                    "    # ),"
                ))

    valid_entry("bc1pw508d6qejxtdg4y5r3zarvary0c5xw7kw508d6qejxtdg4y5r3zarvary0c5xw7kt5nd6y",
                "5128751e76e8199196d454941c45d1b3a323f1433bd6751e76e8199196d454941c45d1b3a323f1433bd6")

    valid_entry("BC1SW50QGDZ25J", "6002751e", force_upper=True)

    valid_entry("bc1zw508d6qejxtdg4y5r3zarvaryvaxxpcs",
                "5210751e76e8199196d454941c45d1b3a323")

    valid_entry("bc1qqqqqp399et2xygdj5xreqhjjvcmzhxw4aywxecjdzew6hylgvses5wp4dt",
                "0020000000c4a5cad46221b2a187905e5266362b99d5e91c6ce24d165dab93e86433",
                pre_comment=(
                    "    # (\n"
                    "    #   \"tb1qqqqqp399et2xygdj5xreqhjjvcmzhxw4aywxecjdzew6hylgvsesrxh6hy\",\n"
                    "    #   \"0020000000c4a5cad46221b2a187905e5266362b99d5e91c6ce24d165dab93e86433\",\n"
                    "    # ),"
                ))

    valid_entry("bc1pqqqqp399et2xygdj5xreqhjjvcmzhxw4aywxecjdzew6hylgvses7epu4h",
                "5120000000c4a5cad46221b2a187905e5266362b99d5e91c6ce24d165dab93e86433",
                pre_comment=(
                    "    # (\n"
                    "    #   \"tb1pqqqqp399et2xygdj5xreqhjjvcmzhxw4aywxecjdzew6hylgvsesf3hn0c\",\n"
                    "    #   \"5120000000c4a5cad46221b2a187905e5266362b99d5e91c6ce24d165dab93e86433\",\n"
                    "    # ),"
                ))

    valid_entry("bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqzk5jj0",
                "512079be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798")

    # PayToAnchor (P2A)
    valid_entry("bc1pfeessrawgf", "51024e73",
                pre_comment="    # PayToAnchor(P2A)")

    w("]")

    return "\n".join(lines) + "\n"


# ── In-place patch ────────────────────────────────────────────────────────────

def find_target_file() -> str:
    """Return the path to rpc_validateaddress.py, or exit with an error."""
    for path in _TARGET_CANDIDATES:
        if os.path.isfile(path):
            return os.path.abspath(path)
    sys.exit(
        "ERROR: cannot locate rpc_validateaddress.py\n"
        "Expected at: " + " or ".join(_TARGET_CANDIDATES)
    )


def apply_to_file(hrp: str) -> None:
    """
    Replace the INVALID_DATA and VALID_DATA blocks inside rpc_validateaddress.py.

    The replacement uses a regex that matches from the variable assignment line
    (e.g. 'INVALID_DATA = [') to the first ']' that appears alone on a line,
    which is the closing bracket of the top-level list.
    """
    target = find_target_file()
    new_blocks = generate_blocks(hrp)

    with open(target, "r", encoding="utf-8") as f:
        source = f.read()

    # Match each top-level list assignment from its opening line to the
    # closing ']' on its own line (non-greedy to avoid eating both blocks at once).
    pattern = re.compile(
        r"^(INVALID_DATA|VALID_DATA)\s*=\s*\[.*?^\]",
        re.MULTILINE | re.DOTALL,
    )

    matches = pattern.findall(source)
    if len(matches) != 2:
        sys.exit(
            f"ERROR: expected 2 replaceable blocks in {target}, found {len(matches)}.\n"
            "Make sure INVALID_DATA and VALID_DATA are top-level list assignments."
        )

    patched = pattern.sub("__BLOCK__", source, count=2)

    # Insert both new blocks where the placeholders are.
    # generate_blocks() returns them concatenated; split on the blank line between them.
    invalid_block, valid_block = new_blocks.split("\nVALID_DATA", 1)
    valid_block = "VALID_DATA" + valid_block

    patched = patched.replace("__BLOCK__", invalid_block.rstrip("\n"), 1)
    patched = patched.replace("__BLOCK__", valid_block.rstrip("\n"), 1)

    with open(target, "w", encoding="utf-8") as f:
        f.write(patched)

    print(f"Patched: {target}  (HRP={hrp})")


# ── Entry point ───────────────────────────────────────────────────────────────

def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate rpc_validateaddress.py test vectors for a given HRP."
    )
    parser.add_argument(
        "--hrp",
        default=DEFAULT_HRP,
        help=f"Target bech32 HRP (default: {DEFAULT_HRP})",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Patch rpc_validateaddress.py in-place instead of printing to stdout.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    if args.apply:
        apply_to_file(args.hrp)
    else:
        print(generate_blocks(args.hrp), end="")


if __name__ == "__main__":
    main()
