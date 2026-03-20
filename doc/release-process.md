Release Process
====================

RNG uses tagged releases, deterministic packaging, published SHA256 manifests, and GitHub build provenance attestations as the public release path.

`main` is for development. Public miners should install from the newest published tag unless they are explicitly testing unreleased changes.

## Release inputs

Before cutting a tag:

1. Confirm `CMakeLists.txt` version fields match the intended release.
2. Confirm the live-chain constants in [`src/kernel/chainparams.cpp`](/src/kernel/chainparams.cpp), [`src/crypto/randomx_hash.cpp`](/src/crypto/randomx_hash.cpp), and [`cmake/script/PatchRandomX.cmake`](/cmake/script/PatchRandomX.cmake) match mainnet.
3. Run the targeted consensus/miner checks:
   - `./build/bin/test_bitcoin --run_test=randomx_tests`
   - `python3 test/functional/feature_internal_miner.py --configfile=build/test/config.ini`
4. Ensure the bootstrap assets in [`bootstrap/`](/bootstrap) are current and their metadata in [`README.md`](/README.md) matches.
5. Update release notes or the security review note if any RNG-specific consensus or miner behavior changed.

## Cut a release

1. Create and push a signed tag, for example `v3.0.0`.
2. Pushing the tag triggers [`.github/workflows/release.yml`](/.github/workflows/release.yml).
3. The workflow builds the platform binaries, packages them with [`scripts/build-release.sh`](/scripts/build-release.sh), publishes `SHA256SUMS`, and uploads the bootstrap assets.
4. Each binary tarball is attested with GitHub build provenance.
5. Container images are published and attested by [`.github/workflows/ghcr.yml`](/.github/workflows/ghcr.yml).

## Deterministic packaging

[`scripts/build-release.sh`](/scripts/build-release.sh) is the canonical packager. It:

- builds `rngd` and `rng-cli` or packages already-built binaries
- includes `rng-load-bootstrap`, `rng-start-miner`, and `rng-doctor`
- writes `release-manifest.json`
- normalizes archive ownership, permissions, and timestamps
- emits a deterministic `rng-<tag>-<platform>.tar.gz`

To reproduce a release tarball locally:

```bash
git checkout vX.Y.Z
git submodule update --init --recursive -- src/crypto/randomx
./scripts/build-release.sh --version vX.Y.Z --platform linux-x86_64
```

## Verify a published release

Use the repository helper:

```bash
./scripts/verify-release.sh --version vX.Y.Z --platform linux-x86_64
```

Or verify the attestation directly with GitHub CLI:

```bash
gh attestation verify rng-vX.Y.Z-linux-x86_64.tar.gz --repo happybigmtn/rng
```

## Installer expectations

[`install.sh`](/install.sh) follows this policy:

- if run from a local repo checkout, it installs that checkout
- if run standalone, it resolves the newest published tag and installs that release
- if no published release can be resolved, it falls back to a source build

This keeps repo-local development convenient while making the public install path tag-first.
