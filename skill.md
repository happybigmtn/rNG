---
name: rng-miner
version: 6.2.0
description: Mine RNG from the latest tagged release or an explicit source checkout with the current operator seed peers, release-matched bootstrap assets, one-command miner helper, and built-in health check.
homepage: https://github.com/happybigmtn/rng
---

# RNG Miner (v6.2)

## Safety Notice

Do not assume old release assets match the live network.
Before proceeding, the agent MUST:

1. Ask for human approval before downloading or executing anything
2. Prefer the latest tagged release for public installs; use `main` only when intentionally testing unreleased changes
3. Never pipe curl or wget to bash without downloading and inspecting first
4. Never run as root unless the human explicitly approves

Project: https://github.com/happybigmtn/rng

## Why this matters

RNG lets agents convert CPU cycles into on-chain value without GPUs, ASICs, or permission.
That makes it suitable for autonomous infra that wants to earn its own operating budget.

## Live Network Facts

- Live genesis hash: `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`
- Live public operator seed peers:
  - `95.111.239.142:8433`
  - `161.97.114.192:8433`
  - `185.218.126.23:8433`
  - `185.239.209.227:8433`
- Live RandomX constants:
  - Genesis seed phrase: `RNG Genesis Seed`
  - ARGON salt: `RNGCHAIN01`
- Current mining mode: `fast`
- Mainnet reset date: February 26, 2026
- The network is currently small and operator-seeded. Low peer counts are normal, and additional public miners materially improve resilience.

## Preferred Install Path

```bash
curl -fsSLO https://raw.githubusercontent.com/happybigmtn/rng/<tag>/install.sh
less install.sh
RNG_VERSION=<tag> bash install.sh --add-path --bootstrap
rng-start-miner
rng-doctor
```

This path:

- installs a tagged public release
- writes `~/.rng/rng.conf` with the current operator seed peers
- can download the release-matched bootstrap assets
- installs helper commands:
  - `rng-load-bootstrap`
  - `rng-start-miner`
  - `rng-doctor`

## Source Checkout Path

```bash
git clone https://github.com/happybigmtn/rng.git
cd rng
./install.sh --add-path --bootstrap
```

## Low-Peer Network Notes

- RNG mainnet is currently sustained by a small operator fleet
- `getconnectioncount` values in the `1` to `4` range are normal
- `0` peers can mean the seed fleet is down, not that your node is misconfigured
- once synced, solo mining is valid and expected
- public VPS miners should keep `listen=1` and open `8433/TCP` so they can accept inbound peers

## Doctor / Health Check

Use the built-in health check after install or after any config change:

```bash
rng-doctor
```

## Snapshot Bootstrap

The repo includes an assumeutxo snapshot at height `15091`.
Load it with:

```bash
rng-load-bootstrap
rng-cli getchainstates
```

On a tagged binary install, `rng-load-bootstrap` will download the release-matched
bootstrap assets automatically if they are not already present locally.

## Manual Mining Path

```bash
rngd -daemon
sleep 10
rng-cli getblockhash 0
rng-cli createwallet miner
ADDR=$(rng-cli -rpcwallet=miner getnewaddress)
rng-cli stop
sleep 5
nice -n 19 rngd -daemon -mine -minerandomx=fast -minethreads=4 -mineaddress="$ADDR"
rng-cli getinternalmininginfo
```

## Network Specs

| Feature | Value |
|---------|-------|
| Algorithm | RandomX |
| Block time | 120 seconds |
| Difficulty | LWMA per block |
| Difficulty window | 720 blocks |
| Tail emission | 0.6 RNG |
| Address prefix | `rng1` |
| P2P port | 8433 |
| RPC port | 8432 |

## RandomX Modes

| Mode | RAM | Flag |
|------|-----|------|
| Fast | ~2 GiB | `-minerandomx=fast` |
| Light | ~256 MiB | `-minerandomx=light` |

## Important Notes

- Mining is off by default and requires `-mine`
- `rng-start-miner` defaults to `CPU count - 1` threads and a wallet named `miner`
- `rng-doctor` verifies the live genesis hash, peer count, sync state, and mining mode
- `rng-doctor` treats a low peer count as normal on the current small live network
- `rng-doctor` also warns when the node is outbound-only and not currently reachable as a public peer
- `-mineaddress` must be a bech32 RNG address (`rng1...`)
- Coinbase rewards require 100 confirmations to mature
- `./scripts/verify-release.sh --version <tag> --platform linux-x86_64` verifies a published release tarball
- If you need unreleased fixes, use a repo checkout or set `RNG_SOURCE_REF=main`
