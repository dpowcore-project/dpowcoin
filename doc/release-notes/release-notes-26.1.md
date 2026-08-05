26.1 Release Notes
==================

Dpowcoin Core version 26.1 is now available from:

  <https://dpowcore.org/download/>

This release includes various bug fixes and performance
improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/dpowcore-project/dpowcoin/issues>

To receive security and update notifications, please subscribe to:

  <https://dpowcore.org/en/list/announcements/join/>


This Is Initial Project Release
==============

How to start mining ?

go to debug console and put command

generatetoaddress nblocks "address" ( maxtries )

Example for infine try

generatetoaddress -1 "Myaddress" -1


Compatibility
==============

Please test and repoort if you find any bugs , but it most work at all recents supported systems.

   <https://github.com/dpowcore-project/dpowcoin/issues>

Notable changes
===============

### Initial rebranding text and some rolders/files.

- [44671ac](https://github.com/dpowcore-project/dpowcoin/commit/44671ac9eebbc0e5b68aaaa0fd40f2e77a130ee2)

### Port Changes, Network Magic Changes, Mainnet Prefixes.

- [950d021](https://github.com/dpowcore-project/dpowcoin/commit/950d021a70da40bc676839a4b20c5dbab4d14506)

### remove BIP30 exepctions

- [7ddea89](https://github.com/dpowcore-project/dpowcoin/commit/7ddea89a50f19355b6326023b8ccdc570a4d98ca)

### Add Yespower and Argon2id Algos, Add dual POW logic, Drop BIP's.

- [13fcd2f](https://github.com/dpowcore-project/dpowcoin/commit/13fcd2f73e5de1adf5050ce09f0ac4c85002f566)

### Blockchain settings, fix tests, and dissable broken due new pow logic.

- [205f279](https://github.com/dpowcore-project/dpowcoin/commit/205f279b1fd93023453439c13434aacc3ce86eb0)


Credits
=======

Thanks to everyone who directly contributed to this release:

- Someunknownman
- Mraksoll
