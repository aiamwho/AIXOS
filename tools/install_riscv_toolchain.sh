#!/bin/sh
set -eu

VERSION="15.2.0-1"
ARCHIVE="xpack-riscv-none-elf-gcc-${VERSION}-darwin-arm64.tar.gz"
URL="https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v${VERSION}/${ARCHIVE}"
SHA256="6588e8351455fad8aca37551f0e5a5543f3346bfa9a837cf03cbd3bdd4989f8f"
DEST="${1:-$HOME/.local/xpack-riscv-none-elf-gcc-${VERSION}}"
TMP="${TMPDIR:-/tmp}/${ARCHIVE}"

if [ -x "$DEST/bin/riscv-none-elf-gcc" ]; then
    printf 'toolchain already installed: %s\n' "$DEST"
    exit 0
fi

curl -L "$URL" -o "$TMP"
ACTUAL=$(shasum -a 256 "$TMP" | awk '{print $1}')
if [ "$ACTUAL" != "$SHA256" ]; then
    printf 'checksum mismatch: expected %s, got %s\n' "$SHA256" "$ACTUAL" >&2
    exit 1
fi

PARENT=$(dirname "$DEST")
mkdir -p "$PARENT"
tar -xzf "$TMP" -C "$PARENT"
EXTRACTED="$PARENT/xpack-riscv-none-elf-gcc-${VERSION}"
if [ "$EXTRACTED" != "$DEST" ]; then
    mv "$EXTRACTED" "$DEST"
fi

printf 'installed: %s\n' "$DEST"
printf 'build with: make riscv RISCV_TOOLCHAIN_DIR=%s\n' "$DEST"
