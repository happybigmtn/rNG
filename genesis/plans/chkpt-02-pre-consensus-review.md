# CHKPT-02: Pre-Consensus Implementation Review

Date: 2026-04-13
Evaluator: Codex
Overall recommendation: GO for `POOL-04`

## Scope

This checkpoint is a read-only implementation review before sharepool consensus
code begins. The authoritative protocol source is `specs/sharepool.md`.
`specs/120426-sharepool-protocol.md` is historical context where it still
describes rejected constants or stale implementation facts.

No consensus, script, miner, wallet, RPC, P2P, or sharechain code was changed by
this checkpoint.

## Checklist

### Spec Constants

Status: GREEN

`specs/sharepool.md` confirms the constants needed by the next implementation
gates:

- Target share spacing: 1 second.
- Mainnet share target ratio: 120 at 120-second block spacing.
- Reward-window work: 7200 target-spacing shares.
- Max orphan shares: 64.
- Claim witness version: 2.
- Optional commitment tag: `RNGS`.
- BIP9 period: 2016 blocks.
- BIP9 threshold: 1916 of 2016 for 95%.

Historical conflicts are understood and not authoritative: older text that says
`block_target / 12`, `witness v2 OP_RETURN`, or `1815 (95%)` is superseded by
the active spec.

### BIP9 Deployment Slot

Status: GREEN

Live `src/consensus/params.h` defines only `DEPLOYMENT_TESTDUMMY` and
`DEPLOYMENT_TAPROOT` in `Consensus::DeploymentPos`, followed by
`MAX_VERSION_BITS_DEPLOYMENTS`. Live `src/deploymentinfo.cpp` has matching
deployment-info entries only for `testdummy` and `taproot`.

Live `src/kernel/chainparams.cpp` assigns only bit 28 for `DEPLOYMENT_TESTDUMMY`
and bit 2 for `DEPLOYMENT_TAPROOT` across all network parameter classes. The
planned `POOL-04` bit 3 assignment has no live-code conflict.

Result: there is no enum slot, deployment-info, or version-bit conflict for
adding `DEPLOYMENT_SHAREPOOL` in `POOL-04`.

### Claim Witness Version

Status: GREEN

Live `src/script/interpreter.cpp` dispatches witness v0 and witness v1 Taproot
explicitly in `VerifyWitnessProgram()`. Other witness versions fall through to
the future-soft-fork compatibility path and return success unless policy flags
discourage upgradable witness programs.

Live script classification treats unknown witness programs as
`TxoutType::WITNESS_UNKNOWN`, and existing tests cover witness version 2 as an
unknown witness destination. No live code assigns witness version 2 to another
feature.

Result: witness version 2 is available for the sharepool claim program.

Non-blocking POOL-07 condition: standard mempool policy includes
`SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM`, so POOL-07 must add the
explicit witness-v2 claim branch and standardness handling described in
`genesis/plans/007-payout-commitment-and-claim-program.md`. This is not a
`POOL-04` blocker because activation wiring does not spend witness-v2 claims.

### Internal Miner Extension Path

Status: GREEN

The internal miner path is clear:

- `src/node/internal_miner.h`: extend `MiningContext` with sharepool context
  such as share target and parent share tip, add an `m_shares_found` statistic,
  and add the minimal worker-to-coordinator handoff needed for accepted shares.
- `src/node/internal_miner.cpp`: `CreateTemplate()` already builds the block
  template, records `ctx->nBits`, `ctx->height`, and the RandomX seed. The share
  target and sharechain context can be populated after the existing block-target
  context is set.
- `src/node/internal_miner.cpp`: `WorkerThread()` has a single per-hash loop.
  The insertion point for dual-target mining is immediately after
  `pow_hash = mining_vm.Hash(header_buf)` and before or adjacent to the current
  block-target `CheckProofOfWork()` branch. A block-finding hash must also be
  recorded as a share.
- `src/node/internal_miner.cpp`: `CoordinatorThread()` already refreshes and
  publishes templates. It is the natural owner for draining worker share events,
  submitting them to the sharechain store, and triggering relay without adding
  locks inside the hot hashing loop.

`src/node/miner.cpp` remains the future commitment insertion surface for
`POOL-07`, not for this checkpoint. Its `BlockAssembler::CreateNewBlock()`
already selects QSB candidates and mempool transactions before constructing the
coinbase, then calls `GenerateCoinbaseCommitment()` after the coinbase is set.
That gives a concrete later insertion point for the payout commitment after the
final block reward value is known.

### QSB Interaction

Status: GREEN

Merged QSB code does not consume a BIP9 deployment slot or a witness version.
It is a local operator candidate path:

- `src/node/miner.cpp` inserts queued QSB candidates before ordinary mempool
  selection through `addQSBTxs()`.
- `src/node/qsb_validation.cpp` classifies QSB funding outputs by custom script
  template and QSB spends by scriptSig preimage.
- QSB spend validation explicitly rejects non-empty witness data.
- QSB admission uses the existing `allow_qsb_toy` path and does not change
  public consensus activation machinery.

Result: no blocking interaction with `DEPLOYMENT_SHAREPOOL`, witness-v2 claim
allocation, or the internal miner extension path was identified.

Future sharepool commitment code must preserve QSB candidate inclusion in block
templates by computing the final reward and commitment after QSB and mempool
transaction selection, which matches the current `BlockAssembler::CreateNewBlock()`
shape.

## Decision

`CHKPT-02` passes. Proceed to `POOL-04` with these guardrails:

- Add `DEPLOYMENT_SHAREPOOL` after `DEPLOYMENT_TAPROOT`.
- Add the matching `"sharepool"` deployment-info entry.
- Use version bit 3 unless live code changes before `POOL-04`.
- Keep production networks `NEVER_ACTIVE`.
- Do not add share relay, payout, claim, wallet, or miner behavior in `POOL-04`.
