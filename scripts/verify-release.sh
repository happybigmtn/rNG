#!/usr/bin/env bash

set -euo pipefail

REPO="${RNG_RELEASE_REPO:-happybigmtn/rng}"
GITHUB_URL="https://github.com/$REPO"
GITHUB_API_URL="https://api.github.com/repos/$REPO"
VERSION="${RNG_VERSION:-}"
PLATFORM="${RNG_RELEASE_PLATFORM:-}"
FILE_PATH=""
SKIP_ATTESTATION=0
TEMP_DIR=""

usage() {
    cat <<'EOF'
Verify an RNG release asset against the published SHA256SUMS and GitHub attestation.

Usage:
  ./scripts/verify-release.sh [--version TAG] [--platform PLATFORM]
                              [--file PATH] [--skip-attestation]

Examples:
  ./scripts/verify-release.sh --version v3.0.0 --platform linux-x86_64
  ./scripts/verify-release.sh --version v3.0.0 --file ./rng-v3.0.0-linux-x86_64.tar.gz
EOF
}

info() { printf '[INFO] %s\n' "$1"; }
warn() { printf '[WARN] %s\n' "$1" >&2; }
error() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }

cleanup() {
    if [ -n "$TEMP_DIR" ]; then
        rm -rf "$TEMP_DIR"
    fi
}

trap cleanup EXIT

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --version)
                [ $# -ge 2 ] || error "--version requires a tag"
                VERSION="$2"
                shift 2
                ;;
            --platform)
                [ $# -ge 2 ] || error "--platform requires a value"
                PLATFORM="$2"
                shift 2
                ;;
            --file)
                [ $# -ge 2 ] || error "--file requires a path"
                FILE_PATH="$2"
                shift 2
                ;;
            --skip-attestation)
                SKIP_ATTESTATION=1
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

download_file() {
    local url dest

    url="$1"
    dest="$2"

    if command -v curl >/dev/null 2>&1; then
        curl -fsSL -o "$dest" "$url"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$dest" "$url"
    else
        error "Neither curl nor wget is available"
    fi
}

download_to_stdout() {
    local url

    url="$1"

    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$url"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O - "$url"
    else
        error "Neither curl nor wget is available"
    fi
}

fetch_latest_release_tag() {
    local response tag_name

    response="$(download_to_stdout "$GITHUB_API_URL/releases/latest" 2>/dev/null || true)"
    [ -n "$response" ] || return 1

    tag_name="$(
        printf '%s' "$response" | python3 -c '
import json
import sys

try:
    payload = json.load(sys.stdin)
except json.JSONDecodeError:
    raise SystemExit(1)

tag = payload.get("tag_name", "")
if not tag:
    raise SystemExit(1)

print(tag)
'
    )" || return 1

    printf '%s\n' "$tag_name"
}

detect_platform() {
    local os arch

    os="$(uname -s | tr '[:upper:]' '[:lower:]')"
    arch="$(uname -m)"

    case "$os" in
        linux*) os="linux" ;;
        darwin*) os="macos" ;;
        *) error "Unsupported OS: $os" ;;
    esac

    case "$arch" in
        x86_64|amd64) arch="x86_64" ;;
        arm64|aarch64) arch="arm64" ;;
        *) error "Unsupported architecture: $arch" ;;
    esac

    printf '%s-%s\n' "$os" "$arch"
}

verify_with_checksums() {
    local target sums_path filtered_sums

    target="$1"
    sums_path="$2"
    filtered_sums="$(mktemp)"

    grep "[[:space:]]$(basename "$target")\$" "$sums_path" > "$filtered_sums" || {
        rm -f "$filtered_sums"
        error "No checksum entry found for $(basename "$target")"
    }

    if command -v sha256sum >/dev/null 2>&1; then
        (cd "$(dirname "$target")" && sha256sum -c "$filtered_sums")
    elif command -v shasum >/dev/null 2>&1; then
        (cd "$(dirname "$target")" && shasum -a 256 -c "$filtered_sums")
    else
        rm -f "$filtered_sums"
        error "No checksum tool found (sha256sum/shasum)"
    fi

    rm -f "$filtered_sums"
}

verify_attestation() {
    local target

    target="$1"

    if [ "$SKIP_ATTESTATION" -eq 1 ]; then
        warn "Skipping GitHub attestation verification"
        return 0
    fi

    if ! command -v gh >/dev/null 2>&1; then
        warn "GitHub CLI not found; skipping attestation verification"
        return 0
    fi

    gh attestation verify "$target" --repo "$REPO"
}

main() {
    local release_file sums_path

    parse_args "$@"

    if [ -z "$VERSION" ]; then
        VERSION="$(fetch_latest_release_tag)" || error "Could not resolve latest RNG release tag"
    fi

    if [ -z "$FILE_PATH" ]; then
        [ -n "$PLATFORM" ] || PLATFORM="$(detect_platform)"
        release_file="rng-${VERSION}-${PLATFORM}.tar.gz"
        TEMP_DIR="$(mktemp -d)"
        download_file "$GITHUB_URL/releases/download/$VERSION/$release_file" "$TEMP_DIR/$release_file"
        download_file "$GITHUB_URL/releases/download/$VERSION/SHA256SUMS" "$TEMP_DIR/SHA256SUMS"
        FILE_PATH="$TEMP_DIR/$release_file"
        sums_path="$TEMP_DIR/SHA256SUMS"
    else
        [ -f "$FILE_PATH" ] || error "Release file not found: $FILE_PATH"
        TEMP_DIR="$(mktemp -d)"
        download_file "$GITHUB_URL/releases/download/$VERSION/SHA256SUMS" "$TEMP_DIR/SHA256SUMS"
        sums_path="$TEMP_DIR/SHA256SUMS"
    fi

    info "Verifying checksum for $(basename "$FILE_PATH") against $VERSION"
    verify_with_checksums "$FILE_PATH" "$sums_path"
    info "Checksum verified"

    verify_attestation "$FILE_PATH"
    info "Release verification succeeded"
}

main "$@"
