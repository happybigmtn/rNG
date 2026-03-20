# RNG

CPU-mineable cryptocurrency for AI agents.

## Live Network

As of March 19, 2026:

- Public installs should target the latest tagged RNG release.
- RNG mainnet was restarted from genesis on February 26, 2026.
- Expected genesis hash: `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`
- Public operator seed peers: `95.111.239.142:8433`, `161.97.114.192:8433`, `185.218.126.23:8433`, `185.239.209.227:8433`
- Live mainnet RandomX constants: genesis seed phrase `RNG Genesis Seed`, ARGON salt `RNGCHAIN01`
- Live miners use RandomX `fast` mode.
- The network is currently small and operator-seeded. Low peer counts are normal, and additional public miners materially improve resilience.
- Source builds from `main` were verified against the first post-reset live headers on March 19, 2026, but `main` should now be treated as prerelease/development state rather than the public install target.

If you are mining today, use the latest tagged release unless you are explicitly validating unreleased source changes.

## Upstream Delta

RNG is a fork of Bitcoin Core `v29.0`, not a rewrite. The exact changes from upstream
are documented in [CHANGES.md](./CHANGES.md). In short:

- PoW changed from SHA256d to RandomX with RNG-specific seed/salt constants
- mainnet identity changed: genesis block, network magic, ports, HRP, binary names, datadir
- chain defaults changed: bootstrap assets, seed peers, mining mode, live-network helpers
- transaction format, script engine, UTXO model, wallet model, and most RPC semantics remain Bitcoin-derived

## Mine From This Repo

### Option A: Verify-first release install

```bash
curl -fsSLO https://raw.githubusercontent.com/happybigmtn/rng/<tag>/install.sh
less install.sh
RNG_VERSION=<tag> bash install.sh --add-path --bootstrap
rng-start-miner
```

The standalone installer now resolves tagged releases by default, creates `~/.rng/rng.conf`,
seeds it with the current operator seed peers, can download the release-matched chain bundle or
assumeutxo snapshot, and installs helper commands `rng-load-bootstrap`,
`rng-start-miner`, `rng-doctor`, and `rng-install-public-node`.

### Option B: Clone and build this checkout

```bash
git clone https://github.com/happybigmtn/rng.git
cd rng
./install.sh --add-path --bootstrap
```

Running `./install.sh` from a repo checkout installs that checkout. Use this path if you are
intentionally validating unreleased source changes.

### Option C: Verify a published release asset

```bash
git clone https://github.com/happybigmtn/rng.git
cd rng
./scripts/verify-release.sh --version <tag> --platform linux-x86_64
```

After install, start mining with:

```bash
rng-start-miner
rng-doctor
```

On a public VPS, you can also install the packaged service/unit path with:

```bash
sudo rng-install-public-node
sudo systemctl enable --now rngd
sudo ufw allow 8433/tcp
```

To run persistent mining under systemd on that same host:

```bash
sudo rng-install-public-miner --address rng1...
sudo systemctl restart rngd
```

## Quick Start

### Fastest path from a release install

```bash
rng-load-bootstrap
rng-start-miner
rng-doctor
```

### Fastest path from a repo checkout

```bash
./install.sh --add-path --bootstrap
rng-start-miner
rng-doctor
```

### Fast bootstrap from the bundled chain bundle or snapshot

The repo includes two bootstrap assets:

- A bundled near-tip datadir archive at height `15244`
- A verified assumeutxo snapshot at height `15091`

Bundled datadir archive metadata:

- Height: `15244`
- File: `bootstrap/rng-mainnet-15244-datadir.tar.gz`
- File SHA256: `bf0bfad8054c73dc732391f2420d8b9f20f3c8276360745706783079a004c73d`

Assumeutxo snapshot metadata:

- Base hash: `2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb`
- UTXO hash: `9ca1b551b9837c0b0e9158436bac5051e4984d39f691e1374c4786a6c0ed5393`

Load it with:

```bash
# from a repo checkout
./scripts/load-bootstrap.sh

# or after install
rng-load-bootstrap
rng-cli getchainstates
```

On a fresh datadir, the helper first extracts the bundled near-tip datadir archive.
If that archive is unavailable, or if you are retrying on a partially initialized
datadir, it falls back to the assumeutxo snapshot path. On a tagged binary install,
`rng-load-bootstrap` will download the release-matched assets automatically if they
are not already present locally. The bundle alone is enough
to come up near tip. The snapshot path still waits for the snapshot base header and
now waits up to 15 minutes by default while printing header progress.

After that, continue with the normal wallet and mining steps below.

### 1. Start the daemon and verify the chain

```bash
rngd -daemon
sleep 10
rng-cli getblockhash 0
rng-cli getconnectioncount
rng-cli getchainstates
rng-cli getblockchaininfo | grep -E '"chain"|"blocks"'
```

Expected genesis hash:

```text
83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4
```

### 2. One-command miner start

```bash
./scripts/start-miner.sh
rng-cli getinternalmininginfo
```

The helper creates or loads a `miner` wallet, derives a payout address, restarts
`rngd` with `-mine`, and defaults to `CPU count - 1` mining threads.

### 2a. Low-Peer Network Expectations

RNG mainnet is currently sustained by a small operator fleet rather than a broad public
miner set.

- `getconnectioncount` may stay in the `1` to `4` range for long stretches
- `0` peers does not automatically mean your config is wrong; it can mean the public seed fleet is temporarily down
- once you are synced, solo mining is valid and expected
- new nodes sync fastest from the bundled snapshot plus the current addnode list

### 2c. Help the network as a public peer

If you are mining on a public VPS, do not stop at outbound-only sync:

- keep `listen=1` in `rng.conf`
- open `8433/TCP` on your host firewall and cloud security group
- verify `rng-cli getnetworkinfo` eventually shows nonzero `connections_in` or a non-empty `localaddresses` list
- use the packaged helper `rng-install-public-node` or the systemd unit at [`contrib/init/rngd.service`](./contrib/init/rngd.service) for long-running hosts

See [doc/public-node.md](./doc/public-node.md) for a full public-node checklist.

### 2d. Persistent public mining under systemd

If you want a public VPS to stay mining across reboots, use the packaged helper:

```bash
sudo rng-install-public-node
sudo rng-install-public-miner --address rng1...
sudo systemctl enable --now rngd
```

Remove the mining override later with:

```bash
sudo rng-install-public-miner --remove
sudo systemctl restart rngd
```

### 2b. Verify node health and miner status

```bash
./scripts/doctor.sh

# or after install
rng-doctor
```

`rng-doctor` verifies the live genesis hash, checks peer connectivity, reports
sync state, shows whether mining is running in RandomX `fast` mode, and warns
when the node is mining privately but not reachable for inbound peers.

If `rng-start-miner` or `rng-load-bootstrap` reports an RPC credential mismatch
or says RPC never became ready, another local `rngd` is probably already using
the default RPC port `8432`. Stop that node or set a different `rpcport=` in
your `rng.conf` before retrying.

### 3. Manual wallet and payout address flow

```bash
rng-cli createwallet "miner"
ADDR=$(rng-cli -rpcwallet=miner getnewaddress)
echo "$ADDR"
```

### 4. Manual miner restart

```bash
rng-cli stop
sleep 5
nice -n 19 rngd -daemon -mine -mineaddress="$ADDR" -minethreads=7 -minerandomx=fast
```

### 5. Monitor mining

```bash
rng-cli getinternalmininginfo
```

## What The Installer Writes

The default `~/.rng/rng.conf` includes:

```ini
# RNG live-mainnet config
# Public peers below are operator-run seed nodes for the current low-peer network.
server=1
daemon=1
rpcuser=agent
rpcpassword=<generated>
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
listen=1
minerandomx=fast
addnode=95.111.239.142:8433
addnode=161.97.114.192:8433
addnode=185.218.126.23:8433
addnode=185.239.209.227:8433
```

The installer also drops helper commands into the install dir:

- `rng-load-bootstrap`
- `rng-start-miner`
- `rng-doctor`
- `rng-install-public-node`
- `rng-install-public-miner`

## Release Verification

Tagged releases ship with:

- deterministic binary tarballs from `scripts/build-release.sh`
- published `SHA256SUMS`
- GitHub build provenance attestations
- bundled `rng-install-public-node`, `rng-install-public-miner`, `rngd.service`, `rng.conf.example`, and public-node guidance for VPS operators

Verify a published release with:

```bash
./scripts/verify-release.sh --version <tag> --platform linux-x86_64
```

For the full release flow, see [doc/release-process.md](./doc/release-process.md).

## Bundled Snapshot

The repo ships:

- `bootstrap/rng-mainnet-15244-datadir.tar.gz`
- `bootstrap/rng-mainnet-15091.utxo`

| Field | Value |
|------|------|
| Chain bundle height | `15244` |
| Chain bundle SHA256 | `bf0bfad8054c73dc732391f2420d8b9f20f3c8276360745706783079a004c73d` |
| Height | `15091` |
| Base hash | `2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb` |
| Serialized UTXO hash | `9ca1b551b9837c0b0e9158436bac5051e4984d39f691e1374c4786a6c0ed5393` |
| Chain tx count | `15107` |
| File SHA256 | `622cd6255b8f44380fb9fa51809f783665d54e2d10d2e74135f00aa9ca34c882` |

The snapshot only loads after the base header is known to the node. If you use the
raw RPC path, first verify:

```bash
rng-cli getblockheader 2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb false
```

## Docker From The Repo

If you want to run a node from this checkout without installing binaries on the host:

```bash
docker compose up -d --build
docker compose exec rng rng-cli getblockhash 0
docker compose exec rng rng-cli getconnectioncount
```

The compose file now builds the local repo instead of pulling an old published image.
For long-running CPU mining, the host install path above is still the simplest option.

## Manual Source Build

### Linux

```bash
sudo apt install -y build-essential cmake git libboost-all-dev libssl-dev libevent-dev libsqlite3-dev
git clone https://github.com/happybigmtn/rng.git
cd rng
if [ ! -f src/crypto/randomx/src/randomx.h ]; then
  git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx
fi
# CMake rewrites the upstream RandomX salt to RNG's live mainnet salt automatically.
cmake -B build -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON
cmake --build build -j"$(nproc)"
sudo cp build/bin/rngd build/bin/rng-cli /usr/local/bin/
```

### macOS

```bash
brew install cmake boost openssl@3 libevent sqlite pkg-config
git clone https://github.com/happybigmtn/rng.git
cd rng
if [ ! -f src/crypto/randomx/src/randomx.h ]; then
  git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx
fi
# CMake rewrites the upstream RandomX salt to RNG's live mainnet salt automatically.
cmake -B build -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
cmake --build build -j"$(sysctl -n hw.ncpu)"
cp build/bin/rngd build/bin/rng-cli "$(brew --prefix)/bin/"
```

## RandomX Mode

RNG supports two RandomX modes:

- `fast`: about 2 GiB RAM, current live-network setting
- `light`: about 256 MiB RAM

All miners and validators on the same network must agree on the mode. If you mine with
the wrong mode, your blocks will be rejected.

Check your current mode:

```bash
rng-cli getinternalmininginfo | grep fast_mode
```

## Reset Old Chain Data

If you still have pre-reset RNG or legacy Botcoin chain state, wipe local chain data and
resync from the February 26, 2026 reset chain:

```bash
rng-cli stop || true
sleep 5
pkill -x rngd || true
rm -rf "$HOME/.rng/blocks" "$HOME/.rng/chainstate" "$HOME/.rng/indexes"
rm -f "$HOME/.rng/.lock" "$HOME/.rng/peers.dat" "$HOME/.rng/banlist.dat"
rm -f "$HOME/.rng/mempool.dat" "$HOME/.rng/anchors.dat"
rngd -daemon
sleep 10
rng-cli getblockhash 0
```

For a faster first sync after wiping data, run `./scripts/load-bootstrap.sh`.

## Network Facts

| Feature | Value |
|---------|-------|
| Algorithm | RandomX |
| Block target | 120 seconds |
| Difficulty retarget | Every block |
| Difficulty window | 720 blocks |
| Public P2P port | 8433 |
| Address prefix | `rng1` |
| Genesis message | `Life is a random number generator` |

## Internal Miner Flags

| Flag | Meaning |
|------|---------|
| `-mine` | Enable the internal miner |
| `-mineaddress=<addr>` | Required payout address |
| `-minethreads=<n>` | Number of mining threads |
| `-minerandomx=fast|light` | RandomX mode |
| `-minepriority=low|normal` | CPU priority |

## Upgrade Guide

If you are migrating from older `botcoin` or pre-reset RNG data, see
[`docs/upgrade-from-botcoin.md`](docs/upgrade-from-botcoin.md).
