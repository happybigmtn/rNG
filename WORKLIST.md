# Worklist

## Spec Truthfulness Follow-Up

- [Optional][CHKPT-01] Decide whether to add a repository-root `install.sh`/`scripts/install.sh`
  for local user installs, or keep operator installation limited to release
  tarballs, Docker, and the public-node/public-miner scripts.
## Operations

- [required] Repair `contabo-validator-01` startup: `/root/.rng/settings.json` is zero bytes as of 2026-04-13 03:52Z, causing `rngd.service` to crash-loop before RPC starts. After repair, verify the daemon reaches the fleet tip before allowing `mine=1` to resume active mining.

## Consensus Follow-Up

- [required] Reconcile the monetary-cap documentation and tests with live consensus:
  RNG currently uses 50 RNG initial subsidy and a 2,100,000-block halving
  interval, which implies a total subsidy schedule around 210M RNG while
  `MAX_MONEY` remains 21M RNG as an amount sanity cap.

## Test Harness Follow-Up

- [Optional][CHKPT-07A] Register `feature_sharepool_relay.py` in the functional
  test runner or document the direct build-tree invocation. In this out-of-source
  build, the repository-root runner looked for a missing `test/config.ini`, and
  the build-tree runner did not list `feature_sharepool_relay`.

## Sharepool Follow-Up

- [Optional][POOL-06-GATE] Re-run relay viability at the confirmed 1-second share cadence after share-producing miner/RPC diagnostics exist. Include the broader historical Plan 006 churn, star-topology convergence, and minority-window experiments once `submitshare`, `getsharechaininfo`, and reward-window inspection are live.
