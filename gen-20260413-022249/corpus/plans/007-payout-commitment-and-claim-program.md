# Compact Payout Commitment and Claim Program

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root, which defines the ExecPlan standard for all plans in this corpus.


## Purpose / Big Picture

After this change, every activated block encodes a compact, deterministic commitment to the proportional reward split owed to all miners who contributed recent work on the sharechain. Any miner can later prove its own share of that reward and spend it without trusting any operator, pool server, or privileged coordinator. Before this change, the sharechain exists (Plan 005) and shares propagate peer-to-peer, but there is no on-chain consequence: blocks still pay the full reward to a single finder. After this change, the reward is locked behind a cryptographic commitment that all nodes enforce, and miners claim their portion through a new witness program that any full node can verify independently.

The observable result: on an activated regtest network, a miner can run `rng-cli getrewardcommitment <blockhash>` and see the Merkle root plus the list of reward leaves with per-miner amounts. A claim transaction built from one of those leaves, with a valid Merkle proof and signature, is accepted into the mempool and confirmed after coinbase maturity (100 blocks). A claim with a tampered proof, wrong amount, or invalid signature is rejected with a clear error.

This plan depends on Plan 005 (Sharechain Data Model, Storage, and P2P Relay) for the `ShareChainView` interface that provides the accepted share window. It depends on Plan 004 (Sharepool Version-Bits Deployment Skeleton) for the `DEPLOYMENT_SHAREPOOL` activation boundary. It does not depend on Plan 006 (the decision gate), though revisions from that gate may feed back into this plan's constants.

Terminology used throughout this plan:

A "reward leaf" is a single entry in the payout commitment representing one miner's accumulated work. It contains a payout script (the Bitcoin-style scriptPubKey where the miner wants to receive funds), an amount in roshi (the smallest RNG unit, 0.00000001 RNG), and the range of share identifiers that contributed to that leaf's work total.

A "reward commitment" is a binary Merkle tree built from all reward leaves for one block. The root of this tree is a 32-byte hash that uniquely identifies the exact reward split.

A "payout commitment output" is a coinbase transaction output whose scriptPubKey is a witness version 2 program where the 32-byte program data is the reward commitment root. Witness version 2 is currently unassigned in Bitcoin's script system and in RNG. All nodes running pre-activation software treat witness v2 outputs as anyone-can-spend (the standard soft-fork upgrade mechanism), so upgraded nodes can enforce the new rules without breaking old nodes.

A "claim spend" is a transaction that spends a payout commitment output by providing a witness stack that proves the spender owns a specific leaf under the committed root. The witness stack contains a Merkle branch (the sibling hashes needed to reconstruct the root), the leaf index, the serialized leaf data, and a signature proving control of the leaf's payout script.

"Coinbase maturity" means the 100-confirmation waiting period before coinbase outputs can be spent. This is defined by the constant `COINBASE_MATURITY` in `src/consensus/amount.h` and enforced in `src/validation.cpp`. Payout commitment outputs are coinbase outputs and therefore subject to this rule.


## Requirements Trace

`R1` (consensus-enforced reward). After activation, every block must carry a payout commitment output whose root matches the deterministic reward split derived from the accepted share window. Blocks missing or mismatching this commitment are invalid.

`R2` (deterministic from share history). The reward commitment must be computable by any node that has the same accepted share window. Two nodes with identical share history must produce byte-identical commitment roots.

`R3` (proportional accrual). Each reward leaf's amount must be proportional to the work contributed by the leaf's payout script within the active reward window, applied to the total block reward (subsidy plus fees).

`R6` (compact commitment). The coinbase carries exactly one additional output regardless of how many miners participate. The block does not grow with the number of miners.

`R7` (trustless claims). A miner claims its reward by proving leaf membership under the committed root, proving the amount matches the leaf, and proving it controls the leaf's payout script. No third party is consulted.

`R8` (pre-activation preservation). Existing blocks, transactions, and mining behavior are unchanged before activation. This plan preserves the existing contract by conditioning all new logic on the `DEPLOYMENT_SHAREPOOL` activation state.

`R11` (truthfulness). Pending reward is visible immediately but not spendable until the coinbase matures. Documentation and RPC output must use "pending" vs "claimable" consistently.


## Scope Boundaries

This plan does not change the sharechain data model or P2P relay (those are Plan 005). It does not change the internal miner to produce shares (that is Plan 008). It does not reduce coinbase maturity. It does not implement wallet tracking of pending or claimable rewards (that is Plan 008). It does not implement the `getrewardcommitment` RPC beyond what is needed for consensus validation (the full user-facing RPC is Plan 008, but a minimal version is included here for testing).

This plan does not change how pre-activation blocks are assembled or validated. The existing `BlockAssembler::CreateNewBlock()` in `src/node/miner.cpp` continues to produce a single coinbase output paying the full reward to `m_options.coinbase_output_script` for all blocks before the activation height.

This plan does not depend on any parallel QSB rollout work described in the local root `EXECPLAN.md`. The inspected checkout did not contain the QSB source files named there. If that work lands later, compatibility should be checked against the merged code, but this payout-commitment plan should not hard-code assumptions about it now.

This plan does not implement any finder bonus. The full block reward (subsidy plus transaction fees) is split proportionally among all contributing miners in the reward window. A finder bonus is a potential future enhancement documented in Plan 002's simulator results.


## Progress

- [ ] Implement `RewardLeaf` and `RewardCommitment` structs in `src/sharechain/payout.h`.
- [ ] Implement `ComputeRewardCommitment()` in `src/sharechain/payout.cpp`.
- [ ] Implement Merkle tree construction and proof verification helpers.
- [ ] Modify `BlockAssembler::CreateNewBlock()` to insert payout commitment output after activation.
- [ ] Add consensus validation in `ConnectBlock()` to reject mismatched commitments.
- [ ] Add `WITNESS_V2_SHAREPOOL_CLAIM` to `TxoutType` enum in `src/script/solver.h`.
- [ ] Implement witness v2 claim verification in `VerifyWitnessProgram()` in `src/script/interpreter.cpp`.
- [ ] Update `IsStandard()` and `IsWitnessStandard()` in `src/policy/policy.cpp` to accept claim spends.
- [ ] Add `getrewardcommitment` RPC for testing.
- [ ] Write unit tests in `src/test/shareclaim_tests.cpp`.
- [ ] Write functional test `test/functional/feature_sharepool_claims.py`.


## Surprises & Discoveries

(No entries yet. This section will be updated as implementation proceeds.)

- Observation: ...
  Evidence: ...


## Decision Log

- Decision: Use witness version 2 with a 32-byte program for the payout commitment output.
  Rationale: Bitcoin's script system reserves witness versions 2-16 for future soft-fork upgrades. Version 2 is the next available version after Taproot (version 1). A 32-byte program matches the established pattern (Taproot also uses 32 bytes). Pre-activation nodes treat witness v2 outputs as anyone-can-spend, which is exactly the soft-fork upgrade mechanism: upgraded nodes enforce new rules while old nodes remain compatible. Using a dedicated witness version keeps claim verification isolated from existing v0 (P2WPKH/P2WSH) and v1 (Taproot) paths in `src/script/interpreter.cpp`.
  Date/Author: 2026-04-12 / Plan authored

- Decision: Build the Merkle tree as a binary tree over reward leaves sorted by payout script hash.
  Rationale: Sorting by payout script hash makes the tree deterministic without requiring a canonical ordering rule tied to share submission time (which could differ between nodes due to relay delays). Any node with the same set of reward leaves produces the same root. Binary Merkle trees are well-understood in the Bitcoin codebase (the block Merkle root in `src/consensus/merkle.cpp` uses the same pattern), so the implementation can follow established patterns.
  Date/Author: 2026-04-12 / Plan authored

- Decision: Claim witness stack format is `[merkle_branch] [leaf_index] [leaf_data] [signature]`.
  Rationale: This format provides everything needed for stateless verification. The verifier reconstructs the root from the leaf data, leaf index, and Merkle branch, then checks the root matches the scriptPubKey program, then checks the amount in leaf data, then checks the signature against the payout script in leaf data. No external state lookup is required beyond what is already in the UTXO set (the commitment output itself). The signature scheme uses the same Schnorr signature verification already available in RNG for Taproot, applied to a message that commits to the claim transaction's sighash.
  Date/Author: 2026-04-12 / Plan authored

- Decision: Each leaf aggregates all work from one payout script across the entire reward window, rather than creating per-share leaves.
  Rationale: A miner that submits 100 shares in the reward window gets one leaf, not 100 leaves. This keeps the Merkle tree small (bounded by the number of distinct payout scripts, not the number of shares) and makes claims cheap (one claim per miner per block, not one per share).
  Date/Author: 2026-04-12 / Plan authored


## Outcomes & Retrospective

(No entries yet. This section will be updated at major milestones and at completion.)


## Context and Orientation

The reader needs to understand five code areas to implement this plan.

First, block assembly in `src/node/miner.cpp`. The function `BlockAssembler::CreateNewBlock()` (line 119) builds a candidate block. It creates a coinbase transaction with one output at line 172-176 that pays `block_reward` (subsidy + fees) to `m_options.coinbase_output_script`. After activation, this function must also insert a second coinbase output whose scriptPubKey encodes the payout commitment root as a witness v2 program. The existing witness commitment (SegWit commitment in an OP_RETURN output) is handled separately at line 191 via `GenerateCoinbaseCommitment()` and must remain unchanged.

Second, block validation in `src/validation.cpp`. The function `Chainstate::ConnectBlock()` (line 2309) validates blocks against the UTXO set. After activation, it must verify that the block's coinbase contains a payout commitment output whose root matches the locally computed reward commitment from the accepted share window.

Third, script verification in `src/script/interpreter.cpp`. The function `VerifyWitnessProgram()` (line 1917) dispatches on witness version. Version 0 handles P2WPKH and P2WSH (lines 1923-1946). Version 1 handles Taproot (lines 1947-1989). Versions 2-16 currently fall through to a compatibility return at line 1997 that succeeds unconditionally (the soft-fork upgrade path). This plan adds a new branch for version 2 with 32-byte programs that performs claim verification.

Fourth, output type classification in `src/script/solver.h` and `src/script/solver.cpp`. The `TxoutType` enum (solver.h line 22) classifies output scripts for policy purposes. The `Solver()` function (solver.cpp) matches scripts against known patterns. The `WITNESS_UNKNOWN` type (solver.h line 34) currently catches all witness programs not explicitly handled. This plan adds a `WITNESS_V2_SHAREPOOL_CLAIM` type so the policy layer can distinguish claim outputs from truly unknown witness programs.

Fifth, mempool standardness in `src/policy/policy.cpp`. The function `IsStandard()` (line 79) checks whether an output script is a recognized type. The function `IsWitnessStandard()` (line 251) checks witness stack constraints. Both must be updated to accept claim outputs and claim spends as standard so they propagate through the mempool rather than being rejected as nonstandard.

The sharechain view interface from Plan 005 provides the data needed to compute reward commitments. Specifically, the `ShareChainView` type exposes the accepted share window: the ordered sequence of validated shares between the window boundaries for a given block height. Each share in the window has a payout script and a work value. This plan consumes that interface but does not implement it.


## Plan of Work

The work proceeds in four stages: data structures, block assembly, consensus validation, and claim-spend verification.

Stage one defines the reward leaf and commitment types in `src/sharechain/payout.h` and implements the computation in `src/sharechain/payout.cpp`. The `ComputeRewardCommitment()` function takes a `ShareChainView`, a previous block index, and a total reward amount. It iterates the share window, aggregates work per payout script, computes each leaf's proportional amount, sorts leaves by the SHA256 hash of their payout script, builds a binary Merkle tree, and returns the root plus the leaf vector. Helper functions for Merkle proof generation (`ComputeMerkleBranch()`) and verification (`VerifyMerkleBranch()`) are included in the same module.

Stage two modifies `BlockAssembler::CreateNewBlock()` in `src/node/miner.cpp`. After the existing coinbase construction at line 176, the function checks whether `DEPLOYMENT_SHAREPOOL` is active for the block being built. If active, it calls `ComputeRewardCommitment()` to get the root, constructs a scriptPubKey of the form `OP_2 <32-byte-root>` (which is a witness v2 program), and inserts it as a second coinbase output. The first coinbase output's value is reduced to zero (or to a dust-avoidance minimum if required), and the second output's value is set to the full block reward. The `coinbase_tx.required_outputs` vector is extended to include the payout commitment output so the mining interface preserves it.

Stage three adds validation in `src/validation.cpp`. In `ConnectBlock()`, after the existing `CheckBlock()` call, the function checks whether the block is post-activation. If so, it replays the share window via `ComputeRewardCommitment()` using the same parameters the assembler used, and verifies that the coinbase contains a witness v2 output whose program matches the computed root. If the commitment is missing or mismatched, the block is rejected with `BLOCK_CONSENSUS` and the reason string "bad-sharepool-commitment".

Stage four implements claim-spend verification. In `VerifyWitnessProgram()` in `src/script/interpreter.cpp`, a new branch handles `witversion == 2 && program.size() == 32 && !is_p2sh`. The claim witness stack must have exactly four elements: a serialized Merkle branch (concatenated 32-byte hashes), a leaf index encoded as a CScriptNum, serialized leaf data (payout script length + payout script + amount as int64 little-endian), and a Schnorr signature. Verification proceeds as: deserialize the leaf data, hash it to get the leaf hash, reconstruct the Merkle root from the leaf hash, leaf index, and branch, compare the reconstructed root to the 32-byte program, verify the amount in the leaf data matches the output value being spent, and verify the Schnorr signature against the payout script in the leaf data using the claim transaction's sighash. If any step fails, the spend is invalid.


## Implementation Units

### Unit 1: Reward Leaf and Commitment Data Structures

Goal: Define the types and pure functions for deterministic reward computation and Merkle proof operations.

Requirements advanced: R2, R3, R6.

Dependencies: Plan 005 (ShareChainView interface). For unit testing before Plan 005 is complete, tests can construct synthetic share windows directly.

Files to create or modify:

- `src/sharechain/payout.h` (new)
- `src/sharechain/payout.cpp` (new)
- `src/test/shareclaim_tests.cpp` (new)

Approach: Define `RewardLeaf` as a struct with fields `CScript payout_script`, `CAmount amount`, `uint256 first_share`, `uint256 last_share`. Define `RewardCommitment` as a struct with fields `std::vector<RewardLeaf> leaves` and `uint256 root`. Implement `ComputeRewardCommitment()` that takes a share window (as a vector of share records for testability), the total reward, and returns a `RewardCommitment`. Implement `HashRewardLeaf()` that serializes a leaf deterministically and returns its SHA256 hash. Implement `BuildRewardMerkleTree()` that takes sorted leaf hashes and returns the root. Implement `ComputeMerkleBranch()` that returns the sibling hashes for a given leaf index. Implement `VerifyMerkleBranch()` that reconstructs the root from a leaf hash, index, and branch and compares it to an expected root.

Test scenarios:

- Given a window with two miners contributing 90% and 10% of work respectively, and a total reward of 50 RNG (5,000,000,000 roshi), the computed leaves have amounts 4,500,000,000 and 500,000,000, and the Merkle root is a deterministic 32-byte hash.
- Given the same share window replayed twice, `ComputeRewardCommitment()` returns byte-identical roots.
- Given a single miner contributing all work, the commitment has one leaf with the full reward amount.
- `VerifyMerkleBranch()` returns true for a correctly constructed branch and false when any branch hash is modified.
- `ComputeMerkleBranch()` for every leaf index in a 5-leaf tree produces branches that all verify against the same root.

### Unit 2: Coinbase Payout Commitment Insertion

Goal: After activation, block assembly inserts a payout commitment output into the coinbase.

Requirements advanced: R1, R6.

Dependencies: Unit 1, Plan 004 (DEPLOYMENT_SHAREPOOL activation).

Files to modify:

- `src/node/miner.cpp`

Tests to modify:

- `src/test/miner_tests.cpp`

Approach: In `BlockAssembler::CreateNewBlock()`, after the line that sets `coinbaseTx.vout[0].nValue = block_reward` (currently line 176), check whether `DEPLOYMENT_SHAREPOOL` is active for the block being assembled by querying the version bits cache against `pindexPrev`. If active, call `ComputeRewardCommitment()` with the share window from the chainstate's sharechain view, `pindexPrev`, and `block_reward`. Construct a second coinbase output: `coinbaseTx.vout.resize(2)`, set `coinbaseTx.vout[1].scriptPubKey` to `CScript() << OP_2 << ToByteVector(commitment.root)`, set `coinbaseTx.vout[1].nValue = block_reward`, and set `coinbaseTx.vout[0].nValue = 0`. Add the commitment output to `coinbase_tx.required_outputs`.

Test scenarios:

- On regtest with sharepool inactive (default), `CreateNewBlock()` produces a coinbase with one value output (the classical winner-takes-all payout). No witness v2 output is present.
- On regtest with sharepool active via `-vbparams=sharepool:0:9999999999:0`, `CreateNewBlock()` produces a coinbase with a zero-value first output and a second output whose scriptPubKey is `OP_2 <32-bytes>` carrying the full block reward.
- The commitment root in the coinbase matches the output of `ComputeRewardCommitment()` called independently with the same share window and reward.

### Unit 3: Consensus Validation of Payout Commitment

Goal: Reject activated blocks whose payout commitment does not match the locally computed reward split.

Requirements advanced: R1, R2.

Dependencies: Units 1 and 2, Plan 004.

Files to modify:

- `src/validation.cpp`

Tests to add:

- `test/functional/feature_sharepool_claims.py` (new, partial -- commitment validation portion)

Approach: In `Chainstate::ConnectBlock()`, after the existing `CheckBlock()` call and the genesis block special case, add a post-activation check. Query the version bits state for `DEPLOYMENT_SHAREPOOL` at the block's height. If active, call `ComputeRewardCommitment()` using the same sharechain view that was available when the block was assembled. Scan the coinbase outputs for a witness v2 program of exactly 32 bytes. If none is found, reject with `BLOCK_CONSENSUS` and reason "bad-sharepool-commitment-missing". If found, compare its program bytes to the computed root. If they differ, reject with reason "bad-sharepool-commitment-mismatch". If the commitment output's value does not equal the expected block reward, reject with reason "bad-sharepool-commitment-amount".

Test scenarios:

- A correctly assembled activated block passes `ConnectBlock()`.
- An activated block with no witness v2 output in the coinbase is rejected with "bad-sharepool-commitment-missing".
- An activated block with a witness v2 output containing the wrong root is rejected with "bad-sharepool-commitment-mismatch".
- An activated block with the correct root but wrong amount is rejected with "bad-sharepool-commitment-amount".
- A pre-activation block with no payout commitment passes `ConnectBlock()` unchanged.

### Unit 4: Witness V2 Claim-Spend Verification

Goal: Allow miners to spend their committed payout leaves after coinbase maturity by providing a Merkle proof and signature.

Requirements advanced: R7.

Dependencies: Unit 1 (Merkle verification), Unit 2 (commitment outputs exist to spend).

Files to modify:

- `src/script/interpreter.cpp`
- `src/script/interpreter.h`
- `src/script/solver.h`
- `src/script/solver.cpp`
- `src/policy/policy.cpp`

Tests to add:

- `src/test/shareclaim_tests.cpp` (extend from Unit 1)

Approach: In `src/script/solver.h`, add `WITNESS_V2_SHAREPOOL_CLAIM` to the `TxoutType` enum after `WITNESS_V1_TAPROOT`. In `src/script/solver.cpp`, add a case in `Solver()` that matches witness version 2 with a 32-byte program and returns this new type. Add the string name "witness_v2_sharepool_claim" in `GetTxnOutputType()`.

In `src/script/interpreter.cpp`, in `VerifyWitnessProgram()`, add a branch after the Taproot branch (line 1989) and before the anchor check (line 1990): `else if (witversion == 2 && program.size() == 32 && !is_p2sh)`. Inside this branch, require the witness stack to have exactly 4 elements. Parse element 0 as a Merkle branch (a byte vector whose length is a multiple of 32). Parse element 1 as a leaf index (CScriptNum). Parse element 2 as serialized leaf data. Parse element 3 as a Schnorr signature. Hash the leaf data to get the leaf hash. Call `VerifyMerkleBranch()` with the leaf hash, index, branch, and the 32-byte program as the expected root. If verification fails, return `SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH`. Deserialize the leaf data to extract the payout script and amount. Verify the amount matches the output value being spent by this input. Verify the Schnorr signature against the payout script using the claim transaction's sighash (BIP341-style sighash adapted for witness v2). If any check fails, return an appropriate script error.

In `src/policy/policy.cpp`, update `IsStandard()` to accept `TxoutType::WITNESS_V2_SHAREPOOL_CLAIM` as a standard output type. Update `IsWitnessStandard()` to accept witness v2 claim spends (4-element witness stack with expected structure) as standard.

Test scenarios:

- A claim transaction with a valid Merkle branch, correct leaf data, and valid signature is accepted by `VerifyScript()`.
- A claim with an invalid Merkle branch (one sibling hash changed) fails with `SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH`.
- A claim with the wrong amount in leaf data (amount doesn't match output value) fails verification.
- A claim with an invalid signature fails with a signature error.
- A claim with too few or too many witness stack elements fails.
- A claim against a pre-activation output (witness v2 but not a payout commitment) succeeds unconditionally via the existing soft-fork compatibility path when the `SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM` flag is not set.

### Unit 5: Testing RPC and Functional Test

Goal: Provide a minimal `getrewardcommitment` RPC for testing and a comprehensive functional test.

Requirements advanced: R2, R7, R11.

Dependencies: Units 1-4.

Files to create or modify:

- `src/rpc/mining.cpp`
- `test/functional/feature_sharepool_claims.py` (new)

Approach: Add a `getrewardcommitment` RPC in `src/rpc/mining.cpp` that takes a block hash, looks up the block, and returns a JSON object with the commitment root (hex), the list of reward leaves (each with payout_script hex, amount in roshi, first_share hex, last_share hex), and the block height. This RPC is read-only and does not modify state.

Write `test/functional/feature_sharepool_claims.py` that starts a regtest network with sharepool activated, mines blocks with shares from two different payout scripts, verifies the commitment roots match expected values, constructs a claim transaction manually, submits it after maturity, and verifies it confirms. Also tests rejection of invalid claims.

Test scenarios:

- `getrewardcommitment` returns a valid JSON object for an activated block.
- The commitment root from the RPC matches the root in the block's coinbase.
- The leaf amounts sum to the total block reward.
- A claim transaction built from RPC data and submitted after 100 confirmations is accepted.
- A claim transaction submitted before maturity is rejected (coinbase spend before maturity).
- Pre-activation blocks return an error or empty result from `getrewardcommitment`.


## Concrete Steps

All commands run from the repository root.

1. After implementing Units 1-4, build the project:

       cmake -S . -B build
       cmake --build build -j"$(nproc)" --target rngd rng-cli test_bitcoin

   Expected: clean build with no new warnings. The sharechain payout module compiles into the existing binary.

2. Run the new unit tests:

       build/src/test/test_bitcoin --run_test=shareclaim_tests

   Expected: all shareclaim tests pass. Output includes test names for Merkle construction, proof verification, reward computation, and claim script verification.

3. Run the functional test:

       test/functional/test_runner.py feature_sharepool_claims.py

   Expected: the test starts a regtest node with `-vbparams=sharepool:0:9999999999:0`, mines activated blocks, verifies payout commitments, builds and submits a claim transaction, and reports success.

4. Verify pre-activation behavior is preserved:

       test/functional/test_runner.py feature_sharepool_activation.py

   Expected: the activation test from Plan 004 still passes. Default regtest with no `-vbparams` override produces classical blocks with no payout commitment.

5. Manual verification on regtest:

       build/src/rngd -regtest -daemon -vbparams=sharepool:0:9999999999:0 -mine -mineaddress=<regtest-addr> -minethreads=2
       build/src/rng-cli -regtest getdeploymentinfo
       # Wait for a few blocks to be mined
       build/src/rng-cli -regtest getrewardcommitment <latest-blockhash>

   Expected: `getdeploymentinfo` shows sharepool active. `getrewardcommitment` returns a JSON object with a 64-character hex root, one or more leaves, and amounts summing to the block reward.


## Validation and Acceptance

The implementation is accepted when all of the following are true.

A known share window (constructed from synthetic test data) produces a payout root that matches the output of the Plan 002 simulator. This proves the node and the simulator agree on the deterministic reward computation.

A block with a tampered payout root is rejected by `ConnectBlock()` on an activated regtest network. The rejection reason is "bad-sharepool-commitment-mismatch" and the block does not enter the chain.

A valid claim transaction spending one committed payout leaf is accepted into the mempool and confirmed after coinbase maturity (100 blocks). The claim's output script receives the correct amount.

A claim with an invalid Merkle branch, wrong amount, or wrong signature is rejected by `VerifyScript()` and does not enter the mempool. The rejection reason is specific enough for a miner to diagnose the problem.

Pre-activation blocks on default regtest (no `-vbparams` override) validate exactly as before. Existing miner tests and activation tests continue to pass.


## Idempotence and Recovery

All steps can be repeated from a clean build directory and a fresh regtest datadir. The functional test creates its own temporary datadir and cleans up after itself. If the sharechain payout module's serialization format changes during development, delete the regtest datadir and rerun the functional test from scratch. No migration is needed because regtest datadirs are disposable.

If a unit test fails, the fix-rebuild-retest cycle is safe to repeat. No persistent state is modified by unit tests.

If the witness v2 claim verification logic proves impossible to express cleanly (for example, if the sighash construction conflicts with existing Bitcoin signature semantics), stop and revise this plan. Do not work around the problem with operator-side settlement, because that would violate R7. Document the blocking issue in the Decision Log and escalate.


## Artifacts and Notes

The leaf serialization format for hashing:

    [varint: payout_script length]
    [bytes: payout_script]
    [int64_le: amount in roshi]

The Merkle tree construction follows the same pattern as Bitcoin's block Merkle tree in `src/consensus/merkle.cpp`: if the number of leaves is odd, the last leaf is duplicated. Hashing uses double-SHA256 (`Hash()` from `src/hash.h`) applied to the concatenation of two child hashes.

The claim witness stack layout:

    witness[0]: merkle_branch  (N * 32 bytes, where N = ceil(log2(leaf_count)))
    witness[1]: leaf_index     (CScriptNum encoding)
    witness[2]: leaf_data      (serialized as above)
    witness[3]: signature      (64-byte Schnorr signature)

The signature message is the BIP341-style sighash of the spending transaction, adapted for witness v2 by using a different sighash epoch byte (0x02 instead of 0x00 for legacy or 0x01 for Taproot). The public key for signature verification is derived from the payout script in the leaf data. If the payout script is a P2WPKH (witness v0 keyhash), the public key is recovered from the keyhash via the spending transaction's witness. If the payout script is a P2TR (witness v1 Taproot), the public key is the x-only key encoded in the Taproot program. Other payout script types are not supported in the initial implementation and are rejected at claim time.

The coinbase structure after activation:

    vout[0]: value=0, scriptPubKey=<original miner script> (preserved for compatibility)
    vout[1]: value=block_reward, scriptPubKey=OP_2 <32-byte commitment root>
    vout[N]: witness commitment (OP_RETURN, unchanged from current behavior)


## Interfaces and Dependencies

This plan depends on the following interfaces from earlier plans.

From Plan 004 (`src/consensus/params.h`):

    enum DeploymentPos {
        DEPLOYMENT_TESTDUMMY,
        DEPLOYMENT_TAPROOT,
        DEPLOYMENT_SHAREPOOL,
        MAX_VERSION_BITS_DEPLOYMENTS
    };

From Plan 005 (`src/sharechain/`), the ShareChainView interface that provides the accepted share window for a given block:

    namespace sharechain {
    class ShareChainView {
    public:
        // Returns the ordered shares in the reward window for the block
        // that would be built on top of prev_block.
        std::vector<ShareRecord> GetRewardWindow(const CBlockIndex& prev_block) const;
    };
    } // namespace sharechain

This plan introduces the following new interfaces.

In `src/sharechain/payout.h`:

    namespace sharechain {

    struct RewardLeaf {
        CScript payout_script;
        CAmount amount;
        uint256 first_share;
        uint256 last_share;
    };

    struct RewardCommitment {
        std::vector<RewardLeaf> leaves;
        uint256 root;
    };

    uint256 HashRewardLeaf(const RewardLeaf& leaf);

    RewardCommitment ComputeRewardCommitment(
        const std::vector<ShareRecord>& window,
        CAmount total_reward);

    std::vector<uint256> ComputeMerkleBranch(
        const std::vector<uint256>& leaf_hashes,
        size_t index);

    bool VerifyMerkleBranch(
        const uint256& leaf_hash,
        size_t index,
        const std::vector<uint256>& branch,
        const uint256& expected_root);

    uint256 BuildRewardMerkleTree(
        const std::vector<uint256>& leaf_hashes);

    } // namespace sharechain

In `src/script/solver.h`, extend the existing `TxoutType` enum:

    enum class TxoutType {
        NONSTANDARD,
        ANCHOR,
        PUBKEY,
        PUBKEYHASH,
        SCRIPTHASH,
        MULTISIG,
        NULL_DATA,
        WITNESS_V0_SCRIPTHASH,
        WITNESS_V0_KEYHASH,
        WITNESS_V1_TAPROOT,
        WITNESS_V2_SHAREPOOL_CLAIM,  // new
        WITNESS_UNKNOWN,
    };

In `src/script/interpreter.cpp`, the new branch in `VerifyWitnessProgram()` handles `witversion == 2 && program.size() == 32`. It calls into `sharechain::VerifyMerkleBranch()` for proof verification and uses the existing Schnorr signature verification infrastructure for signature checking.

In `src/rpc/mining.cpp`, add `getrewardcommitment`:

    getrewardcommitment "blockhash"

    Returns the payout commitment for an activated block.

    Result:
    {
      "root": "hex",
      "height": n,
      "total_reward": n,
      "leaves": [
        {
          "payout_script": "hex",
          "amount": n,
          "first_share": "hex",
          "last_share": "hex"
        },
        ...
      ]
    }

This plan does not specify any QSB interface because the inspected checkout did not contain the QSB source files referenced by the local root `EXECPLAN.md`. Any future compatibility work should be based on merged source, not on local planning prose.
