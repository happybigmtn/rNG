#!/usr/bin/env bash

export LC_ALL=C
set -euo pipefail

VERSION="${RNG_RELEASE_VERSION:-}"
PLATFORM="${RNG_RELEASE_PLATFORM:-}"
KEEP_TEMP=0
SKIP_BUILD=0
TEMP_ROOT=""

usage() {
    cat <<'EOF'
Check whether two same-commit RNG release builds produce identical tarballs.

Usage:
  ./scripts/check-reproducible-release.sh [--version TAG] [--platform PLATFORM]
                                          [--skip-build] [--keep-temp]

By default this script performs two independent builds in temporary build
directories, packages each build with scripts/build-release.sh, and verifies
that the resulting tarballs are byte-identical.

Use --skip-build to perform a faster packaging-only reproducibility check with
the existing build/ binaries.
EOF
}

info() { printf '[INFO] %s\n' "$1"; }
error() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }

cleanup() {
    if [ "$KEEP_TEMP" -eq 0 ] && [ -n "$TEMP_ROOT" ]; then
        rm -rf "$TEMP_ROOT"
    fi
}

trap cleanup EXIT

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
            --skip-build)
                SKIP_BUILD=1
                shift
                ;;
            --keep-temp)
                KEEP_TEMP=1
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

checksum() {
    local file

    file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" | awk '{print $1}'
    else
        error "No checksum tool found (sha256sum/shasum)"
    fi
}

find_tarball() {
    local output_dir tarball_count

    output_dir="$1"
    tarball_count="$(find "$output_dir" -maxdepth 1 -type f -name 'rng-*.tar.gz' | wc -l | tr -d ' ')"
    [ "$tarball_count" = "1" ] || error "Expected exactly one release tarball in $output_dir, found $tarball_count"
    find "$output_dir" -maxdepth 1 -type f -name 'rng-*.tar.gz' -print
}

run_build_release() {
    local build_dir output_dir args=()

    build_dir="$1"
    output_dir="$2"

    if [ -n "$VERSION" ]; then
        args+=(--version "$VERSION")
    fi
    if [ -n "$PLATFORM" ]; then
        args+=(--platform "$PLATFORM")
    fi
    if [ "$SKIP_BUILD" -eq 1 ]; then
        args+=(--skip-build)
    fi

    scripts/build-release.sh "${args[@]}" --build-dir "$build_dir" --output-dir "$output_dir"
}

main() {
    local tmp_parent build_a build_b out_a out_b tar_a tar_b sha_a sha_b

    parse_args "$@"

    tmp_parent="${TMPDIR:-/tmp}"
    TEMP_ROOT="$(mktemp -d "$tmp_parent/rng-release-repro.XXXXXXXXXX")"
    build_a="$TEMP_ROOT/build-a"
    build_b="$TEMP_ROOT/build-b"
    out_a="$TEMP_ROOT/out-a"
    out_b="$TEMP_ROOT/out-b"

    if [ "$SKIP_BUILD" -eq 1 ]; then
        build_a="build"
        build_b="build"
    fi

    info "Building first release artifact"
    run_build_release "$build_a" "$out_a"
    info "Building second release artifact"
    run_build_release "$build_b" "$out_b"

    tar_a="$(find_tarball "$out_a")"
    tar_b="$(find_tarball "$out_b")"

    [ "$(basename "$tar_a")" = "$(basename "$tar_b")" ] || {
        error "Release tarball names differ: $(basename "$tar_a") vs $(basename "$tar_b")"
    }

    sha_a="$(checksum "$tar_a")"
    sha_b="$(checksum "$tar_b")"

    [ "$sha_a" = "$sha_b" ] || {
        printf '%s  %s\n' "$sha_a" "$tar_a" >&2
        printf '%s  %s\n' "$sha_b" "$tar_b" >&2
        error "Release tarball checksums differ"
    }

    cmp -s "$tar_a" "$tar_b" || error "Release tarballs have matching hashes but differ by cmp"
    diff -u "$out_a/SHA256SUMS" "$out_b/SHA256SUMS"

    info "Reproducible release confirmed"
    info "$(basename "$tar_a") sha256=$sha_a"
    if [ "$KEEP_TEMP" -eq 1 ]; then
        info "Kept temporary artifacts in $TEMP_ROOT"
    fi
}

main "$@"
