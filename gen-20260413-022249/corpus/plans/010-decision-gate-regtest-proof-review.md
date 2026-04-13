# Decision Gate: Regtest Proof Review Before Devnet

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds.

This document must be maintained in accordance with `PLANS.md` at the repository root. It is plan 010 in the `genesis/plans/` corpus indexed by `genesis/PLANS.md`.


## Purpose / Big Picture

This plan is a checkpoint, not a coding milestone. Its purpose is to evaluate the regtest proof produced by Plan 009 and decide whether the protocol-native pooled mining implementation is safe and correct enough to deploy on a live multi-node devnet.

Before this gate, all sharepool work has been tested on regtest with controlled miners and scripted scenarios. After this gate passes, Plan 011 will deploy the code to a real multi-node network where adversarial conditions, network partitions, and real-world timing affect behavior. That transition is irreversible in a practical sense: once devnet nodes are running and producing blocks with payout commitments, the cost of a design flaw rises sharply.

The observable outcome of this plan is a written review report with a go or no-go decision. The report must answer six specific questions about reward accuracy, determinism, claim reliability, pre-activation compatibility, security, and resource overhead. If any answer is unsatisfactory, the report must name the specific failure and the plan revision required before the gate can be re-evaluated.

For operators, this plan exists to prevent a premature devnet deployment. For implementers, it provides a structured checklist that turns vague "is it ready?" conversations into measurable criteria. For future contributors reading this after the fact, the decision log and review artifacts record why devnet deployment was approved or blocked.


## Requirements Trace

`R1`. After activation, RNG must replace the legacy "block finder receives the full coinbase reward" contract with a deterministic pooled reward contract enforced by consensus. This gate checks whether that contract produces correct reward splits on regtest.

`R2`. The pooled reward contract must be derived from a public share history. Any fully validating RNG node must be able to replay the same accepted share window and derive the same per-block payout commitment. This gate checks replay determinism.

`R7`. The claim path must be trustless. A miner must be able to prove its entitlement from the payout commitment, its payout key, and the public share window. This gate checks claim reliability.

`R8`. Before activation, existing RNG behavior remains unchanged. This gate checks that pre-activation blocks and transactions still validate correctly after the sharepool code lands.

`R9`. Activation must be staged through RNG's existing version-bits infrastructure first on regtest, then on devnet, then on mainnet. This gate is the mandatory review step between regtest and devnet.

`R11`. The plan must preserve user truthfulness. If wallet display or RPC output makes claims about reward accrual that are not backed by consensus-enforced behavior, this gate must flag the discrepancy.


## Scope Boundaries

This plan does not write new node code, tests, or protocol changes. It reviews and evaluates artifacts produced by Plans 002 through 009.

This plan does not deploy anything to any network. If the gate passes, Plan 011 handles devnet deployment.

This plan does not perform adversarial testing. Adversarial scenarios (share withholding, eclipse attacks, reorgs under attack) are Plan 011's responsibility. This plan reviews whether the regtest proof is solid enough to justify that next step.

This plan does not evaluate mainnet readiness. Even a passing gate here only authorizes devnet deployment. Mainnet readiness requires Plan 011 to succeed and Plan 012 to be completed.


## Progress

- [ ] Collect regtest proof artifacts from Plan 009 completion.
- [ ] Run reward accuracy evaluation: compare regtest payout commitments against simulator predictions for 50+ blocks.
- [ ] Run determinism evaluation: replay share window on an independent node and compare payout commitments.
- [ ] Run claim reliability evaluation: exercise claim transactions across multiple miners and block ranges.
- [ ] Run pre-activation compatibility check: verify that the test suite passes with sharepool deployment inactive.
- [ ] Run security review: audit new witness interpreter, relay surface, and share validation code paths.
- [ ] Run performance measurement: measure CPU and memory overhead of share validation on a 4-core test host.
- [ ] Write review report with measurements, findings, and go/no-go decision.
- [ ] Record decision in Decision Log with rationale.


## Surprises & Discoveries

No discoveries yet. This section will be populated during the review process.


## Decision Log

No decisions yet. The primary decision this plan produces is the go/no-go for devnet deployment, which will be recorded here with full rationale when the review completes.


## Outcomes & Retrospective

Not yet applicable. This section will summarize the review outcome, any blocking issues found, and lessons learned about what the regtest proof did and did not catch.


## Context and Orientation

This plan sits between Plan 009 (Regtest End-to-End Proof) and Plan 011 (Devnet Deployment, Observability, and Adversarial Testing) in the dependency chain defined by `genesis/PLANS.md`. It is the third checkpoint plan in the corpus, after Plan 003 (simulator results) and Plan 006 (share relay viability).

The key files and artifacts this plan reviews are produced by earlier plans. The reviewer needs to understand five things.

First, the "simulator" is a Python tool at `contrib/sharepool/simulate.py` that accepts a share trace and emits the exact payout commitment a block should carry. Plan 002 created it. Plan 003 validated its economics. This plan compares live regtest payout commitments against simulator output to verify that the node's C++ implementation matches the Python reference.

Second, the "sharechain" is the chain of accepted lower-difficulty shares that determines who contributed recent work. It lives in `src/sharechain/` and propagates between peers via P2P messages defined in `src/protocol.h` and handled in `src/net_processing.cpp`. Plan 005 implemented it.

Third, the "payout commitment" is a compact Merkle root in each block's coinbase that commits to the deterministic reward split. It is computed in `src/sharechain/payout.cpp` and validated in `src/validation.cpp`. Plan 007 implemented it.

Fourth, a "claim transaction" is a post-maturity spend that proves a miner's payout leaf exists under the committed root and transfers the correct amount to the miner's address. It uses a new witness-program version interpreted in `src/script/interpreter.cpp`. Plan 007 implemented it.

Fifth, the "regtest proof" is the end-to-end demonstration from Plan 009 where two miners with unequal hashrate mine on an activated regtest network, both accumulate proportional rewards, an independent node replays the same payout commitment, and claim transactions succeed after maturity. The functional tests proving this behavior are `test/functional/feature_sharepool_mining.py`, `test/functional/feature_sharepool_claims.py`, and `test/functional/feature_sharepool_wallet.py`.

The intended operator hardware class matters for the performance review. Share validation overhead should be measured on a modest multi-core host similar to the first real operators expected to run the code if the gate passes. Do not inherit exact fleet assumptions from local planning documents without re-verifying them at review time.


## Plan of Work

The work is a structured review, not a coding task. It proceeds in six evaluation steps, each producing a measurable result, followed by report writing and a decision.

Start by collecting the regtest proof artifacts. Run the Plan 009 functional tests and capture their output. Export the share window and payout commitments from at least 50 consecutive activated blocks using the RPC surfaces that Plan 008 added (such as `getrewardcommitment` and `getsharechaininfo`).

Then run the simulator against the same share trace that the regtest nodes produced. The simulator at `contrib/sharepool/simulate.py` must accept the exported share data and emit payout commitment roots. Compare each root byte-for-byte against what the node computed. If the variance in per-miner reward amounts exceeds 5% over the 50-block window, the reward formula has a bug or the constants are wrong. If the variance exceeds 15%, the formula is fundamentally broken and the gate fails immediately.

Next, test determinism by starting a fresh node, feeding it the same block and share data, and checking that it derives the same payout commitments. The test is simple: export the share window from one node, import it on another, and compare commitment roots. Any disagreement means a determinism bug in the reward window reconstruction code in `src/sharechain/window.cpp` or the commitment computation in `src/sharechain/payout.cpp`.

Then exercise claim transactions. Run a sequence where multiple miners claim their payout leaves from several different blocks. Track the success rate. Any non-deterministic claim failure (a claim that succeeds on retry with identical inputs) indicates a witness interpreter bug in `src/script/interpreter.cpp`. Consistent failures with specific leaf positions or amounts indicate a Merkle proof construction bug in `src/sharechain/payout.cpp`.

Check pre-activation compatibility by running the full existing test suite with the sharepool deployment inactive. No existing test should break. Pay particular attention to `src/test/miner_tests.cpp`, `src/wallet/test/wallet_tests.cpp`, and the functional tests that exercise block template construction.

Perform a focused security review of the new code paths. The reviewer should look at the new witness interpreter for validation bypass vulnerabilities, the share relay surface for denial-of-service vectors (oversized shares, shares referencing nonexistent parents in a loop, rapid share flooding), and the payout commitment for integer overflow or rounding errors in reward splitting.

Measure resource overhead by running a 4-thread mining node on a 4-core test machine with and without sharepool activated. Record CPU utilization, resident memory, and share validation latency. If CPU overhead exceeds 20% compared to non-sharepool mining, the share validation path needs optimization before devnet.

Finally, write the review report. The report must state each of the six evaluation results with evidence, name any blocking issues, and conclude with a go or no-go decision for Plan 011.


## Implementation Units

### Unit 1: Reward Accuracy Evaluation

Goal: verify that regtest payout commitments match simulator predictions within acceptable variance.

Requirements advanced: `R1`, `R2`.

Dependencies: Plan 009 completed.

Files to create or modify: none. This unit reads existing test output and RPC data.

Tests to add or modify: Test expectation: none -- checkpoint plan, artifact is the decision report.

Approach: Run Plan 009's regtest scenario for at least 50 activated blocks with two miners of unequal hashrate (for example, 1 thread versus 4 threads). Export the share window and payout commitments via `getsharechaininfo` and `getrewardcommitment`. Feed the same share trace to `contrib/sharepool/simulate.py`. Compare per-miner reward amounts and commitment roots.

Specific test scenarios:

Over 50 blocks, the per-miner reward ratio must be within 5% of the hashrate ratio. For a 1:4 thread split, the expected reward ratio is approximately 20:80. Measured ratios outside 15:85 to 25:75 indicate a reward formula bug. Commitment roots must match byte-for-byte between node and simulator for every block in the sample.

### Unit 2: Determinism Evaluation

Goal: verify that independent share replay produces identical payout commitments.

Requirements advanced: `R2`.

Dependencies: Plan 009 completed.

Files to create or modify: none.

Tests to add or modify: Test expectation: none -- checkpoint plan, artifact is the decision report.

Approach: Start a fresh regtest node. Sync it against the first node's chain and share data. Compare the payout commitment root for each of the 50 sampled blocks. Any disagreement is a blocking failure.

Specific test scenarios:

The fresh node must report the same `getrewardcommitment` output for every sampled block height. A single mismatch means the share window reconstruction or commitment computation is non-deterministic and must be fixed before devnet.

### Unit 3: Claim Reliability Evaluation

Goal: verify that claim transactions succeed reliably across different miners and block ranges.

Requirements advanced: `R7`, `R11`.

Dependencies: Plan 009 completed, Unit 1 data available.

Files to create or modify: none.

Tests to add or modify: Test expectation: none -- checkpoint plan, artifact is the decision report.

Approach: From the 50-block regtest run, select at least 10 payout leaves belonging to different miners and at different positions in the Merkle tree. Build claim transactions for each and submit them after maturity. Record success and failure. Retry any failure once with identical inputs to distinguish deterministic from non-deterministic failures.

Specific test scenarios:

All 10 claim transactions must succeed on first attempt. If any claim fails non-deterministically (succeeds on retry with identical inputs), the witness interpreter has a bug. If any claim fails deterministically, record the leaf position, amount, and Merkle branch for diagnosis.

### Unit 4: Pre-Activation Compatibility Check

Goal: verify that pre-activation behavior is completely unchanged.

Requirements advanced: `R8`.

Dependencies: Plan 009 completed.

Files to create or modify: none.

Tests to add or modify: Test expectation: none -- checkpoint plan, artifact is the decision report.

Approach: Run the full existing test suite on the codebase with sharepool code present but the deployment inactive (no `-vbparams` override). Record any test failures. Compare the test results against the pre-sharepool baseline.

Specific test scenarios:

Zero test regressions. Any failure in a test that existed before sharepool code landed is a blocking issue. The reviewer must determine whether the failure is caused by sharepool code (blocking) or by a pre-existing flake (non-blocking but must be documented).

### Unit 5: Security Review

Goal: identify vulnerabilities in the new witness interpreter, relay surface, and reward computation.

Requirements advanced: `R1`, `R2`, `R7`.

Dependencies: Plans 005 and 007 completed.

Files to create or modify: none.

Tests to add or modify: Test expectation: none -- checkpoint plan, artifact is the decision report.

Approach: Read the new code in `src/script/interpreter.cpp` (claim verification), `src/net_processing.cpp` (share relay handling), `src/sharechain/payout.cpp` (reward computation), and `src/sharechain/share.cpp` (share validation). For each file, look for: input validation gaps, integer overflow in reward arithmetic, unbounded resource consumption from malformed shares, and any path where an invalid claim could be accepted.

Specific test scenarios:

The security review produces a checklist with findings, not automated test results. Each finding is rated as blocking (must fix before devnet), important (should fix before devnet), or informational (track for later). Zero blocking findings is required for the gate to pass.

### Unit 6: Performance Measurement

Goal: verify that share validation overhead is acceptable for a 4-core validator.

Requirements advanced: `R9`.

Dependencies: Plan 009 completed.

Files to create or modify: none.

Tests to add or modify: Test expectation: none -- checkpoint plan, artifact is the decision report.

Approach: On a 4-core test machine, run an activated regtest node mining with 4 threads and measure CPU utilization and resident memory over a 10-minute window. Then run the same scenario with sharepool deployment inactive and compare. The overhead is the difference.

Specific test scenarios:

CPU overhead must be less than 20%. Memory overhead must be less than 200 MiB. Share validation latency (time from share receipt to acceptance or rejection) must be less than 100 milliseconds for 95% of shares. If any threshold is exceeded, record the specific bottleneck for optimization before devnet.


## Concrete Steps

All commands assume the working directory is the repository root.

1. Run the Plan 009 regtest proof and capture output.

       test/functional/test_runner.py feature_sharepool_mining.py feature_sharepool_claims.py feature_sharepool_wallet.py

   Expected outcome: all three tests pass. If any test fails, the gate cannot proceed until the failure is resolved in Plan 009.

2. Run the full pre-existing test suite with sharepool deployment inactive.

       cmake --build build -j"$(nproc)" --target test_bitcoin
       build/src/test/test_bitcoin
       test/functional/test_runner.py

   Expected outcome: zero regressions compared to the pre-sharepool baseline.

3. Run the simulator comparison.

       python3 contrib/sharepool/simulate.py --scenario <exported-share-trace-from-regtest>

   Expected outcome: the simulator emits commitment roots that match the node's `getrewardcommitment` output for each of the 50 sampled blocks.

4. Start a fresh regtest node and verify replay determinism.

       build/src/rngd -regtest -datadir=/tmp/rng-replay-test -vbparams=sharepool:0:9999999999:0
       build/src/rng-cli -regtest -datadir=/tmp/rng-replay-test getrewardcommitment <height>

   Expected outcome: the fresh node reports the same commitment root as the original node for every sampled height.

5. Measure performance on a 4-core host.

       time build/src/rngd -regtest -daemon -vbparams=sharepool:0:9999999999:0 -mine -mineaddress=<addr> -minethreads=4

   Run for 10 minutes, then compare CPU and memory against a baseline run without sharepool activation. Record the delta.

6. Write the review report with all measurements, findings, and the go/no-go decision.


## Validation and Acceptance

This plan is accepted when all of the following are true.

A written review report exists that addresses all six evaluation criteria with specific measurements and evidence.

The reward accuracy evaluation shows per-miner reward variance within 5% of simulator predictions over 50 blocks, and commitment roots match byte-for-byte.

The determinism evaluation shows zero payout commitment disagreements between independent nodes.

The claim reliability evaluation shows zero non-deterministic claim failures across at least 10 test claims.

The pre-activation compatibility check shows zero test regressions.

The security review produces zero blocking findings.

The performance measurement shows CPU overhead below 20%, memory overhead below 200 MiB, and share validation latency below 100ms at the 95th percentile.

The review report concludes with an explicit go or no-go decision for Plan 011.


## Idempotence and Recovery

This plan is inherently idempotent because it is a review process, not a code change. The review can be re-run at any time by repeating the evaluation steps against the same regtest artifacts. If new code lands between review attempts (for example, a fix for an issue found during the first review), the entire review should be re-run from scratch against the updated codebase.

If the gate produces a no-go decision, the report must name the specific blocking issues. Plans 007, 008, or 009 should be revised to address those issues, and then this gate should be re-evaluated. There is no "partial pass" -- the gate either passes with all criteria met or it does not.

Regtest datadirs used during the review should be kept until the gate decision is final, then cleaned up. Use dedicated datadirs (for example, `/tmp/rng-gate-010-review/`) to avoid contaminating development environments.


## Artifacts and Notes

The primary artifact this plan produces is the review report. The report should follow this structure:

    Review Report: Plan 010 Decision Gate
    Date: <date>
    Reviewer: <name>

    1. Reward Accuracy
       Blocks sampled: <N>
       Hashrate ratio (expected): <X>:<Y>
       Reward ratio (measured): <A>:<B>
       Max per-block variance from simulator: <Z>%
       Commitment root match: <all matched / N mismatches at heights ...>
       Finding: <pass / fail with details>

    2. Determinism
       Independent node replay: <matched / N mismatches>
       Finding: <pass / fail with details>

    3. Claim Reliability
       Claims attempted: <N>
       Claims succeeded: <M>
       Non-deterministic failures: <K>
       Finding: <pass / fail with details>

    4. Pre-Activation Compatibility
       Test regressions: <N>
       Finding: <pass / fail with details>

    5. Security Review
       Blocking findings: <N>
       Important findings: <N>
       Informational findings: <N>
       Finding: <pass / fail with details>

    6. Performance
       CPU overhead: <X>%
       Memory overhead: <X> MiB
       Share validation p95 latency: <X> ms
       Finding: <pass / fail with details>

    Decision: <GO / NO-GO>
    Rationale: <summary>

The decision criteria are not subjective. They are:

If reward variance exceeds 15% from simulator predictions, the reward formula is broken and the gate fails.

If independent replay disagrees on any payout commitment, there is a determinism bug and the gate fails.

If any claim fails non-deterministically, the witness interpreter has a bug and the gate fails.

If any pre-activation test breaks due to sharepool code, the activation logic is wrong and the gate fails.

If the security review finds any blocking issue (validation bypass, unbounded resource consumption, integer overflow in reward arithmetic), the gate fails.

If CPU overhead exceeds 20% for share validation, optimization is needed and the gate fails.

A "soft fail" zone exists between 5% and 15% reward variance: the formula is not broken but the constants may need tuning. In that zone, the reviewer should document the variance and decide whether it is acceptable for devnet (where further tuning can occur) or whether it warrants a Plan 002/003 revision first.


## Interfaces and Dependencies

This plan depends on artifacts from Plans 002, 005, 007, 008, and 009. It does not introduce any new code interfaces.

The review consumes the following RPC surfaces added by earlier plans:

In `src/rpc/mining.cpp`, the `getrewardcommitment` RPC returns the payout commitment root for a given block height. The reviewer uses this to compare against simulator output and to verify cross-node determinism.

In `src/rpc/mining.cpp`, the `getsharechaininfo` RPC returns the current share tip, share count in the reward window, and accepted share statistics. The reviewer uses this to export the share trace for simulator comparison.

In `src/rpc/mining.cpp`, the `getmininginfo` RPC includes `sharepool_active`, `pending_pooled_reward`, and `accepted_shares` fields when the deployment is active. The reviewer uses these to verify wallet truthfulness.

The simulator at `contrib/sharepool/simulate.py` must accept exported share traces in the format established by Plan 002 and emit payout commitment roots plus per-miner leaf amounts.

This plan does not modify any of these interfaces. It consumes them as a reviewer.

Change note: Created plan 010 on 2026-04-12 as the mandatory decision gate between regtest proof (Plan 009) and devnet deployment (Plan 011). Reason: the genesis corpus dependency graph requires an explicit review checkpoint before any multi-node deployment, and the six evaluation criteria provide measurable pass/fail boundaries rather than subjective readiness assessments.
