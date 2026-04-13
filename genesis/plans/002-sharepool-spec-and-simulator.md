# Sharepool Protocol Specification and Economic Simulator

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

After this plan is complete, RNG will have two new artifacts that did not exist before: a protocol specification for protocol-native pooled mining (`specs/sharepool.md`) and a deterministic Python simulator (`contrib/sharepool/simulate.py`) that accepts synthetic share traces and outputs the exact payout commitments a conforming RNG node would produce. Together these artifacts lock the protocol constants, the share object format, the reward-window formula, and the payout-commitment encoding before any consensus C++ code is written.

The user-visible proof is simple. An operator runs the simulator against a two-miner trace file, and it prints a payout commitment root plus per-miner reward leaves whose amounts are proportional to submitted work. Running the same trace a second time produces byte-identical output. Running a modified trace where one miner withholds shares produces a measurably different (and measurably less favorable for the withholder) result, or the simulator exposes that the proposed constants need revision before proceeding.

This plan is the first implementation step in the genesis plan corpus indexed in `genesis/PLANS.md`. It corresponds to Plan 002 in that index. It has no dependencies on other numbered plans, but later plans (particularly 003, the decision-gate checkpoint, and 004 onward, the node implementation) depend on the outputs of this plan.

## Requirements Trace

The following requirements are drawn from `docs/rng-protocol-native-pooled-mining-execplan.md` and `genesis/ASSESSMENT.md`. Each label is referenced throughout this plan.

`R1` (consensus-enforced reward). After activation, RNG must replace the legacy "block finder receives the full coinbase reward" contract with a deterministic pooled reward contract enforced by consensus, not by an operator database or external service. This plan advances R1 by defining the reward-window formula and payout-commitment encoding that consensus code will later enforce.

`R2` (public share history). The pooled reward contract must be derived from a public share history. Any fully validating RNG node must be able to replay the same accepted share window and derive the same per-block payout commitment. This plan advances R2 by specifying the share object fields, sharechain tip-selection rule, and the deterministic replay contract.

`R3` (immediate accrual). A miner that contributes low-difficulty shares must begin accruing a proportional share of every block immediately after those shares enter the accepted share history. This plan advances R3 by defining the reward-window work calculation and proving through the simulator that pending entitlement is visible before a block is found.

`R6` (compact commitment). The consensus design must scale without emitting thousands of direct coinbase outputs per block. This plan advances R6 by specifying the Merkle-tree payout commitment shape and encoding it in the coinbase.

`R7` (trustless claims). A miner must be able to prove its entitlement from the payout commitment, its payout key, and the public share window, without requesting settlement from an operator. This plan advances R7 by defining the claim leaf format and the claim witness program version.

`R11` (truthfulness). The implementation and docs must say explicitly whether immediate wallet spendability is still constrained by the current 100-block coinbase maturity rule. This plan advances R11 by embedding the accrual-vs-claimability distinction into the spec and the simulator output.

## Scope Boundaries

This plan writes specifications and a simulator. It does not write C++ consensus code, P2P relay code, wallet code, or version-bits deployment wiring. Those are Plan 004 and later.

This plan does not activate any behavior on regtest, testnet, or mainnet. It produces only offline artifacts.

This plan does not modify the existing live mining flow or fleet configuration. The local root `EXECPLAN.md` describes parallel QSB rollout work, but corresponding QSB source files were not present in the inspected checkout. Treat that document as separate local planning context, not an in-tree dependency for this plan.

This plan does not define the internal miner's share-production loop or the `getblocktemplate` sharepool section. Those are Plan 008 (miner/wallet integration).

This plan does not produce a final mainnet activation schedule. Activation timing is Plan 012.

This plan updates `specs/consensus.md`, `specs/activation.md`, and `specs/agent-integration.md` only to the extent needed to reflect the new sharepool specification. It does not rewrite those files wholesale.

## Progress

- [ ] Read and incorporate all existing context: `docs/rng-protocol-native-pooled-mining-execplan.md`, `genesis/ASSESSMENT.md`, `genesis/DESIGN.md`, `genesis/FOCUS.md`, `specs/consensus.md`, `specs/activation.md`.
- [ ] Draft `specs/sharepool.md` with share object fields, sharechain tip-selection rule, reward-window formula, payout-commitment encoding, claim format, and activation semantics.
- [ ] Settle target share spacing constant.
- [ ] Settle reward-window work constant.
- [ ] Settle claim witness program version.
- [ ] Settle max orphan shares constant.
- [ ] Settle share target relative to block target.
- [ ] Evaluate finder bonus viability versus publication incentive.
- [ ] Implement `contrib/sharepool/simulate.py` with deterministic replay from share traces.
- [ ] Write example trace files under `contrib/sharepool/examples/`.
- [ ] Validate simulator scenario: 90/10 work split produces proportional reward leaves.
- [ ] Validate simulator scenario: deterministic replay produces identical commitment roots.
- [ ] Validate simulator scenario: reorged share suffix changes only affected window outputs.
- [ ] Validate simulator scenario: pending entitlement visible before block is found.
- [ ] Evaluate share-withholding advantage under proposed constants.
- [ ] Evaluate reward variance for a 10% miner over 100 blocks.
- [ ] Evaluate Merkle commitment size in coinbase.
- [ ] Update `specs/consensus.md` with sharepool activation semantics.
- [ ] Update `specs/activation.md` with `DEPLOYMENT_SHAREPOOL` entry.
- [ ] Update `specs/agent-integration.md` to replace the aspirational pool-mine reference with the protocol-native sharepool description.
- [ ] Write locked constants file with all settled values.
- [ ] Prepare findings for Plan 003 decision gate.

## Surprises & Discoveries

No implementation work has started. This section will be populated as the spec and simulator are developed.

- Observation: (placeholder)
  Evidence: (placeholder)

## Decision Log

- Decision: Start with a specification-plus-simulator plan rather than jumping directly into C++ consensus code.
  Rationale: The sharechain economics and payout constants are the highest-risk unknowns. A Python simulator is the cheapest way to reject bad designs. Writing consensus code on top of unsettled constants wastes effort and creates revert pressure.
  Date/Author: 2026-04-12 / genesis corpus

- Decision: Use the reward-window formula from `docs/rng-protocol-native-pooled-mining-execplan.md` as the starting point, not an arbitrary new design.
  Rationale: The existing exec plan was written after reviewing RNG's consensus, Zend's rBTC pool work, and Bitcoin's UTXO/script constraints. It represents the best current thinking. The simulator's job is to validate or invalidate those choices, not to start over.
  Date/Author: 2026-04-12 / genesis corpus

## Outcomes & Retrospective

This plan has not started. Outcomes will be recorded as work proceeds and at completion. The expected final outcome is a locked protocol spec, a working simulator, settled constants, and a findings package sufficient for the Plan 003 decision gate.

## Context and Orientation

RNG is a Bitcoin Core-derived chain that replaced SHA256d proof of work with RandomX, a CPU-oriented ASIC-resistant hash algorithm. The checked-in repo docs still identify the base as Bitcoin Core `v29.0`, while separate local plans discuss later port work; this plan therefore avoids asserting a newer upstream base as a verified current fact. The chain is live on mainnet. Verified target-repo parameters include 120-second block targets, 50 RNG initial reward, 2.1M-block halving intervals, a 0.6 RNG tail emission floor, Monero-style LWMA per-block difficulty adjustment over a 720-block window, 100-block coinbase maturity, and bech32 addresses with the `rng1` prefix. The smallest unit is called a "roshi" (1 roshi = 0.00000001 RNG).

The current mining model is classical Bitcoin: a single miner finds a block and receives the entire coinbase reward. There is no concept of shares, sharechains, reward windows, or pooled payouts anywhere in the codebase. The existing design document at `docs/rng-protocol-native-pooled-mining-execplan.md` describes the intended architecture in prose but contains zero implementation.

Five terms appear throughout this plan:

A "share" is a proof of work whose RandomX hash meets a target that is easier than the full block target but hard enough to be economically meaningful. A share proves that a miner performed real work without necessarily producing a valid block. Shares are public objects relayed between peers on the RNG network.

A "sharechain" is an ordered sequence of accepted shares, analogous to the blockchain but moving faster (one share every few seconds instead of one block every 120 seconds). The sharechain records which miners contributed recent work and in what proportion.

A "reward window" is the trailing range of accepted shares whose cumulative work determines the payout split for the next block. The window is defined by a total work threshold, not a fixed number of shares. When a block is found, the node looks back along the sharechain until it has accumulated enough cumulative share work to fill the window, and it splits the block reward proportionally among the payout scripts that appear in that window.

A "payout commitment" is a Merkle tree whose leaves are the individual payout entries (script, amount, share range) for one block. The root of this tree is encoded in the coinbase transaction. Any miner can later prove it owns a leaf by providing a Merkle branch and the payout script.

A "claim" is a transaction that spends one leaf of a matured payout commitment. The claim proves three things: the leaf exists under the committed root, the amount and payout script match the leaf, and the spender controls the payout script. Claims use a new witness program version so that existing script paths are not modified.

The key files relevant to this plan are:

- `docs/rng-protocol-native-pooled-mining-execplan.md` -- the existing design document that seeds the architecture.
- `genesis/ASSESSMENT.md` -- the repository assessment documenting what exists, what is missing, and what is aspirational.
- `genesis/DESIGN.md` -- the design document covering information architecture, state coverage, and key design decisions for pooled mining.
- `genesis/PLANS.md` -- the plan index that sequences this plan as 002 in the overall corpus.
- `specs/consensus.md` -- the current consensus parameters specification. Must be updated to describe the post-activation reward contract.
- `specs/activation.md` -- the current activation specification. Must be updated to include `DEPLOYMENT_SHAREPOOL`.
- `specs/agent-integration.md` -- the agent integration specification. Currently aspirational and references an unimplemented `pool-mine` mode. Must be updated to describe the protocol-native sharepool.
- `src/kernel/chainparams.cpp` -- the live chain parameters. Not modified by this plan, but its constants (120s block time, 50 RNG reward, LWMA window of 720 blocks, coinbase maturity of 100) constrain the simulator.
- `src/pow.cpp` -- the RandomX proof-of-work implementation. Not modified by this plan, but share difficulty is derived from the same RandomX hash check.

## Plan of Work

The work proceeds in three phases: specification writing, simulator implementation, and spec updates.

Phase 1 writes `specs/sharepool.md`. This is the protocol specification. It defines the share object with its fields (parent_share, prev_block_hash, payout_script, nBits, nTime, nNonce, and candidate context), the sharechain tip-selection rule (most cumulative accepted share work), the reward-window formula (how much cumulative work defines the trailing window), the payout-leaf format (payout_script, amount in roshi, first_share id, last_share id), the commitment encoding (a binary Merkle tree over sorted payout leaves, with the root inserted as an OP_RETURN output in the coinbase), the claim witness program (a new witness version that verifies Merkle branch plus script ownership), and the activation semantics (BIP9 version-bits deployment with `DEPLOYMENT_SHAREPOOL`). The spec also defines the constants to be settled: target share spacing, reward-window work, claim witness version, max orphan shares, and share target relative to block target.

Phase 2 builds `contrib/sharepool/simulate.py`. The simulator is a single-file Python script with no dependencies beyond the standard library (plus `hashlib` for SHA256). It reads a JSON share trace from stdin or a file, replays the sharechain, computes the reward window for each block event in the trace, builds payout leaves, hashes them into a Merkle tree, and prints the commitment root plus per-miner reward amounts. The simulator must be deterministic: the same input always produces the same output. It must also support a "pending" query that shows accrual before any block is found.

Phase 3 updates existing specs. `specs/consensus.md` gains a new section describing the post-activation coinbase commitment requirement. `specs/activation.md` gains a new table row for `DEPLOYMENT_SHAREPOOL` with the settled parameters. `specs/agent-integration.md` replaces the aspirational `pool-mine` example with a description of the protocol-native share submission flow.

Throughout all three phases, the plan evaluates five questions that the Plan 003 decision gate will judge: (1) Is a finder bonus viable, or does the protocol need a publication incentive to prevent block finders from excluding others' shares? (2) Does share withholding produce unacceptable advantage? (3) Is the reward variance acceptable for small miners? (4) Can the Merkle commitment fit in a standard coinbase? (5) Does the claim witness program design survive analysis?

## Implementation Units

### Unit 1: Protocol Specification

Goal: Write `specs/sharepool.md` defining the share object, sharechain structure, reward window, payout commitment, claim format, and activation semantics.

Requirements advanced: R1, R2, R3, R6, R7, R11.

Dependencies: None.

Files to create or modify:
- `specs/sharepool.md` (new)

Tests to add or modify: Test expectation: none -- this unit produces a prose specification, not executable code.

Approach: Start from the architecture described in `docs/rng-protocol-native-pooled-mining-execplan.md` and refine it into a normative specification. The share object must include at minimum: `share_id` (SHA256 hash of the serialized share header), `parent_share` (id of the previous share in the sharechain), `prev_block_hash` (the block tip the share was built against), `payout_script` (the serialized scriptPubKey that will receive the miner's portion of the reward), `nBits` (the compact difficulty target the share claims to satisfy), `nTime` (the share timestamp), `nNonce` (the RandomX nonce), and `candidate_context` (the block header fields needed to verify the RandomX hash). The reward-window formula must be defined as a cumulative-work threshold: start from the share that produced the block and walk backward along the sharechain, accumulating share work until the total exceeds the reward-window-work constant. All shares in that range contribute proportionally to the payout. The payout commitment must be a binary Merkle tree whose leaves are sorted by payout_script bytes, and whose root is placed in a coinbase OP_RETURN output with a four-byte sharepool tag prefix. The claim format must use a new witness program version (proposed: version 2, since Bitcoin reserves versions 2-16 for future upgrades) where the witness program data is the 32-byte payout commitment root and the witness stack provides the Merkle branch, leaf index, payout amount, and a signature proving control of the payout_script.

Test scenarios: Test expectation: none -- this unit produces a prose specification, not executable code. The specification's correctness is validated by the simulator in Unit 2 and the decision gate in Plan 003.

### Unit 2: Deterministic Python Simulator

Goal: Build `contrib/sharepool/simulate.py` that accepts share traces and outputs payout commitments, demonstrating that the protocol specification from Unit 1 is internally consistent and economically sound.

Requirements advanced: R1, R2, R3, R6, R11.

Dependencies: Unit 1.

Files to create or modify:
- `contrib/sharepool/simulate.py` (new)
- `contrib/sharepool/examples/two-miner-basic.json` (new)
- `contrib/sharepool/examples/ten-miner-varied.json` (new)
- `contrib/sharepool/examples/reorg-suffix.json` (new)
- `contrib/sharepool/examples/pending-only.json` (new)
- `contrib/sharepool/examples/withholding-attack.json` (new)

Tests to add or modify: The simulator is its own test harness. Each example trace is a test scenario. A future `test/functional/feature_sharepool_simulator.py` may be added in a later plan to run the simulator as part of CI, but this unit focuses on the simulator itself.

Approach: The simulator is a single Python file that reads a JSON share trace. A share trace is a JSON array of share objects, each containing the fields defined in Unit 1 plus a `block_event` flag that indicates whether this share also meets the full block target. The simulator maintains a sharechain (an ordered list of accepted shares) and, for each block event, computes the reward window by walking backward through the sharechain until the cumulative work threshold is met. It then builds payout leaves by aggregating work per unique payout_script, computes each leaf's reward amount proportional to its share of window work, sorts leaves by payout_script, builds a binary Merkle tree, and prints the commitment root plus a JSON summary of per-miner payouts. The simulator must also support a `--pending` flag that outputs the current accrual state without a block event, proving R3 (immediate accrual). The simulator must use only deterministic operations: no floating-point reward arithmetic (use integer roshi amounts and distribute remainders to the block finder or the last leaf), no random number generation, no system-dependent behavior.

Test scenarios:
- 90/10 work split: A trace where miner A submits 9 shares and miner B submits 1 share, all at the same difficulty, followed by a block event by miner A. The simulator must output two payout leaves where miner A's amount is approximately 90% of the block reward and miner B's amount is approximately 10%, with exact roshi values summing to the total reward (subsidy plus fees).
- Deterministic replay: Run the simulator twice on the same trace file. Both runs must produce byte-identical commitment roots and identical per-miner payout amounts.
- Reorged share suffix: Two traces that share the first 8 shares but differ in the last 2. The simulator must produce different commitment roots, but the payout amounts for miners whose shares are entirely in the shared prefix must remain unchanged.
- Pending entitlement: A trace with 5 shares and no block event, run with `--pending`. The simulator must output non-zero accrual amounts for each contributing miner even though no block has been found yet.

### Unit 3: Constant Settlement and Withholding Analysis

Goal: Use the simulator to evaluate and lock the protocol constants, and analyze whether share withholding is profitable under the proposed design.

Requirements advanced: R1, R2, R3, R6.

Dependencies: Unit 2.

Files to create or modify:
- `contrib/sharepool/examples/withholding-attack.json` (may be updated from Unit 2)
- `contrib/sharepool/examples/variance-100-blocks.json` (new)
- `specs/sharepool.md` (update with settled constants)

Tests to add or modify: Test expectation: none -- this unit is an analysis task, not a code-delivery task. The analysis results are recorded in the spec and in the findings for Plan 003.

Approach: Run the simulator against a series of carefully constructed traces to answer four questions. First, what target share spacing produces a reasonable share rate at the current network hashrate? The proposed starting point is one share every 10 seconds (12 shares per block interval of 120 seconds). Second, what reward-window work threshold produces a stable and fair payout distribution? The proposed starting point is enough work to cover approximately 720 shares (roughly one hour of shares at 10-second spacing), analogous to the 720-block difficulty window. Third, does withholding shares (mining them but not publishing them) give a miner more than a 5% advantage over honest behavior? If the simulator shows that withholding is profitable beyond this threshold, the protocol needs a countermeasure (such as a small finder bonus or a publication incentive) before proceeding. Fourth, what is the reward variance for a miner contributing 10% of network hashrate over 100 blocks? If variance exceeds 10% of the expected mean reward, the window constants need revision.

Test scenarios:
- Withholding analysis: Two traces with identical total work but one where a miner withholds 20% of its shares. Compare the withholder's reward across both traces. If the withholder earns more than 5% extra in the withholding trace, flag the result for Plan 003.
- Variance analysis: A long trace simulating 100 blocks with a 10% miner. Compute the standard deviation of per-block rewards for that miner. If the coefficient of variation exceeds 10%, flag the result for Plan 003.
- Commitment size: Count the bytes needed for the Merkle commitment in the coinbase OP_RETURN output. The four-byte tag prefix plus the 32-byte Merkle root is 36 bytes. If any proposed encoding exceeds 100 bytes, flag the result for Plan 003.

### Unit 4: Spec Updates

Goal: Update `specs/consensus.md`, `specs/activation.md`, and `specs/agent-integration.md` to reflect the sharepool protocol.

Requirements advanced: R1, R2, R11.

Dependencies: Unit 3 (constants must be settled before specs are updated).

Files to create or modify:
- `specs/consensus.md`
- `specs/activation.md`
- `specs/agent-integration.md`

Tests to add or modify: Test expectation: none -- this unit updates prose specification files.

Approach: In `specs/consensus.md`, add a new "Pooled Mining Rewards" section after the existing "Block Rewards" section. This section must state that after `DEPLOYMENT_SHAREPOOL` activation, the full block reward (subsidy plus transaction fees) is committed to a compact payout structure derived from the accepted share window, and the legacy single-destination coinbase contract is replaced. It must also state that coinbase maturity (100 blocks) applies to the payout commitment output, and claims become valid only after maturity. In `specs/activation.md`, add a new row to the "Active from Genesis" table area (as a separate "Future Deployments" section, since sharepool is not active from genesis) documenting `DEPLOYMENT_SHAREPOOL` with its BIP9 parameters, the claim witness version, and the settled constants. In `specs/agent-integration.md`, replace the aspirational `pool-mine --pool stratum+tcp://...` example with a description of protocol-native share submission, noting that after activation, mining produces shares by default and no external pool URL is needed.

Test scenarios: Test expectation: none -- this unit updates prose specification files. Correctness is verified by reading the updated specs and confirming they are consistent with `specs/sharepool.md` and the simulator output.

## Concrete Steps

All commands below assume the working directory is the repository root unless stated otherwise.

Step 1: Create the sharepool spec.

    mkdir -p specs
    # Write specs/sharepool.md per Unit 1

After this step, `specs/sharepool.md` should exist and be readable. No commands to run; it is a prose document.

Step 2: Create the simulator and example traces.

    mkdir -p contrib/sharepool/examples
    # Write contrib/sharepool/simulate.py per Unit 2
    # Write example trace files per Unit 2

Step 3: Run the simulator against the basic two-miner trace.

    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/two-miner-basic.json

Expected output (example; exact values depend on settled constants):

    Reward window: 10 shares, cumulative work: 0x...
    Payout leaves:
      miner_A (rng1q...): 4500000000 roshi (90.00%)
      miner_B (rng1q...): 500000000 roshi (10.00%)
    Commitment root: <32-byte hex>
    Total distributed: 5000000000 roshi (50.00000000 RNG)

Step 4: Run the deterministic replay check.

    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/two-miner-basic.json > /tmp/run1.json
    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/two-miner-basic.json > /tmp/run2.json
    diff /tmp/run1.json /tmp/run2.json

Expected output: no diff. The two runs produce identical output.

Step 5: Run the reorg scenario.

    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/reorg-suffix.json

Expected output: different commitment root from the basic trace, but shared-prefix miners retain the same reward proportions.

Step 6: Run the pending-entitlement scenario.

    python3 contrib/sharepool/simulate.py --pending contrib/sharepool/examples/pending-only.json

Expected output: non-zero pending accrual amounts for each miner, even though no block event is in the trace.

Step 7: Run the withholding analysis.

    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/withholding-attack.json

Expected output: the withholder's reward is compared to honest behavior. If the advantage exceeds 5%, the output includes a warning.

Step 8: Run the variance analysis.

    python3 contrib/sharepool/simulate.py contrib/sharepool/examples/variance-100-blocks.json --stats

Expected output: per-miner reward statistics over 100 blocks, including mean, standard deviation, and coefficient of variation.

Step 9: Update existing specs per Unit 4.

    # Edit specs/consensus.md, specs/activation.md, specs/agent-integration.md

Step 10: Write the locked constants file.

    # Write contrib/sharepool/constants.json with all settled values

After all steps, verify that the simulator runs cleanly and all scenarios produce expected outputs.

## Validation and Acceptance

The plan is complete when all of the following can be demonstrated.

`specs/sharepool.md` exists, is self-contained, and defines every term, field, formula, and encoding needed for a novice to understand the sharepool protocol without consulting any other document.

`contrib/sharepool/simulate.py` runs against `contrib/sharepool/examples/two-miner-basic.json` and prints a payout commitment root plus per-miner reward amounts that sum to the block reward (50 RNG = 5,000,000,000 roshi at current subsidy).

Running the simulator twice on the same trace file produces byte-identical output.

Running the simulator against the reorg-suffix trace produces a different commitment root but preserves unchanged reward proportions for miners whose shares are entirely in the shared prefix.

Running the simulator with `--pending` against a trace with no block event produces non-zero accrual amounts.

The withholding analysis shows whether the proposed constants pass the Plan 003 threshold (withholder advantage must be under 5%).

The variance analysis shows whether the proposed constants pass the Plan 003 threshold (coefficient of variation for a 10% miner over 100 blocks must be under 10%).

`specs/consensus.md`, `specs/activation.md`, and `specs/agent-integration.md` are updated to reflect the sharepool protocol and are consistent with `specs/sharepool.md`.

A locked constants file exists at `contrib/sharepool/constants.json` with all settled values.

## Idempotence and Recovery

All steps in this plan are additive. They create new files or append sections to existing files. No existing behavior is changed. Running any step multiple times produces the same result.

The simulator is a pure function of its input trace. It can be run any number of times with the same result. If a trace file is modified, the simulator produces a new result reflecting the modification.

If the spec needs revision after the simulator reveals a problem, update `specs/sharepool.md` and rerun the simulator. There is no destructive state to clean up.

If the simulator code changes, rerun all example traces and verify that the output matches expectations. There is no persistent database or service to restart.

## Artifacts and Notes

The minimal share object shape for the spec and simulator:

    {
      "share_id": "<hex, SHA256 of serialized share header>",
      "parent_share": "<hex, id of the previous share>",
      "prev_block_hash": "<hex, block tip this share was built against>",
      "payout_script": "<hex, serialized scriptPubKey>",
      "nBits": "<compact difficulty target>",
      "nTime": <unix timestamp>,
      "nNonce": <uint32>,
      "candidate_context": {
        "version": <int32>,
        "merkle_root": "<hex>",
        "prev_block_hash": "<hex>"
      },
      "work": "<hex, arith_uint256 derived from nBits>",
      "block_event": false
    }

The minimal payout leaf shape:

    {
      "payout_script": "<hex, serialized scriptPubKey>",
      "amount_roshi": <int64>,
      "first_share": "<hex, share id>",
      "last_share": "<hex, share id>"
    }

The Merkle commitment encoding in the coinbase:

    OP_RETURN <4-byte tag: "RNGS"> <32-byte Merkle root>

This is 38 bytes total (1 byte OP_RETURN + 1 byte push length + 4 bytes tag + 1 byte push length + 32 bytes root), well within the 80-byte OP_RETURN data carrier limit defined in `specs/activation.md`.

The proposed starting constants (to be validated or revised by the simulator):

    target_share_spacing:       10 seconds
    reward_window_work:         equivalent to ~720 shares at target share difficulty
    claim_witness_version:      2 (witness v2, reserved by Bitcoin for future upgrades)
    max_orphan_shares:          64
    share_target_ratio:         block_target / 12 (one share every ~10s at 120s blocks)

The truthfulness distinction that must appear in all user-facing surfaces:

    "Pending pooled reward" = deterministic accrued entitlement from accepted shares,
                              visible immediately, not yet spendable.
    "Claimable pooled reward" = the same entitlement after the payout commitment output
                                has matured under RNG's 100-block coinbase maturity rule.

## Interfaces and Dependencies

This plan produces specification and simulation artifacts, not compiled interfaces. However, it defines the shapes that later plans will implement in C++ and Python.

In `specs/sharepool.md`, the specification must define the following structures that later plans will implement:

The share record, which will become `sharechain::ShareRecord` in `src/sharechain/share.h`:

    struct ShareRecord {
        uint256 share_id;
        uint256 parent_share;
        uint256 prev_block_hash;
        uint256 candidate_header_hash;
        uint32_t nTime;
        uint32_t nBits;
        uint32_t nNonce;
        CScript payout_script;
    };

The reward leaf, which will become `sharechain::RewardLeaf` in `src/sharechain/payout.h`:

    struct RewardLeaf {
        CScript payout_script;
        CAmount amount;
        uint256 first_share;
        uint256 last_share;
    };

The reward commitment, which will become `sharechain::RewardCommitment` in `src/sharechain/payout.h`:

    struct RewardCommitment {
        std::vector<RewardLeaf> leaves;
        uint256 root;
    };

The sharepool consensus parameters, which will become part of `Consensus::Params` in `src/consensus/params.h`:

    struct SharePoolParams {
        uint32_t target_share_spacing;   // seconds between target shares
        uint32_t reward_window_work;     // cumulative work threshold for the reward window
        uint8_t claim_witness_version;   // witness program version for claim spends
        uint16_t max_orphan_shares;      // maximum orphan shares held before pruning
    };

The simulator at `contrib/sharepool/simulate.py` depends only on Python 3 standard library modules (`json`, `hashlib`, `sys`, `argparse`, `struct`). It does not depend on any RNG node binary, any external Python package, or any network service.

The spec updates to `specs/consensus.md`, `specs/activation.md`, and `specs/agent-integration.md` do not introduce new file dependencies. They add sections to existing files.

This plan does not evolve any existing code interface. It creates new specification artifacts that constrain the interfaces later plans will build. The boundary between this plan and later plans is clear: this plan produces the "what" and "why" (spec + simulation results), while Plans 004-008 produce the "how" (C++ implementation).
