Dpowcoin Core
=============

Setup
---------------------
Dpowcoin Core is the original Dpowcoin client and it builds the backbone of the network. It downloads and, by default, stores the entire history of Dpowcoin transactions, which requires several hundred gigabytes or more of disk space. Depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to several days or more.

To download Dpowcoin Core, visit [dpowcore.org](https://dpowcore.org/en/wallets/full-node/).

Running
---------------------
The following are some helpful notes on how to run Dpowcoin Core on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/dpowcoin-qt` (GUI) or
- `bin/dpowcoind` (headless)
- `bin/dpowcoin` (wrapper command)

The `dpowcoin` command supports subcommands like `dpowcoin gui`, `dpowcoin node`, and `dpowcoin rpc` exposing different functionality. Subcommands can be listed with `dpowcoin help`.

### Windows

Unpack the files into a directory, and then run dpowcoin-qt.exe.

### macOS

Drag Dpowcoin Core to your applications folder, and then run Dpowcoin Core.

### Need Help?

* See the documentation at the [Bitcoin Wiki](https://en.bitcoin.it/wiki/Main_Page)
for help and more information.
* Ask for help on [Dpowcoin discord](https://discord.gg/b9zkzAgUpH).
* Ask for help on [Dpowcoin telegram](https://t.me/dpowcoin).
* Ask for help on the [Dpowcoin BitcoinTalk Announce Thread](https://bitcointalk.org/index.php?topic=5493459.0).

Building
---------------------
The following are developer notes on how to build Dpowcoin Core on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [macOS Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows-msvc.md)
- [FreeBSD Build Notes](build-freebsd.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [NetBSD Build Notes](build-netbsd.md)

Development
---------------------
The Dpowcoin repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Productivity Notes](productivity.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://doxygen.bitcoincore.org/)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [JSON-RPC Interface](JSON-RPC-interface.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)
- [Internal Design Docs](design/)

### Resources
* Discuss on the [Dpowcoin BitcoinTalk Announce Thread](https://bitcointalk.org/index.php?topic=5493459.0).
* Discuss project-specific development on [Dpowcoin discord](https://discord.gg/b9zkzAgUpH).

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [dpowcoin.conf Configuration File](dpowcoin-conf.md)
- [CJDNS Support](cjdns.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [I2P Support](i2p.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [Managing Wallets](managing-wallets.md)
- [Multisig Tutorial](multisig-tutorial.md)
- [Offline Signing Tutorial](offline-signing-tutorial.md)
- [P2P bad ports definition and list](p2p-bad-ports.md)
- [PSBT support](psbt.md)
- [Reduce Memory](reduce-memory.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Transaction Relay Policy](policy/README.md)
- [ZMQ](zmq.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
