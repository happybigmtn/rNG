rNG

**The cryptocurrency designed for AI agents.** CPU-mineable with RandomX, no special hardware required.

[![GitHub Release](https://img.shields.io/github/v/release/happybigmtn/rng)](https://github.com/happybigmtn/rng/releases)
[![Docker](https://img.shields.io/badge/docker-ghcr.io%2Fhappybigmtn%2Frng-blue)](https://ghcr.io/happybigmtn/rng)

## Quick Install

**🔒 Verify-first (recommended):**
```bash
VERSION=v3.0.0
curl -fsSLO "https://raw.githubusercontent.com/happybigmtn/rng/${VERSION}/install.sh"
less install.sh  # Inspect
bash install.sh --add-path
```

**⚡ One-liner (if you trust us):**
```bash
curl -fsSL https://raw.githubusercontent.com/happybigmtn/rng/master/install.sh | bash
```

## What is RNG?

| Feature | Value |
|---------|-------|
| Algorithm | RandomX (CPU-mineable, like Monero) |
| Block time | 120 seconds |
| Difficulty adjustment | Every block (Monero-style LWMA, 720-block window) |
| Block reward | 50 RNG (halving schedule) + 0.6 RNG tail emission forever |
| Smallest unit | 1 RNG = 100,000,000 roshi |
| Max supply | 1 billion RNG |
| Network | Live mainnet with real peers |
| Genesis restart | February 26, 2026 (v3.0.0 reset) |

Genesis message: `"Life is a random number generator"`

> **Note:** The chain was restarted from genesis on February 26, 2026 for the RNG v3.0.0 network reset.
> All prior chain history is invalidated.
> Balances from pre-reset chain history do not carry over to this network.

**No premine. No ASICs. No permission needed.**

## Installation Options

### One-Line Install
```bash
curl -fsSL https://raw.githubusercontent.com/happybigmtn/rng/master/install.sh | bash
```

### Docker
```bash
docker pull ghcr.io/happybigmtn/rng:v3.0.0
docker run -d --name rng --cpus=0.5 -v "$HOME/.rng:/home/rng/.rng" ghcr.io/happybigmtn/rng:v3.0.0
docker exec rng rng-cli getblockchaininfo
```

### Docker Compose
```bash
curl -fsSLO https://raw.githubusercontent.com/happybigmtn/rng/master/docker-compose.yml
docker-compose up -d
```

### Manual Binary Download
```bash
VERSION=v3.0.0
PLATFORM=linux-x86_64  # also: linux-arm64, macos-x86_64, macos-arm64
wget "https://github.com/happybigmtn/rng/releases/download/${VERSION}/rng-${VERSION}-${PLATFORM}.tar.gz"
tar -xzf "rng-${VERSION}-${PLATFORM}.tar.gz" && cd release
sha256sum -c SHA256SUMS
mkdir -p ~/.local/bin && cp rngd rng-cli ~/.local/bin/
```

### WSL (Windows Subsystem for Linux)
WSL2 behaves like Linux for RNG purposes.

- Use **linux-x86_64** release binaries, or build from source under Ubuntu/WSL.
- Avoid Nix-built artifacts unless your WSL environment includes `/nix/store`.

### Build from Source (Linux)
```bash
sudo apt install -y build-essential cmake git libboost-all-dev libssl-dev libevent-dev libsqlite3-dev
git clone https://github.com/happybigmtn/rng.git && cd rng
git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx
cmake -B build -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON
cmake --build build -j$(nproc)
sudo cp build/bin/rngd build/bin/rng-cli /usr/local/bin/
```

### Build from Source (macOS)
```bash
brew install cmake boost openssl@3 libevent sqlite pkg-config
git clone https://github.com/happybigmtn/rng.git && cd rng
git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx
cmake -B build -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build build -j$(sysctl -n hw.ncpu)
cp build/bin/rngd build/bin/rng-cli $(brew --prefix)/bin/
```

## Quick Start

### Configure
```bash
mkdir -p ~/.rng; RPCPASS=$(openssl rand -hex 16)
cat > ~/.rng/rng.conf << EOF
server=1
daemon=1
rpcuser=agent
rpcpassword=$RPCPASS
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
addnode=185.218.126.23:8433
addnode=95.111.239.142:8433
addnode=161.97.114.192:8433
EOF
```

### Start & Verify
```bash
rngd -daemon; sleep 10
rng-cli getblockchaininfo | grep -E '"chain"|"blocks"'
rng-cli getconnectioncount  # Expected: 5-10 peers
```

### Create Wallet & Mine (Internal Miner)
```bash
rng-cli createwallet "miner"
ADDR=$(rng-cli -rpcwallet=miner getnewaddress)

# Restart daemon with mining enabled (required flags)
rng-cli stop; sleep 5
nice -n 19 rngd -daemon -mine -mineaddress="$ADDR" -minethreads=7

# Monitor
rng-cli getinternalmininginfo
```

### Stop
```bash
rng-cli stop
```

## RandomX Mode: FAST vs LIGHT (critical)

RNG uses RandomX. There are **two modes**:
- `fast` (≈2GB RAM) — **default**
- `light` (≈256MB RAM)

⚠️ **All nodes on a network must agree on the RandomX mode.**
If a miner produces blocks in one mode and validators are verifying in the other, peers will reject headers/blocks with errors like:
- `header with invalid proof of work`
- stuck sync (often halting around the first incompatible height)

Check your node’s mode:
```bash
rng-cli getinternalmininginfo | grep fast_mode
```

Explicitly set the mode on *every* node (miner + validators):
```bash
# FAST mode (recommended if you want to keep historical chain data)
rngd -daemon -minerandomx=fast

# LIGHT mode (lower RAM; requires everyone to use light)
rngd -daemon -minerandomx=light
```

## Genesis Reset Recovery (v3.0.0+)

If your node still has pre-reset chain data, do a clean wipe of local chain history and
resync from RNG genesis.

1) Stop the daemon:
```bash
rng-cli stop || true
sleep 5
pkill -x rngd || true
```

2) Back up wallets only (optional, recommended):
```bash
TS=$(date -u +%Y%m%d-%H%M%S)
mkdir -p "$HOME/.rng-backups/$TS"
cp -a "$HOME/.rng/wallets" "$HOME/.rng-backups/$TS/" 2>/dev/null || true
cp -a "$HOME/.rng/wallet.dat" "$HOME/.rng-backups/$TS/" 2>/dev/null || true
```

3) Wipe local chain state/history:
```bash
rm -rf "$HOME/.rng/blocks" "$HOME/.rng/chainstate" "$HOME/.rng/indexes"
rm -f "$HOME/.rng/.lock" "$HOME/.rng/peers.dat" "$HOME/.rng/banlist.dat"
rm -f "$HOME/.rng/mempool.dat" "$HOME/.rng/anchors.dat"
```

4) Restart with known peers:
```bash
rngd -daemon \
  -addnode=185.218.126.23:8433 \
  -addnode=95.111.239.142:8433 \
  -addnode=161.97.114.192:8433
```

5) Verify you are on the reset network:
```bash
rng-cli getblockhash 0
# expected: 83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4
```

Notes:
- Pre-reset UTXOs are not valid on the reset chain.
- Wallet keys can be kept/imported, but balances start from zero on the new genesis chain.

## How RNG Differs from Bitcoin

**`generatetoaddress` works on mainnet.** This is intentional.

| Aspect | Bitcoin Core | RNG |
|--------|--------------|---------|
| `generatetoaddress` | Regtest only | **Mainnet supported** |
| PoW Algorithm | SHA256 (ASIC-dominated) | RandomX (CPU-mineable) |
| Solo mining | Not viable | Viable for agents |

RandomX is CPU-friendly, making solo mining practical. We enabled `generatetoaddress` on mainnet so agents can mine without pool infrastructure.

## Installer Flags

| Flag | Description |
|------|-------------|
| `--force` | Reinstall even if already present |
| `--add-path` | Add install dir to PATH (modifies shell rc) |
| `--no-verify` | Skip checksum verification (NOT recommended) |
| `--no-config` | Don't create config file |
| `-h, --help` | Show help |

## Trusted Distribution

- ✅ **Docker:** `ghcr.io/happybigmtn/rng:v3.0.0` (multi-arch)
- ✅ **Binaries:** Linux x86_64/arm64, macOS Intel/Apple Silicon
- ✅ **Checksums:** SHA256SUMS included
- ✅ **No sudo:** Installs to `~/.local/bin` by default
- ⚠️ **Windows:** Use WSL2 or Docker

## Commands Reference

| Command | Description |
|---------|-------------|
| `getblockchaininfo` | Network status, block height |
| `getconnectioncount` | Number of peers |
| `getbalance` | Wallet balance |
| `getnewaddress` | Generate receive address |
| `generatetoaddress N ADDR` | Mine N blocks |
| `sendtoaddress ADDR AMT` | Send coins |
| `stop` | Stop daemon |

## AI Agent Skill

For AI agents, see the full skill at:
- **ClawHub:** https://clawhub.ai/happybigmtn/rng-miner
- **Local:** `~/.openclaw/skills/rng-miner/SKILL.md`

---

*01100110 01110010 01100101 01100101*

The revolution will not be centralized.

## Internal Miner (v3.0.0)

RNG includes a high-performance internal miner with multi-threaded RandomX hashing.

### Quick Start
```bash
rngd -daemon -mine -mineaddress=YOUR_RNG1_ADDRESS -minethreads=8
```

### Mining Options

| Flag | Description | Default |
|------|-------------|---------|
| `-mine` | Enable internal miner | OFF |
| `-mineaddress=<addr>` | Payout address (required) | - |
| `-minethreads=<n>` | Number of mining threads | - |
| `-minerandomx=fast\|light` | RandomX mode (fast=2GB RAM) | fast |
| `-minepriority=low\|normal` | CPU priority | low |

### Check Mining Status
```bash
rng-cli getinternalmininginfo
```

Returns:
```json
{
  "running": true,
  "threads": 8,
  "hashrate": 1200.5,
  "blocks_found": 42,
  "stale_blocks": 3,
  "uptime": 3600,
  "fast_mode": true
}
```

### Architecture
- **Coordinator thread**: Creates block templates, monitors chain tip
- **Worker threads**: Pure nonce grinding with stride pattern (no locks)
- **Event-driven**: Reacts instantly to new blocks via ValidationInterface
- **Per-thread RandomX VMs**: Lock-free hashing, ~1200 H/s per 10 threads

### Recommended Setup
```bash
# 8-core machine
rngd -daemon -mine -mineaddress=rng1q... -minethreads=7

# Leave 1 core for system/networking
```
