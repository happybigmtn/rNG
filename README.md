# RNG

CPU-mineable cryptocurrency for AI agents.

## Live Network

As of March 19, 2026:

- The `main` branch is the live-network reference.
- RNG mainnet was restarted from genesis on February 26, 2026.
- Expected genesis hash: `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`
- Public operator seed peers: `95.111.239.142:8433`, `161.97.114.192:8433`, `185.218.126.23:8433`, `185.239.209.227:8433`
- Live mainnet RandomX constants: genesis seed phrase `RNG Genesis Seed`, ARGON salt `RNGCHAIN01`
- Live miners use RandomX `fast` mode.
- The network is currently operator-seeded. Low peer counts and zero third-party miners are normal.
- Source builds from `main` were verified against the first post-reset live headers on March 19, 2026.

If you are mining today, use `./install.sh` or build from source. Tagged release binaries
and container images may lag behind the live network.

## Mine From This Repo

### Option A: Clone and install

```bash
git clone https://github.com/happybigmtn/rng.git
cd rng
./install.sh --add-path --bootstrap
rng-start-miner
```

The installer builds the live `main` branch by default, creates `~/.rng/rng.conf`,
seeds it with the current operator seed peers, can load the bundled assumeutxo snapshot, and
installs helper commands `rng-load-bootstrap`, `rng-start-miner`, and `rng-doctor`.

### Option B: Verify-first installer

```bash
curl -fsSLO https://raw.githubusercontent.com/happybigmtn/rng/main/install.sh
less install.sh
bash install.sh --add-path --bootstrap
```

If you use the repo checkout path above, `--bootstrap` is the fastest first-sync option.
After install, start mining with:

```bash
rng-start-miner
rng-doctor
```

## Quick Start

### Fastest path from a repo checkout

```bash
./install.sh --add-path --bootstrap
rng-start-miner
rng-doctor
```

### Fast bootstrap from the bundled snapshot

The repo includes a verified assumeutxo snapshot at height `15091`:

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

The helper waits until the snapshot base header is present, then runs
`loadtxoutset`. On a fresh node this usually means waiting for header sync first.
If the datadir starts downloading blocks before the base header arrives, rerun the
snapshot load on a fresh datadir after wiping `blocks/` and `chainstate/`.

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

### 2b. Verify node health and miner status

```bash
./scripts/doctor.sh

# or after install
rng-doctor
```

`rng-doctor` verifies the live genesis hash, checks peer connectivity, reports
sync state, and shows whether mining is running in RandomX `fast` mode. On the
current operator-seeded network, low peer counts are expected.

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
minerandomx=fast
addnode=95.111.239.142:8433
addnode=161.97.114.192:8433
addnode=185.218.126.23:8433
addnode=185.239.209.227:8433
```

The installer also drops two helper commands into the install dir:

- `rng-load-bootstrap`
- `rng-start-miner`
- `rng-doctor`

## Bundled Snapshot

The repo ships `bootstrap/rng-mainnet-15091.utxo`, exported from the live chain on
March 19, 2026.

| Field | Value |
|------|------|
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
