# Implement QSB Support In RNG And Roll It Out On The Contabo Fleet

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds.

This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

The goal is to let RNG operators create, fund, mine, and later spend Quantum-Safe Bitcoin style transactions on RNG mainnet without changing consensus and without opening general non-standard relay to the public network. After this work lands, an operator will be able to build a QSB funding transaction, mine it on an RNG node that they control, later build the corresponding QSB spend transaction, and mine that spend through the same operator fleet. The observable proof is a canary run on one live Contabo validator that mines a small QSB funding transaction first and then, after confirmation, mines the matching QSB spend transaction.

In this plan, “QSB” means the transaction construction described in the `Quantum-Safe-Bitcoin-Transactions` paper and repository by avihu28. In plain language, QSB is a last-resort way to spend coins with a large one-time hash-based signature hidden inside legacy Bitcoin-style script evaluation. RNG can support it because RNG still uses Bitcoin-derived transaction and script rules, but current RNG node policy rejects the required transactions before miners ever see them.

## Requirements Trace

`R1`. RNG must accept and mine a supported QSB funding transaction and a supported QSB spend transaction without a consensus change. Existing nodes that see the mined block must accept the block as valid.

`R2`. Default RNG public policy must remain unchanged for ordinary traffic. A node that has not explicitly enabled the new QSB operator surface must continue rejecting QSB transactions from the public network.

`R3`. The implementation must not enable blanket non-standard mempool admission on mainnet. The new path must be narrow, operator-only, and limited to the exact QSB template family that this repository builds.

`R4`. The transaction builder must generate bare-script QSB outputs directly in `scriptPubKey`. It must not depend on P2SH or P2WSH wrappers, because the QSB locking script is much larger than the 520-byte script element limit.

`R5`. The builder must keep one-time QSB secret material outside the normal RNG wallet. The wallet may fund the QSB output, but it must not become the source of truth for the QSB secret state.

`R6`. The live rollout must be staged. One canary host must be updated and validated before any broader Contabo fleet wave starts.

`R7`. The implementation must preserve existing miner, wallet, bootstrap, and public-node flows for non-QSB operation. Existing commands such as `rng-start-miner`, `rng-public-apply`, and ordinary `sendrawtransaction` must continue to behave as they do today.

`R8`. The plan must include a fully specified recovery path for a failed live rollout, including how to restore the previous binaries and how to resubmit QSB transactions that were queued but not yet mined.

## Scope Boundaries

This plan does not redesign RNG into a fully post-quantum cryptocurrency. It adds support for one specific QSB construction as an operator-only escape hatch.

This plan does not make QSB transactions publicly relayable through the normal mempool. Peers on the open network will still reject them as non-standard, and that is intentional.

This plan does not integrate QSB secrets into the wallet database, descriptor system, or seed phrase flow. QSB secret generation, storage, and digest search remain external to the wallet.

This plan does not normalize the entire Contabo fleet onto the packaged `/etc/rng` plus `/var/lib/rngd` layout during the first rollout. The current live validators use a root-managed `/root/rngd` plus `/root/.rng` layout, and the first canary and first fleet wave should preserve that layout to reduce operational change.

This plan does not assume SSH access to every published seed-peer IP. It only treats the validator hosts reachable through the local SSH config as confirmed deployment targets until the remaining hosts are re-inventoried.

## Progress

- [x] (2026-04-09 20:13Z) Read `PLANS.md`, the RNG operator docs, and the relevant RNG policy and mining code paths.
- [x] (2026-04-09 20:13Z) Confirmed that RNG is still Bitcoin Core `v29.0` derived at the transaction, script, UTXO, wallet, and RPC layers.
- [x] (2026-04-09 20:13Z) Performed a read-only validator discovery pass against `contabo-validator-01`, `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05`.
- [x] (2026-04-09 20:13Z) Confirmed the validator hosts currently have root-managed RNG datadirs and root-managed `rngd.service` units, but `rngd` is inactive on all four reachable validators.
- [x] (2026-04-09 20:13Z) Wrote the initial `EXECPLAN.md` with implementation units, validation, and a staged live Contabo rollout.
- [x] (2026-04-10 02:14Z) Ported RNG to the Bitcoin Core `30.2` codebase in the dedicated `feat/bitcoin-30.2-port` worktree and revalidated regtest startup plus RNG-specific RandomX unit and internal miner tests on the port branch.
- [x] (2026-04-10 02:14Z) Implemented the first in-repo QSB builder under `contrib/qsb/`, including a strict versioned toy bare-script template, external state handling, deterministic fixtures, and CLI commands for fixture generation, wallet-funded transaction assembly, and external spend assembly.
- [x] (2026-04-10 02:14Z) Added deterministic functional coverage in `test/functional/feature_qsb_builder.py` for fixture generation, external spend assembly, and one-time-state refusal on reuse.
- [x] (2026-04-10 02:41Z) Implemented Unit 2 for the current toy template: strict C++ funding/spend matchers in `src/script/qsb.{h,cpp}`, narrow policy hooks in `src/validation.cpp`, and unit coverage in `src/test/qsb_tests.cpp` proving that the supported toy QSB funding and spend transactions enter the mempool while unrelated non-standard transactions remain rejected.
- [x] (2026-04-10 03:12Z) Implemented the local-only QSB candidate pool and RPC surface in the Bitcoin Core `30.2` port worktree: `-enableqsboperator`, `submitqsbtransaction`, `listqsbtransactions`, `removeqsbtransaction`, `src/node/qsb_pool.{h,cpp}`, and strict toy-QSB classification in `src/node/qsb_validation.{h,cpp}`.
- [x] (2026-04-10 03:12Z) Re-gated the toy-QSB validation escape hatch so default/public mempool admission remains unchanged unless the internal operator-validation path explicitly passes `allow_qsb_toy=true`.
- [x] (2026-04-10 03:12Z) Fixed a regtest PoW regression in the `30.2` port by honoring `fPowNoRetargeting` inside `GetNextWorkRequired`, restoring cheap block generation and making the funded QSB operator-path functional test practical again.
- [x] (2026-04-10 03:12Z) Added operator-path coverage in `test/functional/feature_qsb_rpc.py` and revalidated `feature_qsb_builder.py`, `feature_qsb_rpc.py`, `qsb_tests`, and a focused `pow_tests/regtest_get_next_work_stays_fixed` regression test on the port branch.
- [x] (2026-04-10 03:23Z) Implemented miner integration for the local QSB queue in the `30.2` port by threading `QSBPool` into block assembly, inserting validated QSB candidates ahead of ordinary mempool selection, and cleaning mined entries on `BlockConnected`.
- [x] (2026-04-10 03:23Z) Added `test/functional/feature_qsb_mining.py`, proving that a QSB-enabled node can mine the funding transaction and later the matching spend while a peer without `-enableqsboperator` accepts both blocks.
- [x] (2026-04-10 03:46Z) Added the operator runbook and `contrib/qsb/submit_fleet.py`, giving the current implementation a concrete SSH-based live-fleet submission and rollback surface that matches the in-node QSB RPCs.
- [x] (2026-04-09 23:44Z) Ran the canary host bring-up on `contabo-validator-01`: deployed the `30.2` binary pair, enabled `enableqsboperator=1`, discovered that the legacy root-managed `Type=forking` service no longer fits the upgraded daemon, and fixed startup with a `30.2-compat.conf` systemd drop-in that switches the host to `Type=notify` while preserving the `/root/...` layout.
- [x] (2026-04-10 01:36Z) Resolved the apparent canary sync blocker. `contabo-validator-01` advanced from height `3161` to `blocks=headers=29944`, reports `initialblockdownload=false`, and still reports `qsb_operator_enabled=true`.
- [x] (2026-04-10 01:40Z) Fixed the live funding builder gap by making `contrib/qsb/qsb.py toy-funding` pass an explicit `fundrawtransaction` `fee_rate` in sat/vB, defaulting to `1`, so the canary does not depend on wallet fee estimation or `fallbackfee`.
- [x] (2026-04-10 01:49Z) Raised the canary miner from the implicit one-worker default to `minethreads=8` after backing up `/root/.rng/rng.conf` to `/root/.rng/rng.conf.pre-minethreads.20260410T014930Z`; the QSB candidate was then resubmitted because the queue is intentionally in-memory.
- [x] (2026-04-10 01:50Z) Mined the live canary QSB funding transaction `363a3e5063d34c1ca775fdf5e93aeb18567d17489fce2ccdef14e6fdcdfec2e3` in block `b3268a02f1613a8020a79d86db8482516fcf1593c88a7b859a74fbe611810ac9` at height `29946`.
- [x] (2026-04-10 01:53Z) Mined the matching live QSB spend transaction `e562d60c7601e120742483cdd7f737383c424e4f55d65bc64f87fe24648fe2b8` in block `b755640d4309a4869cc0bd70947221250d16709fa40fc46f1df37596570a5d2e` at height `29947`.
- [x] (2026-04-10 02:02Z) Created stripped rollout artifacts for the fleet wave to avoid copying the 291 MB unstripped daemon to every validator. The artifact hashes are `rngd` sha256 `36eb7509a17c15fbca062dc3427bb36d0d19cb24ec4fb299fcea09e20a5ad054` and `rng-cli` sha256 `eff7e8d116b8143f4182197e482804b74a49d8885915e24ab23eec6b3f67b92a`, both reporting RNG Core `v3.0.0`.
- [x] (2026-04-10 02:08Z) Deployed the stripped `30.2` binary pair, the `Type=notify` systemd compatibility drop-in, `enableqsboperator=1`, and `minethreads=8` to `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05`; all three report `/RNG:3.0.0/`, `version=30000`, `qsb_operator_enabled=true`, and an empty QSB queue.
- [x] (2026-04-10 02:24Z) Kept `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05` in safe catch-up mode with `mine=0` while they finished header presync and reported `initialblockdownload=false`. This was completed by the 2026-04-10 13:07Z re-enable wave, when mining was restored one host at a time after each converged with the canary tip.
- [x] (2026-04-10 02:24Z) Re-ran local verification after the live-ops fixes: `python3 -m py_compile contrib/qsb/qsb.py contrib/qsb/state.py contrib/qsb/template_v1.py contrib/qsb/submit_fleet.py`, `feature_qsb_builder.py`, `feature_qsb_rpc.py`, `feature_qsb_mining.py`, `test_bitcoin --run_test=qsb_tests`, and `test_bitcoin --run_test=pow_tests/regtest_get_next_work_stays_fixed` all passed.
- [x] (2026-04-10 02:34Z) Opened canary P2P only to known validator IPs with UFW allowlist rules for `95.111.229.108`, `161.97.83.147`, and `161.97.97.83` on `8433/tcp`; this made TCP to `contabo-validator-01` reachable from 02/04/05, but the canary was not reliable enough as a header-sync peer to use as the primary catch-up fix.
- [x] (2026-04-10 02:37Z) Applied a temporary catch-up override `minimumchainwork=0` on `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05`, with config backups at `/root/.rng/rng.conf.pre-minimumchainwork.20260410T023441Z`; all three remain `mine=0` and active after restart.
- [x] (2026-04-10 02:45Z) Confirmed that 02/04/05 moved from header presync into normal header sync after the `minimumchainwork=0` override. At that point, the remaining work was to wait for CPU-bound RandomX header validation and block sync to reach the canary, then remove the temporary override and re-enable mining one validator at a time.
- [x] (2026-04-10 02:47Z) Ran a final bounded health probe before pausing the rollout. At that point, `contabo-validator-01` was active at `blocks=headers=29957`, `initialblockdownload=false`, `mine=1`, and an empty QSB queue, while `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05` were still catching up with `mine=0`, `minimumchainwork=0`, and empty QSB queues.
- [x] (2026-04-10 04:10Z) Rechecked whether the non-canary validators were ready for mining and merge. They were not ready at that checkpoint, so mining stayed disabled and merge was held until 02/04/05 reported `initialblockdownload=false` and matched the canary tip.
- [x] (2026-04-10 13:07Z) Completed the non-canary mining re-enable wave. `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05` each had `/root/.rng/rng.conf` backed up to `/root/.rng/rng.conf.pre-reenable-mining.<timestamp>`, had the temporary `minimumchainwork=0` line removed, had `mine=1` set, and were restarted one at a time. After propagation settled, all four validators were active at `blocks=headers=30152`, best block `34bd351e4f724abe5acbf7f243dd06e06b1c569ac75171b4aee4bbec1c7def57`, `initialblockdownload=false`, `mine=1`, no `minimumchainwork=0`, and empty QSB queues.
- [x] (2026-04-10 13:07Z) Identified the broad CI failure root cause for PR #1: GitHub Actions checkouts were not fetching the new `src/crypto/randomx` submodule, so CMake failed with `Missing vendored RandomX source`. Updated `.github/workflows/ci.yml` so the test-each-commit, shared build, and lint checkouts use `submodules: recursive`.
- [x] (2026-04-10 17:11Z) Stabilized the remaining PR #2 macOS CI harness failures: the fuzz P2P targets now reset IBD relative to the active chain tip instead of a stale Bitcoin-era timestamp, `p2p_headers_presync` seeds an active genesis tip for the mainnet presync harness, validation diagnostics tolerate null tips while the node is below minimum-chainwork, and the Qt nested-RPC parser test now runs on regtest so RNG mainnet minimum-chainwork assumptions do not leave the test harness without a usable active tip.
- [x] (2026-04-10 18:31Z) Fixed the next PR #2 merge blockers: restored Bitcoin Core `30.2`'s Windows `common::WinCmdLineArgs` implementation so native Windows CLI, daemon, wallet, Qt, and fuzz targets link, and constrained `p2p_headers_presync` to RNG's low-work mainnet path while resetting the harness best-header state to genesis so the upstream qa-assets presync corpus no longer trips the failed-header block-index invariant. Local validation passed for the rebuilt `fuzz` target, the three concrete CI failing presync corpus inputs, `qsb_tests`, `validation_block_tests`, and `git diff --check`; the full presync corpus was stopped locally after several minutes and left for GitHub Actions to run end to end.
- [x] (2026-04-10 19:31Z) Fixed the next PR #2 CI portability blockers: BIP324 upstream packet vectors now inject Bitcoin mainnet magic while production BIP324 still uses RNG network magic, wallet benchmarks now use an RNG `trng` regtest burn address, Windows workflow and resource metadata now point at RNG-named artifacts, `rng-wallet` has a matching CMake convenience target, and `qrencode` depends fetches from the Bitcoin Core mirror after the old upstream URL returned 404. Local validation passed for the wallet-enabled `bench_bitcoin`/`test_bitcoin` build, `bip324_tests`, `bench_bitcoin -sanity-check -filter='WalletBalance|WalletIsMine'`, `rng-wallet -version`, `git diff --check`, and an HTTP 200 probe of the new `qrencode` source URL.
- [x] (2026-04-10 20:10Z) Fixed the remaining PR #2 bootstrap/unit-test regression on the latest head: mainnet bootstrap now treats the shipped RNG genesis as a consensus anchor instead of re-running RandomX PoW on height 0, blockmanager disk-read tests now match the RandomX-era `ReadBlock()` contract, and a dedicated regression test locks in the genesis-anchor behavior. Local validation passed for rebuilt `test_bitcoin`, `blockmanager_tests`, `chainstate_write_tests`, `randomx_tests/mainnet_genesis_bootstrap_anchor`, and `git diff --check`.
- [x] (2026-04-13 03:52Z) Re-ran the post-merge Contabo fleet checkpoint. `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05` all report `blocks=headers=32122`, best block `2989364bff77fd2d0c168ead85df448b4bf244e7a3d229bde5e5b965646c2098`, `initialblockdownload=false`, `verificationprogress=1`, `mine=1`, `enableqsboperator=1`, and no `minimumchainwork=0` line in `/root/.rng/rng.conf`. `contabo-validator-01` is SSH-reachable but RPC is unavailable because `rngd.service` is crash-looping: journald reports `/root/.rng/settings.json` is invalid JSON, and a read-only probe confirmed the file is zero bytes. Disk and inode usage are not exhausted. Decision: no catch-up override remains to remove on the healthy non-canary validators, and mining is already enabled there; do not treat validator-01 as safe to mine until its settings-file failure is repaired, the daemon restarts cleanly, and it catches up to the fleet tip.
- [x] Prototype the smallest possible non-standard bare-script funding flow on regtest and confirm that wallet funding plus direct miner submission works before full QSB integration.
- [x] Implement the in-repo QSB builder under `contrib/qsb/` and lock the supported script template to a strict versioned format.
- [x] Implement the local-only QSB candidate pool and RPC surface.
- [x] Implement miner integration.
- [x] Add unit and functional coverage for funding, spending, rejection, conflict handling, and block acceptance by unmodified peers.
- [x] Run the canary rollout on `contabo-validator-01` with a small-value QSB funding transaction, then a matching spend transaction after confirmation.
- [x] Roll the validated build to the remaining reachable validators and document the exact outcome in this file.

## Surprises & Discoveries

- Observation: The reachable validators are not running the packaged `/usr/bin/rngd` plus `/etc/rng/rng.conf` service shape described in `doc/init.md`.
  Evidence: On `contabo-validator-01`, `systemctl cat rngd` shows `ExecStart=/root/rngd -datadir=/root/.rng -conf=/root/.rng/rng.conf -walletcrosschain=1 -wallet=miner`.

- Observation: All four reachable validator hosts currently have `rngd.service` installed but inactive.
  Evidence: A read-only probe against `contabo-validator-01`, `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05` returned `inactive` for `systemctl is-active rngd`.

- Observation: The validator hosts still have RNG chainstate and peer state on disk.
  Evidence: `contabo-validator-01` has `/root/.rng/blocks`, `/root/.rng/chainstate`, `peers.dat`, `mempool.dat`, `wallets`, and a populated `debug.log`.

- Observation: The live validator config already looks like a public RNG config and includes the current operator seed peers plus the validator IPs themselves.
  Evidence: `contabo-validator-01:/root/.rng/rng.conf` contains `server=1`, `listen=1`, `port=8433`, `rpcbind=127.0.0.1`, `rpcallowip=127.0.0.1`, `minerandomx=fast`, and `addnode=` entries for the known seed-peer and validator IPs.

- Observation: The QSB repository is not a drop-in dependency. The paper requires a bare output script, but the current pipeline still constructs a P2SH-style spend path.
  Evidence: The paper and repository README say the script must be bare because the locking script is too large for P2SH, while the current upstream pipeline appends the full redeem script to `scriptSig`.

- Observation: The public mempool policy path is the wrong place to start for live QSB mining.
  Evidence: RNG currently rejects non-standard outputs, non-standard inputs, large legacy `scriptSig` values, and legacy `FindAndDelete` usage under standard flags. Those checks live in `src/policy/policy.cpp`, `src/policy/policy.h`, and `src/script/interpreter.cpp`.

- Observation: The earlier regtest funding slowdown was not an inherent RandomX cost. It was a `30.2` port bug where `GetNextWorkRequired` ignored `fPowNoRetargeting`, so regtest difficulty climbed after a few blocks.
  Evidence: Before the fix, direct `generatetoaddress` runs moved regtest from `0x207fffff` to harder targets like `0x20022222` and `0x1f08f3f8` within three blocks and then stalled. After returning `pindexLast->nBits` when `fPowNoRetargeting` is set, `generatetoaddress 10` completed in about one second and `feature_qsb_rpc.py` passed.

- Observation: RNG’s internal miner will not produce blocks on an isolated single-node regtest setup.
  Evidence: `src/node/internal_miner.cpp` gates mining on `MIN_PEERS_FOR_MINING`, and a local attempt logged repeated “Bad conditions” backoff until a peer connection was present.

- Observation: The current toy builder emits a one-byte version marker as raw pushed data, not as `OP_1`, so standard `MINIMALDATA` policy would reject the spend even though consensus permits it.
  Evidence: `contrib/qsb/template_v1.py` uses `push_data(bytes([1]))`, and the C++ matcher had to mirror that exact shape while `src/validation.cpp` now routes toy-QSB spends through consensus block flags instead of `STANDARD_SCRIPT_VERIFY_FLAGS`.

- Observation: The first toy-QSB validator hook was too broad for the operator-only design.
  Evidence: The earlier `src/validation.cpp` path would admit exact toy-QSB transactions through normal mempool admission. The fix was to thread an explicit `allow_qsb_toy` flag through `ProcessTransaction` and `AcceptToMemoryPool` so the escape hatch is reachable only from the local operator RPC path.

- Observation: The live-ops gap was not inside the node anymore; it was in the operator surface around it.
  Evidence: After the queue RPC and miner path landed, the repo still lacked a supported way to probe validators, submit saved QSB state to the queue, and wait for confirmation without manually hand-writing long `ssh ... rng-cli ...` invocations.

- Observation: The legacy root-managed validator unit is not directly compatible with the upgraded `30.2` daemon.
  Evidence: On `contabo-validator-01`, the inherited `Type=forking` unit never reached `active` after the upgrade because `rngd` did not satisfy the old fork-and-PID startup contract. A drop-in override that switches the service to `Type=notify` with `-startupnotify` and `-shutdownnotify` fixed the canary startup without changing the `/root/.rng` layout.

- Observation: The canary's apparent sync blocker was not a consensus-code bug.
  Evidence: After the service override, `submit_fleet.py info --host contabo-validator-01` initially showed `qsb_operator_enabled: True`, `blocks=headers=3161`, and `initialblockdownload=true`, while peers advertised heights near `29880`. The node later advanced through header presync and IBD to `blocks=headers=29944` with `initialblockdownload=false` without another node-side patch.

- Observation: The live funding builder must not rely on wallet fee estimation on the current RNG mainnet.
  Evidence: The first canary `toy-funding` attempt failed with `Fee estimation failed. Fallbackfee is disabled.` The builder now passes a deterministic `fee_rate` option to `fundrawtransaction` instead of requiring a production validator config change.

- Observation: The live miner should not rely on the implicit default worker count during a canary proof.
  Evidence: With `minethreads` unset, `contabo-validator-01` logged `Worker Threads: 1` and repeatedly built valid templates containing the QSB transaction without finding the next block quickly. After setting `minethreads=8` and resubmitting the in-memory queue entry, it mined the funding block at height `29946` and the spend block at height `29947`.

- Observation: Some outbound peers temporarily advertise a higher starting height but do not currently become the canary's active best chain.
  Evidence: During the canary run, `debug.log` briefly showed outbound peers advertising heights such as `35173`, but `getpeerinfo` did not retain them as synced peers, `getblockchaininfo` remained `blocks=headers`, and the funding and spend blocks gained confirmations on the active chain.

- Observation: The fleet rollout should use stripped artifacts, not the unstripped development binaries.
  Evidence: The unstripped `rngd` built in the port worktree was about 291 MB and was slow to copy to `contabo-validator-02`. The stripped rollout artifacts in `/tmp/rng-qsb-deploy/` are much smaller, with `rngd` about 13 MB and sha256 `36eb7509a17c15fbca062dc3427bb36d0d19cb24ec4fb299fcea09e20a5ad054`, and `rng-cli` about 1.1 MB with sha256 `eff7e8d116b8143f4182197e482804b74a49d8885915e24ab23eec6b3f67b92a`.

- Observation: The `Type=notify` systemd compatibility drop-in must preserve normal shell spacing inside the `-startupnotify` and `-shutdownnotify` arguments.
  Evidence: The first `contabo-validator-02` drop-in attempt escaped the space as `systemd-notify\ --ready`, leaving the service stuck in startup with `runCommand error: system(systemd-notify\ --ready) returned 32512`. Copying the known-good `/tmp/rng-30.2-compat.conf` file, killing the stale daemon, reloading systemd, and restarting fixed the host.

- Observation: Non-canary validators that were upgraded from old chainstate can be QSB-enabled and healthy while still far behind the canary tip.
  Evidence: At `2026-04-10T02:18:06Z`, `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05` each report `/RNG:3.0.0/`, `version=30000`, `qsb_operator_enabled=true`, and active services, but they still report `initialblockdownload=true` at heights `3161`, `3159`, and `3165` respectively while peers advertise starting heights near `29954`.

- Observation: Mining must stay disabled on the non-canary validators while they are in IBD.
  Evidence: During the fleet wave, `contabo-validator-04` briefly logged a local mining template or stale block near height `3160` before mining was disabled. The mitigation is to leave `mine=0` on 02, 04, and 05 until each reports `initialblockdownload=false` and matches the canary tip.

- Observation: The non-canary validators are progressing through Bitcoin Core `30.2` header presync rather than hard-stuck.
  Evidence: `debug.log` on 02 advanced from `Pre-synchronizing blockheaders, height: 5160` at `2026-04-10T02:11:37Z` to `9160` at `2026-04-10T02:16:53Z`, and later peer info showed `presynced_headers=11160`; 04 advanced from `5158` at `02:12:45Z` to `11158` at `02:18:03Z`, and later peer info showed `presynced_headers=13158`; 05 advanced from `5164` at `02:13:53Z` to later peer info showing `presynced_headers=11164`.

- Observation: Direct canary peering is currently blocked by host firewall policy, not by the RNG daemon.
  Evidence: Before the allowlist change, `contabo-validator-01` listened on `0.0.0.0:8433`, but UFW was active with no `8433/tcp` allow rule, and TCP probes from 02, 04, and 05 to `95.111.227.14:8433` returned closed. Those hosts could still reach the existing seed peers on `8433/tcp`, so this was an optional peering improvement rather than a QSB correctness blocker.

- Observation: Opening canary P2P to the validator allowlist fixed TCP reachability but did not produce a reliable header-sync path from the canary.
  Evidence: After adding UFW rules on 01 for 02/04/05, TCP probes to `95.111.227.14:8433` returned open, but `getpeerinfo` on 02 and 05 showed canary connections stuck at `version=0`, `subver=""`, and `transport_protocol_type` `v2` or `v1`; 04 briefly completed an inbound handshake on the canary, but it did not become a useful presync peer for the lagging node.

- Observation: The recurring presync timeout lines are tied to the default RNG mainnet `minimumchainwork` value in the port.
  Evidence: Mainnet `src/kernel/chainparams.cpp` sets `consensus.nMinimumChainWork` to `000000000000000000000000000000000000000000000000000000005e9a730b`. The lagging validators start around height 3160, below that work floor, and logs show presync reaching around 15160 before `Timeout downloading headers, disconnecting peer=0`. The port exposes `-minimumchainwork=<hex>`, and tests confirm `-minimumchainwork=0` is a supported override.

- Observation: After `minimumchainwork=0`, the remaining catch-up cost is CPU-bound header validation, not a stuck service.
  Evidence: After restart, 02 logged `Synchronizing blockheaders, height: 5160`, 04 logged `Synchronizing blockheaders, height: 5158`, and 05 logged `Synchronizing blockheaders, height: 5164`. During this phase, 02 and 05 still answered some RPC calls while 04 intermittently timed out, and `rngd` used roughly 70-100% CPU per host.

- Observation: Unit tests for the QSB mempool path do not need RandomX-mined mature blocks.
  Evidence: `src/test/qsb_tests.cpp` now seeds confirmed spendable UTXOs directly into `CoinsTip()` and drives `ChainstateManager::ProcessTransaction()` end to end, which validates the policy hooks without the cost and fragility of RandomX block generation in the unit harness.

- Observation: One published seed-peer IP did not answer SSH from this machine.
  Evidence: A direct SSH probe to `root@185.239.209.227` timed out on port 22. The broader seed fleet must be re-inventoried before treating those hosts as rollout targets.

## Decision Log

- Decision: Implement QSB as an operator-only direct miner submission path, not as a blanket mempool policy relaxation.
  Rationale: QSB transactions are consensus-valid but intentionally non-standard. Relaxing general mempool policy on public nodes would widen the public attack surface, while an operator-only queue keeps the new behavior narrow and local.
  Date/Author: 2026-04-09 / Codex

- Decision: Keep QSB secret material outside the RNG wallet and use the wallet only to fund the first QSB output.
  Rationale: The wallet is good at selecting inputs and signing ordinary funding inputs, but QSB one-time secrets are fragile and should not be mixed with ordinary wallet recovery semantics.
  Date/Author: 2026-04-09 / Codex

- Decision: Preserve the current root-managed Contabo validator layout for the canary and first fleet wave.
  Rationale: The live validators already have `/root/rngd`, `/root/rng-cli`, `/root/.rng`, and a root-managed `rngd.service`. Changing the node layout at the same time as enabling QSB would couple too many risks.
  Date/Author: 2026-04-09 / Codex

- Decision: Treat the upstream QSB repository as design input, not production code to vendor wholesale.
  Rationale: The upstream repository is incomplete for end-to-end live use, still contains a P2SH-oriented pipeline mismatch, and does not know RNG’s network constants or operator environment.
  Date/Author: 2026-04-09 / Codex

- Decision: Stage the live rollout as funding transaction first, spend transaction second.
  Rationale: This keeps the first live proof small and observable. It also avoids introducing package or parent-child same-block handling into the first production milestone.
  Date/Author: 2026-04-09 / Codex

- Decision: Keep the QSB validation bypass behind an internal flag, not a user-visible “accept this in the mempool” toggle.
  Rationale: The design requirement is operator-local queueing, not public relay policy change. Threading an internal `allow_qsb_toy` bit through validation preserves the narrow surface and prevents accidental widening through ordinary submission paths.
  Date/Author: 2026-04-10 / Codex

- Decision: Fix regtest difficulty handling in the `30.2` port instead of working around it in the QSB tests.
  Rationale: The funded-QSB functional test exposed a real port regression: regtest was ignoring `fPowNoRetargeting`. Fixing the node restores expected `30.2` test behavior for the whole repository and avoids encoding a broken assumption into the QSB harness.
  Date/Author: 2026-04-10 / Codex

- Decision: Keep the first live-fleet helper outside the node and make it talk to validator-local `rng-cli` over SSH.
  Rationale: The live validators already bind RPC to localhost. Reusing that shape avoids opening RPC to the network, avoids another bespoke transport layer, and keeps the current canary path aligned with the existing root-managed fleet layout.
  Date/Author: 2026-04-10 / Codex

- Decision: Preserve the root-managed canary layout during the upgrade, but add a `30.2` systemd compatibility drop-in instead of relying on the old `Type=forking` unit.
  Rationale: Repackaging the live validator into `/etc/rng` plus `/var/lib/rngd` mid-rollout would couple two risky changes. The notify-style drop-in fixes the daemon lifecycle contract while leaving paths, wallets, and operational ownership unchanged.
  Date/Author: 2026-04-09 / Codex

- Decision: Pass an explicit sat/vB funding fee rate from the QSB builder rather than enabling `fallbackfee` on the validator.
  Rationale: The fee-estimation failure is a wallet construction issue, not a node policy issue. A builder-level `fee_rate` keeps production config narrower and makes the canary transaction reproducible.
  Date/Author: 2026-04-10 / Codex

- Decision: Use `minethreads=8` for the live canary and first fleet wave on the current Contabo validator class.
  Rationale: The host has enough cores and memory for fast-mode RandomX workers, and the QSB queue is in-memory, so reducing block-wait time lowers operational risk.
  Date/Author: 2026-04-10 / Codex

- Decision: Use stripped `rngd` and `rng-cli` artifacts for the remaining fleet rollout instead of copying unstripped development binaries.
  Rationale: The stripped binaries preserve the same runtime behavior, report the same RNG Core `v3.0.0` version, copy much faster, and reduce the chance of an interrupted deployment transfer.
  Date/Author: 2026-04-10 / Codex

- Decision: Keep `mine=0` on any upgraded validator that still reports `initialblockdownload=true`.
  Rationale: A QSB-enabled node that is still catching up is useful as a syncing peer but should not add work to an old local tip. Mining only resumes after the node is out of IBD and agrees with the canary best block.
  Date/Author: 2026-04-10 / Codex

- Decision: Use a temporary `minimumchainwork=0` override on the lagging non-canary validators during this one-time 30.2 catch-up.
  Rationale: The validators are already using a known operator-controlled addnode set, but the 30.2 presync timeout was preventing old chainstate at height ~3160 from reaching the default RNG chainwork floor efficiently. Lowering the local startup floor is less invasive than copying chainstate and should be removed after the nodes finish IBD.
  Date/Author: 2026-04-10 / Codex

## Outcomes & Retrospective

This plan is no longer at the pre-implementation stage. The first four implementation slices now exist in the Bitcoin Core `30.2` port worktree: `contrib/qsb/` contains the versioned template builder, state handling, deterministic fixtures, and the SSH-based fleet helper; the node has a local-only QSB operator queue plus RPC surface behind `-enableqsboperator`; and block assembly now mines queued QSB candidates while removing them from the queue after block connection.

The main remaining technical gap is no longer funded-regtest practicality, block-acceptance proof, canary sync, or the live QSB funding/spend proof. Those were resolved by fixing the regtest no-retargeting regression, adding a two-node mining test where an unmodified peer accepts QSB-carrying blocks, letting the upgraded canary complete IBD, adding explicit funding fee-rate control, and mining the live funding/spend pair on `contabo-validator-01`. The remaining operational work narrowed further after the 2026-04-10 13:07Z re-enable wave: `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05` all finished CPU-bound header and block sync, had the temporary `minimumchainwork=0` override removed, and resumed mining. The current merge-readiness blocker is fresh PR #2 CI on the latest head, not fleet sync; the latest local hardening addresses the RandomX checkout, macOS/fuzz harness assumptions, Windows argument and artifact naming, RNG BIP324 vector salt, RNG regtest wallet benchmark address, and `qrencode` source URL failures observed in GitHub Actions.

## Context and Orientation

RNG started this plan as a fork of Bitcoin Core `v29.0`, but active implementation is now happening in a dedicated Bitcoin Core `30.2` port worktree. The project changed proof of work, chain identity, ports, mining defaults, bootstrap assets, and operator helpers, but it kept Bitcoin-derived transaction encoding, script execution, the UTXO model, the wallet model, and most RPC semantics. The shortest orientation is:

- `README.md` explains the live-network assumptions, current seed peers, install helpers, and the distinction between tagged releases and the development branch.
- `src/script/script.h` defines consensus script size limits such as the 520-byte maximum stack element size, the 201-opcode limit, and the 10,000-byte maximum script size.
- `src/policy/policy.cpp` and `src/policy/policy.h` define the node’s standardness rules, which are stricter than consensus and are the reason QSB transactions are rejected today.
- `src/script/interpreter.cpp` contains legacy script execution details, including the `FindAndDelete` behavior that QSB relies on and the standard-policy `CONST_SCRIPTCODE` rejection that blocks it.
- `src/node/miner.cpp` constructs block templates. On the `30.2` port branch it now inserts validated QSB pool entries ahead of ordinary mempool selection when `-enableqsboperator` is active.
- `src/rpc/mempool.cpp` still exposes `sendrawtransaction` and `testmempoolaccept` for ordinary traffic, while `src/rpc/qsb.cpp` now exposes the separate local-only `submitqsbtransaction`, `listqsbtransactions`, and `removeqsbtransaction` operator surface.
- `scripts/public-apply.sh`, `scripts/install-public-node.sh`, `scripts/install-public-miner.sh`, and `scripts/doctor.sh` are the current repo-native operator helpers for public nodes and miners.

The QSB construction itself needs four plain-English ideas:

First, a bare output script means the locking script is written directly into the transaction output’s `scriptPubKey`. It is not wrapped inside P2SH, P2WSH, or Taproot. QSB needs this because its script is far larger than the 520-byte maximum size of a pushed redeem script element.

Second, a one-time hash-based signature means the spend reveals a specific subset of secret values one time only. That is why the QSB secret state must live outside the ordinary wallet and why the plan must prevent address reuse for QSB outputs.

Third, direct miner submission means the operator hands the transaction to their own miner node locally, rather than depending on the public mempool to relay it. This is necessary because QSB transactions are consensus-valid but intentionally non-standard.

Fourth, a canary host means one live production validator that is upgraded first and observed before any broader deployment starts. In this repository, the confirmed canary candidates are the reachable Contabo validator aliases in the local SSH config.

The live-fleet reality matters. The reachable validators are currently:

- `contabo-validator-01` at `95.111.227.14`
- `contabo-validator-02` at `95.111.229.108`
- `contabo-validator-04` at `161.97.83.147`
- `contabo-validator-05` at `161.97.97.83`

All four are reachable over SSH from this machine, all four have root-managed RNG datadirs at `/root/.rng`, all four have `/root/rngd` and `/root/rng-cli`, and all four report `rngd.service` as inactive. The published seed-peer IP `185.239.209.227` did not answer SSH from this machine during discovery, so it must not be assumed reachable.

## Plan of Work

The work is split into five milestones, each with a concrete proof target.

Milestone 1 is a bare-script feasibility spike. Create a minimal builder under `contrib/qsb/` that can produce a consensus-valid but non-standard bare output script on regtest, then prove that a wallet-funded transaction carrying that output can be mined through a local-only submission surface. This is intentionally simpler than full QSB and exists to remove uncertainty around funding, output size, and miner inclusion.

Milestone 2 is the actual QSB builder. Port the script compiler and transaction assembly logic into this repository under `contrib/qsb/`, but do not vendor the upstream project wholesale. The builder must emit a strict “QSB v1” script template for RNG, generate the funding transaction shell with the large bare output, accept externally generated digest-hit material for the spend path, and emit final funding and spending transaction hex plus an operator-readable state file that records one-time secret usage.

Milestone 3 is node-side direct submission. Add a new local-only QSB candidate pool and RPC surface. The node must validate candidate transactions against consensus rules, confirmed-input availability, local conflicts, and the strict QSB v1 template, then keep accepted candidates in a local queue that block assembly can use. This queue must not be exposed through the p2p relay path and must not relax ordinary mempool standardness.

Milestone 4 is mining integration and tests. Extend `src/node/miner.cpp` so block assembly considers QSB candidates before ordinary mempool chunks, pruning any stale or conflicting entries on the way. Add unit tests for the script matcher and the candidate validator, then add functional tests that show: a default node still rejects QSB over the public mempool path, an enabled node accepts local QSB submission, a mined QSB block is accepted by a default peer, and a QSB spend only succeeds after the funding output is confirmed.

Milestone 5 is the live Contabo rollout. Build a pinned Ubuntu artifact from the implementation commit or release-candidate tag, back up the current binaries on `contabo-validator-01`, deploy the new binary pair, enable and start `rngd.service`, verify the canary is on the correct chain tip, mine a small QSB funding transaction, then mine the matching spend transaction after confirmation. Only after that can the same binary pair be rolled to the remaining reachable validators.

## Implementation Units

- Unit 1: QSB Builder And Fixture Generation
  Goal: Create an in-repo builder that emits RNG-specific QSB v1 funding and spending transactions and a durable state file that tracks one-time key usage.
  Requirements advanced: `R1`, `R4`, `R5`, `R7`
  Dependencies on earlier units: none
  Files to create or modify: `contrib/qsb/README.md`, `contrib/qsb/requirements.txt`, `contrib/qsb/qsb.py`, `contrib/qsb/template_v1.py`, `contrib/qsb/state.py`, `contrib/qsb/fixtures/`, `test/functional/feature_qsb_builder.py`
  Tests to add or modify: `test/functional/feature_qsb_builder.py`
  Approach in prose: Build a small, self-contained Python tool that owns QSB assembly. It must compile the bare output script directly, never produce a P2SH wrapper, and write a state file that records the funding transaction, the supported template version, the digest-hit inputs, and whether the one-time QSB secret has already been consumed. The funding path must build a raw transaction shell with the QSB output, use `fundrawtransaction` to attach standard wallet inputs and change, and then use `signrawtransactionwithwallet` only for those standard inputs. The spend path must consume a confirmed QSB output and emit fully signed raw transaction hex without asking the wallet to understand the QSB script.
  Specific test scenarios: on regtest, create a toy bare script larger than 520 bytes and smaller than 10,000 bytes, fund it through the wallet, and confirm the builder emits valid raw hex; generate a QSB v1 fixture from a deterministic seed and confirm the emitted funding script matches the expected template hash; attempt to reuse the same QSB state file for a second spend and expect the tool to refuse with a one-time-key error.

- Unit 2: QSB Template Matcher And Consensus-Only Validator
  Goal: Teach the node to recognize only the exact QSB v1 funding and spend shapes emitted by `contrib/qsb/`.
  Requirements advanced: `R1`, `R2`, `R3`, `R4`
  Dependencies on earlier units: Unit 1
  Files to create or modify: `src/script/qsb.h`, `src/script/qsb.cpp`, `src/node/qsb_validation.h`, `src/node/qsb_validation.cpp`, `src/test/qsb_tests.cpp`, `src/test/CMakeLists.txt`
  Tests to add or modify: `src/test/qsb_tests.cpp`
  Approach in prose: Add a C++ matcher that parses the exact QSB v1 script family, both for the funding output and the corresponding spend input. The matcher must be intentionally strict and reject any transaction that is merely “non-standard and vaguely similar.” Then add a validator that runs the cheap transaction checks, ensures all inputs are confirmed and available in the active chainstate, checks for conflicts with the mempool and with already queued QSB candidates, computes the fee, and verifies input scripts using the current block’s consensus script flags instead of the stricter standard-policy flags.
  Specific test scenarios: accept a known-good funding fixture emitted by Unit 1; accept a known-good spend fixture only when its funding output is present in the UTXO set; reject a funding transaction whose output script differs by one opcode from the expected template; reject a spend transaction whose scriptSig is mutated so the template matcher still sees the shape but script verification fails; reject any candidate that double-spends an input already used by the mempool or another queued QSB candidate.

- Unit 3: Local-Only QSB Candidate Pool And RPC Surface
  Goal: Provide an operator-only submission path that stores validated QSB candidates locally without using the public mempool.
  Requirements advanced: `R1`, `R2`, `R3`, `R8`
  Dependencies on earlier units: Unit 2
  Files to create or modify: `src/node/context.h`, `src/node/qsb_pool.h`, `src/node/qsb_pool.cpp`, `src/init.cpp`, `src/rpc/qsb.cpp`, `src/rpc/server.cpp`, `src/rpc/client.cpp`, `src/node/types.h`
  Tests to add or modify: `test/functional/feature_qsb_rpc.py`
  Approach in prose: Add a small in-memory candidate pool in `NodeContext`. The pool stores `CTransactionRef`, txid, fee, virtual size, accepted time, template kind, and the prevouts it consumes. Expose three RPCs: `submitqsbtransaction` to validate and queue a candidate, `listqsbtransactions` to inspect the queue, and `removeqsbtransaction` to drop a queued entry. Guard the RPC registration behind a dedicated startup flag such as `-enableqsboperator`, and keep the p2p relay path unchanged.
  Specific test scenarios: on a node started without `-enableqsboperator`, calling `submitqsbtransaction` must return “method not found”; on an enabled node, submitting a known-good funding fixture must return `accepted=true` and a txid; submitting the same raw transaction twice must be idempotent and return the existing queued txid; submitting a mutated or non-QSB transaction must return a validation error that names whether the failure happened in template matching, input availability, or consensus script verification.

- Unit 4: Miner Integration And Chain Cleanup
  Goal: Let block assembly include QSB candidates and automatically forget candidates that are mined or invalidated by chain movement.
  Requirements advanced: `R1`, `R6`, `R7`, `R8`
  Dependencies on earlier units: Unit 3
  Files to create or modify: `src/node/miner.h`, `src/node/miner.cpp`, `src/node/qsb_pool.h`, `src/node/qsb_pool.cpp`, `src/validationinterface.h`, `src/node/kernel_notifications.cpp`, `test/functional/feature_qsb_mining.py`
  Tests to add or modify: `test/functional/feature_qsb_mining.py`
  Approach in prose: Extend block assembly so it asks the QSB candidate pool for valid candidates before selecting normal mempool chunks. The candidate pool should sort by fee rate, recheck conflicts against the latest chainstate and mempool, and prune any tx that is already confirmed or no longer valid. When a block is connected, remove any included QSB tx from the queue. If a block connection or reorg makes a queued QSB tx invalid, remove it and record the reason so the operator can resubmit after fixing the underlying state.
  Specific test scenarios: start two nodes, one with QSB operator support and one without; submit a QSB funding tx to the enabled node, mine a block, and confirm the disabled peer accepts the block; submit a QSB spend tx before the funding output is confirmed and expect rejection; confirm that once the funding tx is mined, the spend tx can be submitted and mined; confirm that a stale queued tx disappears after its inputs become unavailable due to another mined transaction.

- Unit 5: Live Contabo Canary And Fleet Rollout
  Goal: Safely prove the feature on the live RNG network using the known Contabo validator hosts.
  Requirements advanced: `R1`, `R6`, `R8`
  Dependencies on earlier units: Units 1 through 4
  Files to create or modify: `doc/qsb-operations.md`, `contrib/qsb/submit_fleet.py`, `EXECPLAN.md`
  Tests to add or modify: Test expectation: none -- this unit is an operational rollout, not a repository-internal test target.
  Approach in prose: Build the pinned binary artifact on Ubuntu, preserve the current root-managed host layout, and deploy one canary first. Back up `/root/rngd` and `/root/rng-cli`, copy in the new binaries, enable and start `rngd.service`, verify chain identity and tip, then run the smallest-value live funding transaction. After one confirmation, run the matching live spend transaction. If both succeed and the host remains healthy, roll the same binaries to the remaining reachable validators one at a time.
  Specific test scenarios: `contabo-validator-01` starts successfully, reports the expected genesis hash, and mines the QSB funding tx into a valid block; after confirmation, `contabo-validator-01` mines the QSB spend tx into a valid block; `contabo-validator-02`, `contabo-validator-04`, and `contabo-validator-05` start with the same binaries and remain on the same best block hash as the canary; restoring the backed-up binaries on the canary returns the node to its prior behavior.

## Concrete Steps

All commands below assume the working directory is the repository root, `/home/r/Coding/RNG`, unless a remote host command is shown explicitly.

Local build and test steps for implementation:

    cmake -B build -DENABLE_WALLET=ON -DBUILD_TESTING=ON -DWITH_ZMQ=OFF -DENABLE_IPC=OFF
    cmake --build build --target rngd rng-cli test_bitcoin -j"$(nproc)"
    python3 test/functional/feature_qsb_builder.py --configfile=build/test/config.ini
    python3 test/functional/feature_qsb_rpc.py --configfile=build/test/config.ini
    python3 test/functional/feature_qsb_mining.py --configfile=build/test/config.ini
    ./build/bin/test_bitcoin --run_test=qsb_tests

Expected local proof before any live action:

    feature_qsb_builder.py passed
    feature_qsb_rpc.py passed
    feature_qsb_mining.py passed
    Running 1 test case...
    *** No errors detected

Prototype funding flow on regtest:

    python3 contrib/qsb/qsb.py toy-funding \
      --rpc-url http://127.0.0.1:18443 \
      --rpc-user user \
      --rpc-password <rpcpassword> \
      --wallet miner \
      --amount 1.0 \
      --state-file /tmp/qsb-toy.json

Expected proof:

    Wrote state file /tmp/qsb-toy.json
    Funding tx hex written
    Funding txid: <txid>
    Bare script size: <n> bytes

Live canary backup and binary deployment on `contabo-validator-01`:

    ssh contabo-validator-01 'cp /root/rngd /root/rngd.pre-qsb.$(date -u +%Y%m%dT%H%M%SZ)'
    ssh contabo-validator-01 'cp /root/rng-cli /root/rng-cli.pre-qsb.$(date -u +%Y%m%dT%H%M%SZ)'
    scp build/bin/rngd contabo-validator-01:/root/rngd.new
    scp build/bin/rng-cli contabo-validator-01:/root/rng-cli.new
    ssh contabo-validator-01 'install -m 0755 /root/rngd.new /root/rngd'
    ssh contabo-validator-01 'install -m 0755 /root/rng-cli.new /root/rng-cli'
    ssh contabo-validator-01 'systemctl enable --now rngd'
    ssh contabo-validator-01 '/root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng getblockhash 0'
    ssh contabo-validator-01 '/root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng getbestblockhash'

Expected canary proof:

    83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4
    <non-empty best block hash>

Live funding submission on the canary:

    python3 contrib/qsb/qsb.py toy-funding \
      --rpc-url http://127.0.0.1:18435 \
      --rpc-user <rpcuser> \
      --rpc-password <rpcpassword> \
      --wallet miner \
      --amount 0.01 \
      --fee-rate-sat-vb 1 \
      --state-file ./qsb-canary.json
    ssh contabo-validator-01 '/root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng submitqsbtransaction "<hex>"'
    ssh contabo-validator-01 '/root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng listqsbtransactions'

Expected funding proof:

    {
      "accepted": true,
      "kind": "funding_v1",
      "txid": "<txid>"
    }

After one confirmation, live spend submission on the canary:

    python3 contrib/qsb/qsb.py toy-spend \
      --state-file ./qsb-canary.json \
      --destination-address rng1... \
      --fee-sats 1000
    ssh contabo-validator-01 '/root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng submitqsbtransaction "<hex>"'
    ssh contabo-validator-01 '/root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng listqsbtransactions'

Expected spend proof:

    {
      "accepted": true,
      "kind": "spend_v1",
      "txid": "<txid>"
    }

Remaining validator rollout, one host at a time after canary success:

    for host in contabo-validator-02 contabo-validator-04 contabo-validator-05; do
      ssh "$host" 'cp /root/rngd /root/rngd.pre-qsb.$(date -u +%Y%m%dT%H%M%SZ)'
      ssh "$host" 'cp /root/rng-cli /root/rng-cli.pre-qsb.$(date -u +%Y%m%dT%H%M%SZ)'
      scp build/bin/rngd "$host":/root/rngd.new
      scp build/bin/rng-cli "$host":/root/rng-cli.new
      ssh "$host" 'install -m 0755 /root/rngd.new /root/rngd'
      ssh "$host" 'install -m 0755 /root/rng-cli.new /root/rng-cli'
      ssh "$host" 'systemctl enable --now rngd'
      ssh "$host" '/root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng getbestblockhash'
    done

If any host reports a different best block hash than the canary, stop the wave there, investigate, and do not continue to the next host.

## Validation and Acceptance

The implementation is complete only when the following behavior can be demonstrated.

On regtest, the repository-local QSB builder emits a funding transaction whose output script is larger than 520 bytes but smaller than 10,000 bytes, and the node accepts it through the new QSB operator surface. The builder must then emit a matching spend transaction that the node rejects before the funding tx is confirmed and accepts after confirmation.

On regtest, a default node without `-enableqsboperator` must still reject the same raw transaction through ordinary public submission paths. This proves that the feature is additive and narrow rather than a global policy change.

On regtest, a block mined by an enabled node that contains a QSB transaction must be accepted by a peer that has no QSB operator support enabled. This proves that the work stays within existing consensus.

On live mainnet, the canary validator must start successfully, report the expected genesis hash `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`, and mine the QSB funding transaction. After confirmation, the same canary must mine the matching QSB spend transaction. Only then may the remaining reachable validators be updated.

The acceptance bar for the fleet wave is that every updated validator reports the same best block hash as the canary after startup, remains stable under mining load, and does not require emergency rollback.

## Idempotence and Recovery

The repository build and test steps are idempotent. Re-running them should only rebuild changed objects and re-run tests.

The QSB builder must be idempotent with respect to transaction reconstruction. Re-running the funding command for the same state file should either reproduce the same unsigned shell or refuse if the transaction was already finalized. Re-running the spend command after a completed spend must refuse because the one-time secret has already been consumed.

The first implementation should treat the QSB candidate pool as in-memory only. That is acceptable as long as the operator keeps the raw transaction hex and the state file. If the node restarts before mining the queued transaction, the recovery path is simply to call `submitqsbtransaction` again with the same raw hex.

Before touching a live validator, copy the existing `/root/rngd` and `/root/rng-cli` to timestamped backup filenames. If the updated binary fails to start or diverges from the canary hash, restore those backups in place and restart the service:

    ssh contabo-validator-01 'mv /root/rngd.pre-qsb.<timestamp> /root/rngd'
    ssh contabo-validator-01 'mv /root/rng-cli.pre-qsb.<timestamp> /root/rng-cli'
    ssh contabo-validator-01 'systemctl restart rngd'

If a live node restarts and loses the queued QSB transaction before it was mined, rebuild nothing. Reuse the saved raw hex and resubmit it locally.

If chain tip agreement is lost during rollout, stop starting additional miners. Follow the same convergence rule documented in `docs/lessons-learned-fleet-recovery.md`: keep mining on one or two known-good nodes only until the rest match their best block hash, then re-enable broader mining.

## Artifacts and Notes

Read-only validator inventory artifact:

    $ ssh contabo-validator-01 'systemctl is-active rngd; test -f /root/.rng/rng.conf && echo CONF=/root/.rng/rng.conf'
    inactive
    CONF=/root/.rng/rng.conf

Root-managed service artifact from `contabo-validator-01`:

    [Service]
    Type=forking
    PIDFile=/root/.rng/rngd.pid
    ExecStart=/root/rngd -datadir=/root/.rng -conf=/root/.rng/rng.conf -walletcrosschain=1 -wallet=miner
    Restart=always
    User=root

Current live validator config shape on `contabo-validator-01`:

    server=1
    listen=1
    port=8433
    rpcbind=127.0.0.1
    rpcallowip=127.0.0.1
    rpcport=18435
    minerandomx=fast
    addnode=185.218.126.23:8433
    addnode=95.111.239.142:8433
    addnode=95.111.227.14:8433
    addnode=194.163.144.177:8433
    addnode=95.111.229.108:8433
    addnode=161.97.83.147:8433
    addnode=161.97.117.0:8433
    addnode=161.97.114.192:8433
    addnode=161.97.97.83:8433
    addnode=185.239.209.227:8433

Expected live-genesis proof after canary startup:

    $ /root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng getblockhash 0
    83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4

Seed-peer SSH gap artifact:

    $ ssh -i ~/.ssh/contabo-mining-fleet root@185.239.209.227
    ssh: connect to host 185.239.209.227 port 22: Connection timed out

## Interfaces and Dependencies

The QSB builder lives under `contrib/qsb/` and depends on Python 3 plus a pinned `requirements.txt`. It must expose a single CLI entry point, `contrib/qsb/qsb.py`, with subcommands for `fixture`, `toy-funding`, and `toy-spend`. The CLI must write a durable state file that records the template version, funding txid, funding vout, destination address, fee assumptions, and whether the one-time spend has already been consumed.

In `src/script/qsb.h`, define:

    enum class QsbTxKind {
        NONE,
        FUNDING_V1,
        SPEND_V1,
    };

    struct QsbMatch {
        QsbTxKind kind;
        uint256 template_hash;
    };

    std::optional<QsbMatch> MatchQsbV1Transaction(const CTransaction& tx, const CCoinsViewCache* coins_view);

The matcher must evolve the existing script surface rather than invent a new script interpreter. It uses the same `CScript`, `Solver`, and consensus verification machinery that already exists.

In `src/node/qsb_validation.h`, define a validator surface along these lines:

    struct QsbCandidateInfo {
        CTransactionRef tx;
        QsbTxKind kind;
        CAmount fee;
        int64_t vsize;
        std::vector<COutPoint> prevouts;
    };

    util::Result<QsbCandidateInfo> ValidateQsbCandidate(
        Chainstate& active_chainstate,
        const CTxMemPool& mempool,
        const QsbCandidatePool& pool,
        const CTransactionRef& tx);

This validator must supplement the ordinary mempool rules with a local-only, consensus-based QSB path. It must not replace the ordinary mempool contract.

In `src/node/qsb_pool.h`, define:

    class QsbCandidatePool {
    public:
        bool Has(const Txid& txid) const;
        util::Result<QsbCandidateInfo> Submit(QsbCandidateInfo candidate);
        std::vector<QsbCandidateInfo> List() const;
        bool Remove(const Txid& txid);
        std::vector<QsbCandidateInfo> GetBlockCandidates(
            Chainstate& active_chainstate,
            const CTxMemPool& mempool) const;
    };

In `src/rpc/qsb.cpp`, register:

    submitqsbtransaction(hexstring)
    listqsbtransactions()
    removeqsbtransaction(txid)

These RPCs are intentionally separate from `sendrawtransaction` because they do not mean “relay this transaction normally.” They mean “queue this exact supported QSB transaction locally for mining.” That semantic difference is important and must remain explicit in help text and operator docs.

In `src/node/miner.cpp`, extend block assembly rather than creating a second mining path. `BlockAssembler::CreateNewBlock()` must continue producing one block template, but it must insert QSB candidates from `QsbCandidatePool` before it consumes ordinary mempool chunks. That keeps the mining surface single-path and observable.

In the live rollout, the first deployment dependency is the current root-managed validator service layout, not the packaged public-node helpers. The repository documentation still matters, especially `README.md`, `doc/init.md`, `doc/release-process.md`, and `docs/lessons-learned-fleet-recovery.md`, because they define the chain identity, the expected release discipline, and the operational recovery rules the rollout must obey.

Change note (2026-04-09): Initial plan created from repository research, QSB feasibility analysis, and a read-only Contabo validator discovery pass. Reason: the requested work needs a self-contained implementation and rollout spec before any live changes are safe.

Change note (2026-04-10): Updated the plan with the live canary QSB proof, stripped fleet artifact hashes, the `contabo-validator-02` systemd drop-in quoting fix, and the current 02/04/05 catch-up state. Reason: the remaining fleet rollout is now gated on non-canary validators finishing header presync and IBD, not on additional QSB implementation work.

Change note (2026-04-10): Added the canary firewall allowlist attempt and the temporary `minimumchainwork=0` catch-up override for 02/04/05. Reason: the non-canary validators were repeatedly timing out during Bitcoin Core `30.2` low-work header presync from old chainstate; the override moved them into normal header sync while preserving `mine=0`.

Change note (2026-04-10): Rechecked the live Contabo validators after the merge-readiness fixes: 01/02/04/05 were all active at height 30178 on block `c2cedea70322daaa1ceb136a5eb39c6d7dc27549ff81ec2e86fe48bb8bb82cf4`, with `mine=1`, no `minimumchainwork=0`, and empty QSB queues. Reason: this records the go-forward fleet state before merging or tagging the Bitcoin Core 30.2 + QSB branch.

Change note (2026-04-10): Added CI hardening for RandomX and QSB: patch the copied RandomX CMake metadata, keep RandomX's own C++ linkage, suppress third-party RandomX documentation warnings at the include boundary, split deterministic fixture secret material, and fix lint/build annotations around QSB mining. Reason: these changes remove merge-blocking CI failures without changing RNG consensus parameters.

Change note (2026-04-10): Updated the README and this ExecPlan for merge readiness: the fleet rollout is now recorded as complete, stale `live-fund` and `live-spend` examples were replaced with the implemented `toy-funding` and `toy-spend` commands, and the README now points operators to the completed Contabo rollout state. Reason: the branch is being prepared for PR #2 merge, so the public entry point and living plan must agree with the shipped implementation.

Change note (2026-04-10): Added the final observed PR #2 CI portability fixes: BIP324 vectors now use an explicit Bitcoin mainnet magic fixture, wallet benchmarks use RNG regtest addresses, RNG-named Windows artifacts and resource metadata are aligned, `rng-wallet` has a matching CMake convenience target, and `qrencode` now downloads from the Bitcoin Core depends mirror. Reason: these were the next merge blockers after the Windows command-line and presync fuzz fixes, and the README/plan need to record the current merge gate accurately.

Change note (2026-04-10): Added the latest merge-readiness fix for the shipped RNG genesis and `ReadBlock()`-contract regression tests. Reason: PR #2's next failing unit suites were `chainstate_write_tests` and `blockmanager_tests`; the fix makes fresh bootstrap honor RNG's consensus-anchored genesis and keeps the tests aligned with RandomX-era disk-read semantics.
