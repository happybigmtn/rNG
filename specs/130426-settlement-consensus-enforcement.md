# Specification: Settlement Consensus Enforcement

Plan 007 from the genesis corpus. This is the single highest-priority unbuilt
code surface: the three integration slices that give sharepool settlement
economic meaning in consensus.

## Objective

Land witness-v2 claim verification, ConnectBlock settlement enforcement, and
multi-leaf payout commitment. After this work, an activated regtest node will:

1. Build coinbase settlement outputs whose state hash commits to the full
   reward-window payout leaf set.
2. Reject blocks with missing or invalid settlement commitments.
3. Accept valid claim transactions spending mature settlement UTXOs.
4. Reject duplicate claims, value-draining claims, and malformed successors.

## Evidence Status

### Verified Facts

All of the following have been verified by reading the actual source files.

**Settlement helpers exist and are tested** (`src/consensus/sharepool.{h,cpp}`,
266 lines):

- `SettlementLeaf` struct: `payout_script` (CScript), `amount_roshi` (int64_t),
  `first_share_id` (uint256), `last_share_id` (uint256). Serializable.
- `SettlementDescriptor` struct: `version` (uint64_t, initial 1), `payout_root`
  (uint256), `leaf_count` (uint32_t). Serializable with CompactSize encoding.
- Tagged hash functions: `HashSettlementLeaf()`, `HashSettlementDescriptor()`,
  `HashSettlementClaimFlag()`, `ComputeSettlementStateHash()`. Each uses
  domain-separated double-SHA256 with tags `RNGSharepoolLeaf`,
  `RNGSharepoolDescriptor`, `RNGSharepoolClaimFlag`, `RNGSharepoolState`.
- Leaf ordering: `SettlementLeafLess()` sorts by `Hash(payout_script)`, then by
  raw script bytes on tie.
- Solo leaf factory: `MakeSoloSettlementLeaf()` derives a synthetic share ID
  from prev_block_hash, height, payout_script, and amount using the
  `RNGSharepoolSoloLeaf` tag.
- Merkle tree: `ComputeSettlementMerkleRoot()` uses Bitcoin-style
  duplicate-last-hash. `ComputeSettlementMerkleBranch()` and
  `ComputeSettlementMerkleRootFromBranch()` for inclusion proofs.
- Status tree: `SettlementStatusTreeSize()` = next power of two.
  `InitialSettlementClaimedFlags()` = all false. Padding leaves permanently
  marked claimed. `ComputeSettlementClaimStatusRoot()`,
  `ComputeSettlementClaimStatusBranch()`.
- State hash: `ComputeInitialSettlementStateHash()` composes descriptor +
  initial claim status root.
- Script output: `BuildSettlementScriptPubKey()` produces `OP_2 <state_hash>`.
- Value accounting: `ComputeRemainingSettlementValue()` sums unclaimed leaf
  amounts.

**Solo settlement coinbase is wired** (`src/node/miner.cpp`, lines 82-199):

- `SharepoolDeploymentActiveAfter()` checks BIP9 activation via
  `Consensus::DEPLOYMENT_SHAREPOOL`.
- When active: coinbase output 0 = empty (value=0, miner script), output 1 =
  settlement (full block_reward, `OP_2 <state_hash>`).
- Uses `MakeSoloSettlementLeaf()` with a single payout script, sorted through
  `SortSettlementLeaves()`, then `ComputeInitialSettlementStateHash()`.
- When inactive: single coinbase output with full block_reward to miner script.

**Settlement spec is complete** (`specs/sharepool-settlement.md`, 407 lines):

- Defines witness-v2 program semantics, claim witness format (5 elements),
  verification rules, state transition rules, value conservation, maturity,
  and non-goals for v1.

**Reference model passes** (`contrib/sharepool/settlement_model.py`, ~660 lines):

- Five scenarios producing deterministic vectors at
  `contrib/sharepool/reports/pool-07b-settlement-vectors.json`.

**Parity tests pass** (`src/test/sharepool_commitment_tests.cpp`, 262 lines):

- Three test cases reproducing reference vectors: initial state, claim branch
  reconstruction, and final claim + fee funding.

**SigVersion enum** (`src/script/interpreter.h`, line 190):

- `BASE=0`, `WITNESS_V0=1`, `TAPROOT=2`, `TAPSCRIPT=3`. No WITNESS_V2 or
  SETTLEMENT variant.

**Script verification flags** (`src/script/interpreter.h`, lines 46-148):

- Script verification flags live in `src/script/interpreter.h`, not
  `src/script/script.h`.
- The current highest explicit flag is
  `SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_PUBKEYTYPE = (1U << 20)`.
- The next free flag bit for `SCRIPT_VERIFY_SHAREPOOL` is therefore
  `(1U << 21)` unless another flag lands first.

**VerifyWitnessProgram** (`src/script/interpreter.cpp`, line 1912):

- Handles witversion 0 (P2WSH, P2WPKH), witversion 1 + 32 bytes (Taproot),
  and pay-to-anchor. All other version/size combinations return true for
  soft-fork compatibility.

**validation.cpp has no sharepool references** (confirmed: zero matches for
"sharepool" or "settlement" in src/validation.cpp, 6607 lines):

- `ConnectBlock()` at line 2411 checks `block.vtx[0]->GetValueOut() >
  blockReward` at line 2710 but has no settlement output structure checks.

**DEPLOYMENT_SHAREPOOL is BIP9-gated** (`src/kernel/chainparams.cpp`):

- Defined on all networks. Bit 3. NEVER_ACTIVE on mainnet, testnet, signet.
  Regtest uses period=144, threshold=108 (75%).

### Recommendations

- Do not add a new `SigVersion::WITNESS_V2` enum value. Witness-v2 settlement
  verification is not general script execution. Instead, add a
  `VerifySharepoolSettlement()` function in `src/consensus/sharepool.{h,cpp}`
  and call it from `VerifyWitnessProgram()` when `witversion == 2 &&
  program.size() == 32 && !is_p2sh`. This keeps consensus logic in one place
  and avoids polluting the script interpreter's signature-verification
  machinery.

- Add a new `SCRIPT_VERIFY_SHAREPOOL` flag to gate the verification. Before
  activation, witness-v2 32-byte programs continue to pass as unknown witness
  programs (soft-fork compatible). After activation, the flag is set and the
  settlement program is verified.

- Add error codes `SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED`,
  `SCRIPT_ERR_SHAREPOOL_WITNESS_SIZE`, and
  `SCRIPT_ERR_SHAREPOOL_DESCRIPTOR_VERSION` to
  `src/script/script_error.{h,cpp}`.

- ConnectBlock enforcement should be two separate checks: (1) coinbase
  settlement output presence and value, and (2) per-transaction claim
  conservation. Both are needed. The coinbase check runs once per block. The
  claim check runs for each transaction spending a settlement UTXO.

- Multi-leaf reward-window commitment should fall back to the existing solo leaf
  when the sharechain store is empty or absent, preserving the current miner
  behavior as a degenerate case.

- Add a canonical reward-window data-availability gate before consensus
  enforcement of multi-leaf fairness. A local `SharechainStore::BestTip()` walk
  is valid mining policy, but it is not safe as a block-validity input unless
  the block or consensus-persisted state gives every validator the same share
  tip, share records, and proof material.

### Hypotheses / Unresolved Questions

- **Activation flag threading**: `VerifyWitnessProgram()` currently receives
  `flags` but not chain state. The `SCRIPT_VERIFY_SHAREPOOL` flag must be set
  by the caller (likely in `ConnectBlock()` or `AcceptToMemoryPool()`) based
  on `DeploymentActiveAt()`. The exact threading path is not yet designed.

- **Mempool claim validation**: Should mempool validation enforce the same
  witness-v2 checks as ConnectBlock, or should claims be policy-only until
  mined? The plan says "same checks" but the implementation path through
  `AcceptToMemoryPool` needs to be traced.

- **Claim transaction ordering**: Multiple claims against the same settlement
  are serialized by UTXO single-spend. The mempool can hold at most one
  unconfirmed claim per settlement UTXO. This is handled by existing mempool
  conflict logic, but needs verification.

- **Canonical reward-window data availability**: Multi-leaf reward-window
  enforcement cannot depend on whatever shares a validator happened to receive
  over P2P. Before Unit C becomes consensus-enforced, the design must define
  how all validators derive the same payout leaves after restart and across
  reorgs.

- **SharechainStore access in block assembler**: `BlockAssembler` currently
  receives a `QSBPool*`. Adding a `SharechainStore*` follows the same pattern.
  The exact method for the reward-window walk (`GetRewardWindow()`) is
  specified in the plan but not yet implemented in `src/node/sharechain.h`.

## Unit A: Witness-v2 Settlement Program Verification

### Current State

`VerifyWitnessProgram()` in `src/script/interpreter.cpp` (line 1912) dispatches
on witness version. The relevant else-chain for unknown versions (line 1987):

```cpp
} else {
    if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM) {
        return set_error(serror, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM);
    }
    // Other version/size/p2sh combinations return true for future softfork compatibility
    return true;
}
```

Witness version 2 with a 32-byte program currently falls through here and
succeeds unconditionally (anyone-can-spend). This is the soft-fork upgrade path.

### What Must Be Built

**New function** in `src/consensus/sharepool.{h,cpp}`:

```cpp
bool VerifySharepoolSettlement(
    const std::vector<unsigned char>& program,  // 32-byte state_hash from scriptPubKey
    const std::vector<std::vector<unsigned char>>& witness_stack,
    ScriptError* serror);
```

This function:

1. Requires `witness_stack.size() == 5`. Error: `SCRIPT_ERR_SHAREPOOL_WITNESS_SIZE`.

2. Deserializes element 0 as `SettlementDescriptor`. Requires
   `descriptor.version == SETTLEMENT_DESCRIPTOR_VERSION`. Error:
   `SCRIPT_ERR_SHAREPOOL_DESCRIPTOR_VERSION`.

3. Deserializes element 1 as `leaf_index` (CScriptNum). Requires
   `0 <= leaf_index < descriptor.leaf_count`. Error:
   `SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED`.

4. Deserializes element 2 as `SettlementLeaf` (leaf_data).

5. Deserializes element 3 as `payout_branch` (concatenated 32-byte hashes,
   length must be a multiple of 32).

6. Deserializes element 4 as `status_branch` (concatenated 32-byte hashes,
   length must be a multiple of 32).

7. Computes `leaf_hash = HashSettlementLeaf(leaf_data)`.

8. Recomputes `payout_root` from `leaf_hash`, `leaf_index`, and
   `payout_branch` via `ComputeSettlementMerkleRootFromBranch()`. Requires
   `payout_root == descriptor.payout_root`. Error:
   `SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED`.

9. Computes `old_status_leaf = HashSettlementClaimFlag(leaf_index, false)`.

10. Recomputes `old_claim_status_root` from `old_status_leaf`, `leaf_index`,
    and `status_branch` via `ComputeSettlementMerkleRootFromBranch()`.

11. Computes `expected_state_hash = ComputeSettlementStateHash(descriptor,
    old_claim_status_root)`.

12. Requires `expected_state_hash == program` (the 32-byte witness-v2 program
    from the spent UTXO). Error: `SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED`.

On success, the function returns true. The caller (ConnectBlock or mempool)
handles state-transition validation (output checks, value conservation)
separately.

**Note**: The function does NOT verify transaction outputs. It only proves "this
leaf exists and is unclaimed under this state hash." Output verification is
Unit B's responsibility.

**New dispatch** in `VerifyWitnessProgram()`:

```cpp
} else if (witversion == 2 && program.size() == 32 && !is_p2sh) {
    if (!(flags & SCRIPT_VERIFY_SHAREPOOL)) return set_success(serror);
    return consensus::sharepool::VerifySharepoolSettlement(
        program, witness.stack, serror);
}
```

This must appear before the catch-all unknown-witness-version branch.

**New script verification flag** in `src/script/interpreter.h`:

```cpp
SCRIPT_VERIFY_SHAREPOOL = (1U << 21),
```

The bit position must not conflict with existing flags. The inspected checkout
uses bit 20 for `SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_PUBKEYTYPE`, so bit 21 is
the next available slot unless intervening work changes the flag set.

**New error codes** in `src/script/script_error.{h,cpp}`:

- `SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED`
- `SCRIPT_ERR_SHAREPOOL_WITNESS_SIZE`
- `SCRIPT_ERR_SHAREPOOL_DESCRIPTOR_VERSION`

### Files to Modify

| File | Change |
|------|--------|
| `src/consensus/sharepool.h` | Add `VerifySharepoolSettlement()` declaration |
| `src/consensus/sharepool.cpp` | Implement `VerifySharepoolSettlement()` |
| `src/script/interpreter.cpp` | Add witversion==2 dispatch in `VerifyWitnessProgram()` |
| `src/script/interpreter.h` | Add `SCRIPT_VERIFY_SHAREPOOL` flag |
| `src/script/script_error.h` | Add error enum values |
| `src/script/script_error.cpp` | Add error strings |

### Files to Create

| File | Purpose |
|------|---------|
| `src/test/sharepool_claim_tests.cpp` | Unit tests for `VerifySharepoolSettlement()` |

### Test Scenarios

1. **Valid claim witness**: Construct a settlement UTXO, build a correct 5-element
   witness from reference vectors, verify passes.
2. **Wrong leaf index**: Change leaf_index to out-of-range value, verify fails
   with `SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED`.
3. **Tampered leaf data**: Modify `amount_roshi` in leaf, verify payout root
   mismatch fails.
4. **Already-claimed leaf**: Supply `status_branch` that assumes `claimed=true`
   for the target leaf. The reconstructed state hash will not match the
   prevout program (since prevout commits to the unclaimed state). Fails with
   `SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED`.
5. **Wrong descriptor version**: Set `descriptor.version = 2`, verify fails with
   `SCRIPT_ERR_SHAREPOOL_DESCRIPTOR_VERSION`.
6. **Wrong witness stack size**: Supply 4 or 6 elements, verify fails with
   `SCRIPT_ERR_SHAREPOOL_WITNESS_SIZE`.
7. **Pre-activation soft-fork compat**: Without `SCRIPT_VERIFY_SHAREPOOL` flag,
   witness-v2 32-byte program passes unconditionally.
8. **Truncated branch**: Supply a payout_branch with length not divisible by 32,
   verify fails.

## Unit B: ConnectBlock Commitment Enforcement

### Current State

`ConnectBlock()` at line 2411 of `src/validation.cpp` validates blocks. The
coinbase value check at line 2709-2712:

```cpp
CAmount blockReward = nFees + GetBlockSubsidy(pindex->nHeight, params.GetConsensus());
if (block.vtx[0]->GetValueOut() > blockReward && state.IsValid()) {
    state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-cb-amount", ...);
}
```

There are no sharepool-related checks anywhere in validation.cpp. The subsidy
check allows the coinbase to pay up to `blockReward` across any number of
outputs in any format.

### What Must Be Built

**Coinbase settlement output check** (once per block, in ConnectBlock):

When `DeploymentActiveAt(*pindex, params.GetConsensus(),
Consensus::DEPLOYMENT_SHAREPOOL)` is true:

1. Scan coinbase outputs for witness-v2 32-byte programs (script pattern:
   `OP_2 <32 bytes>`). Count them.
2. Require exactly one such output. Rejection reason: `"bad-cb-settlement-count"`.
3. Require that output's value equals the full block reward
   (`nFees + GetBlockSubsidy()`). Rejection reason:
   `"bad-cb-settlement-value"`.
4. The existing `block.vtx[0]->GetValueOut() > blockReward` check remains
   unchanged. The settlement output accounts for the full reward; the other
   coinbase output(s) may have value=0 (for the miner's script marker).

**Set SCRIPT_VERIFY_SHAREPOOL flag**:

When computing script verification flags for a block where sharepool is active,
include `SCRIPT_VERIFY_SHAREPOOL`. This causes `VerifyWitnessProgram()` to
dispatch witness-v2 settlement programs to `VerifySharepoolSettlement()`.

The flag must be included in `GetBlockScriptFlags()` (or equivalent) when
`DeploymentActiveAt()` returns true for DEPLOYMENT_SHAREPOOL.

**Claim transaction conservation check** (per-transaction, in ConnectBlock):

For each non-coinbase transaction in the block, check whether input 0 spends
a witness-v2 32-byte-program UTXO. If it does, this is a settlement claim
transaction. Enforce:

1. Output 0 exists. Its `scriptPubKey` and `nValue` must match the claimed
   leaf. The claimed leaf is recoverable from the witness stack element 2
   (already deserialized and verified by Unit A).

2. Let `old_value` = the spent settlement UTXO's value.

3. If `old_value > leaf.amount_roshi`:
   - Output 1 must exist.
   - Output 1 must be a witness-v2 32-byte program (`OP_2 <new_state_hash>`).
   - Output 1 value must equal `old_value - leaf.amount_roshi`.
   - `new_state_hash` must equal `ComputeSettlementStateHash(descriptor,
     new_claim_status_root)` where `new_claim_status_root` is computed by
     flipping the claimed leaf from 0 to 1 in the status branch.

4. If `old_value == leaf.amount_roshi`:
   - No successor settlement output may exist (no witness-v2 32-byte program
     among outputs).

5. Value conservation: `old_value == leaf.amount_roshi + successor_value`.
   (Where `successor_value` is 0 when no successor exists.)

6. Fee rule: The sum of non-settlement inputs must cover all non-mandatory
   outputs plus the transaction fee. The settlement input's value may only
   flow to the payout output and successor output.

**Implementation approach**: Add a `ValidateSharepoolClaim()` function, called
from the transaction validation loop in ConnectBlock when input 0 is identified
as a settlement UTXO. This function receives the transaction, the spent UTXO
(from the coins view), and returns a validation result.

The new_state_hash verification requires re-extracting the descriptor and
status_branch from the witness. Since Unit A already validated these, the
conservation check can trust the deserialized witness data and focus on the
output structure and value arithmetic.

### Files to Modify

| File | Change |
|------|--------|
| `src/validation.cpp` | Add coinbase settlement check, claim conservation check, SCRIPT_VERIFY_SHAREPOOL flag |
| `src/consensus/sharepool.h` | Add `ValidateSharepoolClaim()` declaration |
| `src/consensus/sharepool.cpp` | Implement `ValidateSharepoolClaim()` |

### Files to Create

| File | Purpose |
|------|---------|
| `src/test/validation_sharepool_tests.cpp` | Integration tests for ConnectBlock enforcement |

### Test Scenarios

1. **Activated block with valid settlement output**: accepted.
2. **Activated block missing settlement output**: rejected with
   `"bad-cb-settlement-count"`.
3. **Activated block with two settlement outputs**: rejected with
   `"bad-cb-settlement-count"`.
4. **Activated block with wrong settlement value**: rejected with
   `"bad-cb-settlement-value"`.
5. **Valid claim with successor**: payout output matches leaf, successor has
   correct new_state_hash and value. Accepted.
6. **Claim with wrong payout amount**: rejected.
7. **Claim with wrong payout script**: rejected.
8. **Claim with missing successor when claims remain**: rejected.
9. **Claim with spurious successor on final claim**: rejected.
10. **Claim spending immature settlement**: rejected by existing coinbase
    maturity check (100 blocks).
11. **Pre-activation block without settlement**: accepted (unchanged behavior).
12. **Claim with fee taken from settlement value**: rejected (value
    conservation violation).

## Unit C: Multi-Leaf Reward-Window Commitment

### Current State

`CreateNewBlock()` in `src/node/miner.cpp` (lines 183-199) builds only the solo
case:

```cpp
const bool sharepool_active{SharepoolDeploymentActiveAfter(pindexPrev, m_chainstate.m_chainman)};
coinbaseTx.vout.clear();
if (sharepool_active) {
    coinbaseTx.vout.emplace_back(/*nValue=*/0, m_options.coinbase_output_script);
    const auto ordered_leaves{consensus::sharepool::SortSettlementLeaves({
        consensus::sharepool::MakeSoloSettlementLeaf(
            pindexPrev->GetBlockHash(), nHeight,
            m_options.coinbase_output_script, block_reward),
    })};
    const uint256 settlement_state_hash{consensus::sharepool::ComputeInitialSettlementStateHash(ordered_leaves)};
    coinbaseTx.vout.emplace_back(
        block_reward,
        consensus::sharepool::BuildSettlementScriptPubKey(settlement_state_hash));
}
```

This creates a single-leaf settlement where the solo miner receives the full
reward. The synthetic share ID is derived from block context, not from actual
shares.

`BlockAssembler` receives a `const QSBPool*` for quick-spend-bypass. It does
not currently receive a `SharechainStore*`.

### What Must Be Built

**Prerequisite: canonical reward-window decision gate**:

Before `ConnectBlock` can reject a block for an incorrect multi-leaf payout
split, the implementation must close the data-availability contract. A local
`SharechainStore::BestTip()` walk is valid for mining policy, but it is not
consensus-safe unless all validating nodes can deterministically identify the
same share tip and reconstruct the same share records after restart and across
reorgs. The gate must choose and document one mechanism, for example:

1. the block commits to a share tip plus a required share availability/proof
   rule,
2. the block carries enough descriptor/leaf/proof data to verify the payout set
   directly, or
3. multi-leaf reward fairness remains non-consensus policy, which would not
   satisfy the trustless-default-pool objective and must be explicitly rejected
   or escalated.

**SharechainStore reward-window query**:

Add to `src/node/sharechain.h`:

```cpp
struct RewardWindowEntry {
    CScript payout_script;
    arith_uint256 work;  // accumulated PoW work for this script in the window
    uint256 first_share_id;
    uint256 last_share_id;
};

std::vector<RewardWindowEntry> GetRewardWindow(
    const uint256& share_tip,
    const arith_uint256& work_threshold) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
```

The method walks backward from the best share tip, accumulating work up to the
7200-share target window. Shares are aggregated by `payout_script`. Returns one
entry per distinct payout script in the window. Its first implementation is a
mining-policy helper unless the prerequisite data-availability gate defines a
consensus-safe replay path.

**BlockAssembler integration**:

1. Add `SharechainStore*` member to `BlockAssembler`, passed through constructor.
2. In `CreateNewBlock()`, when sharepool is active:
   - If `SharechainStore*` is non-null and has a best share tip:
    - Call `GetRewardWindow()` using the canonical share tip selected by the
      data-availability gate, or use it as mining policy only until that gate is
      closed.
     - Convert entries to `SettlementLeaf` objects. The `amount_roshi` for each
       leaf is proportional to its work fraction of the total window work,
       applied to `block_reward`. Rounding: largest-remainder method to ensure
       leaf amounts sum exactly to `block_reward`.
     - Sort via `SortSettlementLeaves()`.
     - Compute state hash via `ComputeInitialSettlementStateHash()`.
   - If `SharechainStore*` is null or the sharechain is empty:
     - Fall back to the existing solo-settlement leaf (current behavior).
3. The coinbase output structure is unchanged: output 0 = marker (value=0),
   output 1 = settlement (full block_reward).

**Amount rounding**: For N leaves with proportional amounts, use integer
division with largest-remainder allocation:

```
base_amount[i] = (work[i] * block_reward) / total_work
remainder[i] = (work[i] * block_reward) % total_work
distribute (block_reward - sum(base_amount)) extra roshi to leaves with largest remainders
```

This ensures `sum(amount_roshi) == block_reward` exactly.

### Files to Modify

| File | Change |
|------|--------|
| `src/node/miner.cpp` | Replace solo-only with reward-window multi-leaf |
| `src/node/miner.h` | Add `SharechainStore*` to `BlockAssembler` |
| `src/node/sharechain.h` | Add `GetRewardWindow()` method |
| `src/node/sharechain.cpp` | Implement `GetRewardWindow()` |

### Files to Modify (tests)

| File | Change |
|------|--------|
| `src/test/miner_tests.cpp` | Add multi-leaf settlement scenarios |

### Test Scenarios

1. **Two payout scripts, 60/40 work split**: settlement has two leaves with
   amounts summing to block_reward, proportional to work.
2. **Empty sharechain (cold start)**: solo fallback produces single-leaf
   settlement identical to current behavior.
3. **Single payout script in sharechain**: produces single-leaf settlement
   (degenerate multi-leaf).
4. **Reward window shorter than threshold**: uses all available shares.
5. **Amount rounding**: three payout scripts with work that produces non-integer
   roshi splits. Verify amounts sum exactly to block_reward.
6. **Solo leaf regression**: existing miner_tests continue to pass unchanged.

## Consensus Invariants

These 10 rules are from `specs/sharepool-settlement.md` and must be enforced
by the combination of Units A, B, and C:

| # | Invariant | Enforced By |
|---|-----------|-------------|
| 1 | One block creates at most one settlement output | Unit B: coinbase check |
| 2 | Settlement value = full block reward | Unit B: coinbase check |
| 3 | Descriptor immutable across successors | Unit A: state_hash verification + Unit B: successor new_state_hash uses same descriptor |
| 4 | Claim-status root: only one flip 0->1 per claim | Unit A: verifies unclaimed + Unit B: verifies successor has exactly one flip |
| 5 | Exact committed amount per claim | Unit B: payout output check |
| 6 | Value conservation: old = claimed + new | Unit B: conservation check |
| 7 | No double claims | Unit A: state_hash encodes claim status; claimed leaf produces wrong hash |
| 8 | No successor on final claim | Unit B: final claim shape check |
| 9 | Successor required when remaining value > 0 | Unit B: non-final claim shape check |
| 10 | Fees from non-settlement inputs only | Unit B: fee conservation check |

## Acceptance Criteria

- `build/bin/test_bitcoin --run_test=sharepool_claim_tests` passes: all
   Unit A verification scenarios (valid claim, wrong index, tampered leaf,
   double claim, wrong version, wrong stack size, pre-activation compat).

- `build/bin/test_bitcoin --run_test=validation_sharepool_tests` passes: all
   Unit B enforcement scenarios (coinbase checks, claim conservation, value
   draining rejection, maturity).

- `build/bin/test_bitcoin --run_test=miner_tests` passes: existing solo case
   unchanged, new multi-leaf cases produce correct commitments.

- `build/bin/test_bitcoin --run_test=sharepool_commitment_tests` passes:
   existing parity tests unaffected.

- `python3 contrib/sharepool/settlement_model.py --self-test` passes:
   reference model unchanged.

- Full build with no warnings:
   `cmake --build build -j$(nproc) 2>&1 | grep -c warning` returns 0.

- A canonical reward-window data contract is documented and tested before any
   consensus rule validates multi-leaf fairness from sharechain data. The test
   must show two validators with different relay histories either derive the
   same payout leaves from block-provided context or reject the block for a
   deterministic, data-availability-specific reason.

- On an activated regtest chain: mine a block, wait 100 blocks for maturity,
   construct and broadcast a valid claim transaction, verify it is accepted
   and the successor UTXO appears.

## Verification

Pre-implementation (verify existing code compiles and tests pass):

```bash
cmake --build build -j$(nproc)
build/bin/test_bitcoin --run_test=sharepool_commitment_tests
build/bin/test_bitcoin --run_test=miner_tests
python3 contrib/sharepool/settlement_model.py --self-test
```

Post-Unit-A:

```bash
build/bin/test_bitcoin --run_test=sharepool_claim_tests
```

Post-Unit-B:

```bash
build/bin/test_bitcoin --run_test=validation_sharepool_tests
```

Post-Unit-C:

```bash
build/bin/test_bitcoin --run_test=miner_tests
```

Full validation after all units:

```bash
cmake --build build -j$(nproc)
build/bin/test_bitcoin --run_test=sharepool_commitment_tests
build/bin/test_bitcoin --run_test=sharepool_claim_tests
build/bin/test_bitcoin --run_test=validation_sharepool_tests
build/bin/test_bitcoin --run_test=miner_tests
python3 contrib/sharepool/settlement_model.py --self-test
```

## Open Questions

1. **SCRIPT_VERIFY_SHAREPOOL flag bit**: The inspected checkout puts script
   verification flags in `src/script/interpreter.h` and currently uses bit 20
   for `SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_PUBKEYTYPE`. Use bit 21 unless
   another flag lands first.

2. **GetBlockScriptFlags threading**: The path from `ConnectBlock()` to script
   verification flag computation needs to be traced to confirm where
   `SCRIPT_VERIFY_SHAREPOOL` should be injected.

3. **Mempool claim policy**: Should mempool accept claims before activation
   height is reached (treating them as standard)? The safe default is to
   require the same activation check in `AcceptToMemoryPool()`.

4. **Batched multi-leaf claims**: The spec says one-leaf-per-claim for v1. This
   is a non-goal. But the enforcement code should be structured so batched
   claims can be added later without refactoring.

5. **SharechainStore locking**: Public `SharechainStore` methods currently lock
   internally and are annotated `EXCLUSIVE_LOCKS_REQUIRED(!m_mutex)`. The
   reward-window method should follow that pattern, and lock ordering relative
   to `cs_main` needs to be defined.

6. **Reward window work threshold**: The 7200-share target is from the spec.
   The exact conversion from "share count" to "work threshold" depends on
   share difficulty, which is not yet fully specified for this integration
   point.

7. **Claim witness re-extraction in Unit B**: Unit B needs the deserialized
   leaf data from the witness to check payout output correctness. Since Unit A
   already validated the witness, Unit B can re-deserialize it (stateless) or
   Unit A can pass through the deserialized data. The interface between the
   two needs to be designed.
