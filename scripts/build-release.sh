#!/usr/bin/env bash

export LC_ALL=C
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
  ./scripts/build-release.sh --version v3.0.0 --platform windows-x86_64 --skip-build
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
        mingw*|msys*|cygwin*) os="windows" ;;
        *) error "Unsupported OS for release packaging: $os" ;;
    esac

    case "$arch" in
        x86_64|amd64) arch="x86_64" ;;
        arm64|aarch64) arch="arm64" ;;
        *) error "Unsupported architecture for release packaging: $arch" ;;
    esac

    printf '%s-%s\n' "$os" "$arch"
}

validate_platform() {
    case "$PLATFORM" in
        linux-x86_64|linux-arm64|macos-x86_64|macos-arm64|windows-x86_64|windows-arm64) ;;
        *) error "Unsupported release platform: $PLATFORM" ;;
    esac
}

is_windows_platform() {
    case "$PLATFORM" in
        windows-*) return 0 ;;
        *) return 1 ;;
    esac
}

binary_name() {
    local name suffix

    name="$1"
    suffix=""
    if is_windows_platform; then
        suffix=".exe"
    fi

    printf '%s%s\n' "$name" "$suffix"
}

resolve_binary_path() {
    local name filename candidate

    name="$1"
    filename="$(binary_name "$name")"

    for candidate in \
        "$BUILD_DIR/bin/$filename" \
        "$BUILD_DIR/bin/Release/$filename" \
        "$BUILD_DIR/src/$filename" \
        "$BUILD_DIR/src/Release/$filename"; do
        if [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return
        fi
    done

    error "Missing release binary $filename under $BUILD_DIR"
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

    if is_windows_platform; then
        error "Windows release packaging expects an existing MSVC or MinGW build; rerun with --skip-build"
    fi

    if [ "$(uname -s)" = "Darwin" ]; then
        openssl_flag=(-DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)")
    fi

    info "Building RNG release binaries"
    cmake -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=OFF \
        -DENABLE_IPC=OFF \
        -DWITH_ZMQ=OFF \
        -DENABLE_WALLET=ON \
        "${openssl_flag[@]}"
    cmake --build "$BUILD_DIR" -j"$(cpu_count)" --target rngd rng-cli
}

maybe_strip_binary() {
    local file

    file="$1"
    if is_windows_platform; then
        return
    fi

    if command -v strip >/dev/null 2>&1; then
        strip "$file" 2>/dev/null || true
    fi
    if command -v objcopy >/dev/null 2>&1; then
        objcopy --remove-section .note.gnu.build-id "$file" 2>/dev/null || true
    fi
}

make_manifest() {
    local stage_dir commit rngd_name rng_cli_name

    stage_dir="$1"
    commit="$(git rev-parse HEAD)"
    rngd_name="$(binary_name rngd)"
    rng_cli_name="$(binary_name rng-cli)"

    python3 - "$stage_dir/release-manifest.json" "$VERSION" "$PLATFORM" "$commit" "$SOURCE_DATE_EPOCH" "$rngd_name" "$rng_cli_name" <<'PY'
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
        sys.argv[6],
        sys.argv[7],
        "rng-load-bootstrap",
        "rng-start-miner",
        "rng-doctor",
        "rng-install-public-node",
        "rng-install-public-miner",
        "rng-public-apply",
        "rngd.service",
        "rng.conf.example",
        "PUBLIC-NODE.md",
        "COPYING",
    ],
}
path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="ascii")
PY
}

package_release() {
    local package_root stage_root tarball rngd_binary rng_cli_binary rngd_name rng_cli_name

    mkdir -p "$OUTPUT_DIR"

    package_root="rng-${VERSION}-${PLATFORM}"
    stage_root="$(mktemp -d)/$package_root"
    mkdir -p "$stage_root"

    rngd_name="$(binary_name rngd)"
    rng_cli_name="$(binary_name rng-cli)"
    rngd_binary="$(resolve_binary_path rngd)"
    rng_cli_binary="$(resolve_binary_path rng-cli)"

    cp "$rngd_binary" "$stage_root/$rngd_name"
    cp "$rng_cli_binary" "$stage_root/$rng_cli_name"
    cp scripts/load-bootstrap.sh "$stage_root/rng-load-bootstrap"
    cp scripts/start-miner.sh "$stage_root/rng-start-miner"
    cp scripts/doctor.sh "$stage_root/rng-doctor"
    cp scripts/install-public-node.sh "$stage_root/rng-install-public-node"
    cp scripts/install-public-miner.sh "$stage_root/rng-install-public-miner"
    cp scripts/public-apply.sh "$stage_root/rng-public-apply"
    cp contrib/init/rngd.service "$stage_root/rngd.service"
    cp contrib/init/rng.conf.example "$stage_root/rng.conf.example"
    cp doc/public-node.md "$stage_root/PUBLIC-NODE.md"
    cp COPYING "$stage_root/COPYING"

    chmod 755 "$stage_root/$rngd_name" "$stage_root/$rng_cli_name" \
        "$stage_root/rng-load-bootstrap" "$stage_root/rng-start-miner" \
        "$stage_root/rng-doctor" "$stage_root/rng-install-public-node" \
        "$stage_root/rng-install-public-miner" "$stage_root/rng-public-apply"
    chmod 644 "$stage_root/rngd.service" "$stage_root/rng.conf.example" \
        "$stage_root/PUBLIC-NODE.md" "$stage_root/COPYING"

    maybe_strip_binary "$stage_root/$rngd_name"
    maybe_strip_binary "$stage_root/$rng_cli_name"
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
    validate_platform
    resolve_version
    resolve_source_date_epoch

    if [ "$SKIP_BUILD" -eq 0 ]; then
        build_binaries
    fi

    resolve_binary_path rngd >/dev/null
    resolve_binary_path rng-cli >/dev/null

    rm -f "$OUTPUT_DIR/SHA256SUMS"
    package_release
    append_checksums

    info "Release packaging complete for $VERSION ($PLATFORM)"
}

main "$@"
