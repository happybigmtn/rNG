# Settlement Design Review Checkpoint

## Status

Date: 2026-04-13

Verdict: implementation-ready.

This checkpoint closes the settlement-design ambiguity that previously blocked
`POOL-07`. The protocol now has:

- an explicit settlement state machine in `specs/sharepool-settlement.md`
- a deterministic reference model in `contrib/sharepool/settlement_model.py`
- checked-in transition vectors in
  `contrib/sharepool/reports/pool-07b-settlement-vectors.json`

The remaining work for `POOL-07` is code, not protocol definition.

## Core Decision

RNG should implement sharepool settlement as a witness-v2 state-transition
program, not as a one-shot Merkle membership proof against a static reward UTXO.

That means:

- one block creates one settlement output
- each valid claim spends the current settlement output
- the claim pays exactly one committed reward leaf to its committed
  `payout_script`
- unless it is the final claim, the transaction creates one successor
  settlement output for the unclaimed remainder
- the successor output commits to the same immutable payout root and a new
  claim-status root with one additional claimed bit

This resolves the UTXO single-spend problem without introducing a pool operator,
off-chain ledger, or privileged sequencer.

## Responsibility Split

The protocol must keep witness-program verification narrow and deterministic,
and move value/accounting checks into transaction/block validation.

### Witness-v2 Interpreter Responsibilities

The witness-v2 sharepool branch in `src/script/interpreter.cpp` should validate
only the things that are intrinsic to the witness program:

1. deserialize the settlement descriptor
2. validate `leaf_index`
3. deserialize `leaf_data`
4. reconstruct the immutable payout root from `leaf_data`, `leaf_index`, and
   `payout_branch`
5. reconstruct the current claim-status root from `leaf_index`, `status_branch`,
   and the required old flag `claimed = 0`
6. reconstruct the prevout state hash from:
   - settlement descriptor
   - reconstructed old claim-status root
7. require the reconstructed state hash to equal the 32-byte witness program in
   the prevout script
8. derive the next claim-status root by flipping that one leaf to
   `claimed = 1`

The interpreter should not decide transaction fee policy, settlement-output
conservation, or output ordering beyond what is needed to identify the witness
program being executed.

### Transaction / Block Validation Responsibilities

`CheckTxInputs` / `ConnectBlock` / equivalent consensus-path validation must
enforce the state transition:

1. input `0` is the settlement input
2. output `0` pays exactly:
   - `scriptPubKey == leaf.payout_script`
   - `nValue == leaf.amount_roshi`
3. if unclaimed value remains:
   - output `1` exists
   - output `1` is `OP_2 <new_state_hash>`
   - output `1`.value equals old settlement value minus claimed amount
4. if this is the final claim:
   - no successor settlement output exists
5. the settlement input's value is conserved exactly across:
   - the mandatory payout output
   - the optional successor settlement output
6. any fee or claimant change must be funded by non-settlement inputs
7. coinbase maturity still applies to the settlement output

This split is deliberate. It keeps witness verification stateless and keeps
value/accounting rules in the same consensus layer that already reasons about
inputs, outputs, maturity, and value conservation.

## Standardness / Mempool Position

Post-activation standardness should accept only the narrow v1 claim shape:

- one settlement input at index `0`
- one mandatory payout output at index `0`
- zero or one successor settlement output at index `1`
- optional extra inputs and outputs only if they do not drain settlement value

Pre-activation, witness v2 remains an upgrade placeholder under existing RNG
rules. Post-activation, witness v2 32-byte programs are reserved for sharepool
settlement only.

This means:

- pre-activation compatibility remains soft-fork safe
- post-activation nodes can treat sharepool witness-v2 spends as a dedicated
  standard type

## Wallet / Claim Helper Consequences

The chosen v1 design intentionally does not require an inner claimant signature.

That is correct because the protocol constrains the payout destination itself:

- the claim output must pay the exact committed `payout_script`

So anyone may assemble and broadcast a valid claim transaction, but the funds
still emerge only at the script already committed in the payout leaf.

This is acceptable for v1 and simplifies:

- wallet auto-claim
- deterministic vectors
- consensus code in the witness-v2 branch

It also means claim helpers do not need delegated signing authority.

## Explicit V1 Non-Goals

The following are intentionally deferred:

- batched multi-leaf claims
- alternate payout destinations during claim
- in-protocol claim-helper fees
- general-purpose non-sharepool witness-v2 programs

Keeping v1 narrow is the right tradeoff for a consensus feature of this risk
profile.

## Ready Signal

`POOL-07` is now ready to begin implementation against:

- `specs/sharepool.md`
- `specs/sharepool-settlement.md`
- `contrib/sharepool/reports/pool-07b-settlement-vectors.json`

The next implementation slice should build:

- `src/consensus/sharepool.{h,cpp}`
- coinbase settlement-output insertion in `src/node/miner.cpp`
- witness-v2 settlement verification in `src/script/interpreter.cpp`
- settlement transition and conservation checks in `src/validation.cpp`
