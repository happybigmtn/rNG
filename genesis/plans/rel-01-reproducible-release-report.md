# REL-01 Reproducible Release Report

Date: 2026-04-13

## Scope

Verify that two independent same-commit linux-x86_64 release builds produce byte-identical tarballs, and fix release-path nondeterminism if found.

## Red Proof

- `test -x scripts/check-reproducible-release.sh` failed because no reproducibility verifier existed.
- Two packaging runs from the same existing binaries already produced the same tarball checksum, but the naive diff proof compared full `sha256sum` lines and failed because the output paths differed.
- Two fresh build directories initially produced different tarballs. Extracting both tarballs showed every staged file matched except `rngd`.

## Root Cause

Two independent `rngd` builds differed in release artifacts for two reasons:

- RandomX is copied into the CMake build tree before patching, and RandomX assertions embedded copied temporary source paths through `__FILE__`.
- After prefix-mapping those RandomX paths, the final staged `rngd` differed only in the 20-byte `.note.gnu.build-id` note. Removing that note from copied release binaries made the stripped staged binaries byte-identical while leaving normal build outputs untouched.

## Fix

- Added `scripts/check-reproducible-release.sh` to run two independent release builds and compare tarball names, SHA256 hashes, bytes, and `SHA256SUMS`.
- Set the release build script to configure with `CMAKE_BUILD_TYPE=Release`.
- Added RandomX source/build prefix maps for GNU/Clang builds so copied RandomX paths normalize inside compiled objects.
- Normalized staged release binaries by stripping as before and removing `.note.gnu.build-id` when `objcopy` is available.

## Final Proof

`scripts/check-reproducible-release.sh` passed for two fresh same-commit linux-x86_64 builds.

Final artifact:

```text
rng-v3.0.0-linux-x86_64.tar.gz sha256=4d6d0fe99a0f407fd054f22c438df1638ae65c98913156db7809358d74ca097f
```
