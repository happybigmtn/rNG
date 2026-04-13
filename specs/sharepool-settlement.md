# Sharepool Settlement State Machine

## Status

This document is the authoritative protocol specification for the sharepool
settlement and claim-accounting model that blocks `POOL-07` in
`IMPLEMENTATION_PLAN.md`.

`specs/sharepool.md` remains the top-level sharepool protocol spec. This file
defines the missing piece that `specs/sharepool.md` intentionally left open: how
one compact pooled-reward output can support many trustless claims without
reintroducing a pool operator or violating UTXO single-spend rules.

## Problem Statement

RNG's sharepool design wants one compact on-chain reward commitment per block,
not one coinbase output per miner. The naive design is:

- coinbase creates one witness-v2 output whose 32-byte program is the payout
  commitment root
- each miner later proves membership under that root and claims its amount

That naive design is incomplete because a UTXO can only be spent once. The
first valid claimant would consume the entire shared reward output unless the
protocol defines how the remaining unclaimed balance persists on-chain.

This specification resolves that by defining the payout output as a
state-transition covenant:

- each claim spends the current settlement output
- pays exactly one committed leaf to that leaf's payout script
- creates a successor settlement output for the unclaimed remainder
- updates an on-chain claim-state root so the same leaf cannot be claimed twice

This is a consensus change. It is not expressible with current Bitcoin script
alone, and RNG must define it as a witness-v2 sharepool settlement program.

## Design Summary

Each activated block creates one settlement output whose witness-v2 program
commits to:

- the immutable payout leaf set for that block
- the current claim-status tree for those leaves
- the fixed leaf count

The initial claim-status tree marks every leaf as unclaimed.

Each claim transaction proves:

- one reward leaf exists under the immutable payout root
- the same leaf index is still unclaimed under the current claim-status root

Then consensus requires the claim transaction to:

- create one payout output paying the exact committed amount to the exact
  committed payout script
- create a successor settlement output with the same immutable payout root and a
  new claim-status root where that one leaf is marked claimed
- reduce the settlement output value by exactly the claimed amount

When the final unclaimed leaf is claimed, the successor settlement output is
omitted and the settlement is exhausted.

This produces a trustless, operator-free, many-claim state machine while still
keeping one compact settlement output per block.

## Terms

Settlement descriptor:
- immutable metadata for one pooled reward block:
  - `version`
  - `payout_root`
  - `leaf_count`

Claim-status root:
- Merkle root over `leaf_count` boolean claim flags in payout-leaf order
- `0` means unclaimed
- `1` means claimed

Settlement state:
- the pair `(settlement_descriptor, claim_status_root)`
- committed by the witness-v2 program hash

Settlement output:
- the spendable witness-v2 coinbase output that holds the remaining unclaimed
  reward for one block

Successor settlement output:
- the new settlement output created by a valid claim spend when claims remain

Payout leaf:
- one deterministic reward assignment for one payout script:
  - `payout_script`
  - `amount_roshi`
  - `first_share_id`
  - `last_share_id`

## Consensus Invariants

After `DEPLOYMENT_SHAREPOOL` activation, the following invariants must hold for
every settlement output and claim transaction:

1. One block creates at most one settlement output.
2. The settlement output value equals the full block reward allocated to the
   sharepool for that block.
3. The settlement descriptor never changes across successor outputs.
4. The claim-status root may only change by flipping one leaf from `0` to `1`.
5. A claim transaction may only release the exact committed amount for the one
   leaf it claims.
6. The settlement input's value is conserved exactly as:
   `old_settlement_value = claimed_leaf_amount + new_settlement_value`.
7. No transaction may claim a leaf already marked `1`.
8. If `old_settlement_value == claimed_leaf_amount`, the successor settlement
   output must be absent.
9. If `old_settlement_value > claimed_leaf_amount`, the successor settlement
   output must be present.
10. Fees must be paid entirely from non-settlement inputs. The settlement input
    cannot be used as a fee reservoir.

## Settlement Descriptor and Program

### Settlement Descriptor

The immutable settlement descriptor is:

- `version`: CompactSize, initial value `1`
- `payout_root`: 32 bytes
- `leaf_count`: CompactSize, count of real payout leaves before tree padding

The descriptor hash is:

`descriptor_hash = SHA256(SHA256("RNGSharepoolDescriptor" || serialized_descriptor))`

### State Program

The witness-v2 settlement program commits to the live state:

`state_hash = SHA256(SHA256("RNGSharepoolState" || serialized_descriptor || claim_status_root))`

The settlement output script is:

- `OP_2 <32-byte state_hash>`

After activation, witness version 2 with a 32-byte program is reserved for
sharepool settlement. RNG does not support unrelated witness-v2 destinations in
the first version.

## Payout Leaves

The immutable payout leaves are the leaves already defined in
`specs/sharepool.md`:

- `payout_script`
- `amount_roshi`
- `first_share_id`
- `last_share_id`

Leaf ordering is the deterministic order from `specs/sharepool.md`:

- primary key: `Hash(payout_script)`
- tie break: raw `payout_script` bytes

The payout Merkle tree is the immutable binary tree over those ordered leaves.
Its root is `payout_root`.

### Leaf Count and Padding

`leaf_count` is the number of real payout leaves.

For claim-status proofs, the tree uses:

- `status_tree_size = next_power_of_two(max(1, leaf_count))`

Leaves from `leaf_count` up to `status_tree_size - 1` are virtual padding
leaves permanently fixed to `claimed = 1` so they cannot be claimed and do not
introduce trailing ambiguity.

## Claim-Status Tree

The claim-status tree is a binary Merkle tree aligned by payout-leaf index.

For real payout leaf index `i`, the status leaf hash is:

`status_leaf_hash(i, claimed_flag) = SHA256(SHA256("RNGSharepoolClaimFlag" || CompactSize(i) || byte(claimed_flag)))`

Where:

- `claimed_flag = 0x00` means unclaimed
- `claimed_flag = 0x01` means claimed

Initial state for a newly mined settlement output:

- every real payout leaf is `claimed = 0`
- every padding leaf is `claimed = 1`

`claim_status_root` is the binary Merkle root over those status leaves using the
same duplicate-last-hash rule already used for Bitcoin-style Merkle trees.

## Coinbase Creation Rules

When sharepool is active for a block:

1. Build the payout leaves from the active reward window.
2. Compute `payout_root`.
3. Compute the initial `claim_status_root` with all real leaves unclaimed.
4. Compute `state_hash`.
5. Create one settlement output:
   - script: `OP_2 <state_hash>`
   - value: full block reward committed to the sharepool

The optional `RNGS` OP_RETURN discovery marker may remain, but it is metadata
only. The settlement output is the only spendable pooled-reward funding output.

## Claim Transaction Shape

The first-version claim path is intentionally narrow.

Required shape:

- exactly one settlement input
- the settlement input must be input index `0`
- one mandatory payout output at output index `0`
- zero or one successor settlement output at output index `1`
- optional additional non-settlement inputs may appear after input `0`
- optional change outputs funded by non-settlement inputs may appear after the
  mandatory outputs

The mandatory payout output must be:

- scriptPubKey == `leaf.payout_script`
- nValue == `leaf.amount_roshi`

No alternate payout destination is allowed in v1. This removes the need for an
inner payout-script signature and makes claims safely permissionless: anyone may
construct the claim transaction, but the funds can only emerge at the committed
destination.

## Claim Witness

The settlement input witness stack is exactly five elements:

1. `settlement_descriptor`
2. `leaf_index`
3. `leaf_data`
4. `payout_branch`
5. `status_branch`

Where:

- `settlement_descriptor` is the serialized immutable descriptor
- `leaf_index` is a CScriptNum-encoded non-negative index
- `leaf_data` is the serialized payout leaf
- `payout_branch` is the concatenated 32-byte sibling hashes needed to prove
  `leaf_data` under `payout_root`
- `status_branch` is the concatenated 32-byte sibling hashes needed to prove
  the status leaf under `claim_status_root`

There is no separate signature in v1 because the payout output is
consensus-constrained to the committed payout script.

## Claim Verification Rules

Validation of a settlement claim occurs partly in witness-program verification
and partly in transaction input validation.

### Witness-Program Verification

Given the settlement prevout script `OP_2 <state_hash>` and the five witness
elements above, the verifier must:

1. Deserialize `settlement_descriptor`.
2. Require `descriptor.version == 1`.
3. Deserialize `leaf_index` and require `0 <= leaf_index < leaf_count`.
4. Deserialize `leaf_data`.
5. Compute `descriptor_hash` and the current `state_hash` from:
   - `descriptor`
   - the current `claim_status_root`

The current `claim_status_root` is not separately transmitted; it is recovered
by recomputing the old status root from:

- `leaf_index`
- `status_branch`
- `claimed_flag = 0`

The verifier must then:

6. Compute `leaf_hash` from `leaf_data`.
7. Recompute the payout root from `leaf_hash`, `leaf_index`, and
   `payout_branch`; require it equals `descriptor.payout_root`.
8. Recompute the old status root from `leaf_index`, `status_branch`, and
   `claimed_flag = 0`.
9. Recompute `state_hash` from `descriptor` and the old status root; require it
   equals the 32-byte witness program in the prevout script.

If any of those checks fail, the claim is invalid.

### State Transition Verification

After reconstructing the old status root, the verifier computes:

- `new_status_root` by recomputing the same branch with `claimed_flag = 1`

Then transaction-level validation must enforce:

1. Output `0` exists and exactly equals the claimed payout leaf:
   - `scriptPubKey == leaf.payout_script`
   - `nValue == leaf.amount_roshi`
2. Let `old_value` be the settlement prevout value.
3. If `old_value > leaf.amount_roshi`:
   - output `1` must exist
   - output `1` must be `OP_2 <new_state_hash>`
   - output `1`.value must equal `old_value - leaf.amount_roshi`
4. If `old_value == leaf.amount_roshi`:
   - output `1` must not be a successor settlement output
   - no successor settlement output may appear anywhere else in the tx
5. `new_state_hash` is computed from:
   - the same immutable `settlement_descriptor`
   - `new_status_root`

The settlement input's value accounting is exact. Any non-mandatory outputs,
transaction fee, or claimant change must be fully covered by non-settlement
inputs.

### Value Conservation Rule

Let:

- `settlement_input_value` be the value of input `0`
- `settlement_output_value` be:
  - `leaf.amount_roshi` if there is no successor settlement output
  - `leaf.amount_roshi + successor_output_value` otherwise

Consensus must require:

- `settlement_output_value == settlement_input_value`

This prevents the settlement pool from silently paying claim fees or unrelated
outputs.

## Maturity and Ordering

The settlement output is a coinbase output and inherits normal coinbase
maturity. A claim spend is invalid until the funding block reaches
`COINBASE_MATURITY`.

Multiple claims against the same settlement are serialized by the UTXO set:

- only one transaction may spend the current settlement output
- every valid claim creates the unique next settlement output state

This removes double-claim races without any trusted sequencer.

## Wallet and Miner Consequences

This model changes later implementation work in two useful ways:

1. `POOL-08` wallet auto-claim no longer needs to build a separate inner
   payout-script signature for v1. It only needs to:
   - find the local leaf
   - build the payout proof
   - spend the current settlement output
   - optionally add its own fee-paying input

2. Third-party claim helpers are safe. A helper may build and broadcast a claim
   transaction, but the funds still go to the committed payout script.

## Non-Goals for V1

The first version does not attempt:

- batched claims that flip multiple status bits at once
- alternate payout destinations during claim
- in-protocol fee reimbursement for claim helpers
- non-sharepool uses of witness v2 32-byte programs

These can be layered later if the base state machine proves sound.

## Remaining Open Questions

The core UTXO accounting problem is resolved by this spec. Remaining questions
are narrower and should not block `POOL-07A`:

- Should v1 permit batched multi-leaf claims, or keep one-leaf-per-claim for
  simpler consensus code?
- Should the optional `RNGS` OP_RETURN marker stay once RPC and wallet discovery
  are mature?
- Should claim-helper incentives be added later, or is wallet self-claim
  sufficient?

## Verification

Specification-only verification:

```bash
test -f specs/sharepool-settlement.md
```

Implementation-stage verification must add deterministic test vectors for:

- initial state-hash construction
- one claim transition
- final claim transition
- duplicate-claim rejection
- settlement-value conservation with extra non-settlement inputs
