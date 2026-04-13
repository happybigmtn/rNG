# Completed Tasks

## 2026-04-13

- `TRUTH-01` — Added canonical `specs/randomx.md` documenting the fixed genesis seed for all block heights, cleaned stale RandomX schedule claims from `specs/120426-randomx-pow.md`, and corrected the RandomX unit-test comment that cited the missing spec as the source for legacy constants. Commit: `1663ead0eb43d22b1f6788837bf243d26bf510a8`.
  Validation: `if grep -i "2048\|rotation\|epoch" specs/randomx.md; then exit 1; else echo "no stale seed-schedule terms in specs/randomx.md"; fi`; `grep "fixed\|genesis.*seed\|all.*height" specs/randomx.md`; `cmake --build build -j$(nproc)`; `build/bin/test_bitcoin --run_test=randomx_tests`.

- `SYNC-01` — Confirmed branch synchronization was already satisfied on `main`: QSB source, RPC, miner integration, Python builder, and tests are present. Removed the stale pending queue item without modifying QSB logic. Evidence commits: `bf58671eb3` (QSB support), `8e33f25b30` (merge to main), `3fe83d3c4a` (current queue baseline before cleanup).
  Validation: `cmake --build build -j$(nproc)`; `build/bin/test_bitcoin --run_test=qsb_tests`; `python3 test/functional/feature_qsb_builder.py --configfile=build/test/config.ini`; `python3 test/functional/feature_qsb_rpc.py --configfile=build/test/config.ini`; `python3 test/functional/feature_qsb_mining.py --configfile=build/test/config.ini`.

- `SYNC-02` — Completed the read-only post-merge fleet checkpoint and documented the current validator state in `EXECPLAN.md`. Validators 02/04/05 are synced and mining-enabled; validator 01 is not healthy because `rngd.service` crash-loops on a zero-byte `settings.json`. Queue baseline commit before this checkpoint: `86d08b85b0`.
  Validation: read-only SSH probes against `contabo-validator-01`, `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05` using `/root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng getblockchaininfo`; `getmininginfo`; config grep for `mine`, `minimumchainwork`, and `enableqsboperator`; validator-01 `journalctl -u rngd`, filesystem usage, and `python3 -m json.tool /root/.rng/settings.json`.
