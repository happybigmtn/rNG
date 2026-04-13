# Decision Gate: Simulator Results and Protocol Constants

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

This plan is a checkpoint, not an implementation task. It produces no code changes. Its purpose is to evaluate the outputs of Plan 002 (Sharepool Protocol Specification and Economic Simulator) and make a go/no-go decision on whether to proceed with the C++ implementation in Plans 004 and beyond. After this plan is complete, the team will have a written decision report that either confirms the proposed protocol constants are sound and authorizes implementation, or identifies specific problems that must be resolved before implementation begins.

The user-visible proof is a decision report file at `genesis/plans/003-decision-report.md` that answers six questions with quantitative evidence from the simulator. The report either says "go: proceed to Plan 004 with these locked constants" or "no-go: revise Plan 002 because of these specific findings." There is no middle ground. The decision gate exists precisely to prevent building consensus code on top of unsettled or broken economics.

This plan is the second step in the genesis plan corpus indexed in `genesis/PLANS.md`. It corresponds to Plan 003 in that index. It depends entirely on Plan 002 having been completed: the sharepool spec must exist at `specs/sharepool.md`, the simulator must run at `contrib/sharepool/simulate.py`, and the example traces and analysis results from Plan 002 must be available.

## Requirements Trace

This plan does not directly advance any implementation requirement. Instead, it validates that the artifacts from Plan 002 satisfy the preconditions for implementation.

`R1` (consensus-enforced reward). This plan evaluates whether the proposed reward-window formula and payout-commitment encoding are correct and complete enough to be implemented in consensus code. If the formula produces unexpected results under adversarial conditions, R1 is not yet satisfiable.

`R2` (public share history). This plan evaluates whether the deterministic replay contract holds: does the simulator produce identical outputs from identical inputs? If replay is not deterministic, R2 cannot be implemented.

`R3` (immediate accrual). This plan evaluates whether the pending-entitlement scenario from the simulator produces non-zero accrual for contributing miners before a block is found. If accrual is zero or undefined before a block event, R3 needs redesign.

`R6` (compact commitment). This plan evaluates whether the Merkle commitment fits within the coinbase OP_RETURN data carrier limit (80 bytes). If it does not, R6 needs a different encoding.

`R7` (trustless claims). This plan evaluates whether the proposed claim witness program design is expressible as a new witness-program version without requiring new opcodes. If new opcodes are needed, the activation path changes from soft fork to hard fork.

`R11` (truthfulness). This plan verifies that the spec and simulator correctly distinguish between "pending pooled reward" (visible, not spendable) and "claimable pooled reward" (spendable after 100-block maturity). If this distinction is absent or ambiguous, the documentation contract is broken.

## Scope Boundaries

This plan produces a decision report. It does not produce any code changes, spec modifications, or configuration changes. If the decision is "no-go," the corrective action happens in a revised Plan 002, not in this plan.

This plan does not evaluate the C++ implementation, because no C++ implementation exists at this stage. It evaluates only the protocol specification and the simulator output.

This plan does not evaluate P2P relay performance, wallet integration, or devnet behavior. Those are concerns for later decision gates (Plan 006 for relay viability, Plan 010 for regtest proof review).

This plan does not evaluate any parallel QSB rollout work or fleet operational state. The local root `EXECPLAN.md` may still be relevant operationally, but it is not part of the verified source baseline for this simulator gate.

This plan does not produce activation parameters, deployment timelines, or mainnet schedules. Activation is Plan 012.

## Progress

- [ ] Confirm Plan 002 is complete: `specs/sharepool.md` exists, `contrib/sharepool/simulate.py` runs, all example traces produce expected output.
- [ ] Review the sharepool spec for completeness and internal consistency.
- [ ] Review the simulator's deterministic replay property: run each trace twice and verify byte-identical output.
- [ ] Evaluate Question 1: Are the proposed share spacing and window constants economically sound?
- [ ] Evaluate Question 2: Does share withholding produce unacceptable advantage?
- [ ] Evaluate Question 3: Is the reward variance acceptable for small miners?
- [ ] Evaluate Question 4: Can the Merkle commitment fit in a standard coinbase?
- [ ] Evaluate Question 5: Does the claim witness program design survive analysis?
- [ ] Evaluate Question 6: Is version-bits activation realistic, or does the design require a hard fork?
- [ ] Write the decision report at `genesis/plans/003-decision-report.md`.
- [ ] Record the go/no-go recommendation.
- [ ] If go: confirm the locked constants file at `contrib/sharepool/constants.json` is authoritative.
- [ ] If no-go: document the specific findings and required revisions for Plan 002.

## Surprises & Discoveries

No evaluation work has started. This section will be populated as the decision gate proceeds.

- Observation: (placeholder)
  Evidence: (placeholder)

## Decision Log

- Decision: Create an explicit decision gate between the spec/simulator phase and the implementation phase.
  Rationale: The sharechain economics and payout constants are the highest-risk unknowns in the entire pooled mining design. Building consensus code on top of unsettled constants creates revert pressure and wastes developer time. A formal checkpoint forces the team to confront simulator results honestly before proceeding.
  Date/Author: 2026-04-12 / genesis corpus

- Decision: Define quantitative pass/fail thresholds rather than subjective "looks good" criteria.
  Rationale: Without concrete thresholds, decision gates become rubber stamps. The thresholds chosen (5% withholding advantage, 10% reward variance, 100-byte commitment size) are informed by the practical constraints of a small CPU-mined network and the Bitcoin-derived coinbase and OP_RETURN limits.
  Date/Author: 2026-04-12 / genesis corpus

## Outcomes & Retrospective

This plan has not started. Outcomes will be recorded as the evaluation proceeds and when the decision report is finalized. The expected outcome is a clear go or no-go recommendation with quantitative evidence.

## Context and Orientation

This plan is a checkpoint in the genesis plan corpus for RNG's protocol-native pooled mining work. It sits between Plan 002 (specification and simulator) and Plan 004 (version-bits deployment skeleton). Its role is to prevent premature implementation.

RNG is a Bitcoin Core-derived chain with RandomX proof of work. The checked-in docs still identify the base as Bitcoin Core `v29.0`, so this plan does not assert a completed newer upstream port as a verified current fact. The chain is live on mainnet with 120-second block targets, 50 RNG initial reward, and 100-block coinbase maturity. No sharechain code exists in the verified target-repo source. The sharepool protocol specification and simulator are being developed in Plan 002.

Six questions must be answered by this decision gate. Each question has a quantitative threshold that determines pass or fail. If any question fails, the recommendation is no-go, and Plan 002 must be revised before implementation proceeds. The questions and thresholds are:

Question 1: Are the proposed share spacing and window constants economically sound? The proposed starting constants from Plan 002 are: target share spacing of 10 seconds (one share every 10 seconds, or about 12 shares per 120-second block interval), reward-window work equivalent to approximately 720 shares at target share difficulty. "Economically sound" means: the share rate is achievable by a single-thread miner on commodity hardware, the window is long enough to smooth reward variance but short enough that miners see meaningful accrual within a reasonable time, and the constants do not create perverse incentives.

Question 2: Does share withholding produce unacceptable advantage? "Share withholding" means a miner mines valid shares but does not publish them, hoping to manipulate the reward window in its favor. The threshold is: if a withholder gains more than 5% additional reward compared to honest behavior in the simulator's withholding-attack trace, the proposed constants need revision. This 5% threshold is stricter than Bitcoin's selfish-mining analysis (which tolerates up to ~25% for a 33% miner) because RNG is a small network where even modest advantages compound quickly.

Question 3: Is the reward variance acceptable for small miners? A miner contributing 10% of network hashrate should see predictable per-block rewards. The threshold is: if the coefficient of variation (standard deviation divided by mean) of per-block rewards for a 10% miner over 100 simulated blocks exceeds 10%, the window constants need revision. High variance defeats the purpose of pooled mining, which is to make small miners' rewards predictable.

Question 4: Can the Merkle commitment fit in a standard coinbase? The proposed encoding from Plan 002 is an OP_RETURN output containing a 4-byte tag prefix ("RNGS") plus a 32-byte Merkle root, totaling 38 bytes of data (plus script overhead). The threshold is: if the total coinbase commitment encoding exceeds 100 bytes, the encoding needs revision. This leaves headroom within the 80-byte OP_RETURN data carrier limit for the data payload itself, and additional space in the coinbase for the standard height commitment (BIP34) and any other existing commitments.

Question 5: Does the claim witness program design survive analysis? The proposed design uses witness version 2, where the witness program data is the 32-byte payout commitment root and the witness stack provides the Merkle branch, leaf index, payout amount, and a signature proving ownership of the payout script. "Survives analysis" means: the claim can be expressed as a new witness-program interpreter in `src/script/interpreter.cpp` without requiring new opcodes, without modifying existing witness version 0 or version 1 (Taproot) behavior, and without creating an anyone-can-spend vulnerability for non-upgraded nodes during the activation transition. If the analysis reveals that claim verification needs computation beyond what a witness-program interpreter can provide (for example, if it needs access to the UTXO set or to sharechain state during script execution), the design must be revised.

Question 6: Is version-bits activation realistic, or does the design require a hard fork? The proposed activation uses BIP9 version-bits signaling, which is how RNG activated Taproot from genesis. For the sharepool deployment, version-bits activation is realistic if and only if: (a) old nodes that have not upgraded treat the new payout commitment output as valid (which is true if the commitment is an OP_RETURN output, since OP_RETURN is consensus-valid), (b) old nodes treat the new claim witness program version as anyone-can-spend (which is true for witness versions 2-16 under current Bitcoin script rules), and (c) no existing consensus rule is violated by the post-activation coinbase format. If any of these conditions fails, the activation must be a hard fork, which changes the deployment complexity and risk profile significantly.

## Plan of Work

This is a review and evaluation task, not a coding task. The work proceeds in three steps:

Step 1: Verify Plan 002 completeness. Confirm that `specs/sharepool.md`, `contrib/sharepool/simulate.py`, and all example traces exist and produce expected output. If any artifact is missing or broken, the decision gate cannot proceed and Plan 002 must be completed first.

Step 2: Run the six evaluations. For each question, run the relevant simulator scenario (or perform the relevant analysis) and record the quantitative result against the threshold. The evaluator should record the exact simulator commands, inputs, and outputs so that the results are reproducible.

Step 3: Write the decision report. The report must state each question, the threshold, the observed result, and whether the result passes or fails. If all six questions pass, the recommendation is "go: proceed to Plan 004 with the locked constants." If any question fails, the recommendation is "no-go: revise Plan 002" with specific guidance on what needs to change.

## Implementation Units

### Unit 1: Plan 002 Completeness Verification

Goal: Confirm that all prerequisites for the decision gate exist and are functional.

Requirements advanced: None directly; this unit verifies that Plan 002 has advanced R1, R2, R3, R6, R7, R11.

Dependencies: Plan 002 must be complete.

Files to create or modify: None.

Tests to add or modify: Test expectation: none -- checkpoint plan. Verification is manual: confirm the spec file exists, the simulator runs, and example traces produce output.

Approach: Read `specs/sharepool.md` end to end and verify it defines all required elements (share object, sharechain tip selection, reward window, payout commitment, claim format, activation semantics). Run `python3 contrib/sharepool/simulate.py contrib/sharepool/examples/two-miner-basic.json` and confirm it produces a commitment root and per-miner payout amounts. Run each example trace and confirm it produces output without errors. Verify that `contrib/sharepool/constants.json` exists and contains all proposed constants.

Test scenarios: Test expectation: none -- checkpoint plan. The completeness check is a prerequisite for the evaluation, not a test.

### Unit 2: Quantitative Evaluation of Six Questions

Goal: Evaluate each decision question against its threshold and record the result.

Requirements advanced: None directly; this unit validates that Plan 002's artifacts are sufficient for implementation.

Dependencies: Unit 1 (completeness verification must pass first).

Files to create or modify: None during evaluation. Results are recorded in the decision report (Unit 3).

Tests to add or modify: Test expectation: none -- checkpoint plan.

Approach: For each of the six questions, run the relevant analysis. Question 1 (economic soundness): review the target share spacing and reward-window work constants in `contrib/sharepool/constants.json`, run the basic two-miner trace, and assess whether the share rate and window size are reasonable for the current RNG network (4-10 nodes, commodity CPU hardware, 120-second block targets). Question 2 (withholding): run `python3 contrib/sharepool/simulate.py contrib/sharepool/examples/withholding-attack.json` and compare the withholder's reward to the honest baseline. If the advantage exceeds 5%, record a failure. Question 3 (variance): run `python3 contrib/sharepool/simulate.py contrib/sharepool/examples/variance-100-blocks.json --stats` and check the coefficient of variation for the 10% miner. If it exceeds 10%, record a failure. Question 4 (commitment size): count the bytes in the proposed coinbase commitment encoding. If the encoding exceeds 100 bytes, record a failure. This can be verified by reading the spec and running the simulator with verbose output showing the serialized commitment. Question 5 (witness program): perform a design analysis of the claim witness program. Verify that the claim can be expressed as a new witness-program interpreter, that it does not require new opcodes, and that non-upgraded nodes treat it as anyone-can-spend. This is a reasoning task, not a simulator task. Question 6 (activation path): verify the three conditions for version-bits activation: old nodes accept the OP_RETURN commitment output, old nodes treat witness v2 as anyone-can-spend, and no existing consensus rule is violated by the post-activation coinbase format. This is also a reasoning task.

Test scenarios: Test expectation: none -- checkpoint plan.

### Unit 3: Decision Report

Goal: Write the decision report with quantitative evidence and a clear go/no-go recommendation.

Requirements advanced: None directly; this unit produces the gating artifact for Plans 004+.

Dependencies: Unit 2 (all six evaluations must be complete).

Files to create or modify:
- `genesis/plans/003-decision-report.md` (new)
- `contrib/sharepool/constants.json` (confirm or update based on evaluation)

Tests to add or modify: Test expectation: none -- checkpoint plan.

Approach: Write a structured document with one section per question. Each section states the question, the pass/fail threshold, the observed result (with exact simulator output or reasoning), and the verdict. The final section states the overall recommendation: go or no-go. If go, confirm that `contrib/sharepool/constants.json` contains the authoritative locked constants. If no-go, describe the specific revisions needed and which Plan 002 units must be re-executed.

Test scenarios: Test expectation: none -- checkpoint plan.

## Concrete Steps

All commands below assume the working directory is the repository root unless stated otherwise.

Step 1: Verify Plan 002 prerequisites.

    test -f specs/sharepool.md && echo "Spec exists" || echo "MISSING: specs/sharepool.md"
    test -f contrib/sharepool/simulate.py && echo "Simulator exists" || echo "MISSING: simulator"
    test -f contrib/sharepool/constants.json && echo "Constants file exists" || echo "MISSING: constants"
    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/two-miner-basic.json

Expected output for the simulator run: a JSON object with a commitment root and per-miner payout amounts summing to 5,000,000,000 roshi (50 RNG). If the simulator fails or the files do not exist, stop and complete Plan 002 first.

Step 2: Run the deterministic replay check.

    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/two-miner-basic.json > /tmp/gate-run1.json
    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/two-miner-basic.json > /tmp/gate-run2.json
    diff /tmp/gate-run1.json /tmp/gate-run2.json && echo "PASS: deterministic" || echo "FAIL: non-deterministic"

Expected output: "PASS: deterministic".

Step 3: Run the withholding analysis.

    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/withholding-attack.json

Expected output: a comparison showing the withholder's reward versus honest behavior. Record the percentage advantage.

Step 4: Run the variance analysis.

    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/variance-100-blocks.json --stats

Expected output: per-miner statistics including mean, standard deviation, and coefficient of variation for the 10% miner. Record the coefficient of variation.

Step 5: Verify commitment size.

    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/two-miner-basic.json --verbose

Expected output: the serialized commitment encoding with byte count. Verify the total is under 100 bytes.

Step 6: Analyze the witness program design.

No command to run. Read `specs/sharepool.md` and evaluate whether the claim witness program can be expressed as a new witness-program interpreter without new opcodes. Document the reasoning.

Step 7: Analyze the activation path.

No command to run. Read `specs/sharepool.md` and evaluate the three conditions for version-bits activation. Document the reasoning.

Step 8: Write the decision report.

    # Write genesis/plans/003-decision-report.md per Unit 3

After this step, the decision report should exist and contain a clear go/no-go recommendation.

## Validation and Acceptance

The plan is complete when all of the following are true.

A decision report exists at `genesis/plans/003-decision-report.md`.

The decision report addresses all six questions with quantitative evidence or documented reasoning.

Each question has a clear pass or fail verdict against its stated threshold.

The overall recommendation is either "go" or "no-go" with no ambiguity.

If "go": `contrib/sharepool/constants.json` is confirmed as authoritative and is consistent with the decision report.

If "no-go": the decision report specifies exactly which Plan 002 units must be re-executed and what must change.

## Idempotence and Recovery

This plan is purely evaluative. It can be re-run at any time by re-running the simulator scenarios and re-reading the spec. If Plan 002 artifacts change (because the spec is revised or the simulator is updated), this decision gate must be re-run from the beginning.

If the decision is "no-go" and Plan 002 is revised, the decision report from this plan becomes stale. A new evaluation must be performed and a new decision report written. The old decision report should be preserved with a dated "superseded" note at the top rather than deleted, so the reasoning history is retained.

There are no destructive operations in this plan. No files are modified except the decision report itself. No node binaries, chain data, or fleet configurations are touched.

## Artifacts and Notes

The decision criteria, collected in one place for reference:

    Question 1 (economic soundness):
      Pass: share spacing and window work are achievable on commodity hardware
            and do not create perverse incentives.
      Fail: share rate is unachievable, window is too short or too long, or
            constants create perverse incentives.

    Question 2 (withholding):
      Pass: withholder advantage is under 5%.
      Fail: withholder advantage is 5% or more.
      Threshold source: stricter than Bitcoin's selfish-mining tolerance because
                        RNG is a small network where modest advantages compound.

    Question 3 (variance):
      Pass: coefficient of variation for a 10% miner over 100 blocks is under 10%.
      Fail: coefficient of variation is 10% or more.
      Threshold source: pooled mining's purpose is reward predictability. If a
                        10% miner's per-block rewards vary by more than 10% of
                        the mean, the protocol is not delivering on its promise.

    Question 4 (commitment size):
      Pass: total coinbase commitment encoding is under 100 bytes.
      Fail: encoding is 100 bytes or more.
      Threshold source: Bitcoin OP_RETURN data carrier limit is 80 bytes. The
                        proposed encoding uses 38 bytes of data, well under this
                        limit. The 100-byte threshold includes script overhead.

    Question 5 (witness program):
      Pass: claim can be expressed as witness-program interpreter without new
            opcodes, and non-upgraded nodes treat it as anyone-can-spend.
      Fail: claim requires new opcodes or modifies existing witness behavior.

    Question 6 (activation path):
      Pass: all three soft-fork conditions hold (OP_RETURN accepted by old nodes,
            witness v2 is anyone-can-spend for old nodes, no existing consensus
            rule violated by post-activation coinbase).
      Fail: any condition fails, requiring hard fork.

The decision report template:

    # Decision Gate 003: Evaluation Report

    Date: <date>
    Evaluator: <name>
    Plan 002 status: complete / incomplete

    ## Question 1: Economic Soundness
    Threshold: achievable share rate, reasonable window, no perverse incentives
    Observed: <evidence from simulator>
    Verdict: PASS / FAIL

    ## Question 2: Share Withholding
    Threshold: withholder advantage < 5%
    Observed: <exact percentage from simulator>
    Verdict: PASS / FAIL

    ## Question 3: Reward Variance
    Threshold: CV for 10% miner over 100 blocks < 10%
    Observed: <exact CV from simulator>
    Verdict: PASS / FAIL

    ## Question 4: Commitment Size
    Threshold: total encoding < 100 bytes
    Observed: <exact byte count>
    Verdict: PASS / FAIL

    ## Question 5: Witness Program Feasibility
    Threshold: expressible as witness-program interpreter, no new opcodes
    Observed: <reasoning>
    Verdict: PASS / FAIL

    ## Question 6: Activation Path
    Threshold: all three soft-fork conditions hold
    Observed: <reasoning per condition>
    Verdict: PASS / FAIL

    ## Overall Recommendation
    <GO / NO-GO>

    If GO: constants locked in contrib/sharepool/constants.json are authoritative.
    Proceed to Plan 004.

    If NO-GO: the following revisions are required before re-evaluation:
    <specific items>

## Interfaces and Dependencies

This plan depends on the following artifacts from Plan 002:

- `specs/sharepool.md` -- the protocol specification. Must be complete and internally consistent.
- `contrib/sharepool/simulate.py` -- the deterministic Python simulator. Must run without errors on all example traces.
- `contrib/sharepool/examples/two-miner-basic.json` -- the basic two-miner trace for Questions 1 and 4.
- `contrib/sharepool/examples/withholding-attack.json` -- the withholding analysis trace for Question 2.
- `contrib/sharepool/examples/variance-100-blocks.json` -- the variance analysis trace for Question 3.
- `contrib/sharepool/examples/pending-only.json` -- the pending-entitlement trace for R3 verification.
- `contrib/sharepool/constants.json` -- the proposed constants file.

This plan produces one new artifact:

- `genesis/plans/003-decision-report.md` -- the decision report with go/no-go recommendation.

This plan does not produce any compiled code, library interfaces, RPC surfaces, or P2P messages. It does not modify any existing code file. It does not evolve any existing interface.

The downstream dependency is clear: Plans 004 through 012 in the genesis corpus cannot proceed until this decision gate produces a "go" recommendation. If the recommendation is "no-go," Plan 002 must be revised and this gate must be re-evaluated before any implementation work begins.
