Dpowcoin Core version v31.3 Release Notes
=========================================

Dpowcoin Core version v31.3 is now available from:

  <https://github.com/dpowcore-project/dpowcoin/releases/tag/v31.3>

  <https://dpowcore.org/en/wallets/full-node/>

This release includes new features, various bug fixes and performance
improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/dpowcore-project/dpowcoin/issues>

To receive security and update notifications, please subscribe to:

  <https://github.com/dpowcore-project/dpowcoin/releases>


How to Upgrade
==============

⚠️  BACKUP YOUR WALLET BEFORE UPGRADING

1. Shut down your wallet completely and wait until it has fully closed
   (this might take a few minutes).

2. Back up your wallet.dat file before upgrading.
   Default wallet.dat locations:

     Windows : %APPDATA%\Dpowcoin\wallet\wallet.dat
     macOS   : ~/Library/Application Support/Dpowcoin/wallet/wallet.dat
     Linux   : ~/.dpowcoin/wallet/wallet.dat

   If you configured a custom data directory (-datadir), your wallet.dat
   is located at: <your-custom-datadir>/wallet/wallet.dat

3. Copy wallet.dat to a safe location (external drive, encrypted backup)
   before proceeding with the upgrade.

Then run the installer (on Windows) or just copy over
`/Applications/Dpowcoin-Qt` (on macOS) or `dpowcoind`/`dpowcoin-qt` (on Linux).


Compatibility
=============

Dpowcoin Core is supported and tested on operating systems using the
Linux Kernel 3.17+, macOS 13+, and Windows 10+.
Dpowcoin Core should also work on most other Unix-like systems but is not as
frequently tested on them.


Notable Changes
===============

### doc: update release notes for v31.2 and add history release notes

> [`4a361c5`](https://github.com/dpowcore-project/dpowcoin/commit/4a361c5fea93f6fcfc1886ca42072c6f05dce1eb) · 2026-08-06

| File | Lines |
|------|-------|
| [`doc/release-notes.md`](https://github.com/dpowcore-project/dpowcoin/commit/4a361c5fea93f6fcfc1886ca42072c6f05dce1eb#diff-e04cb1e9f29f76897c6b84334238f621ff45ba13cf180ad1d0d442d50ebe51f7R1-R2) | [+1-2, 4, 6, 59, 61, 65-94, 98, 100, 104-120, 124, 126, 130-152, 156, 158, 162-176, 180, 182, 186-201, 205, 207, 211-242, 246, 248, 252-430, 434-1044, 1046-1063, 1065-1075, 1077-1079, 1081-1133, 1135-1140, 1142-1154, 1158, 1160, 1164-1188, 1192, 1194, 1198-1200, 1204, 1206, 1210-1213, 1215, 1219, 1221, 1225-1227, 1229, 1233, 1235, 1239-1254, 1256-1290, 1294, 1296, 1300-1307, 1309, 1313, 1315, 1319, 1321-1338](https://github.com/dpowcore-project/dpowcoin/commit/4a361c5fea93f6fcfc1886ca42072c6f05dce1eb#diff-e04cb1e9f29f76897c6b84334238f621ff45ba13cf180ad1d0d442d50ebe51f7R1-R2) |
| [`doc/release-notes/release-notes-31.2.md`](https://github.com/dpowcore-project/dpowcoin/commit/4a361c5fea93f6fcfc1886ca42072c6f05dce1eb#diff-b15120e2cd1ce8541aee770fb1bfe4a78f5f5330acb2c21d11434e9f01889ab1R1-R1351) | [+1-1351](https://github.com/dpowcore-project/dpowcoin/commit/4a361c5fea93f6fcfc1886ca42072c6f05dce1eb#diff-b15120e2cd1ce8541aee770fb1bfe4a78f5f5330acb2c21d11434e9f01889ab1R1-R1351) |

*2 file(s) · ✏️ 1 modified · 🆕 1 new · +2526 / -933 lines*

---

### Backport 19 bitcoin/bitcoin PRs from official 31.x (v31.2/v31.3 prep): private broadcast, flatfile/sock/net fixes, tx INV dedup, asmap, tests

> [`a880ac5`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b) · 2026-11-18

| File | Lines |
|------|-------|
| [`CMakeLists.txt`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-1e7de1ae2d059d21e1dd75d5812d5a34b0222cef273b7c3a2af62eb747f9d20aR31) | [+31](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-1e7de1ae2d059d21e1dd75d5812d5a34b0222cef273b7c3a2af62eb747f9d20aR31) |
| [`ci/test/03_test_script.sh`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-028c14f389228331dc3d0f95a570e19f06cf17112556a164109a51609c3a4c2dR15) | [+15](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-028c14f389228331dc3d0f95a570e19f06cf17112556a164109a51609c3a4c2dR15) |
| [`cmake/introspection.cmake`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a40a3c5438390e95b298aa2f342cb2364380f3d863e34459212d9ca6f97b3e93R11-R13) | [+11-13](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a40a3c5438390e95b298aa2f342cb2364380f3d863e34459212d9ca6f97b3e93R11-R13) |
| [`contrib/init/dpowcoind.openrc`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-b1404a7bc487086274c12b4ebf7aa5f337da201463f322572e343f77608560b6R24) | [+24, 33](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-b1404a7bc487086274c12b4ebf7aa5f337da201463f322572e343f77608560b6R24) |
| [`doc/bips.md`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-c1b79495d2afaf2e9a8effb6fe0117c28a820bf99cff43552ad307d52118ec28R82) | [+82](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-c1b79495d2afaf2e9a8effb6fe0117c28a820bf99cff43552ad307d52118ec28R82) |
| [`doc/build-netbsd.md`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-7d5c6b91f526de15be9457dd7a0b22df3f5e17d01dfc326905e217c20a06b334R15) | [+15](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-7d5c6b91f526de15be9457dd7a0b22df3f5e17d01dfc326905e217c20a06b334R15) |
| [`doc/build-osx.md`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-e9ba5bd8b63e72aa2d54d02960c9209823938981691cc8786ba323b54de0659cR107) | [+107](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-e9ba5bd8b63e72aa2d54d02960c9209823938981691cc8786ba323b54de0659cR107) |
| [`doc/tor.md`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a33b1b15efbc14c65460ef7a883b530ca52144a4f06642e4a188351d959f9894R14-R18) | [+14-18, 47, 59](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a33b1b15efbc14c65460ef7a883b530ca52144a4f06642e4a188351d959f9894R14-R18) |
| [`share/examples/dpowcoin.conf`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-03b2f682f890977f25a0b878c0a26d084ef266275555b92582761c050bac424fR311) | [+311, 601-606](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-03b2f682f890977f25a0b878c0a26d084ef266275555b92582761c050bac424fR311) |
| [`src/chain.h`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-05137bf4d07f31a6cc237b1dd772e0b38bc2a60610a7ca86827e98fc126e8407R112) | [+112, 115](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-05137bf4d07f31a6cc237b1dd772e0b38bc2a60610a7ca86827e98fc126e8407R112) |
| [`src/flatfile.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-da96029cc55b65038e4e824d8ce5876dd4de8227ed0c6ee6966ce285f31fc427R48) | [+48, 62-63, 65-66](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-da96029cc55b65038e4e824d8ce5876dd4de8227ed0c6ee6966ce285f31fc427R48) |
| [`src/flatfile.h`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-b81ce3d3ee4212c2fe412334d15075fa9003370823822381198baa9f4c2eec7aR17) | [+17, 23](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-b81ce3d3ee4212c2fe412334d15075fa9003370823822381198baa9f4c2eec7aR17) |
| [`src/init.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-b1e19192258d83199d8adaa5ac31f067af98f63554bfdd679bd8e8073815e69dR596) | [+596, 693, 695, 1728, 1750-1760, 2294](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-b1e19192258d83199d8adaa5ac31f067af98f63554bfdd679bd8e8073815e69dR596) |
| [`src/kernel/chain.h`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-d3d6e652e68e3ef8083f804a2d9bf6bbe6f56d8be131ec27caa2d22aadd26308R10) | [+10, 25](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-d3d6e652e68e3ef8083f804a2d9bf6bbe6f56d8be131ec27caa2d22aadd26308R10) |
| [`src/net.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-00021eed586a482abdb09d6cdada1d90115abe988a91421851960e26658bed02R1671-R1675) | [+1671-1675, 2064-2074, 3870, 3875, 4149-4152](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-00021eed586a482abdb09d6cdada1d90115abe988a91421851960e26658bed02R1671-R1675) |
| [`src/net.h`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-422879cc8bfac56d4380c865f381b58afeb344bc355bbc7f47c581e4491b6b4bR750-R751) | [+750-751, 1367](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-422879cc8bfac56d4380c865f381b58afeb344bc355bbc7f47c581e4491b6b4bR750-R751) |
| [`src/net_processing.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-6875de769e90cec84d2e8a9c1b962cdbcda44d870d42e4215827e599e11e90e3R60) | [+60, 88, 763-764, 4168-4169, 4208-4210, 5232-5237, 5250](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-6875de769e90cec84d2e8a9c1b962cdbcda44d870d42e4215827e599e11e90e3R60) |
| [`src/netaddress.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-1ce9cdfdcb1a8474c626ebd0d6eb261d2a91d4c6ac1bbed8feb57a94c61b5701R346-R351) | [+346-351, 443](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-1ce9cdfdcb1a8474c626ebd0d6eb261d2a91d4c6ac1bbed8feb57a94c61b5701R346-R351) |
| [`src/netaddress.h`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-9281caf81406a5c4e11f572f613a4cc9769f39c66579ed75a4c957f30b39d836R165) | [+165](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-9281caf81406a5c4e11f572f613a4cc9769f39c66579ed75a4c957f30b39d836R165) |
| [`src/node/blockstorage.h`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-ed3f90693a242b38b9719af171de8f55183576957676dfa358945bea22276bd5R60-R61) | [+60-61](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-ed3f90693a242b38b9719af171de8f55183576957676dfa358945bea22276bd5R60-R61) |
| [`src/node/data/ip_asn.dat`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b) | 🔄 binary / full replacement |
| [`src/private_broadcast.h`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-3ef5e45a5e964dd0121f7f28bf609b2368db3e64370973764a12f2b73b5a21aeL211) | [-211](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-3ef5e45a5e964dd0121f7f28bf609b2368db3e64370973764a12f2b73b5a21aeL211) |
| [`src/rpc/mempool.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-9c5b83de6dc84af277e352c88b9291aa44340a3c75f572a0b51661eb0a838de9R59-R64) | [+59-64, 66](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-9c5b83de6dc84af277e352c88b9291aa44340a3c75f572a0b51661eb0a838de9R59-R64) |
| [`src/streams.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-6d04aca71dd4079e13471cd3c1699c3e55314fb04c5c6de062ffbdcc8db42024R16) | [+16, 38, 46, 134](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-6d04aca71dd4079e13471cd3c1699c3e55314fb04c5c6de062ffbdcc8db42024R16) |
| [`src/streams.h`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-4020c723bb55e114bdc7ff769086a765dcc7ccfb61da2047a315db16c0c7a8b4R447) | [+447](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-4020c723bb55e114bdc7ff769086a765dcc7ccfb61da2047a315db16c0c7a8b4R447) |
| [`src/test/denialofservice_tests.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a4c9875091483bafb92d8ccaa035dff408a7a5c44e5b4eb55ff51fb0e5112ed2R320) | [+320, 390-403, 412-430](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a4c9875091483bafb92d8ccaa035dff408a7a5c44e5b4eb55ff51fb0e5112ed2R320) |
| [`src/test/fuzz/cluster_linearize.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-19de5751410d99240288e48827f7d39f60c2a9964d3d844117c795e3c6a2d702L614) | [-614](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-19de5751410d99240288e48827f7d39f60c2a9964d3d844117c795e3c6a2d702L614) |
| [`src/test/fuzz/locale.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-f853082b5c9feaba950e18b83e8918bf70d138bf4406a95248336771b4a1fdfaL48) | [-48](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-f853082b5c9feaba950e18b83e8918bf70d138bf4406a95248336771b4a1fdfaL48) |
| [`src/test/fuzz/netaddress.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-29b48f78026724e573923833c9850c99ef3594d5c2e65a29b277076ffca485bdR58) | [+58, 74-76, 88-90](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-29b48f78026724e573923833c9850c99ef3594d5c2e65a29b277076ffca485bdR58) |
| [`src/test/fuzz/p2p_transport_serialization.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-e13d8784c65eb3b2a33c476cc7c3979aca851a0a3d37407f35f49e9bd48b8ea5L89) | [-89](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-e13d8784c65eb3b2a33c476cc7c3979aca851a0a3d37407f35f49e9bd48b8ea5L89) |
| [`src/test/kernel/test_kernel.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-f638e65d9278276b2a6ac42ffbbd26d079c3dba752feed5038a25a1ee565d320R9-R10) | [+9-10](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-f638e65d9278276b2a6ac42ffbbd26d079c3dba752feed5038a25a1ee565d320R9-R10) |
| [`src/test/main.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-2a6d01acd80c98b39ca7a5aa6ab9e0eb56b878109378fa9c1bdbc5049c2925dcR8-R9) | [+8-9](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-2a6d01acd80c98b39ca7a5aa6ab9e0eb56b878109378fa9c1bdbc5049c2925dcR8-R9) |
| [`src/test/netbase_tests.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-1d804b435c22c29ffc21022ad3a88a8b7d3bd44ae451c3d50e4700344ffef60dR62-R64) | [+62-64, 79](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-1d804b435c22c29ffc21022ad3a88a8b7d3bd44ae451c3d50e4700344ffef60dR62-R64) |
| [`src/test/rbf_tests.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-d9ce07e92ef38a0cbd88468433545d25cc124d34382f62ad8077249eef6e929fL124-L128) | [-124-128](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-d9ce07e92ef38a0cbd88468433545d25cc124d34382f62ad8077249eef6e929fL124-L128) |
| [`src/test/txgraph_tests.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a8bfda03bd9f3006fee9126c386e398eeb3d6d176881cb96708018c081bda5d1R293) | [+293](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a8bfda03bd9f3006fee9126c386e398eeb3d6d176881cb96708018c081bda5d1R293) |
| [`src/util/fs_helpers.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-c24fd5f39e56e513a81d84b494862608c1620c194e5a7efbded93953b6ef43e2R144) | [+144, 147, 181, 216](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-c24fd5f39e56e513a81d84b494862608c1620c194e5a7efbded93953b6ef43e2R144) |
| [`src/util/fs_helpers.h`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-bc3aba4e856660600a446395d094882ea164e3becc663ac57fae8f7841d4685eR46-R77) | [+46-77, 79](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-bc3aba4e856660600a446395d094882ea164e3becc663ac57fae8f7841d4685eR46-R77) |
| [`src/util/sock.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-76b083bb360dbbd34a65d8f15dfcf143acb02061f55b4f35d99ad1607016d8dbR217) | [+217, 222-224](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-76b083bb360dbbd34a65d8f15dfcf143acb02061f55b4f35d99ad1607016d8dbR217) |
| [`src/wallet/test/coinselector_tests.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-36ddaeb9e3a5c1aaaccd6b1ed6c770e8344e33dbfd4876b5f0726d84ab47cbabL172) | [-172](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-36ddaeb9e3a5c1aaaccd6b1ed6c770e8344e33dbfd4876b5f0726d84ab47cbabL172) |
| [`src/wallet/test/util.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-de599d9a8f1ba87b441fa95ddb92737eef323ec968f0a9f3c38ec5a3b374045cL69) | [-69, 93](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-de599d9a8f1ba87b441fa95ddb92737eef323ec968f0a9f3c38ec5a3b374045cL69) |
| [`src/wallet/test/wallet_tests.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-157f588a09ff1da05b8c74acfcfb4da21917b6a97ed5d2e702228d621d23e66dL701) | [-701](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-157f588a09ff1da05b8c74acfcfb4da21917b6a97ed5d2e702228d621d23e66dL701) |
| [`test/functional/feature_config_args.py`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-2babd91c181a89aaeb4a2bd610c8b6bcab0f0b4f5dd20251a3ad0e1742b56380R435) | [+435](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-2babd91c181a89aaeb4a2bd610c8b6bcab0f0b4f5dd20251a3ad0e1742b56380R435) |
| [`test/functional/feature_proxy.py`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-156fee7ba1e072ea2ff94156a03c2c8b5e4885ee2181ecff97d3c772723802fbR441-R445) | [+441-445, 452-456, 472-473, 489-497](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-156fee7ba1e072ea2ff94156a03c2c8b5e4885ee2181ecff97d3c772723802fbR441-R445) |
| [`test/functional/feature_prune_stale_fork.py`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a3ca99b0ca0c130d916075778b9707628d25d3cad59268537c6b5105a4cb3953R1-R39) | [+1-39](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-a3ca99b0ca0c130d916075778b9707628d25d3cad59268537c6b5105a4cb3953R1-R39) |
| [`test/functional/p2p_ibd_txrelay.py`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-b08a5f65b8749b135c81cca38dcb3dbcce49471a761023fc133607ae480c029eR16-R22) | [+16-22, 25-28, 57-59](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-b08a5f65b8749b135c81cca38dcb3dbcce49471a761023fc133607ae480c029eR16-R22) |
| [`test/functional/p2p_tx_download.py`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-d5657ef7092f5ab95d162966ae1e3e96e0f669d81fe4cde90caa523f8bdfa56cR19) | [+19, 322-387, 459](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-d5657ef7092f5ab95d162966ae1e3e96e0f669d81fe4cde90caa523f8bdfa56cR19) |
| [`test/functional/rpc_net.py`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-355d4500a3163e554fc3c3051b21b6bc9a97bcb35b146bff17d5fc1b4e4fd650R331) | [+331, 336-337](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-355d4500a3163e554fc3c3051b21b6bc9a97bcb35b146bff17d5fc1b4e4fd650R331) |
| [`test/functional/test_runner.py`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-437d7f6e9f2229879b60aae574a8217f14c643bbf3cfa9225d8011d6d52df00cR135) | [+135](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-437d7f6e9f2229879b60aae574a8217f14c643bbf3cfa9225d8011d6d52df00cR135) |
| [`test/sanitizer_suppressions/lsan`](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-9e2a7f9fadfcd9477fd39d4365571cbc62a82bc2c1d8523425decfe92a79229bR2-R3) | [+2-3](https://github.com/dpowcore-project/dpowcoin/commit/a880ac596e2f06a1323c838ea8a01f60c4a2da4b#diff-9e2a7f9fadfcd9477fd39d4365571cbc62a82bc2c1d8523425decfe92a79229bR2-R3) |

*49 file(s) · ✏️ 47 modified · 🆕 1 new · 🔄 1 binary · +354 / -125 lines*

---

### Permisions: set executable permissions

> [`dd6321d`](https://github.com/dpowcore-project/dpowcoin/commit/dd6321d760e1a019fd98746feb43ab4435dd26de) · 2026-11-18

| File | Lines |
|------|-------|
| [``](https://github.com/dpowcore-project/dpowcoin/commit/dd6321d760e1a019fd98746feb43ab4435dd26de) | — |

*1 file(s) · ✏️ 1 modified · +0 / -0 lines*

---

### Fix BACKPORT comments: m_blocks_unlinked guard now in 31.x, FakeNodeClock typo

> [`91a9578`](https://github.com/dpowcore-project/dpowcoin/commit/91a957858b4cbb5e7aedf5a87ea6418e5445c97c) · 2026-11-19

| File | Lines |
|------|-------|
| [`src/node/blockstorage.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/91a957858b4cbb5e7aedf5a87ea6418e5445c97c#diff-114c2880ec1ff2c5293ac65ceda0637bf92c05745b74b58410585a549464a33fR527-R529) | [+527-529](https://github.com/dpowcore-project/dpowcoin/commit/91a957858b4cbb5e7aedf5a87ea6418e5445c97c#diff-114c2880ec1ff2c5293ac65ceda0637bf92c05745b74b58410585a549464a33fR527-R529) |
| [`src/test/util/time.h`](https://github.com/dpowcore-project/dpowcoin/commit/91a957858b4cbb5e7aedf5a87ea6418e5445c97c#diff-9cfd7a9bb9f935e8e172e5c156189b38611051801a53264766ce81ed539c4e2aR40) | [+40](https://github.com/dpowcore-project/dpowcoin/commit/91a957858b4cbb5e7aedf5a87ea6418e5445c97c#diff-9cfd7a9bb9f935e8e172e5c156189b38611051801a53264766ce81ed539c4e2aR40) |

*2 file(s) · ✏️ 2 modified · +4 / -3 lines*

---

### Handle CMake 4.5 warning category in CI

> [`752ad51`](https://github.com/dpowcore-project/dpowcoin/commit/752ad51739c7db87e305ea49db1d0258147cdb29) · 2026-11-19

| File | Lines |
|------|-------|
| [`.github/ci-test-each-commit-exec.py`](https://github.com/dpowcore-project/dpowcoin/commit/752ad51739c7db87e305ea49db1d0258147cdb29#diff-3db8f8c36e425c8a0e717f585382f08c6632394e9adb3ebb48364c176d069a35R6) | [+6, 21-32, 40-42, 48](https://github.com/dpowcore-project/dpowcoin/commit/752ad51739c7db87e305ea49db1d0258147cdb29#diff-3db8f8c36e425c8a0e717f585382f08c6632394e9adb3ebb48364c176d069a35R6) |
| [`.github/ci-windows.py`](https://github.com/dpowcore-project/dpowcoin/commit/752ad51739c7db87e305ea49db1d0258147cdb29#diff-4e6c0a55a7b89305e553d45b51e79624225dfdbe82b906b29f711200717f3599R8) | [+8, 69-80, 87-91](https://github.com/dpowcore-project/dpowcoin/commit/752ad51739c7db87e305ea49db1d0258147cdb29#diff-4e6c0a55a7b89305e553d45b51e79624225dfdbe82b906b29f711200717f3599R8) |
| [`ci/test/03_test_script.sh`](https://github.com/dpowcore-project/dpowcoin/commit/752ad51739c7db87e305ea49db1d0258147cdb29#diff-028c14f389228331dc3d0f95a570e19f06cf17112556a164109a51609c3a4c2dR98-R105) | [+98-105](https://github.com/dpowcore-project/dpowcoin/commit/752ad51739c7db87e305ea49db1d0258147cdb29#diff-028c14f389228331dc3d0f95a570e19f06cf17112556a164109a51609c3a4c2dR98-R105) |

*3 file(s) · ✏️ 3 modified · +43 / -0 lines*

---

### Update chain params to height 257100

> [`14bf7ef`](https://github.com/dpowcore-project/dpowcoin/commit/14bf7ef2600529833888fd5063188c86cc61417e) · 2026-11-19

| File | Lines |
|------|-------|
| [`src/kernel/chainparams.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/14bf7ef2600529833888fd5063188c86cc61417e#diff-e20339c384d6f19b519ea2de7f4ba4fed92a36d66a80f0339b09927c3fa38d6dR120-R121) | [+120-121, 181, 193-198, 203-206](https://github.com/dpowcore-project/dpowcoin/commit/14bf7ef2600529833888fd5063188c86cc61417e#diff-e20339c384d6f19b519ea2de7f4ba4fed92a36d66a80f0339b09927c3fa38d6dR120-R121) |

*1 file(s) · ✏️ 1 modified · +13 / -6 lines*

---

### doc: update manual pages and bitweb.conf example for v31.3

> [`3ad0b0b`](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82) · 2026-11-19

| File | Lines |
|------|-------|
| [`doc/man/dpowcoin-cli.1`](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-611a53c06473c8663e3a3d32553066828f5fdb1436785d2c4dd1d9639baea8b0R2) | [+2, 4, 18](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-611a53c06473c8663e3a3d32553066828f5fdb1436785d2c4dd1d9639baea8b0R2) |
| [`doc/man/dpowcoin-qt.1`](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-be3750d66c91b7bd564c4ca8c6c5b5f22964e267e4ff160996cf3a66992ea3fbR2) | [+2, 4, 9, 35, 381, 724-729](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-be3750d66c91b7bd564c4ca8c6c5b5f22964e267e4ff160996cf3a66992ea3fbR2) |
| [`doc/man/dpowcoin-tx.1`](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-f5a4f533772bbd8912b2908dcf0e4b5eaceafef5ada9f356aa82266344ba7042R2) | [+2, 4, 12](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-f5a4f533772bbd8912b2908dcf0e4b5eaceafef5ada9f356aa82266344ba7042R2) |
| [`doc/man/dpowcoin-util.1`](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-52613e43a897de4780006687521c41e4362a99b8db814ad4e32364a5b3d92691R2) | [+2, 4, 12](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-52613e43a897de4780006687521c41e4362a99b8db814ad4e32364a5b3d92691R2) |
| [`doc/man/dpowcoin-wallet.1`](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-3f6570e0284c18f1a5a21ecd91be310b44c382c55a843eec10dda7ac3174850eR2) | [+2, 4, 9](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-3f6570e0284c18f1a5a21ecd91be310b44c382c55a843eec10dda7ac3174850eR2) |
| [`doc/man/dpowcoin.1`](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-dd45c68282256599d4aa43eed051b2983cde0d4b898288036ac9b7beddc0d71fR2) | [+2, 4](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-dd45c68282256599d4aa43eed051b2983cde0d4b898288036ac9b7beddc0d71fR2) |
| [`doc/man/dpowcoind.1`](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-c9c296d4b9dcec36c6b44197012622bf3662fe74a9cfe6689d89bce549f906cfR2) | [+2, 4, 9, 35, 381, 724-729](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-c9c296d4b9dcec36c6b44197012622bf3662fe74a9cfe6689d89bce549f906cfR2) |
| [`share/examples/dpowcoin.conf`](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-03b2f682f890977f25a0b878c0a26d084ef266275555b92582761c050bac424fR26) | [+26](https://github.com/dpowcore-project/dpowcoin/commit/3ad0b0b76a35525aa9d6443cc0863f73c1737f82#diff-03b2f682f890977f25a0b878c0a26d084ef266275555b92582761c050bac424fR26) |

*8 file(s) · ✏️ 8 modified · +37 / -33 lines*

---

### Backport 3 bitcoin/bitcoin PRs from official 31.x: disconnect only the misbehaving peer, cmake non-target directive cleanup, memusage Clang 24 fix; revert CMake 4.5 CI workaround

> [`4409153`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73) · 2026-11-19

| File | Lines |
|------|-------|
| [`.github/ci-test-each-commit-exec.py`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-3db8f8c36e425c8a0e717f585382f08c6632394e9adb3ebb48364c176d069a35L6) | [-6, 21-32, 40-42, 48](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-3db8f8c36e425c8a0e717f585382f08c6632394e9adb3ebb48364c176d069a35L6) |
| [`.github/ci-windows.py`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-4e6c0a55a7b89305e553d45b51e79624225dfdbe82b906b29f711200717f3599L8) | [-8, 69-80, 87-91](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-4e6c0a55a7b89305e553d45b51e79624225dfdbe82b906b29f711200717f3599L8) |
| [`CMakeLists.txt`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-1e7de1ae2d059d21e1dd75d5812d5a34b0222cef273b7c3a2af62eb747f9d20aL699-L701) | [-699-701](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-1e7de1ae2d059d21e1dd75d5812d5a34b0222cef273b7c3a2af62eb747f9d20aL699-L701) |
| [`ci/test/03_test_script.sh`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-028c14f389228331dc3d0f95a570e19f06cf17112556a164109a51609c3a4c2dL98-L105) | [-98-105](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-028c14f389228331dc3d0f95a570e19f06cf17112556a164109a51609c3a4c2dL98-L105) |
| ~~`cmake/module/WarnAboutGlobalProperties.cmake`~~ | 🗑 deleted |
| [`src/CMakeLists.txt`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-148715d6ea0c0ea0a346af3f6bd610d010d490eca35ac6a9b408748f7ca9e3f4R8-R11) | [+8-11](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-148715d6ea0c0ea0a346af3f6bd610d010d490eca35ac6a9b408748f7ca9e3f4R8-R11) |
| [`src/memusage.h`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-33f5ad9ae321c77fe0cd7e467ced4221209f9d6edb18e0d843c637295517789dL71) | [-71, 168, 183](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-33f5ad9ae321c77fe0cd7e467ced4221209f9d6edb18e0d843c637295517789dL71) |
| [`src/net.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-00021eed586a482abdb09d6cdada1d90115abe988a91421851960e26658bed02R3870) | [+3870, 3875](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-00021eed586a482abdb09d6cdada1d90115abe988a91421851960e26658bed02R3870) |
| [`src/net.h`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-422879cc8bfac56d4380c865f381b58afeb344bc355bbc7f47c581e4491b6b4bR1367) | [+1367](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-422879cc8bfac56d4380c865f381b58afeb344bc355bbc7f47c581e4491b6b4bR1367) |
| [`src/net_processing.cpp`](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-6875de769e90cec84d2e8a9c1b962cdbcda44d870d42e4215827e599e11e90e3R5247) | [+5247, 5250](https://github.com/dpowcore-project/dpowcoin/commit/44091538d8943c851d7e41063c3e3aa4b38bcd73#diff-6875de769e90cec84d2e8a9c1b962cdbcda44d870d42e4215827e599e11e90e3R5247) |

*10 file(s) · ✏️ 9 modified · 🗑 1 deleted · +9 / -55 lines*

---

Credits
=======

Thanks to everyone who directly contributed to this release:

- mraksoll4

Dpowcoin Core is based on Bitcoin Core.
Original Bitcoin Core developers:
  <https://github.com/bitcoin/bitcoin/graphs/contributors>
