# Worklist

## Spec Truthfulness Follow-Up

- [CHKPT-01] `specs/consensus.md` and `specs/120426-consensus-chain-rules.md` still document a 0.6 RNG tail emission and unbounded/tail-emission supply. Live `src/validation.cpp` has no tail floor: `GetBlockSubsidy()` halves `50 * COIN` and returns `0` for `halvings >= 64`; `src/consensus/amount.h` still sets `MAX_MONEY = 21000000 * COIN`, not 1,000,000,000 RNG.
- [CHKPT-01] `specs/120426-network-identity.md` and `specs/120426-wallet-rpc-surface.md` state protocol version `70100`; live `src/node/protocol_version.h` sets `PROTOCOL_VERSION = 70016` and `WTXID_RELAY_VERSION = 70016`.
- [CHKPT-01] `specs/120426-network-identity.md` states mainnet DNS seeds `seed1.rng.network`, `seed2.rng.network`, and `seed3.rng.network`; live `src/kernel/chainparams.cpp` mainnet only has four operator IPv4 `vSeeds` (`95.111.239.142`, `161.97.114.192`, `185.218.126.23`, `185.239.209.227`).
- [CHKPT-01] `specs/120426-qsb-operator-support.md` still says QSB code is only in a separate branch and absent from the inspected checkout. Live `main` contains `src/script/qsb.{h,cpp}`, `src/node/qsb_pool.{h,cpp}`, `src/node/qsb_validation.{h,cpp}`, `src/rpc/qsb.cpp`, `contrib/qsb/`, and QSB functional/unit tests.
- [CHKPT-01] `specs/120426-operator-onboarding.md` and `specs/120426-release-distribution.md` describe repository-root `Dockerfile`, `docker-compose.yml`, `bootstrap/` assets, and `scripts/install.sh`/`install.sh`; none of those files/directories exist in the live checkout. `scripts/load-bootstrap.sh` can fetch release assets and looks for `bootstrap/`, but no bundled assets are tracked.
- [CHKPT-01] `specs/120426-release-distribution.md` describes container build verification and Dockerfile-derived dependencies even though the live checkout has no root `Dockerfile`, and references release process evidence from `CHANGES.md` even though no `CHANGES.md` is tracked.
- [CHKPT-01] `specs/120426-consensus-chain-rules.md`, `specs/120426-wallet-rpc-surface.md`, and parts of `specs/120426-qsb-operator-support.md` describe the current tree as Bitcoin Core v29.0-derived or pre-merge. Live `README.md` says the current tree is based on Bitcoin Core `30.2`; QSB is merged.
- [CHKPT-01] Sharepool specs use `1815/2016` as "95%" in planned activation text. Live mainnet BIP9 entries use `1815` of `2016` as `90%`; if a future sharepool spec wants 95%, the threshold must not be copied as `1815`.
- [POOL-01] Historical sharepool planning text still says `share_target = block_target / 12` and describes a "witness v2 OP_RETURN" commitment. `specs/sharepool.md` corrects the target arithmetic for RNG's `hash <= target` PoW rule and separates the proposed spendable witness-v2 commitment output from optional OP_RETURN metadata.

## Operations

- Repair `contabo-validator-01` startup: `/root/.rng/settings.json` is zero bytes as of 2026-04-13 03:52Z, causing `rngd.service` to crash-loop before RPC starts. After repair, verify the daemon reaches the fleet tip before allowing `mine=1` to resume active mining.

## Sharepool Follow-Up

- [POOL-06-GATE] Re-run relay viability at the confirmed 1-second share cadence after share-producing miner/RPC diagnostics exist. Include the broader historical Plan 006 churn, star-topology convergence, and minority-window experiments once `submitshare`, `getsharechaininfo`, and reward-window inspection are live.
