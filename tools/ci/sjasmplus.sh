#!/bin/sh
# tools/ci/sjasmplus.sh -- sjasmplus for the simulator jobs, which assemble
# C-BIOS (tools/cbios.sh checks its SHA-1s) and the test carts with it.
# Linux and macOS build it from source at setup.sh's commit. Windows gets
# the project's own Windows build of 1.23.1. It goes on the PATH for the
# steps after this one.
set -eu
SJASMPLUS_COMMIT=69569f353fd1de38a6fb474ce9d543d87566f75a   # sjasmplus v1.23.1.1, as tools/ci/setup.sh
WIN_ZIP=sjasmplus-1.23.1.win.zip
WIN_URL=https://github.com/z00m128/sjasmplus/releases/download/v1.23.1/$WIN_ZIP
WIN_SHA256=fa0ca77e6e6dcdad77b2c64607b325dd6366844ab6438bfc97661fdfbc29aa35

case "${RUNNER_OS:-$(uname)}" in
    Windows*|MINGW*|MSYS*)
        dir=$(cygpath -u "${RUNNER_TEMP:-/tmp}")/sjasmplus
        mkdir -p "$dir"
        curl -fsSL -o "$dir/$WIN_ZIP" "$WIN_URL"
        echo "$WIN_SHA256  $dir/$WIN_ZIP" | sha256sum -c -
        unzip -q -o "$dir/$WIN_ZIP" -d "$dir"
        bin=$dir/sjasmplus-1.23.1.win
        path=$(cygpath -w "$bin")
        ;;
    *)
        dir=${RUNNER_TEMP:-/tmp}/sjasmplus
        rm -rf "$dir"
        git init -q "$dir"
        git -C "$dir" fetch -q --depth 1 https://github.com/z00m128/sjasmplus "$SJASMPLUS_COMMIT"
        git -C "$dir" checkout -q FETCH_HEAD
        git -C "$dir" submodule update -q --init --depth 1
        make -C "$dir" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" >/dev/null
        bin=$dir
        path=$bin
        ;;
esac
"$bin/sjasmplus" --version | head -1
if [ -n "${GITHUB_PATH:-}" ]; then echo "$path" >> "$GITHUB_PATH"; fi
