#!/usr/bin/env bash

set -euo pipefail

VERSION="${RNG_RELEASE_VERSION:-}"
PLATFORM="${RNG_RELEASE_PLATFORM:-}"
BUILD_DIR="${RNG_BUILD_DIR:-build}"
OUTPUT_DIR="${RNG_RELEASE_OUTPUT_DIR:-dist}"
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-}"
SKIP_BUILD=0

usage() {
    cat <<'EOF'
Build and package a deterministic RNG release tarball.

Usage:
  ./scripts/build-release.sh [--version TAG] [--platform PLATFORM]
                             [--build-dir DIR] [--output-dir DIR]
                             [--skip-build]

Examples:
  ./scripts/build-release.sh --version v3.0.0 --platform linux-x86_64
  ./scripts/build-release.sh --version v3.0.0 --platform macos-arm64 --skip-build
EOF
}

info() { printf '[INFO] %s\n' "$1"; }
error() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }

cpu_count() {
    nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 1
}

detect_platform() {
    local os arch

    os="$(uname -s | tr '[:upper:]' '[:lower:]')"
    arch="$(uname -m)"

    case "$os" in
        linux*) os="linux" ;;
        darwin*) os="macos" ;;
        *) error "Unsupported OS for release packaging: $os" ;;
    esac

    case "$arch" in
        x86_64|amd64) arch="x86_64" ;;
        arm64|aarch64) arch="arm64" ;;
        *) error "Unsupported architecture for release packaging: $arch" ;;
    esac

    printf '%s-%s\n' "$os" "$arch"
}

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --version)
                [ $# -ge 2 ] || error "--version requires a value"
                VERSION="$2"
                shift 2
                ;;
            --platform)
                [ $# -ge 2 ] || error "--platform requires a value"
                PLATFORM="$2"
                shift 2
                ;;
            --build-dir)
                [ $# -ge 2 ] || error "--build-dir requires a path"
                BUILD_DIR="$2"
                shift 2
                ;;
            --output-dir)
                [ $# -ge 2 ] || error "--output-dir requires a path"
                OUTPUT_DIR="$2"
                shift 2
                ;;
            --skip-build)
                SKIP_BUILD=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                error "Unknown argument: $1"
                ;;
        esac
    done
}

resolve_version() {
    if [ -n "$VERSION" ]; then
        return
    fi

    VERSION="$(git describe --tags --exact-match 2>/dev/null || true)"
    if [ -z "$VERSION" ]; then
        VERSION="v$(sed -n 's/^set(CLIENT_VERSION_MAJOR //p' CMakeLists.txt | tr -d ')' | head -1).$(sed -n 's/^set(CLIENT_VERSION_MINOR //p' CMakeLists.txt | tr -d ')' | head -1).$(sed -n 's/^set(CLIENT_VERSION_BUILD //p' CMakeLists.txt | tr -d ')' | head -1)"
    fi
}

resolve_source_date_epoch() {
    if [ -n "$SOURCE_DATE_EPOCH" ]; then
        return
    fi

    SOURCE_DATE_EPOCH="$(git log -1 --format=%ct HEAD)"
}

ensure_randomx_present() {
    if [ -f "src/crypto/randomx/src/randomx.h" ]; then
        return
    fi

    error "Missing src/crypto/randomx; initialize the RandomX submodule first"
}

build_binaries() {
    local openssl_flag=()

    ensure_randomx_present

    if [ "$(uname -s)" = "Darwin" ]; then
        openssl_flag=(-DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)")
    fi

    info "Building RNG release binaries"
    cmake -B "$BUILD_DIR" \
        -DBUILD_TESTING=OFF \
        -DENABLE_IPC=OFF \
        -DWITH_ZMQ=OFF \
        -DENABLE_WALLET=ON \
        "${openssl_flag[@]}"
    cmake --build "$BUILD_DIR" -j"$(cpu_count)" --target rngd rng-cli
}

maybe_strip_binary() {
    local file

    file="$1"
    if command -v strip >/dev/null 2>&1; then
        strip "$file" 2>/dev/null || true
    fi
}

make_manifest() {
    local stage_dir commit

    stage_dir="$1"
    commit="$(git rev-parse HEAD)"

    python3 - "$stage_dir/release-manifest.json" "$VERSION" "$PLATFORM" "$commit" "$SOURCE_DATE_EPOCH" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
payload = {
    "version": sys.argv[2],
    "platform": sys.argv[3],
    "git_commit": sys.argv[4],
    "source_date_epoch": int(sys.argv[5]),
    "artifacts": [
        "rngd",
        "rng-cli",
        "rng-load-bootstrap",
        "rng-start-miner",
        "rng-doctor",
        "COPYING",
    ],
}
path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="ascii")
PY
}

package_release() {
    local package_root stage_root tarball

    mkdir -p "$OUTPUT_DIR"

    package_root="rng-${VERSION}-${PLATFORM}"
    stage_root="$(mktemp -d)/$package_root"
    mkdir -p "$stage_root"

    cp "$BUILD_DIR/bin/rngd" "$stage_root/rngd"
    cp "$BUILD_DIR/bin/rng-cli" "$stage_root/rng-cli"
    cp scripts/load-bootstrap.sh "$stage_root/rng-load-bootstrap"
    cp scripts/start-miner.sh "$stage_root/rng-start-miner"
    cp scripts/doctor.sh "$stage_root/rng-doctor"
    cp COPYING "$stage_root/COPYING"

    chmod 755 "$stage_root/rngd" "$stage_root/rng-cli" \
        "$stage_root/rng-load-bootstrap" "$stage_root/rng-start-miner" "$stage_root/rng-doctor"
    chmod 644 "$stage_root/COPYING"

    maybe_strip_binary "$stage_root/rngd"
    maybe_strip_binary "$stage_root/rng-cli"
    make_manifest "$stage_root"

    tarball="$OUTPUT_DIR/${package_root}.tar.gz"

    python3 - "$stage_root" "$tarball" "$SOURCE_DATE_EPOCH" <<'PY'
import gzip
import os
import stat
import sys
import tarfile
from pathlib import Path

source_root = Path(sys.argv[1]).resolve()
tarball = Path(sys.argv[2]).resolve()
mtime = int(sys.argv[3])

def normalized_mode(path: Path) -> int:
    if path.is_dir():
        return 0o755
    if os.access(path, os.X_OK):
        return 0o755
    return 0o644

with tarball.open("wb") as raw:
    with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=mtime) as gz:
        with tarfile.open(fileobj=gz, mode="w", format=tarfile.PAX_FORMAT) as archive:
            for path in [source_root, *sorted(source_root.rglob("*"))]:
                arcname = path.relative_to(source_root.parent).as_posix()
                info = archive.gettarinfo(str(path), arcname)
                info.uid = 0
                info.gid = 0
                info.uname = "root"
                info.gname = "root"
                info.mtime = mtime
                info.mode = normalized_mode(path)
                if path.is_file():
                    with path.open("rb") as handle:
                        archive.addfile(info, handle)
                else:
                    archive.addfile(info)
PY

    info "Built release tarball $tarball"
}

append_checksums() {
    local tarball

    tarball="$OUTPUT_DIR/rng-${VERSION}-${PLATFORM}.tar.gz"
    if command -v sha256sum >/dev/null 2>&1; then
        (cd "$OUTPUT_DIR" && sha256sum "$(basename "$tarball")" >> SHA256SUMS)
    elif command -v shasum >/dev/null 2>&1; then
        (cd "$OUTPUT_DIR" && shasum -a 256 "$(basename "$tarball")" >> SHA256SUMS)
    else
        error "No checksum tool found (sha256sum/shasum)"
    fi
}

main() {
    parse_args "$@"

    [ -n "$PLATFORM" ] || PLATFORM="$(detect_platform)"
    resolve_version
    resolve_source_date_epoch

    if [ "$SKIP_BUILD" -eq 0 ]; then
        build_binaries
    fi

    [ -x "$BUILD_DIR/bin/rngd" ] || error "Missing $BUILD_DIR/bin/rngd"
    [ -x "$BUILD_DIR/bin/rng-cli" ] || error "Missing $BUILD_DIR/bin/rng-cli"

    rm -f "$OUTPUT_DIR/SHA256SUMS"
    package_release
    append_checksums

    info "Release packaging complete for $VERSION ($PLATFORM)"
}

main "$@"
