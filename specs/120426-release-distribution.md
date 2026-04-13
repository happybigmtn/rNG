# Specification: Release and Distribution

## Objective

Define the build, packaging, verification, and distribution pipeline for RNG binaries. Covers the CMake build system, release tarball construction, checksum verification, bootstrap asset bundling, container image, and the release lifecycle.

## Evidence Status

### Verified Facts (grounded in source code and build files)

**Build system**:
- Build tool: CMake 3.22+ (`CMakeLists.txt`, ~700 lines)
- C++ standard: C++20 (no extensions)
- Client name in build: `RNG Core`
- Version: `3.0.0` (release)
- Copyright year: 2026
- Output binaries: `rngd`, `rng-cli`
- GUI binary: `rng-qt` (optional, disabled by default)

**Build dependencies** (from Dockerfile and build-release.sh):
- `build-essential`, `cmake`, `git`, `pkg-config`
- `libboost-all-dev` (filesystem, thread at minimum)
- `libssl-dev` (OpenSSL 3.0+)
- `libevent-dev`
- `libsqlite3-dev`
- `secp256k1` (vendored or system)
- RandomX v1.2.1 (vendored at `src/crypto/randomx/`)

**Standard build command** (from README and build-release.sh):
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON
cmake --build build -j$(nproc)
```

**macOS additional flag**:
```bash
-DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
```

**Release builder** (`scripts/build-release.sh`):
- Auto-detects version from `git describe --tags` or `CMakeLists.txt`
- Auto-detects platform: `linux-x86_64`, `linux-arm64`, `macos-x86_64`, `macos-arm64`
- Configures release binaries with `CMAKE_BUILD_TYPE=Release`
- Tarball naming: `rng-${VERSION}-${PLATFORM}.tar.gz`
- Tarball format: PAX, `tar.gz`, owner normalized to `root:root`
- Source date epoch: from `git log` commit timestamp (for reproducibility)
- Permissions: 0755 (executables), 0644 (data files)
- Staged binaries are stripped when `strip` is available; Linux GNU build-id notes are removed from staged copies when `objcopy` is available so the final tarball does not inherit nondeterministic build IDs
- Checksums: SHA256SUMS file rewritten for the output directory on each run
- Flags: `--version`, `--platform`, `--build-dir`, `--output-dir`, `--skip-build`

**Reproducible release check** (`scripts/check-reproducible-release.sh`):
- Performs two independent release builds in temporary build directories and compares the resulting tarballs byte-for-byte
- Supports `--version`, `--platform`, `--skip-build`, and `--keep-temp`
- On 2026-04-13, same-commit linux-x86_64 verification produced identical `rng-v3.0.0-linux-x86_64.tar.gz` artifacts with SHA256 `4d6d0fe99a0f407fd054f22c438df1638ae65c98913156db7809358d74ca097f`

**Tarball contents**:
- `rngd` (0755)
- `rng-cli` (0755)
- `rng-load-bootstrap` (0755)
- `rng-start-miner` (0755)
- `rng-doctor` (0755)
- `rng-install-public-node` (0755)
- `rng-install-public-miner` (0755)
- `rng-public-apply` (0755)
- `rngd.service` (0644)
- `rng.conf.example` (0644)
- `PUBLIC-NODE.md` (0644)
- `COPYING` (0644)
- `release-manifest.json` (0644)

**Release manifest** (`release-manifest.json`):
```json
{
  "version": "<version>",
  "platform": "<platform>",
  "git_commit": "<HEAD SHA>",
  "source_date_epoch": <unix timestamp>,
  "artifacts": ["rngd", "rng-cli", ...]
}
```

**Release verification** (`scripts/verify-release.sh`):
- Verifies a published release tarball checksum against the published GitHub `SHA256SUMS`
- Verifies GitHub attestations when `gh` is available, unless `--skip-attestation` is passed
- Accepts `--version`, `--platform`, `--file`, and `--skip-attestation`; positional tarball arguments are not supported
- Exists in `scripts/` directory

**Container image** (`Dockerfile`):
- Build stage: Ubuntu 24.04
- Runtime stage: Ubuntu 24.04, non-root user `rng`
- Runtime dependencies: `libevent-2.1-7t64`, `libevent-extra-2.1-7t64`, `libevent-pthreads-2.1-7t64`, `libsqlite3-0`, `libstdc++6`
- Exposed ports: `8432` (RPC), `8433` (P2P)
- Entrypoint: `rng-docker-entrypoint`, default command: `rngd -printtoconsole`
- `rngd --version` works without secrets; daemon startup requires `RNG_RPC_PASSWORD` so the image has no hardcoded RPC password
- RandomX submodule: vendored at `src/crypto/randomx/` (`v1.2.1`)

**Docker Compose** (`docker-compose.yml`):
- Not present in repository root

**Bootstrap assets** (bundled in release and repository):
- Chain bundle: `bootstrap/rng-mainnet-29944-datadir.tar.gz`
  - SHA256: `fd2db803584a99089812b4d59b9dd92f52821149a8add329d246635a406a22b4`
  - Height: 29944
- UTXO snapshot: `bootstrap/rng-mainnet-29944.utxo`
  - SHA256: `70bde51d839bb000c4455d493e873553486e9c2b34c5734bb08d073d9d3d11a1`
  - Height: 29944
  - Base block hash: `4287ff94a9fc6197b66efa47fc8493e5d64cfab78f910a24952446e76bce742b`
  - UTXO set hash: `e3beaab3c1031e45b1b63d08d74331cc08a0541e44b91cb8f7c73fb1b3f40562`

**Live binary hashes** (from EXECPLAN.md, QSB-enabled build):
- `rngd`: `36eb7509a17c15fbca062dc3427bb36d0d19cb24ec4fb299fcea09e20a5ad054`
- `rng-cli`: `eff7e8d116b8143f4182197e482804b74a49d8885915e24ab23eec6b3f67b92a`

**Release process** (from README and repository history):
- Tag-first releases: create git tag, then build
- Release artifacts committed via `git tag`
- Verification: `scripts/verify-release.sh` against SHA256SUMS

### Recommendations (intended system)

- Plan 012 proposes updating all specs and operator docs as part of mainnet activation preparation — the release process would include documentation validation
- Bootstrap assets should be refreshed with each significant chain height milestone

### Hypotheses / Unresolved Questions

- Cross-machine reproducibility is not yet verified; same-machine, same-commit linux-x86_64 reproducibility is verified by `scripts/check-reproducible-release.sh`
- Whether cross-compilation (e.g., ARM64 on x86_64) is supported via the `depends/` system

## Acceptance Criteria

- `cmake -B build ... && cmake --build build` produces `rngd` and `rng-cli` on Linux x86_64
- `build-release.sh` produces a tarball containing all listed artifacts
- Tarball checksums match those recorded in SHA256SUMS
- `verify-release.sh` successfully validates a freshly built tarball
- Release manifest JSON contains accurate version, platform, and git commit
- Container builds successfully from Dockerfile
- Container exposes ports 8432 and 8433
- Container runs as non-root user `rng`
- `rngd --version` inside the container reports `RNG Core v3.0.0`
- Bootstrap assets inside the tarball allow a new node to start syncing from height 29944
- All helper scripts in the tarball are executable (mode 0755)
- The tarball uses PAX format with normalized ownership for reproducibility

## Verification

```bash
# Build from source
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON
cmake --build build -j$(nproc)
./build/src/rngd --version
# Expected: RNG Core v3.0.0

# Build release tarball
./scripts/build-release.sh --version v3.0.0
ls dist/rng-v3.0.0-linux-x86_64.tar.gz
cat dist/SHA256SUMS

# Verify a published release asset
./scripts/verify-release.sh --version v3.0.0 --file dist/rng-v3.0.0-linux-x86_64.tar.gz --skip-attestation

# Verify same-commit release reproducibility on the current platform
./scripts/check-reproducible-release.sh

# Inspect tarball contents
tar tzf dist/rng-v3.0.0-linux-x86_64.tar.gz
# Should list: rngd, rng-cli, helper scripts, service file, config example, manifest

# Build container
docker build -t rng:local .
docker run --rm rng:local --version
# Expected: RNG Core v3.0.0

# Verify bootstrap asset checksums
sha256sum bootstrap/rng-mainnet-29944-datadir.tar.gz
# Expected: fd2db803584a99089812b4d59b9dd92f52821149a8add329d246635a406a22b4

sha256sum bootstrap/rng-mainnet-29944.utxo
# Expected: 70bde51d839bb000c4455d493e873553486e9c2b34c5734bb08d073d9d3d11a1
```

## Open Questions

1. Are release builds reproducible across two different machines, not just across two clean same-machine build directories?
2. Should the `depends/` cross-compilation system be documented for ARM64 builds, or is native compilation the only supported path?
3. Should bootstrap assets be hosted externally (e.g., GitHub Releases, CDN) rather than committed to the repository? The chain bundle is ~60 MB and will grow.
4. Is there a GPG signing process for releases, or is SHA256SUMS the only verification mechanism?
5. Should the Dockerfile be updated to use a newer Ubuntu base (24.04) for longer support lifetime?
6. Should CI/CD pipelines be formalized for automated release building and testing?
