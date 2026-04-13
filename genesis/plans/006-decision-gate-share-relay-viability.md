# Decision Gate: Share Relay Viability on Small Network

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.


## Purpose / Big Picture

This is a checkpoint plan. It does not add features or ship code. It evaluates whether the share relay protocol implemented in Plan 005 works well enough on a small network (4 to 10 nodes) to justify building the payout commitment and claim machinery on top of it. The output is a relay performance report with a go/no-go decision for proceeding to Plan 007 (Compact Payout Commitment and Claim Program).

The reason this gate exists is that the sharechain relay is the most novel and uncertain subsystem in the pooled mining design. RNG's block relay piggybacks on Bitcoin Core's highly optimized compact-block and header-first infrastructure. Shares have none of that optimization yet. If shares cannot propagate reliably on a 4-10 node network, the payout commitment built on top of them will produce inconsistent results across nodes, and the entire pooled mining design fails. It is cheaper to discover and fix relay problems now than after payout consensus rules are committed.

After this checkpoint, the team will know whether shares propagate within acceptable latency, whether bandwidth overhead is tolerable, whether orphan handling works under churn, whether the sharechain converges when nodes have unequal connectivity, and whether a minority miner can reliably get shares into the accepted reward window. Each of these questions has a quantitative threshold that triggers a specific remediation if the threshold is missed.


## Requirements Trace

`R4`. Share admission and share propagation must be peer-to-peer. This checkpoint evaluates whether R4 is satisfied in practice, not just in code. If shares do not propagate reliably between peers, R4 is not met regardless of what the code says.

`R2`. The pooled reward contract must be derived from a public share history. If relay is unreliable, different nodes will have different share histories, and R2 fails because nodes will derive different payout commitments for the same block.

`R5`. Block construction must remain miner-built. This checkpoint evaluates whether miners can construct valid blocks with sharepool sections when the share tip may be slightly different across nodes due to relay latency.

`R9`. Activation must be staged. This checkpoint is one of the staging gates. Passing it means regtest share relay is viable and the next stage (payout implementation) can begin. Failing it means the relay protocol needs revision before more consensus code is built.

`R10`. Solo mining must remain possible. This checkpoint verifies that a solo miner's shares form a valid sharechain even without other participants.


## Scope Boundaries

This plan does not add code to the repository. It runs experiments using the infrastructure from Plans 004 and 005.

This plan does not evaluate payout commitment correctness, claim program behavior, wallet integration, or mining UX. Those are later plans' scope.

This plan does not test on mainnet or devnet. All experiments run on regtest.

This plan does not define the final share difficulty constant or reward window size. It tests with the values currently configured in the regtest `SharePoolParams`. If those values are not yet set by the simulator (Plans 002-003), the test uses reasonable placeholders and documents them.

This plan does not test adversarial scenarios such as share withholding attacks, selfish share mining, or eclipse attacks on the sharechain. Those are Plan 011's scope (Devnet Deployment, Observability, and Adversarial Testing). This checkpoint is about basic relay mechanics, not game-theoretic robustness.


## Progress

- [ ] Set up a 4-node regtest cluster with sharepool active.
- [ ] Run a sustained share production test (target: 10 minutes of continuous share generation).
- [ ] Measure share propagation latency across all node pairs.
- [ ] Measure per-node bandwidth overhead from share relay.
- [ ] Test orphan handling under node churn (stop and restart nodes mid-run).
- [ ] Test sharechain convergence with unequal connectivity (one node connected to only one peer).
- [ ] Test minority miner share inclusion (one miner at 10% of total share rate).
- [ ] Write the relay performance report.
- [ ] Record go/no-go decision in Decision Log.


## Surprises & Discoveries

No entries yet. This section will be updated as experiments run.


## Decision Log

No entries yet. The go/no-go decision from this checkpoint will be recorded here with full rationale and evidence.


## Outcomes & Retrospective

No entries yet. This section will be updated when the checkpoint completes.


## Context and Orientation

This plan depends on Plan 004 (the deployment skeleton) and Plan 005 (the sharechain data model, storage, and relay). Both must be completed and passing tests before this checkpoint begins.

This section explains the test setup and the five questions this checkpoint must answer, with enough detail for a novice to reproduce the experiments.

The test environment is a regtest cluster. "Regtest" is RNG's regression test mode, which allows instant block generation without real proof-of-work mining, configurable deployment activation, and ephemeral datadirs. A "regtest cluster" means multiple `rngd` processes running on the same machine (or on a small set of machines), each with its own datadir and P2P port, connected to each other via `addnode` configuration.

All nodes in the cluster are started with `-vbparams=sharepool:0:9999999999:0` to force the sharepool deployment to `ACTIVE` state. This is the same activation mechanism tested in Plan 004.

Shares are generated either by the `submitshare` RPC (manually or via a test script) or by a share-generation test harness that produces valid shares at a configurable rate. The test harness must create shares with valid RandomX proofs, which means it must use the RandomX hashing functions from `src/pow.cpp` or the Python-accessible RandomX bindings if available. If generating valid RandomX proofs is too slow for the test harness, an alternative is to use a mock share target that accepts any hash (by setting `nBits` to the maximum allowed value), which is acceptable for relay testing because relay validation checks are orthogonal to RandomX performance.

Five questions must be answered, each with a threshold:

Question 1: Share propagation latency. When a share is submitted to one node, how long until all other nodes in the cluster have it? The measurement is wall-clock time from `submitshare` return on the originating node to `getsharechaininfo` on the farthest node reporting the new tip. Acceptable threshold: less than 5 seconds across 4 nodes. If latency exceeds 10 seconds, the relay protocol needs revision (possible causes: validation bottleneck, missing parallelism, serialization overhead).

Question 2: Bandwidth overhead. How much network traffic does share relay add per node? The measurement is bytes sent and received on each node's P2P connections during the test window, compared to a baseline with no share activity. Acceptable threshold: less than 10 KB/s per node. If bandwidth exceeds 50 KB/s, share compression or relay batching is needed.

Question 3: Orphan handling under churn. When a node disconnects and reconnects, do orphan shares get resolved correctly? The test stops one node mid-run, lets the other nodes continue producing shares, then restarts the stopped node and measures whether it catches up. Acceptable threshold: orphan rate (shares that arrive before their parent) below 20% of total shares received. If orphan rate exceeds 20%, the parent-tracking or share-request protocol needs improvement (possible fixes: request parents proactively, batch parent-child pairs).

Question 4: Convergence with unequal connectivity. If one node is connected to only one other node (not the full mesh), does it still converge on the same share tip? The test configures node D to connect only to node A, while nodes A, B, and C form a full mesh. Shares produced by B and C should still reach D through A. Acceptable threshold: node D's best tip matches the mesh nodes' best tip within 10 seconds of any share announcement. If convergence fails, the relay protocol's forwarding or inventory announcement logic has a bug.

Question 5: Minority miner inclusion. If one miner produces shares at 10% of the total rate, do its shares appear in the reward window? The test has one node submitting shares at 1/10 the rate of the other nodes. After the test window, the reward window (from Plan 005's `RewardWindow::GetWindow`) should contain approximately 10% of shares from the minority miner. Acceptable threshold: the minority miner's shares constitute at least 5% of the window (allowing for variance). If the minority miner's shares are systematically excluded, the tip selection or window construction has a bias.


## Plan of Work

The work proceeds in three phases: setup, execution, and analysis.

Phase 1 (Setup) prepares the test cluster. Write a test orchestration script (or extend `test/functional/feature_sharepool_relay.py` from Plan 005) that starts 4 regtest nodes with sharepool active, connects them in a configurable topology (full mesh for most tests, partial mesh for the convergence test), and provides helper functions to submit shares, query `getsharechaininfo`, and measure timing.

Phase 2 (Execution) runs the five experiments described in Context and Orientation. Each experiment produces raw data: timestamps, byte counts, share IDs, orphan counts, and window contents. The script should log this data to a structured format (JSON lines or CSV) for analysis.

Phase 3 (Analysis) processes the raw data, computes the metrics, compares against thresholds, and writes the relay performance report. The report is a section added to this ExecPlan file (in Artifacts and Notes) plus the go/no-go decision recorded in the Decision Log.


## Implementation Units

### Unit A: Test Cluster Orchestration

Goal: A repeatable script that stands up a 4-node regtest cluster with sharepool active and provides measurement primitives.

Requirements advanced: R9 (staged validation).

Dependencies on earlier units: Plans 004 and 005 fully implemented.

Files to create or modify:
- `test/functional/feature_sharepool_relay_benchmark.py` (create, or extend `feature_sharepool_relay.py`)

Tests to add or modify: This unit is itself a test artifact.

Approach: Use the existing `test/functional/test_framework/` infrastructure to start 4 `BitcoinTestFramework` nodes. Configure them with `-vbparams=sharepool:0:9999999999:0`. Provide helper methods: `submit_share_to_node(node_index, share_hex)` returns the share ID and submission timestamp; `wait_for_share_on_node(node_index, share_id, timeout)` polls `getsharechaininfo` until the node reports the share as the tip or a descendant of it; `measure_bandwidth(node_index)` reads `getpeerinfo` byte counters before and after a test window.

Specific test scenarios: The orchestration script itself is verified by running a single share submission and confirming all 4 nodes see it. This is the "smoke test" before the longer experiments.

### Unit B: Experiment Execution

Goal: Run all five experiments and collect raw measurement data.

Requirements advanced: R4, R2, R10.

Dependencies on earlier units: Unit A.

Files to create or modify:
- `test/functional/feature_sharepool_relay_benchmark.py` (extend)

Tests to add or modify: The experiments are test methods within the benchmark script.

Approach: Each experiment is a method in the test script. The script logs structured results. The latency experiment submits 50 shares (one every 2 seconds) to a random node and measures propagation time to all other nodes. The bandwidth experiment measures `getpeerinfo` byte counters before and after a 5-minute share production run. The churn experiment stops node C after 2 minutes, continues for 3 minutes, then restarts node C and measures catch-up. The convergence experiment reconfigures the topology to a star (node A as hub) and repeats the latency test. The minority experiment has 3 nodes submitting at rate X and 1 node at rate X/9, then reads the reward window.

Specific test scenarios:
- Latency experiment: 50 shares, measure p50 and p99 propagation time.
- Bandwidth experiment: 5 minutes of share production at 1 share/second, measure total bytes.
- Churn experiment: stop node C for 3 minutes, measure orphan rate after restart.
- Convergence experiment: star topology, measure whether leaf node's tip matches hub's tip.
- Minority experiment: 3:1 share rate ratio, measure minority miner's window representation.

### Unit C: Analysis and Report

Goal: Process raw data, compare against thresholds, write the performance report, and record the go/no-go decision.

Requirements advanced: R9.

Dependencies on earlier units: Unit B.

Files to create or modify:
- This file (`genesis/plans/006-decision-gate-share-relay-viability.md`): update Artifacts and Notes with the report, update Decision Log with the go/no-go.

Tests to add or modify: None.

Approach: Compute the metrics from the raw data. Compare each metric against its threshold. Write the report as a structured section in this file. If all metrics pass, record a "go" decision. If any metric fails, record the failure, the likely cause, and the recommended remediation before proceeding to Plan 007.

Specific test scenarios: Not applicable (this is analysis, not code execution).


## Concrete Steps

All commands assume the working directory is the repository root. Plans 004 and 005 must be completed and passing tests.

Build the node if not already built:

    cmake -B build -DENABLE_WALLET=ON -DBUILD_TESTING=ON -DWITH_ZMQ=OFF -DENABLE_IPC=OFF
    cmake --build build --target rngd rng-cli test_bitcoin -j"$(nproc)"

Run the relay benchmark test:

    python3 test/functional/feature_sharepool_relay_benchmark.py --configfile=build/test/config.ini

Expected outcome: the script runs for approximately 15-20 minutes (covering all five experiments), prints structured results, and exits with a summary.

Expected summary format:

    === Share Relay Viability Report ===
    Latency (p50):       <X> ms   (threshold: < 5000 ms)   PASS/FAIL
    Latency (p99):       <X> ms   (threshold: < 10000 ms)  PASS/FAIL
    Bandwidth per node:  <X> KB/s (threshold: < 10 KB/s)   PASS/FAIL
    Orphan rate:         <X>%     (threshold: < 20%)        PASS/FAIL
    Convergence:         <X> ms   (threshold: < 10000 ms)   PASS/FAIL
    Minority inclusion:  <X>%     (threshold: > 5%)         PASS/FAIL
    Overall:             GO / NO-GO

After the benchmark completes, update this file's Decision Log and Artifacts sections with the results.


## Validation and Acceptance

This checkpoint is accepted when:

All five experiments have been run and produced quantitative results.

The results are documented in the Artifacts and Notes section of this file with enough detail for another person to understand the methodology and reproduce the experiments.

The Decision Log contains a go/no-go entry with explicit rationale tied to the measured values and thresholds.

If the decision is "go," Plan 007 can proceed. If the decision is "no-go," the Decision Log must name the specific failures and propose remediations (such as relay batching, share compression, or parent-proactive fetching) that would be implemented before re-running this checkpoint.


## Idempotence and Recovery

All experiments run on ephemeral regtest datadirs. They can be repeated any number of times without side effects. Each run produces fresh measurement data.

If the benchmark script crashes mid-run, the regtest nodes may be left running. Clean up by killing any stray `rngd` processes and deleting the test datadirs (typically under `/tmp/`).

If a single experiment produces anomalous results (for example, a one-time latency spike due to system load), the experiment should be repeated at least once before recording a failure. The report should note the number of runs and any anomalies.

The benchmark script should be deterministic in its share generation (using fixed seeds for share construction) so that results are reproducible across runs on the same hardware.


## Artifacts and Notes

This section will be populated with the relay performance report after the experiments run. The report will include:

1. Test environment description (hardware, OS, number of CPU cores, available memory).
2. Share generation parameters (share target, generation rate, total shares produced).
3. Network topology for each experiment.
4. Raw measurement tables (one row per share for latency, one row per node for bandwidth).
5. Computed metrics (p50, p99, mean, max for latency; KB/s for bandwidth; percentages for orphan rate and minority inclusion).
6. Threshold comparison table.
7. Go/no-go recommendation with rationale.

Decision criteria summary (repeated here for reference during analysis):

    Metric                  Acceptable        Revision Trigger
    ------                  ----------        ----------------
    Propagation latency     < 5s (p50)        > 10s (p99)
    Bandwidth per node      < 10 KB/s         > 50 KB/s
    Orphan rate             < 20%             > 20%
    Convergence (star)      < 10s             fails to converge
    Minority inclusion      > 5% of window    0% or systematically excluded

Remediation options if thresholds are missed:

If latency is too high: Profile the share validation path in `src/net_processing.cpp`. Likely causes are synchronous RandomX hash computation blocking the message handler, or serialization overhead. Fixes include validating shares in a background thread or caching RandomX VM instances for share validation.

If bandwidth is too high: Implement share inventory batching (collect share announcements for a short interval and send one `SHAREINV` with multiple IDs instead of one per share). Alternatively, implement share header relay (send only the share ID and header fields first, and request the full share only if the receiving node does not already have it).

If orphan rate is too high: Implement proactive parent fetching. When a node receives an orphan share, it should immediately send a `GETSHARE` for the parent rather than waiting for the parent to arrive via normal relay. Alternatively, bundle parent-child pairs in a single `SHARE` message.

If convergence fails: Debug the `SHAREINV` forwarding logic. A hub node must forward share announcements to all its peers, not just the peer it received the share from. Verify that the relay logic in `src/net_processing.cpp` broadcasts `SHAREINV` to all connected peers except the sender.

If minority miner is excluded: Debug the reward window construction in `src/sharechain/window.cpp`. Verify that the window walks back by cumulative work, not by share count, so that a minority miner's shares (which have the same per-share work as majority shares) are included proportionally.


## Interfaces and Dependencies

This plan depends on Plan 004 (deployment skeleton) and Plan 005 (sharechain data model, storage, and relay). Specifically, it depends on the following interfaces from Plan 005:

`submitshare` RPC in `src/rpc/mining.cpp` for injecting shares into the network.

`getsharechaininfo` RPC in `src/rpc/mining.cpp` for querying the current best tip, chain height, and orphan count.

`RewardWindow::GetWindow()` in `src/sharechain/window.h` for reading the reward window contents (used in the minority miner experiment).

`getpeerinfo` RPC (existing Bitcoin Core RPC) for measuring bandwidth via `bytessent` and `bytesrecv` fields.

This plan does not produce new code interfaces. Its output is a performance report and a go/no-go decision that gates Plan 007. The decision is recorded in this file's Decision Log and referenced by Plan 007's dependency declaration.

Later plans that depend on this one: Plan 007 (Compact Payout Commitment and Claim Program) should not proceed until this checkpoint records a "go" decision. If the decision is "no-go," the relay protocol must be revised (likely by modifying `src/net_processing.cpp` and `src/protocol.h` from Plan 005) and this checkpoint must be re-run.
