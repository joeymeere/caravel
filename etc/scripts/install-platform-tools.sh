#!/usr/bin/env bash
#
# Install a platform-tools release where `caravel build` looks for it:
#   ~/.cache/solana/<version>/platform-tools
#
#   etc/scripts/install-platform-tools.sh v1.57   (or: make platform-tools)
#
# Skips the download if that version is already installed.

set -euo pipefail

version="${1:-v1.57}"
case "$version" in v*) ;; *) version="v$version" ;; esac

case "$(uname -s)" in
    Linux)  os=linux ;;
    Darwin) os=osx ;;
    *) echo "install-platform-tools: unsupported OS $(uname -s)" >&2; exit 1 ;;
esac
case "$(uname -m)" in
    x86_64|amd64)  arch=x86_64 ;;
    arm64|aarch64) arch=aarch64 ;;
    *) echo "install-platform-tools: unsupported arch $(uname -m)" >&2; exit 1 ;;
esac

dest="$HOME/.cache/solana/$version/platform-tools"
if [ -x "$dest/llvm/bin/clang" ]; then
    echo "platform-tools $version already installed at $dest"
    exit 0
fi

url="https://github.com/anza-xyz/platform-tools/releases/download/$version/platform-tools-$os-$arch.tar.bz2"
echo "Downloading $url"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
curl -fsSL --retry 3 -o "$tmp/pt.tar.bz2" "$url"

mkdir -p "$tmp/extract"
tar -xjf "$tmp/pt.tar.bz2" -C "$tmp/extract"
if [ ! -x "$tmp/extract/llvm/bin/clang" ]; then
    echo "install-platform-tools: archive has no llvm/bin/clang" >&2
    exit 1
fi

mkdir -p "$(dirname "$dest")"
rm -rf "$dest"
mv "$tmp/extract" "$dest"
echo "Installed platform-tools $version to $dest"
"$dest/llvm/bin/clang" --version | head -1
