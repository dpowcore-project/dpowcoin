# Dpowcoin Genesis Blocks

This document records the genesis block parameters for every network operated
by Dpowcoin Core. All four networks share the same coinbase message and merkle
root because they share the same genesis output script — only `nTime` and
`nNonce` differ.

## Coinbase

```
pszTimestamp     = "One POW? Why not two? 17/04/2024"
genesisOutput    = 04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f
genesisReward    = 50 DPC
hashMerkleRoot   = 10f5376e5169449acf540bb89615fb337319bb5e31de16f792665bf6e3518eb3
```

The genesis output script uses the same OP_CHECKSIG-with-uncompressed-pubkey
convention as Bitcoin's mainnet genesis. The timestamp string encodes the
project's defining property: **two proof-of-work algorithms must succeed for
a header to be valid** (Yespower **AND** chained Argon2id, AND-mode).

## Mainnet

| Field | Value |
|-------|-------|
| `nTime` | `1713510000` (2024-04-19 03:40:00 UTC) |
| `nNonce` | `8808588` |
| `nBits` | `0x1f1fffff` |
| `nVersion` | `1` |
| `hashGenesisBlock` | `d86f8a0582e779830f182befeaaabc8c73a159b6b06530910758daf17ce31e36` |

The mainnet genesis nonce was specifically mined so that **both** `Yespower`
**and** `chained Argon2id` produce a hash below `nBits`. This is the only
block in the Dpowcoin chain that is allowed to skip Dual PoW validation
(see `CheckProofOfWork` short-circuit, fix C1) — its dual-mining property
is asserted at compile time via the constants above.

## Testnet (legacy testnet3-style)

| Field | Value |
|-------|-------|
| `nTime` | `1713307031` (2024-04-16 21:17:11 UTC) |
| `nNonce` | `4` |
| `nBits` | `0x207fffff` |
| `hashGenesisBlock` | `50b91b8074496181a7245d505f1e416a419b6ec9730c34dab78b3f8a277f66a9` |

## Testnet4 (BIP94)

Testnet4 is inherited from Bitcoin Core 28.x and is provided for upstream
compatibility. Dpowcoin does not actively operate a testnet4 network.

| Field | Value |
|-------|-------|
| `pszTimestamp` | `"03/May/2024 000000000000000000001ebd58c244970b3aa9d783bb001011fbe8ea8e98e00e"` |
| `hashGenesisBlock` | `00000000da84f2bafbbc53dee25a72ae507ff4914b867c565be350b0da8bf043` |
| `enforce_BIP94` | `true` (testnet4-only) |

## Signet

| Field | Value |
|-------|-------|
| `nTime` | `1713296393` (2024-04-16 18:19:53 UTC) |
| `nNonce` | `5` |
| `nBits` | `0x207fffff` |
| `hashGenesisBlock` | `8006ef13ffa802f6f71f4891f9245a391088802711e643929f882de6ff042ddd` |

## Regtest

Inherited unchanged from Bitcoin Core (so existing regtest fixtures keep
working byte-for-byte).

| Field | Value |
|-------|-------|
| `nTime` | `1296688602` |
| `nNonce` | `2` |
| `nBits` | `0x207fffff` |
| `hashGenesisBlock` | `3d96e9f00b7c9a8f9104393435b5f3fd597b5cdd95ae67d9251cfc622a575a22` |

## Verifying

After build, every genesis hash above is asserted at startup. To check the
Dual PoW property of mainnet's genesis empirically on regtest:

```bash
dpowcoind -regtest -daemon
dpowcoin-cli -regtest getyespowerpowhash 0   # ≤ powLimit
dpowcoin-cli -regtest getargon2idpowhash 0   # ≤ powLimit
```

Both commands return a hex hash that, interpreted as little-endian uint256,
is below the regtest `powLimit` — i.e. genesis was mined under both
algorithms.
