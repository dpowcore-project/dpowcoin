#!/usr/bin/env python3
#
# Copyright (c) 2018-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
import contextlib
from fnmatch import fnmatch
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile

sys.path.append(str(Path(__file__).resolve().parent))
from download_utils import download_from_url

TAR = os.getenv('TAR', 'tar')

GITHUB_REPO = "dpowcore-project/dpowcoin"

SHA256_SUMS = {
    "7f1c73a43545850b45ed32be64bc742b81b143d6719aea03f8038ed1941475a3": {"tag": "v26.3.2", "archive": "dpowcoin-26.3.2-aarch64-linux-gnu.tar.gz"},
    "7e41628d1d76d9b6f154146cd649ea106da73f614a8ee49383cfa66d8e4cda23": {"tag": "v26.3.2", "archive": "dpowcoin-26.3.2-arm-linux-gnueabihf.tar.gz"},
    "9905db599182b1a501a8d0b87a32c2469f7136e33faf0fc74c4352d2ee4ee980": {"tag": "v26.3.2", "archive": "dpowcoin-26.3.2-arm64-apple-darwin.tar.gz"},
    "8a2f06d069b144ac15d03f047379484171fbfacf9d41576523f1d3581f9c2ae8": {"tag": "v26.3.2", "archive": "dpowcoin-26.3.2-powerpc64-linux-gnu.tar.gz"},
    "e56614ffde65c98a468a2bd307f0598028988400605fad0a3b41182553408f6e": {"tag": "v26.3.2", "archive": "dpowcoin-26.3.2-riscv64-linux-gnu.tar.gz"},
    "5432f1a21be17b98421cef7a2e6471f80e4f9cd5032572d42612466442378526": {"tag": "v26.3.2", "archive": "dpowcoin-26.3.2-x86_64-apple-darwin.tar.gz"},
    "98a000effaef1a4de27e935592413e9a81f6d4f6af95e6ed1dc6337643e5e534": {"tag": "v26.3.2", "archive": "dpowcoin-26.3.2-x86_64-linux-gnu.tar.gz"},
    "a2ae6d0e96239fce17c80d46893eb1a2142250e82bbf3a7e0cbde1148e1e9169": {"tag": "v26.3.2", "archive": "dpowcoin-26.3.2-win64.zip"},
}


@contextlib.contextmanager
def pushd(new_dir) -> None:
    previous_dir = os.getcwd()
    os.chdir(new_dir)
    try:
        yield
    finally:
        os.chdir(previous_dir)


def get_latest_tag() -> str:
    """Fetch the tag of the latest release from GitHub API."""
    url = f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest"
    print("Fetching latest release tag from GitHub...")
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "dpowcoin-downloader"})
        with urllib.request.urlopen(req) as response:
            data = json.loads(response.read().decode())
            tag = data["tag_name"]
            print(f"Latest release: {tag}")
            return tag
    except Exception as e:
        print(f"Failed to fetch latest release: {e}", file=sys.stderr)
        sys.exit(1)


def download_binary(tag, args) -> int:
    if Path(tag).is_dir():
        if not args.remove_dir:
            print('Using cached {}'.format(tag))
            return 0
        shutil.rmtree(tag)

    host = args.host
    if tag < "v23" and host in ["x86_64-apple-darwin", "arm64-apple-darwin"]:
        host = "osx64"

    archive_format = 'tar.gz'
    if host == 'win64':
        archive_format = 'zip'

    archive = f'dpowcoin-{tag[1:]}-{host}.{archive_format}'

    # GitHub Releases download URL
    archive_url = f'https://github.com/{GITHUB_REPO}/releases/download/{tag}/{archive}'

    try:
        download_from_url(archive_url, archive)
    except Exception as e:
        print(f"\nDownload failed: {e}", file=sys.stderr)
        print("Retrying download after failure ...", file=sys.stderr)
        time.sleep(12)
        try:
            download_from_url(archive_url, archive)
        except Exception as e2:
            print(f"\nDownload failed a second time: {e2}", file=sys.stderr)
            return 1

    hasher = hashlib.sha256()
    with open(archive, "rb") as afile:
        hasher.update(afile.read())
    archiveHash = hasher.hexdigest()

    if archiveHash not in SHA256_SUMS or SHA256_SUMS[archiveHash]['archive'] != archive:
        if archive in [v['archive'] for v in SHA256_SUMS.values()]:
            print(f"Checksum {archiveHash} did not match", file=sys.stderr)
        else:
            print("Checksum for given version doesn't exist", file=sys.stderr)
        return 1

    print("Checksum matched")
    Path(tag).mkdir()

    # Extract archive
    if host == 'win64':
        try:
            with zipfile.ZipFile(archive, 'r') as zip_file:
                zip_file.extractall(tag)
            # Remove the top level directory to match tar's --strip-components=1
            extracted_items = os.listdir(tag)
            top_level_dir = os.path.join(tag, extracted_items[0])
            # Move all files & subdirectories up one level
            for item in os.listdir(top_level_dir):
                shutil.move(os.path.join(top_level_dir, item), tag)
            # Remove the now-empty top-level directory
            os.rmdir(top_level_dir)
        except Exception as e:
            print(f"Zip extraction failed: {e}", file=sys.stderr)
            return 1
    else:
        ret = subprocess.run([TAR, '-zxf', archive, '-C', tag,
                              '--strip-components=1',
                              'dpowcoin-{tag}'.format(tag=tag[1:])]).returncode
        if ret != 0:
            print(f"Failed to extract the {tag} tarball", file=sys.stderr)
            return ret

    Path(archive).unlink()

    if tag >= "v23" and tag < "v28.2" and args.host == "arm64-apple-darwin":
        # Starting with v23 there are arm64 binaries for ARM (e.g. M1, M2) mac.
        # Until v28.2 they had to be signed to run.
        binary_path = f'{os.getcwd()}/{tag}/bin/'

        for arm_binary in os.listdir(binary_path):
            # Is it already signed?
            ret = subprocess.run(
                ['codesign', '-v', binary_path + arm_binary],
                stderr=subprocess.DEVNULL,  # Suppress expected stderr output
            ).returncode
            if ret == 1:
                # Have to self-sign the binary
                ret = subprocess.run(
                    ['codesign', '-s', '-', binary_path + arm_binary]
                ).returncode
                if ret != 0:
                    print(f"Failed to self-sign {tag} {arm_binary} arm64 binary", file=sys.stderr)
                    return 1

                # Confirm success
                ret = subprocess.run(
                    ['codesign', '-v', binary_path + arm_binary]
                ).returncode
                if ret != 0:
                    print(f"Failed to verify the self-signed {tag} {arm_binary} arm64 binary", file=sys.stderr)
                    return 1

    return 0


def set_host(args) -> int:
    if platform.system().lower() == 'windows':
        if platform.machine() != 'AMD64':
            print('Only 64bit Windows supported', file=sys.stderr)
            return 1
        args.host = 'win64'
        return 0
    host = os.environ.get('HOST', subprocess.check_output(
        './depends/config.guess').decode())
    platforms = {
        'aarch64-*-linux*': 'aarch64-linux-gnu',
        'powerpc64le-*-linux-*': 'powerpc64le-linux-gnu',
        'riscv64-*-linux*': 'riscv64-linux-gnu',
        'x86_64-*-linux*': 'x86_64-linux-gnu',
        'x86_64-apple-darwin*': 'x86_64-apple-darwin',
        'aarch64-apple-darwin*': 'arm64-apple-darwin',
    }
    args.host = ''
    for pattern, target in platforms.items():
        if fnmatch(host, pattern):
            args.host = target
    if not args.host:
        print('Not sure which binary to download for {}'.format(host), file=sys.stderr)
        return 1
    return 0


def main(args) -> int:
    if args.latest:
        args.tags = [get_latest_tag()]

    Path(args.target_dir).mkdir(exist_ok=True, parents=True)
    print("Releases directory: {}".format(args.target_dir))
    ret = set_host(args)
    if ret:
        return ret
    with pushd(args.target_dir):
        for tag in args.tags:
            ret = download_binary(tag, args)
            if ret:
                return ret
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        epilog='''
        HOST can be set to any of the `host-platform-triplet`s from
        depends/README.md for which a release exists.
        ''',
    )
    parser.add_argument('-r', '--remove-dir', action='store_true',
                        help='remove existing directory.')
    parser.add_argument('-t', '--target-dir', action='store',
                        help='target directory.', default='releases')
    parser.add_argument('--latest', action='store_true',
                        help='automatically download the latest release from GitHub.')
    all_tags = sorted([*set([v['tag'] for v in SHA256_SUMS.values()])])
    parser.add_argument('tags', nargs='*', default=all_tags,
                        help='release tags. e.g.: v0.18.1 v0.20.0rc2 '
                        '(if not specified, the full list needed for '
                        'backwards compatibility tests will be used)'
                        )
    args = parser.parse_args()
    sys.exit(main(args))
