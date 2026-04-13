# IMPLEMENTATION_PLAN

## Current Objective

Land protocol-native trustless pooled mining on regtest, with pooled
participation becoming the default mining path once `DEPLOYMENT_SHAREPOOL` is
active. The current loop objective is not devnet rollout, fleet operations, or
release polish. It is to finish the consensus, miner, wallet, and proof path
that makes:

1. the reward split consensus-enforced,
2. the reward window validator-deterministic and restart/reindex-stable,
3. share production automatic when mining is enabled post-activation, and
4. claims and pooled balances visible and correct without operator trust.

## Auto Loop Focus

`auto loop` should work top-down through **Priority Work** only, and stop at
every checkpoint/gate. The intended critical path is:

1. `POOL-07F` -> `CHKPT-07A`
2. `POOL-07G` -> `POOL-07H` -> `CHKPT-07B`
3. `POOL-07I` -> `GATE-07C` -> `POOL-07J`
4. `POOL-08A` -> `POOL-08B` -> `CHKPT-08A`
5. `POOL-08C` -> `POOL-08D` -> `POOL-08E` -> `CHKPT-08B`
6. `CHKPT-03` -> `GATE-03`

## Hard Constraints For The Loop

- Do not start any item in **Follow-On Work** until `GATE-03` records a GO
  decision.
- Do not implement wallet accounting, `getrewardcommitment`, or end-to-end
  proof logic against transient local relay history. Those surfaces must depend
  on the canonical reward-window contract selected in `GATE-07C`.
- Do not treat the current solo settlement fallback as a complete product. It
  is only a compatibility fallback until `POOL-07J` lands.
- Once `DEPLOYMENT_SHAREPOOL` is active, the intended steady state is "mining
  implies share production" unless the node lacks the required sharechain wiring
  entirely. Avoid introducing a new long-lived opt-in toggle that recreates a
  separate solo-mining mode post-activation.
- When a task is labeled checkpoint/gate, finish its validation and update the
  referenced report or evidence before moving to the next cluster.

## Priority Work

### Cluster 1: Settlement Consensus Verification

- [ ] `POOL-07F` Witness-v2 settlement program verification and interpreter dispatch

  Spec: `specs/130426-settlement-consensus-enforcement.md`
  Why now: Consensus-critical claim verification is the single largest unbuilt piece blocking the entire sharepool lifecycle. Every downstream task (ConnectBlock enforcement, wallet claims, e2e proof) depends on the script interpreter recognizing witness-v2.
  Codebase evidence: `src/script/interpreter.cpp` dispatches witversion 0 (P2WSH/P2WPKH) and 1 (Taproot) but has no witversion==2 branch; the else clause at line ~1988 falls through to future-softfork "anyone can spend" semantics. `src/script/interpreter.h` has no `SCRIPT_VERIFY_SHAREPOOL` flag and currently uses bit 20 for `SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_PUBKEYTYPE`, making bit 21 the next free flag in this checkout. `src/script/script_error.h` has no sharepool error codes. `src/consensus/sharepool.h` has settlement helper functions (HashSettlementLeaf, ComputeSettlementStateHash, Merkle root/branch computation) but no `VerifySharepoolSettlement()` function.
  Owns: `src/consensus/sharepool.{h,cpp}` (add VerifySharepoolSettlement), `src/script/interpreter.cpp` (add witversion==2 dispatch), `src/script/interpreter.h` (add SCRIPT_VERIFY_SHAREPOOL flag), `src/script/script_error.{h,cpp}` (add SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED, SCRIPT_ERR_SHAREPOOL_WITNESS_SIZE, SCRIPT_ERR_SHAREPOOL_DESCRIPTOR_VERSION)
  Integration touchpoints: `src/consensus/sharepool.{h,cpp}` helper functions consumed by verifier; `src/script/interpreter.cpp` VerifyWitnessProgram dispatch; settlement leaf/descriptor structs from sharepool.h; Merkle branch verification from existing ComputeSettlementPayoutBranch/ComputeSettlementClaimStatusBranch
  Scope boundary: Implements Unit A from the spec only. Does NOT modify validation.cpp (that is POOL-07G/H). Does NOT set the flag in ConnectBlock (that is POOL-07G). Verifier logic: check stack size==5, deserialize descriptor (version==1), deserialize leaf_index (0 ≤ index < leaf_count), verify payout branch against descriptor.payout_root, verify claim-status branch against expected status root, verify state_hash matches ComputeSettlementStateHash. Pre-activation behavior: witversion==2 continues to fall through if SCRIPT_VERIFY_SHAREPOOL is not set.
  Acceptance criteria: (1) VerifySharepoolSettlement returns true for a valid 5-element witness stack claiming the first leaf of a solo settlement. (2) Returns false with SCRIPT_ERR_SHAREPOOL_WITNESS_SIZE for stack size != 5. (3) Returns false with SCRIPT_ERR_SHAREPOOL_DESCRIPTOR_VERSION for descriptor.version != 1. (4) Returns false with SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED for tampered leaf data, wrong index, tampered branch, or double-claim (status bit already flipped). (5) Pre-activation (flag not set): witversion==2 falls through to future-softfork success. (6) Post-activation witness-v2 32-byte programs are reserved for sharepool settlement; malformed/non-sharepool 32-byte programs fail under SCRIPT_VERIFY_SHAREPOOL, while other future witness versions retain existing compatibility behavior.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build -R sharepool_verification` for new unit tests. Manual inspection: set a breakpoint in VerifyWitnessProgram, feed a witness-v2 spend, confirm dispatch reaches VerifySharepoolSettlement.
  Required tests: `src/test/sharepool_verification_tests.cpp` — minimum 9 scenarios: (1) valid solo-leaf claim, (2) valid multi-leaf claim (index > 0), (3) wrong stack size, (4) wrong descriptor version, (5) wrong leaf index (out of bounds), (6) tampered leaf data, (7) tampered payout branch, (8) double-claim (status bit flipped), (9) pre-activation compatibility (flag not set, witversion==2 succeeds). Each test constructs a settlement output, builds a claim witness stack, and calls VerifyScript.
  Dependencies: None (builds on existing sharepool.h helpers which are already tested via sharepool_commitment_tests.cpp)
  Estimated scope: M
  Completion signal: All 9+ verification test scenarios pass; `ctest --test-dir build -R sharepool` shows no regressions in existing sharepool_commitment_tests.

- [ ] `CHKPT-07A` Solo settlement claim verification checkpoint

  Spec: `specs/130426-settlement-consensus-enforcement.md`
  Why now: Witness-v2 verification is the highest-risk consensus change. Stop and validate before building ConnectBlock enforcement on top of it.
  Codebase evidence: After POOL-07F, the verifier and dispatch exist but ConnectBlock does not yet enforce the flag. This checkpoint confirms the verifier is correct in isolation.
  Owns: No new files. Review-only checkpoint.
  Integration touchpoints: Verifier in sharepool.{h,cpp}, dispatch in interpreter.cpp, flag in interpreter.h, errors in script_error.{h,cpp}
  Scope boundary: Run the full test suite. Manually construct a claim transaction on regtest and verify the script interpreter path. Do not proceed to POOL-07G if any test fails or if the verifier produces unexpected error codes.
  Acceptance criteria: (1) `ctest --test-dir build` passes with zero failures. (2) `test/functional/test_runner.py feature_sharepool_relay` passes (no regression). (3) Manual regtest exercise: activate sharepool, mine a block with solo settlement output, construct a claim transaction with correct witness stack, submit via sendrawtransaction, confirm it enters the mempool (script verification passes with unknown-witness semantics since flag not yet set in ConnectBlock).
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build` for full unit tests. `test/functional/test_runner.py feature_sharepool_relay` for relay regression.
  Required tests: No new tests. Runs existing suites.
  Dependencies: `POOL-07F`
  Estimated scope: XS
  Completion signal: Full unit test suite and relay functional test pass. Manual regtest claim transaction enters mempool.

### Cluster 2: ConnectBlock Enforcement

- [ ] `POOL-07G` Coinbase settlement output enforcement in ConnectBlock

  Spec: `specs/130426-settlement-consensus-enforcement.md`
  Why now: Without ConnectBlock enforcement, any coinbase shape is valid post-activation. This is the first half of making settlement outputs consensus-mandatory.
  Codebase evidence: `src/validation.cpp` (6607 lines) has zero references to "sharepool", "settlement", or "DEPLOYMENT_SHAREPOOL". `src/node/miner.cpp` already produces solo settlement outputs when activated (line ~185-198: MakeSoloSettlementLeaf + ComputeInitialSettlementStateHash + BuildSettlementScriptPubKey) and checks activation via SharepoolDeploymentActiveAfter() at line ~82. The flag SCRIPT_VERIFY_SHAREPOOL exists after POOL-07F but is not yet included in GetBlockScriptFlags().
  Owns: `src/validation.cpp` (add coinbase check in ConnectBlock, add SCRIPT_VERIFY_SHAREPOOL to GetBlockScriptFlags for activated blocks)
  Integration touchpoints: `src/consensus/params.h` DEPLOYMENT_SHAREPOOL enum; `src/script/interpreter.h` SCRIPT_VERIFY_SHAREPOOL flag; `src/node/miner.cpp` settlement output construction (must match what enforcement validates); `src/kernel/chainparams.cpp` activation parameters
  Scope boundary: Implements the coinbase-check portion of Unit B only. Post-activation blocks must contain exactly one OP_2 settlement output with nValue == block_reward. Adds SCRIPT_VERIFY_SHAREPOOL to script flags for activated blocks (enabling witness-v2 verification from POOL-07F). Does NOT implement claim transaction conservation checks (that is POOL-07H). Does NOT modify the miner or wallet.
  Acceptance criteria: (1) Post-activation block with missing settlement output is rejected by ConnectBlock. (2) Post-activation block with settlement output of wrong value is rejected. (3) Post-activation block with two settlement outputs is rejected. (4) Pre-activation block without settlement output is accepted. (5) Post-activation block with correct solo settlement output is accepted. (6) SCRIPT_VERIFY_SHAREPOOL is set for post-activation blocks, enabling witness-v2 enforcement.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build -R miner_tests` (existing miner tests should still pass). New unit tests for enforcement. `test/functional/test_runner.py feature_sharepool_relay` for regression.
  Required tests: `src/test/sharepool_enforcement_tests.cpp` — minimum 6 scenarios: (1) valid post-activation block accepted, (2) missing settlement output rejected, (3) wrong value rejected, (4) duplicate settlement outputs rejected, (5) pre-activation block without settlement accepted, (6) SCRIPT_VERIFY_SHAREPOOL flag present in GetBlockScriptFlags for activated blocks.
  Dependencies: `POOL-07F` (SCRIPT_VERIFY_SHAREPOOL flag definition)
  Estimated scope: S
  Completion signal: All 6 enforcement tests pass. Existing miner_tests and sharepool_commitment_tests pass.

- [ ] `POOL-07H` Claim transaction conservation enforcement in ConnectBlock

  Spec: `specs/130426-settlement-consensus-enforcement.md`
  Why now: Without claim-conservation enforcement, anyone can drain a settlement output with arbitrary transactions. This is the second half of making the settlement lifecycle consensus-safe.
  Codebase evidence: `src/validation.cpp` ConnectBlock currently has no per-transaction settlement checks. `src/consensus/sharepool.cpp` has ComputeRemainingSettlementValue() (line ~253) which computes old_value - claimed_amount, and ComputeSettlementStateHash() for successor state computation. The verifier from POOL-07F validates the witness stack; this task validates the transaction structure.
  Owns: `src/validation.cpp` (add claim-conservation checks in ConnectBlock's per-transaction loop)
  Integration touchpoints: `src/consensus/sharepool.{h,cpp}` for ComputeRemainingSettlementValue, ComputeSettlementStateHash, BuildSettlementScriptPubKey; `src/script/interpreter.cpp` witness-v2 verification (runs first, then conservation check runs); settlement UTXO identification (OP_2 + 32-byte program)
  Scope boundary: Implements the claim-conservation portion of Unit B. When input 0 of a transaction spends a settlement UTXO: (1) output 0 must match the claimed leaf's payout_script and amount, (2) if old_value > leaf.amount, output 1 must be a successor settlement output with correct new state_hash and value == old_value - leaf.amount, (3) if old_value == leaf.amount, no successor output, (4) value conservation: old_value == leaf.amount + successor_value, (5) transaction fees must come from non-settlement inputs only. Does NOT implement multi-leaf commitment (POOL-07J). Works with solo-leaf settlements as currently produced.
  Acceptance criteria: (1) Valid claim of sole leaf with no successor: accepted, full payout to payout_script. (2) Valid claim of one leaf with successor: accepted, payout + successor with correct state_hash. (3) Wrong payout amount: rejected. (4) Wrong payout script: rejected. (5) Missing successor when remaining value > 0: rejected. (6) Spurious successor when remaining value == 0: rejected. (7) Fee sourced from settlement input: rejected. (8) Immutable descriptor: successor descriptor matches original. (9) Double-claim of same leaf: rejected (status bit already flipped, wrong state_hash). (10) Fee funded by non-settlement input: accepted.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build -R sharepool_enforcement` plus `ctest --test-dir build -R sharepool_verification` for regression.
  Required tests: `src/test/sharepool_enforcement_tests.cpp` (extend from POOL-07G) — add minimum 10 scenarios covering all acceptance criteria. Each test constructs a block with settlement output, creates a claim transaction, and calls ConnectBlock to verify acceptance/rejection.
  Dependencies: `POOL-07G` (coinbase enforcement and SCRIPT_VERIFY_SHAREPOOL flag set in ConnectBlock)
  Estimated scope: M
  Completion signal: All 10 claim-conservation test scenarios pass. Existing miner_tests, sharepool_commitment_tests, sharepool_verification_tests pass.

- [ ] `CHKPT-07B` Settlement enforcement regression checkpoint

  Spec: `specs/130426-settlement-consensus-enforcement.md`
  Why now: ConnectBlock enforcement is the most consensus-critical code in the sharepool. A regression here risks chain splits. Stop and validate before building multi-leaf commitment.
  Codebase evidence: After POOL-07G and POOL-07H, both coinbase enforcement and claim conservation are active in ConnectBlock for post-activation blocks.
  Owns: No new files. Review-only checkpoint.
  Integration touchpoints: validation.cpp, sharepool.{h,cpp}, interpreter.cpp, miner.cpp
  Scope boundary: Run full test suite. Manually exercise the solo settlement claim lifecycle on regtest: activate → mine block → wait maturity → construct claim tx → submit → verify acceptance → verify payout received. Do not proceed to multi-leaf if this lifecycle fails.
  Acceptance criteria: (1) `ctest --test-dir build` all pass. (2) `test/functional/test_runner.py` all sharepool tests pass. (3) Manual regtest lifecycle: mine 1 block post-activation, mine 100 blocks for maturity, construct claim transaction for solo leaf, submit, confirm acceptance in next block, verify payout UTXO exists with correct amount.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build` for all unit tests. `test/functional/test_runner.py feature_sharepool_relay feature_sharepool_relay_benchmark` for functional regression. Manual regtest claim exercise.
  Required tests: No new tests. Exercises existing suites and manual verification.
  Dependencies: `POOL-07H`
  Estimated scope: XS
  Completion signal: Full test suite green. Manual solo-leaf claim lifecycle succeeds on regtest.

### Cluster 3: Reward Window and Multi-Leaf Commitment

- [ ] `POOL-07I` Reward-window walk in SharechainStore

  Spec: `specs/130426-sharechain-storage-relay.md`
  Why now: Multi-leaf commitment requires knowing which miners contributed shares in the trailing reward window. This is the data source for proportional reward distribution.
  Codebase evidence: `src/node/sharechain.h` defines SharechainStore with AddShare, GetShare, Contains, BestTip, Height, ShareCount, OrphanCount — but no GetRewardWindow(). `src/node/sharechain.cpp` (332 lines) has AddShare, ValidateShare, AcceptShare, ResolveOrphans, UpdateBestTip, LoadFromDisk — but no reward-window walk. The spec defines the reward window as the trailing set of shares from best tip, accumulating work backward until 7200 target-spacing shares of work are collected.
  Owns: `src/node/sharechain.{h,cpp}` (add GetRewardWindow method and RewardWindowEntry struct)
  Integration touchpoints: SharechainStore::BestTip() provides the starting point; ShareRecord::parent_share links backward; share_nBits provides per-share work; payout_script identifies the miner; the returned entries are consumed by BlockAssembler in POOL-07J
  Scope boundary: Add `struct RewardWindowEntry { CScript payout_script; arith_uint256 work; uint256 first_share_id; uint256 last_share_id; }` and a public `GetRewardWindow(...)` method that locks internally like existing SharechainStore accessors. Walk backward from BestTip via parent_share, accumulate work per payout_script, stop when total accumulated work reaches 7200 target-spacing shares (threshold from consensus params). Return empty vector if sharechain is empty or has no best tip. This is mining-policy data until GATE-07C defines the consensus replay contract. Does NOT modify BlockAssembler (POOL-07J). Does NOT define the largest-remainder distribution algorithm (POOL-07J).
  Acceptance criteria: (1) Empty sharechain returns empty vector. (2) Single-miner sharechain returns one entry with all accumulated work. (3) Two-miner sharechain returns two entries with work proportional to their share counts. (4) Window truncates at 7200-work threshold (shares beyond the window excluded). (5) Entries include correct first_share_id and last_share_id per payout_script. (6) Disconnected shares (orphans) are not included.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build -R sharechain` for unit tests.
  Required tests: `src/test/sharechain_tests.cpp` (extend) — add minimum 6 scenarios: empty chain, single miner, two miners, window truncation, first/last share IDs, orphan exclusion.
  Dependencies: None (SharechainStore already built and tested)
  Estimated scope: S
  Completion signal: All 6 reward-window test scenarios pass. Existing sharechain_tests pass.

- [ ] `GATE-07C` Canonical reward-window data-availability contract

  Spec: `specs/130426-sharepool-protocol.md`
  Why now: Multi-leaf payout fairness is only trustless if every validator derives the same reward window. A local P2P `SharechainStore::BestTip()` can differ across nodes, and the current settlement output only stores `OP_2 <state_hash>`; that is not enough to prove which shares were eligible for a block.
  Codebase evidence: `src/node/sharechain.{h,cpp}` stores and relays shares outside the block chain. `src/node/miner.cpp` currently writes only a solo settlement state hash into the coinbase. `SettlementDescriptor` in `src/consensus/sharepool.h` contains version, payout_root, and leaf_count, but no share tip or data-availability commitment. `src/validation.cpp` currently has no sharepool code and therefore no established replay path.
  Owns: Generated spec/plan updates for the chosen contract first; later code ownership depends on the selected mechanism.
  Integration touchpoints: `SharechainStore::GetRewardWindow()` from POOL-07I, `BlockAssembler` in `src/node/miner.cpp`, settlement descriptor/state hashing in `src/consensus/sharepool.{h,cpp}`, `ConnectBlock` in `src/validation.cpp`, `getrewardcommitment` and wallet settlement tracking in POOL-08.
  Scope boundary: Research/design decision only. Decide how the block-validity
  path identifies the canonical share tip, share records, and payout leaf set.
  Acceptable outcomes include a committed share tip plus mandatory
  share-availability/proof rules, an explicit descriptor/leaf/proof payload in
  the block, or another deterministic mechanism. Do not implement multi-leaf
  consensus enforcement in this gate. Reject or escalate any option where
  reward fairness remains local policy rather than consensus, because that does
  not meet the trustless-default-pool objective. The decision must explicitly
  choose where the canonical data lives at validation time: block data, witness
  data, undo/indexed sidecar data, or another replayable consensus surface.
  Acceptance criteria: (1) Decision artifact names the selected
  data-availability mechanism. (2) It explains how two validators with
  different relay histories validate the same block deterministically. (3) It
  explains how `getrewardcommitment` and wallet tracking reconstruct leaves
  after restart/reindex. (4) It defines reorg behavior. (5) It states what data
  is consensus-critical versus operator diagnostic only. (6) It records a clear
  rejection of any design that requires trust in local relay completeness. (7)
  It names the concrete follow-on code paths that will change (`miner.cpp`,
  `validation.cpp`, RPC reconstruction, wallet tracking).
  Verification: Review the amended generated specs and the decision artifact
  against `src/node/sharechain.{h,cpp}`, `src/consensus/sharepool.{h,cpp}`,
  `src/node/miner.cpp`, and `src/validation.cpp`; confirm every future task
  that consumes leaves depends on this gate and that the chosen mechanism
  survives restart/reindex and divergent-relay-history scenarios.
  Required tests: None in this gate. It must name the tests required by the
  selected mechanism, including a two-validator divergent-relay-history test, a
  restart/reindex reconstruction test, and a reorg replay test.
  Dependencies: `POOL-07I` (reward-window walk exists as a candidate input)
  Estimated scope: S
  Completion signal: Decision artifact committed; POOL-07J, POOL-08C, and wallet tracking tasks can cite a concrete data contract rather than "stored or recomputed" ambiguity.

- [ ] `POOL-07J` Multi-leaf payout commitment in BlockAssembler

  Spec: `specs/130426-settlement-consensus-enforcement.md`
  Why now: Solo settlement (one leaf, full reward to block finder) is the current fallback. Multi-leaf commitment distributes rewards proportionally, which is the core purpose of the sharepool.
  Codebase evidence: `src/node/miner.cpp` line ~185-198 constructs solo settlement with MakeSoloSettlementLeaf. `src/node/miner.h` line ~183 shows BlockAssembler constructor takes `const QSBPool*` but not `SharechainStore*`. Existing helpers in `src/consensus/sharepool.{h,cpp}`: SortSettlementLeaves, BuildSettlementDescriptor, ComputeInitialSettlementStateHash, BuildSettlementScriptPubKey are all ready for multi-leaf use. The solo path creates a single leaf; multi-leaf creates N leaves from GetRewardWindow entries.
  Owns: `src/node/miner.{h,cpp}` (add SharechainStore* parameter to BlockAssembler, replace solo-only path with multi-leaf when reward window is non-empty)
  Integration touchpoints: SharechainStore::GetRewardWindow() from POOL-07I; data-availability contract from GATE-07C; consensus::sharepool helpers for leaf construction, sorting, descriptor building; ConnectBlock enforcement from POOL-07G validates settlement shape and later uses the GATE-07C contract for fairness; `src/init.cpp` or `src/node/context.h` wires SharechainStore into BlockAssembler at construction
  Scope boundary: (1) Add `const SharechainStore*` parameter to BlockAssembler (nullable; solo fallback when null or empty). (2) When active and reward window non-empty: call GetRewardWindow() using the canonical input selected by GATE-07C, convert entries to SettlementLeaf with amount = floor(block_reward × entry.work / total_work), apply largest-remainder method for rounding (sort remainders descending by Hash(payout_script) tiebreak, distribute leftover roshi one-per-leaf top-down), sort leaves, build descriptor, compute state hash. (3) When inactive or empty reward window: fall back to existing solo leaf. (4) Wire SharechainStore into BlockAssembler construction site (init.cpp or miner creation call). Does NOT modify InternalMiner (POOL-08A).
  Acceptance criteria: (1) Empty reward window produces solo settlement leaf (identical to current behavior). (2) Single-miner reward window produces single leaf with full block reward. (3) Two-miner reward window produces two leaves with amounts summing exactly to block_reward. (4) Largest-remainder rounding: no roshi lost or created. (5) Leaf ordering matches SortSettlementLeaves (ascending Hash(payout_script)). (6) Resulting block passes ConnectBlock enforcement from POOL-07G.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build -R miner` for miner tests. Construct a regtest scenario: inject shares from 3 different payout scripts via P2P, mine a block, verify the coinbase commitment contains 3 leaves with proportional amounts summing to block_reward.
  Required tests: `src/test/miner_tests.cpp` (extend) — add minimum 5 scenarios: solo fallback (null SharechainStore), solo fallback (empty reward window), two-miner multi-leaf, three-miner with rounding, block passes ConnectBlock. `test/functional/feature_sharepool_commitment.py` — new functional test: 3-node regtest with P2P-injected shares from different scripts, verify multi-leaf commitment in mined block.
  Dependencies: `POOL-07I` (GetRewardWindow), `GATE-07C` (canonical data contract), `POOL-07G` (ConnectBlock validates settlement shape)
  Estimated scope: M
  Completion signal: Multi-leaf miner tests pass. Functional test confirms 3-miner commitment on regtest. Existing miner_tests pass (solo fallback path unchanged).

### Cluster 4: Share Production and Diagnostics

- [ ] `POOL-08A` Dual-target share production in InternalMiner

  Spec: `specs/130426-miner-share-production.md`
  Why now: The sharechain is empty in practice because no code produces shares. Without share production, multi-leaf commitments always fall back to solo. This is the supply side of the sharepool.
  Codebase evidence: `src/node/internal_miner.h` MiningContext struct has: block, seed_hash, nBits, job_id, height — no share_target, share_nBits, or parent_share. Constructor takes ChainstateManager&, interfaces::Mining&, CConnman* — no SharechainStore*. `src/node/internal_miner.cpp` worker hot loop (line ~453) calls CheckProofOfWork with single target only. No SubmitShare method exists. SharechainStore is used in `src/net_processing.cpp` for P2P relay but not connected to the miner.
  Owns: `src/node/internal_miner.{h,cpp}`
  Integration touchpoints: `src/node/sharechain.{h,cpp}` SharechainStore::AddShare() for storing produced shares; `src/net_processing.cpp` RelayShareInv() is currently a PeerManagerImpl method, so this task must add an explicit relay hook or node-level share submission service instead of assuming CConnman exposes it; `src/consensus/params.h` DEPLOYMENT_SHAREPOOL for activation gating; `src/node/miner.cpp` SharepoolDeploymentActiveAfter() for activation check pattern
  Scope boundary: (1) Add share_target (uint256), share_nBits (uint32_t),
  parent_share (uint256) to MiningContext. Computed once per template refresh,
  not per hash. (2) Add SharechainStore* parameter to InternalMiner constructor
  (nullable; share production disabled if null). (3) In worker hot loop: after
  block target check, add `else if (share_nBits != 0 &&
  hash_meets_target(rx_hash, share_target))` to call SubmitShare. A hash
  meeting block target also produces a share. (4) Add SubmitShare method:
  construct ShareRecord, call AddShare, increment m_shares_found counter, relay
  through the explicit peer-manager/node-context hook. (5) Add
  GetSharesFound() accessor. (6) Activation gating: share_nBits set to 0
  pre-activation. (7) Share target formula: `share_target = min(powLimit,
  block_target × consensus.nPowTargetSpacing)` for the current 1-second share
  cadence. (8) Post-activation default behavior: if sharepool is active and the
  miner has a wired SharechainStore plus relay hook, enabling internal mining
  must automatically produce shares; no extra long-lived sharepool opt-in flag.
  Does NOT modify wallet or RPCs. Does NOT modify BlockAssembler.
  Acceptance criteria: (1) Pre-activation: share_nBits == 0, no shares
  produced. (2) Post-activation: shares produced at expected rate (≈
  block_difficulty / share_difficulty × block_rate). (3) Block-finding hash
  produces both block submission and share submission. (4) Share-only hash
  produces share submission only. (5) Produced shares pass ValidateShare. (6)
  Produced shares appear in SharechainStore. (7) Produced shares relay to
  connected peers via P2P. (8) GetSharesFound() increments on each share. (9)
  Dual-target overhead < 1% of per-hash cycle time (one additional uint256
  comparison). (10) On an activated regtest node with sharepool wiring present,
  starting internal mining produces shares without a separate sharepool-specific
  enable step.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build -R miner` for unit tests. Regtest exercise: activate sharepool on 2-node network, enable mining, verify `getsharechaininfo` (after POOL-08B) or direct SharechainStore inspection shows shares from both nodes.
  Required tests: `src/test/miner_share_tests.cpp` — minimum 6 scenarios: (1) pre-activation no shares, (2) post-activation share produced for share-meeting hash, (3) block-finding hash also produces share, (4) share passes ValidateShare, (5) share_target formula correctness, (6) GetSharesFound counter. `test/functional/feature_sharepool_miner.py` — 2-node regtest: activate, mine, verify shares relay.
  Dependencies: None (SharechainStore is already built; P2P relay handlers exist in net_processing.cpp)
  Estimated scope: M
  Completion signal: All 6 unit tests pass. Functional test shows shares produced and relayed on 2-node regtest.

- [ ] `POOL-08B` Sharepool diagnostic RPCs

  Spec: `specs/130426-wallet-rpc-integration.md`
  Why now: Without RPCs, there is no way to inspect sharechain state or submit external shares. Needed for debugging, testing, and the e2e proof.
  Codebase evidence: `src/rpc/mining.cpp` has no submitshare, getsharechaininfo, or sharepool references. The relay benchmark functional test (line ~394 comment) confirms: "No node-native submitshare/getsharechaininfo RPC exists in this slice, so the benchmark injects shares over P2P." `src/rpc/blockchain.cpp` has getdeploymentinfo which reports DEPLOYMENT_SHAREPOOL status.
  Owns: `src/rpc/mining.cpp` (add submitshare and getsharechaininfo RPCs, extend getmininginfo with sharepool object)
  Integration touchpoints: SharechainStore for share submission and state queries; net_processing for share relay after RPC submission; DEPLOYMENT_SHAREPOOL for activation gating; existing getmininginfo RPC structure
  Scope boundary: (1) `submitshare "hexdata"`: deserialize ShareRecord, call SharechainStore::AddShare with full validation, relay to peers if accepted, return `{"accepted": true}` or RPC error. Gate: RPC_MISC_ERROR if sharepool not active. (2) `getsharechaininfo`: return `{tip, height, orphan_count, total_shares, difficulty}` from SharechainStore accessors. Gate: RPC_MISC_ERROR if not active. (3) Extend `getmininginfo`: add `sharepool: {active: true, sharechain_height, reward_window_size, pending_shares}` when active; omit entirely when inactive. (4) Extend `getinternalmininginfo`: add `shares_found` counter. Does NOT implement getrewardcommitment (POOL-08C). Does NOT modify wallet.
  Acceptance criteria: (1) submitshare accepts valid share hex, returns accepted. (2) submitshare rejects invalid share with actionable error (bad-share-pow, bad-share-version, etc.). (3) getsharechaininfo returns accurate sharechain state. (4) getmininginfo includes sharepool object when active. (5) getmininginfo omits sharepool when inactive. (6) All three RPCs return RPC_MISC_ERROR pre-activation. (7) getinternalmininginfo includes shares_found.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build -R rpc` for RPC tests. Regtest exercise: activate, submit share via RPC, query getsharechaininfo, verify share count increments.
  Required tests: `test/functional/feature_sharepool_rpc.py` — minimum 7 scenarios matching acceptance criteria: valid submit, invalid submit, getsharechaininfo state, getmininginfo active, getmininginfo inactive, pre-activation error, shares_found counter.
  Dependencies: None (SharechainStore exists; relay exists in net_processing)
  Estimated scope: S
  Completion signal: All 7 RPC functional test scenarios pass. Existing mining RPC tests pass.

- [ ] `CHKPT-08A` Multi-leaf commitment with produced shares checkpoint

  Spec: `specs/130426-settlement-consensus-enforcement.md`
  Why now: This is the first point where all core sharepool components work together: miner produces shares, sharechain stores them, reward window aggregates them, BlockAssembler commits multi-leaf, ConnectBlock enforces it. Verify integration before adding wallet/claim complexity.
  Codebase evidence: After POOL-07J, POOL-08A, and POOL-08B, the full share→commitment pipeline exists.
  Owns: No new files. Integration verification checkpoint.
  Integration touchpoints: InternalMiner (share production), SharechainStore (storage + reward window), BlockAssembler (multi-leaf commitment), ConnectBlock (enforcement), RPCs (visibility)
  Scope boundary: Exercise on 3-node regtest: activate sharepool, mine with different addresses on each node, use getsharechaininfo to verify shares accumulate, mine enough blocks for reward window to fill, inspect coinbase commitments for multi-leaf structure, verify leaf amounts are proportional to hashrate contribution. Do not proceed to wallet integration if multi-leaf commitments are malformed or amounts don't sum correctly.
  Acceptance criteria: (1) `ctest --test-dir build` all pass. (2) All sharepool functional tests pass. (3) 3-node regtest shows multi-leaf commitments with 3 distinct payout scripts. (4) Leaf amounts sum exactly to block_reward. (5) getsharechaininfo reports expected share counts and heights on all nodes.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build` for full unit suite. `test/functional/test_runner.py feature_sharepool_relay feature_sharepool_rpc feature_sharepool_commitment feature_sharepool_miner` for all sharepool functional tests. Manual 3-node regtest inspection.
  Required tests: No new tests. Exercises existing suites.
  Dependencies: `POOL-07J`, `POOL-08A`, `POOL-08B`
  Estimated scope: XS
  Completion signal: Full test suite green. Manual 3-node regtest shows correct multi-leaf commitments.

### Cluster 5: Wallet Integration

- [ ] `POOL-08C` getrewardcommitment RPC

  Spec: `specs/130426-wallet-rpc-integration.md`
  Why now: Needed for the e2e proof to verify commitment contents from outside the node. Simpler than wallet balance tracking; provides visibility into what BlockAssembler committed.
  Codebase evidence: No getrewardcommitment RPC exists in `src/rpc/mining.cpp` or `src/rpc/blockchain.cpp`.
  Owns: `src/rpc/blockchain.cpp` (add getrewardcommitment RPC)
  Integration touchpoints: CBlock coinbase transaction for settlement output extraction; settlement state hash in OP_2 output; canonical reward-window data contract from GATE-07C for leaf enumeration; DEPLOYMENT_SHAREPOOL activation gating
  Scope boundary: `getrewardcommitment "blockhash"`: look up block, find settlement output (OP_2 + 32 bytes), extract and return `{blockhash, state_hash, leaves: [{payout_script, amount_roshi}], total_committed}`. Gate: RPC_MISC_ERROR if not active or block is pre-activation. For multi-leaf: enumerate leaves only through the data-availability mechanism selected by GATE-07C. Do not reconstruct leaves from this node's transient local share relay history unless GATE-07C makes that state consensus-persisted and replayable. For solo: return single leaf. Does NOT modify wallet.
  Acceptance criteria: (1) Returns correct single leaf for solo-settlement block. (2) Returns correct multiple leaves for multi-leaf block using the canonical GATE-07C data source. (3) Leaf amounts sum to total_committed == block_reward. (4) Pre-activation block returns error. (5) Inactive deployment returns error. (6) Restart/reindex test still returns the same leaves for an already-mined multi-leaf block.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build`. Regtest: mine post-activation block, call getrewardcommitment, verify output matches expected commitment.
  Required tests: `test/functional/feature_sharepool_rpc.py` (extend) — add scenarios: getrewardcommitment for solo block, getrewardcommitment for multi-leaf block, pre-activation error, restart/reindex leaf reconstruction.
  Dependencies: `POOL-07J` (multi-leaf commitments exist in blocks), `GATE-07C` (canonical leaf data source exists)
  Estimated scope: S
  Completion signal: getrewardcommitment returns correct data for solo and multi-leaf blocks.

- [ ] `POOL-08D` Wallet pooled balance tracking

  Spec: `specs/130426-wallet-rpc-integration.md`
  Why now: Miners need to see their pending and claimable rewards. This is the user-facing surface that makes the sharepool tangible.
  Codebase evidence: `src/wallet/rpc/coins.cpp` getbalances (line ~402-455) returns only `mine.trusted`, `mine.untrusted_pending`, `mine.immature`, `mine.used`. No "pooled" field. No settlement UTXO tracking in wallet code.
  Owns: `src/wallet/rpc/coins.cpp` (extend getbalances), `src/wallet/wallet.{h,cpp}` or `src/wallet/receive.{h,cpp}` (add settlement UTXO tracking)
  Integration touchpoints: ConnectBlock creates settlement UTXOs; wallet's TransactionAddedToMempool/BlockConnected notifications; settlement output identification (OP_2 + 32 bytes); coinbase maturity (100 blocks); DEPLOYMENT_SHAREPOOL activation gating; getrewardcommitment/canonical GATE-07C data for leaf lookup
  Scope boundary: (1) Extend getbalances with `pooled: {pending, claimable}` when sharepool is active and canonical leaf data is available. Omit when inactive; return unavailable/omit pooled fields rather than infer multi-leaf balances from local-only share relay state. (2) `pooled.pending`: sum of committed amounts for this wallet's payout scripts in settlement outputs with < 100 confirmations. Informational only, not spendable. (3) `pooled.claimable`: sum of committed amounts for this wallet's payout scripts in settlement outputs with ≥ 100 confirmations and unclaimed. (4) Track settlement UTXOs: on block connection, scan coinbase for settlement output, match leaves against wallet scripts using the canonical leaf source, store mapping `{outpoint → leaf_index, payout_script, amount, confirmation_height, claim_status}`. (5) State transitions: PENDING_MATURITY (< 100 conf) → CLAIMABLE (≥ 100 conf, unclaimed). Does NOT implement auto-claim (POOL-08E). Does NOT implement manual claim RPC. Open question: fee estimation for claims defaults to wallet normal fee estimation. Open question: wallet encryption interaction deferred to auto-claim task.
  Acceptance criteria: (1) getbalances includes pooled.pending for unmatured settlement. (2) getbalances includes pooled.claimable for matured unclaimed settlement. (3) Omits pooled field pre-activation. (4) Amounts match committed leaf amounts exactly. (5) Pending transitions to claimable after 100 confirmations. (6) Already-claimed leaves not counted in claimable.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build -R wallet` for wallet tests. Regtest: activate, mine block, check getbalances shows pooled.pending, mine 100 blocks, check pooled.claimable appears.
  Required tests: `test/functional/feature_sharepool_wallet.py` — minimum 6 scenarios: (1) pre-activation no pooled field, (2) pending after 1 confirmation, (3) claimable after 100 confirmations, (4) correct amounts, (5) multi-leaf only counts wallet's own scripts, (6) claimed leaf removed from claimable.
  Dependencies: `POOL-07H` (ConnectBlock enforcement creates valid settlement UTXOs), `POOL-07J` (multi-leaf for realistic testing), `POOL-08C` (leaf lookup API), `GATE-07C` (canonical leaf data source)
  Estimated scope: M
  Completion signal: All 6 wallet functional test scenarios pass. getbalances shows correct pooled values on regtest.

- [ ] `POOL-08E` Auto-claim mechanism

  Spec: `specs/130426-wallet-rpc-integration.md`
  Why now: Without auto-claim, miners must manually construct claim transactions. This is the final step before the system is usable end-to-end.
  Codebase evidence: No auto-claim code exists in `src/wallet/`. Zero matches for "autoclaim", "auto_claim", or "auto-claim" in the wallet directory.
  Owns: `src/wallet/wallet.{h,cpp}` or new `src/wallet/sharepool_claim.{h,cpp}` (auto-claim logic), `src/init.cpp` (add -noautoclaim flag)
  Integration touchpoints: Wallet settlement UTXO tracking from POOL-08D; ConnectBlock claim conservation rules from POOL-07H; VerifySharepoolSettlement from POOL-07F; wallet fee estimation; wallet coin selection for fee-funding inputs; existing wallet transaction broadcast path
  Scope boundary: (1) Trigger: on block connection, check for settlement UTXOs reaching 100 confirmations with this wallet's payout script. (2) Claim tx construction: input 0 = matured settlement UTXO, inputs 1..n = fee-funding from non-settlement wallet UTXOs (standard coin selection), output 0 = payout to committed payout_script with exact committed amount, output 1 = successor settlement output if not final claim. (3) Witness stack: 5 elements per spec (descriptor, leaf_index, leaf_data, payout_branch, status_branch). (4) State transition: CLAIMABLE → CLAIM_BROADCAST (on broadcast) → CLAIMED (on confirmation). (5) Opt-out: `-noautoclaim` flag disables auto-claim. (6) Fee estimation: use wallet's normal fee estimator. (7) Wallet encryption: if wallet is locked, queue claim for when wallet is next unlocked. Does NOT implement batched multi-leaf claims (one claim per transaction). Does NOT implement manual claimreward RPC (future enhancement).
  Acceptance criteria: (1) Auto-claim fires for matured settlement with wallet's payout script. (2) Claim transaction passes ConnectBlock enforcement. (3) Payout arrives in wallet as mine.untrusted_pending, then mine.trusted. (4) Successor output has correct state_hash and value. (5) Final claim (last leaf) has no successor. (6) -noautoclaim disables auto-claim. (7) Locked wallet queues claim until unlocked. (8) Fee comes from non-settlement inputs. (9) pooled.claimable decreases after claim broadcast.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build` full suite. Regtest: activate, mine blocks until matured, verify auto-claim fires, verify payout in getbalances mine.trusted, verify pooled.claimable decreases.
  Required tests: `test/functional/feature_sharepool_wallet.py` (extend) — add minimum 7 scenarios: (1) auto-claim fires on maturity, (2) claim tx valid per consensus, (3) payout arrives in wallet, (4) successor output correct, (5) final claim no successor, (6) -noautoclaim disables, (7) fee from non-settlement input.
  Dependencies: `POOL-08D` (settlement UTXO tracking), `POOL-07H` (ConnectBlock validates claims)
  Estimated scope: M
  Completion signal: All 7 auto-claim functional tests pass. Full regtest lifecycle: mine → mature → auto-claim → payout in wallet.

- [ ] `CHKPT-08B` Full claim lifecycle checkpoint

  Spec: `specs/130426-testing-end-to-end-proof.md`
  Why now: All sharepool components are now implemented. Verify the complete lifecycle before writing the formal e2e proof test.
  Codebase evidence: After POOL-08E, every component in the sharepool pipeline exists: miner produces shares, sharechain stores them, reward window aggregates, BlockAssembler commits multi-leaf, ConnectBlock enforces, wallet tracks balances, auto-claim fires.
  Owns: No new files. Integration verification checkpoint.
  Integration touchpoints: All sharepool components end-to-end.
  Scope boundary: Full lifecycle on 2-node regtest: (1) activate sharepool, (2) mine with different addresses, (3) verify shares relay, (4) mine enough for reward window, (5) verify multi-leaf commitments, (6) mine 100+ blocks for maturity, (7) verify auto-claim fires, (8) verify payouts in getbalances. Do not proceed to e2e proof if any step fails.
  Acceptance criteria: (1) `ctest --test-dir build` all pass. (2) All sharepool functional tests pass. (3) Manual 2-node lifecycle succeeds end-to-end.
  Verification: `cmake --build build -j$(nproc) && ctest --test-dir build`. `test/functional/test_runner.py` all sharepool tests. Manual 2-node regtest lifecycle.
  Required tests: No new tests. Exercises all existing suites.
  Dependencies: `POOL-08E`
  Estimated scope: XS
  Completion signal: Full test suite green. Manual 2-node regtest full lifecycle succeeds.

### Cluster 6: System Proof

- [ ] `CHKPT-03` Regtest end-to-end proof

  Spec: `specs/130426-testing-end-to-end-proof.md`
  Why now: This is the formal proof that the sharepool works. It is the primary input to the decision gate.
  Codebase evidence: Only 2 sharepool functional tests exist: `test/functional/feature_sharepool_relay.py` (88 lines, relay only) and `test/functional/feature_sharepool_relay_benchmark.py` (relay latency measurement). No full-lifecycle e2e test exists.
  Owns: `test/functional/feature_sharepool_e2e.py` (new)
  Integration touchpoints: All sharepool components; regtest BIP9 activation via `-vbparams=sharepool:0:9999999999:0`; getdeploymentinfo, getsharechaininfo, getrewardcommitment, getbalances RPCs; canonical GATE-07C leaf data source; P2P share relay; ConnectBlock enforcement
  Scope boundary: Implement Plan 009 from the spec. 4-node regtest mesh.
  Lifecycle stages: (1) BIP9 activation: mine 108 of 144 blocks, verify
  getdeploymentinfo shows ACTIVE. (2) Share production: enable mining on all
  nodes, verify shares appear in getsharechaininfo on all nodes. (3) Default
  participation proof: start one fresh node after activation with ordinary
  mining enabled and verify it joins the sharepool path without a dedicated
  sharepool opt-in toggle. (4) Block mining: mine 50 blocks, verify multi-leaf
  commitments via getrewardcommitment using the canonical leaf data source. (5)
  Restart/reindex proof: verify an already-mined multi-leaf block still exposes
  the same leaves. (6) Maturity wait: mine 100 additional blocks. (7) Claim
  verification: verify getbalances shows pooled.claimable > 0, verify
  auto-claim fires, verify balance transitions to mine.trusted. (8)
  Proportional reward: verify each miner's cumulative reward within +/-5% of
  hashrate proportion. (9) Negative tests: submit invalid share (wrong PoW),
  verify rejection; submit claim with wrong amount, verify rejection. Test must
  be deterministic and reproducible (fixed seeds where needed). Does NOT test
  devnet scenarios (DEVNET-01). Does NOT test adversarial attacks beyond basic
  negative cases.
  Acceptance criteria: (1) BIP9 lifecycle completes: DEFINED → STARTED →
  LOCKED_IN → ACTIVE. (2) All 4 nodes produce and relay shares. (3) A fresh
  activated mining node participates in the sharepool path without a dedicated
  sharepool opt-in flag. (4) Multi-leaf commitments present in post-activation
  blocks. (5) Restart/reindex proof reconstructs the same commitment leaves.
  (6) Claims succeed after maturity. (7) Proportional reward within +/-5% of
  hashrate. (8) Negative tests: invalid share rejected, invalid claim rejected.
  (9) Test passes on 2 consecutive runs (reproducibility).
  Verification: `test/functional/feature_sharepool_e2e.py` passes. Run twice to confirm reproducibility.
  Required tests: `test/functional/feature_sharepool_e2e.py` — single comprehensive test covering all 8 acceptance criteria.
  Dependencies: `POOL-08E` (auto-claim), `POOL-08B` (RPCs for inspection), `POOL-08C` (getrewardcommitment), `GATE-07C` (canonical leaf data source)
  Estimated scope: M
  Completion signal: feature_sharepool_e2e.py passes on 2 consecutive runs with all assertions green.

- [ ] `GATE-03` Decision gate GO/NO-GO review

  Spec: `specs/130426-testing-end-to-end-proof.md`
  Why now: This is the formal decision point that determines whether to proceed to devnet adversarial testing.
  Codebase evidence: No decision gate report exists for the post-implementation phase.
  Owns: `genesis/plans/010-decision-gate-regtest-proof-review.md` (update with evidence)
  Integration touchpoints: CHKPT-03 e2e proof results; full test suite results; sharepool unit tests; pre-existing test regression check
  Scope boundary: (1) Run CHKPT-03 e2e test twice, record results. (2) Run `ctest --test-dir build` full unit suite, record results. (3) Run `test/functional/test_runner.py` full functional suite, record results. (4) Review reward distribution: is each miner's reward within +/-5% of hashrate proportion? (5) Review for consensus bugs: any assertion failures, unexpected rejections, or chain splits? (6) Write GO or NO-GO with evidence. GO criteria: all of the above pass. NO-GO triggers: any test failure, non-reproducibility, reward distribution outside tolerance, or consensus bugs. NO-GO recovery: fix issues, loop back to the failing task, re-run gate. Does NOT start devnet deployment (DEVNET-01). Does NOT modify mainnet parameters.
  Acceptance criteria: (1) Decision report written with pass/fail evidence for each GO criterion. (2) If GO: all criteria met, report states GO with evidence links. (3) If NO-GO: report identifies specific failures and links to the task that must be re-executed.
  Verification: Review the decision report. Verify all referenced test outputs match actual results.
  Required tests: No new tests. Reviews outputs of existing tests.
  Dependencies: `CHKPT-03`
  Estimated scope: S
  Completion signal: Decision report committed with GO or NO-GO determination and evidence.

### Execution Note

The older coarse-grain umbrella items `POOL-07`, `POOL-08`, and the earlier
coarse `CHKPT-03` have been decomposed into the cluster tasks above and are no
longer active work items. Do not revive or re-open them in loop execution; use
the decomposed tasks and gates instead.

## Follow-On Work (locked until `GATE-03` GO)

These tasks are intentionally deferred. `auto loop` should not select them
while any **Priority Work** item remains open or while `GATE-03` is unresolved.

### Cluster 7: Post-Proof Release Path

- [ ] `DEVNET-RES-01` 1-second share relay bandwidth measurement

  Spec: `specs/130426-sharechain-storage-relay.md`
  Why now: The 10-second relay benchmark passed (p50 58.6ms, p99 79.2ms, 100% propagation, 0.063 KB/s), but 1-second cadence is only extrapolated (~0.6 KB/s linear scaling). Measurement must happen before devnet to validate feasibility.
  Codebase evidence: `test/functional/feature_sharepool_relay_benchmark.py` exists but uses 10-second share injection interval. The benchmark comment notes the 1-second extrapolation is unmeasured. Confirmed constant: share spacing is 1 second (POOL-03R decision).
  Owns: `test/functional/feature_sharepool_relay_benchmark.py` (adapt interval), results report
  Integration touchpoints: InternalMiner share production (POOL-08A) or P2P injection at 1-second cadence; SharechainStore; net_processing P2P relay
  Scope boundary: Re-run the relay benchmark at 1-second injection cadence. Measure p50, p99, max latency, bandwidth per node, orphan events, propagation percentage. Compare with 10-second results. If p99 latency > 500ms or orphan rate > 5%, flag as risk for devnet. Does NOT change the 1-second constant (that was confirmed by POOL-03R). Only measures feasibility.
  Acceptance criteria: (1) Benchmark runs at 1-second cadence for ≥ 120 shares. (2) p99 latency reported. (3) Bandwidth per node reported. (4) Orphan rate reported. (5) Results documented with pass/fail against thresholds.
  Verification: `test/functional/feature_sharepool_relay_benchmark.py` with 1-second parameters.
  Required tests: Modified benchmark script.
  Dependencies: `POOL-08A` (share-producing miner for realistic test), or can use P2P injection
  Estimated scope: S
  Completion signal: Benchmark results committed with 1-second cadence measurements.

- [ ] `DEVNET-01` Devnet deployment and 48-hour adversarial testing

  Spec: `specs/130426-activation-deployment.md`
  Why now: Regtest proves correctness in isolation. Devnet proves stability under adversarial conditions over time.
  Codebase evidence: No devnet infrastructure exists. 4 Contabo validators currently run mainnet (3 healthy, 1 crash-looping).
  Owns: Devnet deployment scripts, monitoring dashboards, adversarial test scripts, stability report
  Integration touchpoints: All sharepool components; fleet management scripts in `scripts/`; validator infrastructure
  Scope boundary: Plan 011 from spec. 4+ nodes across 2+ hosts. Phase 1 (24h): stability baseline with all honest nodes. Phase 2: 7 adversarial scenarios (share withholding, eclipse, relay spam, orphan flooding, claim front-running, settlement draining, reorg with shares). Phase 3: results documented. Does NOT activate mainnet. Requires GATE-03 GO.
  Acceptance criteria: (1) 48+ hours of operation. (2) All 7 adversarial scenarios executed. (3) Share withholding advantage < 5%. (4) No memory leaks (RSS stable over 48h). (5) Share relay maintains > 95% propagation. (6) No consensus failures or chain splits. (7) Results report committed.
  Verification: Monitoring metrics (block rate, share rate, relay latency, memory, peer count) collected continuously. Adversarial scenario results documented per spec.
  Required tests: Adversarial test scripts (new). Monitoring collection scripts.
  Dependencies: `GATE-03` (must be GO), `DEVNET-RES-01` (1-second bandwidth validated)
  Estimated scope: L
  Completion signal: 48-hour stability report committed with all 7 adversarial scenarios passing thresholds.

- [ ] `MAINNET-01` Mainnet activation parameter preparation

  Spec: `specs/130426-activation-deployment.md`
  Why now: After devnet proves stability, mainnet parameters must be set and a release binary produced.
  Codebase evidence: `src/kernel/chainparams.cpp` sets mainnet DEPLOYMENT_SHAREPOOL to NEVER_ACTIVE. Threshold is 1916/2016 (95%). nStartTime must be updated to a future date with minimum 4-week upgrade window.
  Owns: `src/kernel/chainparams.cpp` (update nStartTime), release binary, deployment documentation
  Integration touchpoints: BIP9 deployment parameters; release pipeline (`scripts/build-release.sh`, `.github/workflows/release.yml`); operator documentation
  Scope boundary: (1) Set mainnet nStartTime to a date providing ≥ 4-week upgrade window after binary release. (2) Set min_activation_height if warranted. (3) Update testnet params (optional early activation for staging). (4) Build release binary. (5) Update operator docs with upgrade instructions. Does NOT implement speedy-trial or LOT=true (governance decisions deferred). Does NOT change threshold from 95%.
  Acceptance criteria: (1) nStartTime set to future date. (2) Release binary builds reproducibly. (3) Operator docs updated. (4) Testnet activation tested.
  Verification: `scripts/build-release.sh` produces binary. `scripts/check-reproducible-release.sh` passes. Testnet activation observed.
  Required tests: Existing CI pipeline. Testnet activation verification.
  Dependencies: `DEVNET-01` (devnet stability proven)
  Estimated scope: M
  Completion signal: Release binary published with updated mainnet activation parameters.

### Cluster 8: Operational Hygiene (non-blocking for regtest proof)

- [ ] `OPS-01` DNS seeds or alternative peer discovery

  Spec: `specs/130426-node-operations-fleet.md`
  Why now: The spec flags absent DNS seeds as NOT MET. All 4 seed peers are hardcoded IPs on a single ASN (Contabo). DNS seeds provide resilient peer discovery.
  Codebase evidence: `src/kernel/chainparams.cpp` hardcodes 4 seed peer IPs. No DNS seed entries configured. `doc/` references seed1/2/3.rng.network but these are not implemented.
  Owns: `src/kernel/chainparams.cpp` (add vSeeds entries), DNS seed infrastructure
  Integration touchpoints: P2P peer discovery; DNS seed protocol (Bitcoin Core standard); network bootstrapping
  Scope boundary: Add at least 2 DNS seeds on different ASNs. Verify seeds resolve to active nodes. Does NOT remove hardcoded IPs (kept as fallback).
  Acceptance criteria: (1) At least 2 DNS seeds configured. (2) Seeds resolve to valid peers. (3) New node bootstraps via DNS seeds. (4) Seeds on ≥ 2 distinct ASNs.
  Verification: `dig seed1.rng.network` resolves. New node connects via DNS discovery.
  Required tests: Manual verification of DNS resolution and peer connection.
  Dependencies: None
  Estimated scope: S
  Completion signal: DNS seeds configured and resolving.

- [ ] `OPS-02` Seed peer ASN diversification

  Spec: `specs/130426-node-operations-fleet.md`
  Why now: All 4 hardcoded seed peers are on Contabo (single ASN). A Contabo outage would prevent new nodes from bootstrapping. HIGH RISK per spec.
  Codebase evidence: `src/kernel/chainparams.cpp` hardcodes 4 Contabo IPs. All validators on Contabo infrastructure.
  Owns: `src/kernel/chainparams.cpp` (update seed IPs), new validator provisioning
  Integration touchpoints: Validator infrastructure; seed peer list; peer discovery
  Scope boundary: Provision at least 1 validator on a non-Contabo provider. Update seed peer list to include it. Does NOT require migrating existing validators.
  Acceptance criteria: (1) Seed peers span ≥ 2 autonomous systems. (2) Non-Contabo seed peer is reachable and synced.
  Verification: `whois` confirms different ASN. Node connects to non-Contabo peer.
  Required tests: Manual connectivity verification.
  Dependencies: None
  Estimated scope: S
  Completion signal: Seed peer list includes peers on ≥ 2 ASNs.

- [ ] `OPS-03` Validator-01 crash-loop repair

  Spec: `specs/130426-node-operations-fleet.md`
  Why now: contabo-validator-01 has been crash-looping since 2026-04-13 03:52Z due to a zero-byte settings.json file. Simple fix but reduces fleet from 4 to 3 healthy validators.
  Codebase evidence: Per WORKLIST.md: "Repair contabo-validator-01 startup (settings.json is zero bytes as of 2026-04-13 03:52Z, causing crash loop)".
  Owns: Validator-01 configuration
  Integration touchpoints: Fleet health; peer count; mining capacity
  Scope boundary: SSH to validator-01, delete or replace the zero-byte `/root/.rng/settings.json` with valid JSON (`{}`), restart the node. Verify it syncs to chain tip.
  Acceptance criteria: (1) validator-01 starts without crash. (2) Syncs to current chain tip. (3) scripts/doctor.sh reports healthy.
  Verification: `ssh contabo-validator-01 'rng-cli getblockchaininfo'` returns current height. `scripts/doctor.sh` passes.
  Required tests: None. Operational verification only.
  Dependencies: None
  Estimated scope: XS
  Completion signal: validator-01 healthy and synced.

- [ ] `OPS-04` Bootstrap versioning and refresh schedule

  Spec: `specs/130426-node-operations-fleet.md`
  Why now: Bootstrap snapshot is at height 29944 (~60 MB) with no versioning or refresh schedule. As the chain grows, stale bootstraps slow onboarding.
  Codebase evidence: `bootstrap/` directory contains height-29944 snapshot. `scripts/load-bootstrap.sh` loads it. No version metadata or automation for refresh.
  Owns: `bootstrap/` versioning scheme, refresh automation
  Integration touchpoints: `scripts/load-bootstrap.sh`; onboarding documentation; release pipeline
  Scope boundary: Add version metadata to bootstrap (height, date, hash). Establish refresh schedule (e.g., every 10,000 blocks). Automate snapshot generation. Does NOT require new binary.
  Acceptance criteria: (1) Bootstrap has version metadata. (2) Refresh schedule documented. (3) New snapshot at current height available.
  Verification: `scripts/load-bootstrap.sh` loads versioned snapshot. Height matches current chain.
  Required tests: None. Operational verification.
  Dependencies: None
  Estimated scope: S
  Completion signal: Versioned bootstrap at current height published with refresh schedule documented.

- [ ] `OPS-05` Cross-machine reproducible build verification

  Spec: `specs/130426-node-operations-fleet.md`
  Why now: Same-machine linux-x86_64 reproducibility is verified. Cross-machine is NOT verified per spec. Reproducible builds are essential for trust.
  Codebase evidence: `scripts/check-reproducible-release.sh` exists. `scripts/build-release.sh` exists. CI builds on Linux, macOS, Windows. Cross-machine comparison not automated.
  Owns: CI workflow extension or standalone verification script
  Integration touchpoints: `.github/workflows/release.yml`; `scripts/build-release.sh`; `scripts/check-reproducible-release.sh`
  Scope boundary: Build release binary on 2+ different machines (or CI runners). Compare SHA256 hashes. Document results. Does NOT block mainnet activation but is a trust signal.
  Acceptance criteria: (1) Release binary SHA256 matches across 2+ machines. (2) Results documented with machine specs.
  Verification: `sha256sum` comparison of release artifacts from different machines.
  Required tests: None. Verification script.
  Dependencies: None
  Estimated scope: S
  Completion signal: Cross-machine SHA256 match documented.

### Cluster 9: Research Backlog (do not interrupt priority work)

- [ ] `RESEARCH-01` fPowAllowMinDifficultyBlocks interaction with LWMA difficulty

  Spec: `specs/130426-node-operations-fleet.md`
  Why now: fPowAllowMinDifficultyBlocks=true on mainnet is flagged as a security risk in the spec. Its interaction with LWMA difficulty adjustment is unresolved. This is a research task, not an implementation task.
  Codebase evidence: The spec notes this flag is active on mainnet. LWMA uses a 720-block window with 60-timestamp outlier cut.
  Owns: Research report documenting the interaction
  Integration touchpoints: `src/pow.cpp` (LWMA implementation); `src/kernel/chainparams.cpp` (fPowAllowMinDifficultyBlocks setting); consensus difficulty rules
  Scope boundary: Analyze whether fPowAllowMinDifficultyBlocks allows difficulty manipulation on mainnet with LWMA. Document: (1) Under what conditions does min-difficulty kick in? (2) Can an attacker exploit this to mine easy blocks? (3) If yes, what is the mitigation? (4) Should the flag be changed to false? This is research only. Does NOT change any code.
  Acceptance criteria: (1) Report documents the interaction clearly. (2) Risk assessment: exploitable or not. (3) Recommendation: keep, change, or defer.
  Verification: Code review of pow.cpp and chainparams.cpp.
  Required tests: None. Analysis only.
  Dependencies: None
  Estimated scope: S
  Completion signal: Research report committed with risk assessment and recommendation.

- [ ] `RESEARCH-02` Claim throughput queue depth under load

  Spec: `specs/130426-economic-model-validation.md`
  Why now: One claim per block per settlement output means a backlog can form with many miners. The spec lists this as an unmodeled economic surface. Research needed before mainnet.
  Codebase evidence: Settlement spec defines one claim per transaction. No batched claims. With N leaves per settlement, it takes N blocks to drain one settlement output.
  Owns: Analysis report
  Integration touchpoints: Settlement consensus rules; mempool policy; block size limits
  Scope boundary: Analyze: (1) With M active miners, how many blocks to drain a settlement? (2) At what M does the claim queue become problematic? (3) Should batched claims be considered for v2? Research only, no code changes.
  Acceptance criteria: (1) Queue depth analysis for 10, 50, 100 miner scenarios. (2) Block delay per miner documented. (3) Recommendation for v1 vs v2.
  Verification: Analytical calculation verified against simulator.
  Required tests: None. Analysis only.
  Dependencies: None
  Estimated scope: S
  Completion signal: Analysis report committed.

- [ ] `RESEARCH-03` Share pruning and eviction strategy

  Spec: `specs/130426-sharechain-storage-relay.md`
  Why now: SharechainStore grows indefinitely. At 1-second cadence, that is ~120 shares per block, ~86,400 per day. The spec flags this as an open question: "When/how should old shares be pruned?"
  Codebase evidence: `src/node/sharechain.cpp` LoadFromDisk replays all shares. No pruning logic exists. LevelDB key prefix 's' used for all shares.
  Owns: Research report, design proposal
  Integration touchpoints: SharechainStore; LevelDB; GetRewardWindow (only needs shares within window)
  Scope boundary: Analyze: (1) Storage growth rate at 1-second cadence. (2) When can shares be safely pruned (after leaving reward window)? (3) Memory impact of full in-memory replay. (4) Propose pruning strategy. Research only, no code changes.
  Acceptance criteria: (1) Growth rate quantified. (2) Pruning boundary defined (shares older than reward window + safety margin). (3) Implementation approach documented.
  Verification: Storage measurement on regtest with 10,000+ shares.
  Required tests: None. Analysis only.
  Dependencies: `POOL-07I` (GetRewardWindow defines the retention boundary)
  Estimated scope: S
  Completion signal: Research report committed with pruning strategy.

## Completed / Already Satisfied

- [x] `DONE-POW` RandomX PoW mining system

  Spec: `specs/130426-randomx-pow-mining.md`
  Evidence: Vendored RandomX v1.2.1 in `src/crypto/randomx/`. Fixed genesis seed "RNG Genesis Seed", Argon salt "RNGCHAIN01". LWMA difficulty (720-block window, 60-timestamp outlier cut, 120s target). 32,122+ mainnet blocks with no PoW failures. Internal miner in `src/node/internal_miner.{h,cpp}` (coordinator + N workers, stride-based nonce partitioning, RandomX fast mode). Unit tests in `src/test/randomx_tests`. Initial subsidy 50 RNG, halving interval 2,100,000 blocks, coinbase maturity 100.

- [x] `DONE-ECON` Economic model and simulator validation

  Spec: `specs/130426-economic-model-validation.md`
  Evidence: `contrib/sharepool/simulate.py` (789 lines): deterministic offline simulator. 1-second candidate confirmed by POOL-03R: seed 42 CV 3.33%, stress seeds 1-20 max CV 8.06% (all < 10% threshold). 2-second candidate rejected (CV 10.33%). 10-second baseline rejected (CV 25.10%). Withholding advantage 0.00% (below 5% threshold). `contrib/sharepool/settlement_model.py` (660 lines): 5 deterministic settlement transition vectors in `contrib/sharepool/reports/pool-07b-settlement-vectors.json`. C++ parity tests in `src/test/sharepool_commitment_tests.cpp` reproduce all reference hashes.

- [x] `DONE-SHARE-STORE` Sharechain data model, storage, and validation

  Spec: `specs/130426-sharechain-storage-relay.md`
  Evidence: `src/node/sharechain.{h,cpp}` (133+332 lines): ShareRecord struct (version, parent_share, prev_block_hash, candidate_header, share_nBits, payout_script), LevelDB storage with key prefix 's', orphan buffer (max 64, FIFO eviction), best-tip by cumulative work with deterministic tie-break, LoadFromDisk replay. ValidateShare enforces 7 rejection reasons (bad-share-version, share-prevblock-mismatch, bad-block-target, bad-share-target, share-target-too-hard, share-target-too-easy, share-pow-invalid). Unit tests in `src/test/sharechain_tests.cpp` (6 scenarios: serialization, chain insertion, orphan resolution, buffer bound, invalid PoW rejection, LevelDB persistence).

- [x] `DONE-RELAY` P2P share relay

  Spec: `specs/130426-sharechain-storage-relay.md`
  Evidence: `src/net_processing.cpp` handles shareinv, getshare, share messages (activation-gated via SharepoolRelayActive). Message limits: shareinv 1000, getshare 1000, share 16. Invalid shares trigger Misbehaving(). Oversized messages rejected regardless of activation. `test/functional/feature_sharepool_relay.py` (88 lines): 3-node regtest relay test. `test/functional/feature_sharepool_relay_benchmark.py`: 10-second cadence benchmark results: p50 58.6ms, p99 79.2ms, max 88.8ms, 0.063 KB/s, 0 orphans, 100% propagation.

- [x] `DONE-SETTLE-HELPERS` Settlement consensus helper functions

  Spec: `specs/130426-settlement-consensus-enforcement.md`
  Evidence: `src/consensus/sharepool.{h,cpp}` (75+266 lines): SettlementLeaf and SettlementDescriptor structs, HashSettlementLeaf, HashSettlementDescriptor, HashSettlementClaimFlag, ComputeSettlementStateHash, MakeSoloSettlementLeaf, SortSettlementLeaves, BuildSettlementDescriptor, BuildSettlementScriptPubKey, ComputeSettlementPayoutRoot, ComputeSettlementPayoutBranch, ComputeSettlementClaimStatusRoot, ComputeSettlementClaimStatusBranch, ComputeRemainingSettlementValue. Tagged SHA256 prefixes: "RNGSharepoolLeaf", "RNGSharepoolState". All hashes verified against Python reference model via `src/test/sharepool_commitment_tests.cpp` (3 test scenarios matching pool-07b-settlement-vectors.json).

- [x] `DONE-SOLO-SETTLE` Solo settlement coinbase construction

  Spec: `specs/130426-sharepool-protocol.md`
  Evidence: `src/node/miner.cpp` line ~185-198: when SharepoolDeploymentActiveAfter (line ~82), constructs solo settlement leaf via MakeSoloSettlementLeaf, computes state hash via ComputeInitialSettlementStateHash, creates OP_2 output via BuildSettlementScriptPubKey with nValue = block_reward. Tested in `src/test/miner_tests.cpp`.

- [x] `DONE-BIP9` BIP9 DEPLOYMENT_SHAREPOOL activation boundary

  Spec: `specs/130426-activation-deployment.md`
  Evidence: `src/consensus/params.h` defines DEPLOYMENT_SHAREPOOL enum. `src/kernel/chainparams.cpp`: mainnet bit 3, period 2016, threshold 1916 (95%), NEVER_ACTIVE. Regtest activatable via `-vbparams=sharepool:0:9999999999:0`. `src/rpc/blockchain.cpp` getdeploymentinfo reports sharepool deployment status. Referenced in 7 files: params.h, chainparams.cpp, miner.cpp, blockchain.cpp, miner_tests.cpp, versionbits_tests.cpp, net_processing.cpp.

- [x] `DONE-QSB` QSB operator support

  Spec: `specs/130426-node-operations-fleet.md`
  Evidence: `src/script/qsb.{h,cpp}`, `src/node/qsb_pool.{h,cpp}`, `src/node/qsb_validation.{h,cpp}`. RPCs: submitqsbtransaction, listqsbtransactions, removeqsbtransaction. Mainnet proven: 3 canary transactions mined at heights 29946-29947. Functional tests: feature_qsb_builder.py, feature_qsb_rpc.py, feature_qsb_mining.py.

- [x] `DONE-FLEET` Mainnet fleet and CI

  Spec: `specs/130426-node-operations-fleet.md`
  Evidence: Mainnet live at height ~32,122. 3 of 4 validators healthy (validator-01 crash-looping on zero-byte settings.json). Network magic 0xB07C010E, P2P port 8433, Bech32 HRP "rng". BIP324 encrypted transport from genesis. CI in `.github/workflows/ci.yml` covers Linux, macOS, Windows. `scripts/doctor.sh` health check. `scripts/build-release.sh` and `scripts/check-reproducible-release.sh` for releases. Same-machine linux-x86_64 reproducibility verified.

- [x] `DONE-PROTOCOL` Sharepool protocol specification

  Spec: `specs/130426-sharepool-protocol.md`
  Evidence: `specs/sharepool.md` (canonical spec): 5-layer protocol design, 10 consensus invariants, confirmed constants (1-second share spacing, 120 share target ratio, 7200 reward window shares, max 64 orphans, witness v2, BIP9 bit 3 period 2016 threshold 1916). `specs/sharepool-settlement.md`: settlement state machine, covenant rules, commitment tracking. All constants confirmed by POOL-03R decision with simulator evidence.
