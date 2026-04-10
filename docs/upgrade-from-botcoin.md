# Upgrade Guide: `botcoin` -> `rng` (Genesis Reset)

RNG mainnet was restarted from genesis on February 26, 2026 (v3.0.0).
This is not an in-place chain continuity upgrade.

## What carries forward

- Wallet keys and addresses
- Wallet files/backups
- RPC and operational tooling (with renamed binaries/config paths)

## What does not carry forward

- Old `botcoin`/pre-reset chain history
- Pre-reset UTXOs and balances

Balances start from zero on the reset RNG chain unless you mine or receive coins on
the new network.

## 1) Stop old services

```bash
botcoin-cli stop || true
rng-cli stop || true
sleep 5
pkill -x botcoind || true
pkill -x rngd || true
```

## 2) Back up wallets and keys

```bash
TS=$(date -u +%Y%m%d-%H%M%S)
mkdir -p "$HOME/rng-backup-$TS"
cp -a "$HOME/.botcoin/wallets" "$HOME/rng-backup-$TS/" 2>/dev/null || true
cp -a "$HOME/.botcoin/wallet.dat" "$HOME/rng-backup-$TS/" 2>/dev/null || true
```

Optional key export before migration:

```bash
botcoin-cli listwallets
botcoin-cli -rpcwallet=<wallet_name> dumpwallet "$HOME/rng-backup-$TS/<wallet_name>.dump"
```

## 3) Install RNG binaries and config

Use `rngd` / `rng-cli` and `~/.rng/rng.conf`. Current public peers are operator-run
seed nodes:

```ini
addnode=95.111.239.142:8433
addnode=161.97.114.192:8433
addnode=185.218.126.23:8433
addnode=185.239.209.227:8433
```

## 4) Wipe old chain state and start fresh

```bash
mkdir -p "$HOME/.rng"
rm -rf "$HOME/.rng/blocks" "$HOME/.rng/chainstate" "$HOME/.rng/indexes"
rm -f "$HOME/.rng/.lock" "$HOME/.rng/peers.dat" "$HOME/.rng/banlist.dat"
rm -f "$HOME/.rng/mempool.dat" "$HOME/.rng/anchors.dat"
rngd -daemon
```

## 5) Optional fast bootstrap

If you cloned this repo, you can load the bundled assumeutxo snapshot before
normal sync:

```bash
# from a repo checkout
./scripts/load-bootstrap.sh

# or after install
rng-load-bootstrap
rng-cli getchainstates
```

If blocks start downloading before the snapshot can load, wipe `blocks/` and
`chainstate/` and retry on a fresh datadir.

## 6) Verify reset genesis

```bash
rng-cli getblockhash 0
# expected: 83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4
```

## 7) (Optional) import dumped keys

If you exported wallet dumps:

```bash
rng-cli createwallet "restored"
rng-cli -rpcwallet=restored importwallet "$HOME/rng-backup-<timestamp>/<wallet_name>.dump"
```

Imported keys do not restore old-chain balances; they restore key ownership on the
new chain.

## 8) Start mining quickly

If you installed from this repo, the simplest path is:

```bash
rng-start-miner
rng-cli getinternalmininginfo
```

Low peer counts are normal on the current operator-seeded network. If
`getconnectioncount` stays at `0`, that usually means the public seed fleet is
temporarily down, not that you are on the wrong chain.

## Guardrails

- Do not run `botcoind` and `rngd` against the same active datadir.
- Keep backups of `~/.botcoin` until you complete the first restart and wallet import checks.
