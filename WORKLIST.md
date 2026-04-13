# Worklist

## Spec Truthfulness Follow-Up

- [Optional][CHKPT-01] Decide whether to add a repository-root `install.sh`/`scripts/install.sh`
  for local user installs, or keep operator installation limited to release
  tarballs, Docker, and the public-node/public-miner scripts.
## Operations

- [required] Repair `contabo-validator-01` startup: `/root/.rng/settings.json` is zero bytes as of 2026-04-13 03:52Z, causing `rngd.service` to crash-loop before RPC starts. After repair, verify the daemon reaches the fleet tip before allowing `mine=1` to resume active mining.

## Sharepool Follow-Up

- [Optional][POOL-06-GATE] Re-run relay viability at the confirmed 1-second share cadence after share-producing miner/RPC diagnostics exist. Include the broader historical Plan 006 churn, star-topology convergence, and minority-window experiments once `submitshare`, `getsharechaininfo`, and reward-window inspection are live.
