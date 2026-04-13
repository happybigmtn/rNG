# Specification: Operator Onboarding and Tooling

## Objective

Define the operator-facing tooling for installing, bootstrapping, health-checking, and running RNG nodes and miners. Covers the install script, bootstrap assets, miner startup, health diagnostics, systemd integration, and the public node/miner setup scripts.

## Evidence Status

### Verified Facts (grounded in source code and scripts)

**Install script** (`scripts/install.sh`):
- Supports two install modes: tagged release download or source build
- Default install directory: `$HOME/.local/bin` (overridable via `$RNG_INSTALL_DIR`)
- Default data directory: `$HOME/.rng` (overridable via `$RNG_DATA_DIR`)
- Flags: `--force`, `--add-path`, `--bootstrap`, `--skip-deps`, `--no-verify`, `--no-config`
- Generates `rng.conf` with random RPC password (`openssl rand -hex 16`), `server=1`, `daemon=1`, `minerandomx=fast`, and 4 hardcoded `addnode` entries
- Installs helper scripts: `rng-load-bootstrap`, `rng-start-miner`, `rng-doctor`, `rng-install-public-node`, `rng-install-public-miner`

**Bootstrap assets** (`bootstrap/`):
- Chain bundle: `rng-mainnet-29944-datadir.tar.gz` (height 29944)
  - SHA256: `fd2db803584a99089812b4d59b9dd92f52821149a8add329d246635a406a22b4`
- AssumeUTXO snapshot: `rng-mainnet-29944.utxo` (height 29944)
  - Base block hash: `4287ff94a9fc6197b66efa47fc8493e5d64cfab78f910a24952446e76bce742b`
  - UTXO set hash: `e3beaab3c1031e45b1b63d08d74331cc08a0541e44b91cb8f7c73fb1b3f40562`
  - Transaction count: 29962
  - SHA256: `70bde51d839bb000c4455d493e873553486e9c2b34c5734bb08d073d9d3d11a1`
- Bootstrap header wait timeout: 900 seconds (`$RNG_BOOTSTRAP_HEADER_WAIT_SECONDS`)

**Miner startup script** (`scripts/start-miner.sh`):
- Creates wallet if not exists (default name: `miner`, overridable via `$RNG_WALLET`)
- Derives new payout address or reuses `$RNG_MINEADDRESS`
- Default thread count: CPU count minus 1 (minimum 1)
- Default RandomX mode: `fast`
- Default priority: `low` (nice 19)
- Daemon command: `rngd -daemon -mine -mineaddress=... -minethreads=... -minerandomx=... -minepriority=...`
- Validates address before starting

**Health check** (`scripts/doctor.sh`):
- Verifies genesis hash matches `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`
- Checks peer count, sync state, mining status via RPC
- Reports: chain, blocks, headers, connections (in/out), mining running, mining threads, RandomX mode, local addresses
- Supports `--json` output, `--strict` validation, `--expect-public`, `--expect-miner` flags
- RPC calls used: `getblockcount`, `getblockhash 0`, `getconnectioncount`, `getblockchaininfo`, `getnetworkinfo`, `getinternalmininginfo`

**Public node setup** (`scripts/install-public-node.sh`):
- Creates system user `rng` (overridable via `$RNG_SERVICE_USER`)
- Binary install: `/usr/local/bin/rngd`, `/usr/local/bin/rng-cli`
- Config: `/etc/rng/rng.conf` (mode 600, owned `root:rng`)
- Data: `/var/lib/rngd` (mode 710, owned `rng:rng`)
- Systemd unit: `/etc/systemd/system/rngd.service`
- Copies all helper scripts to `/usr/local/bin/`
- Flag: `--enable-now` to start service immediately

**Public miner setup** (`scripts/install-public-miner.sh`):
- Creates systemd drop-in at `/etc/systemd/system/rngd.service.d/mining.conf`
- Requires `--address rng1...` (payout address)
- Configures: `--threads N`, `--randomx fast|light`, `--priority low|normal`
- Flag: `--enable-now` to restart with mining
- Flag: `--remove` to remove mining override
- Drop-in ExecStart includes: `-mine`, `-mineaddress`, `-minethreads`, `-minerandomx`, `-minepriority`, `-startupnotify`, `-shutdownnotify`
- Sets `Nice=19` in systemd unit

**Release builder** (`scripts/build-release.sh`):
- Auto-detects version from `git describe --tags` or `CMakeLists.txt`
- CMake flags: `-DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON`
- macOS adds: `-DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)`
- Tarball includes: `rngd`, `rng-cli`, all helper scripts, `rngd.service`, `rng.conf.example`, `PUBLIC-NODE.md`, `COPYING`, `release-manifest.json`
- Tarball format: `tar.gz`, PAX format, normalized owner `root:root`
- Checksums: SHA256SUMS file
- Manifest: JSON with version, platform, git_commit, source_date_epoch, artifacts list
- Platform detection: `linux-x86_64`, `linux-arm64`, `macos-x86_64`, `macos-arm64`

**Container** (`Dockerfile`):
- Build stage: Ubuntu 22.04, same CMake flags as release script
- Runtime stage: Ubuntu 22.04, non-root user `rng`
- Runtime deps: `libboost-filesystem1.74.0`, `libboost-thread1.74.0`, `libevent-2.1-7`, `libevent-pthreads-2.1-7`, `libsqlite3-0`, `libssl3`
- Exposed ports: `8432` (RPC), `8433` (P2P)
- Entrypoint: `rngd`, default CMD: `-printtoconsole`
- Default config: `server=1`, `rpcuser=agent`, `rpcpassword=changeme`, `minerandomx=fast`, 4 addnode entries

### Recommendations (intended system)

- Plan 008 proposes adding `pool-mine` and `--pool` flags to the miner startup flow — not yet implemented
- Agent-wallet creation (`createagentwallet` RPC) is documented in `specs/agent-integration.md` but not implemented

### Hypotheses / Unresolved Questions

- Whether the Dockerfile's hardcoded `rpcpassword=changeme` is a security concern for production container deployments
- How frequently the bootstrap assets should be refreshed as the chain grows (current bundle is at height 29944)

## Acceptance Criteria

- `install.sh` with no flags installs `rngd` and `rng-cli` to `$HOME/.local/bin` and creates a valid `rng.conf`
- `install.sh --bootstrap` loads the chain bundle and the node starts syncing from height 29944
- `rng-start-miner` creates a wallet, derives an address, and starts `rngd` with mining enabled
- `rng-doctor` reports PASS for genesis hash verification on a correctly synced mainnet node
- `rng-doctor --json` outputs machine-readable JSON health report
- `rng-doctor --expect-miner` returns non-zero exit code if mining is not active
- `rng-install-public-node` creates systemd service with correct paths and permissions
- `rng-install-public-node --enable-now` starts the service immediately after installation
- `rng-install-public-miner --address rng1...` creates a systemd drop-in that enables mining
- `rng-install-public-miner --remove` removes the mining drop-in and restores non-mining operation
- `build-release.sh` produces a tarball with all expected artifacts and a SHA256SUMS file
- The release manifest JSON contains accurate version, platform, and git commit
- All scripts start with `set -euo pipefail` or equivalent error handling
- Bootstrap chain bundle SHA256 matches `fd2db803584a99089812b4d59b9dd92f52821149a8add329d246635a406a22b4`
- Bootstrap UTXO snapshot SHA256 matches `70bde51d839bb000c4455d493e873553486e9c2b34c5734bb08d073d9d3d11a1`

## Verification

```bash
# Test install flow
./install.sh --force --add-path
which rngd && which rng-cli

# Test bootstrap
./install.sh --bootstrap
rng-cli getblockcount
# Should show height >= 29944

# Test miner startup
rng-start-miner --threads 2 --randomx fast
rng-cli getinternalmininginfo | jq '.running'
# Expected: true

# Test health check
rng-doctor
# Expected: all checks PASS

rng-doctor --json | jq '.genesis_valid'
# Expected: true

# Test public node setup (requires root)
sudo rng-install-public-node --enable-now
sudo systemctl status rngd
# Expected: active (running)

# Test public miner setup (requires root)
sudo rng-install-public-miner --address rng1q... --threads 4
sudo systemctl status rngd
# Expected: active, mining enabled

# Test release build
./scripts/build-release.sh --version v3.0.0
ls dist/rng-v3.0.0-linux-x86_64.tar.gz
cat dist/SHA256SUMS

# Verify bootstrap checksums
sha256sum bootstrap/rng-mainnet-29944-datadir.tar.gz
# Expected: fd2db803584a99089812b4d59b9dd92f52821149a8add329d246635a406a22b4

sha256sum bootstrap/rng-mainnet-29944.utxo
# Expected: 70bde51d839bb000c4455d493e873553486e9c2b34c5734bb08d073d9d3d11a1
```

## Open Questions

1. Should bootstrap assets be versioned and hosted at a stable URL, or continue shipping in-repo? As chain grows, the bundle size increases.
2. Is the Dockerfile's default `rpcpassword=changeme` acceptable? Production containers should generate random credentials.
3. Should `rng-doctor` verify the RandomX seed/salt in addition to genesis hash? This would catch misconfigured builds.
4. Should the install script support unattended/non-interactive mode for CI/CD pipelines?
5. How should operator scripts handle the transition to pooled mining (plan 008)? E.g., should `rng-start-miner` gain a `--pool` flag?
