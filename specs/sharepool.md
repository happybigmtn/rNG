# Sharepool Protocol

## Status

This document is the active protocol specification for RNG's planned
protocol-native pooled mining work. It is a specification artifact only. No
sharepool consensus, P2P, miner, wallet, or RPC code exists in the live tree
yet. The offline simulator exists in `contrib/sharepool/` and is the current
economic proof surface for this protocol.

POOL-03 decision (2026-04-13): no-go on the original candidate constants. The
simulator reported 25.10% CV for a 10% miner over 100 blocks, above the 10%
decision threshold. The failed 10-second target share spacing with 720
target-spacing shares is a rejected baseline, not the consensus set.

POOL-02R simulator sweep (2026-04-13): committed evidence in
`contrib/sharepool/reports/pool-02r-revised-sweep.json` shows the 1-second
primary candidate passes the required 10% miner / 100-block variance threshold
for seed `42` and seeds `1..20` (max CV 8.06%), while the 2-second secondary
candidate fails the stress check (max CV 10.33%) and the 10-second baseline
remains rejected (seed `42` CV 25.10%). The withholding metric remains 0.00%
advantage, below the 5% threshold.

POOL-03R decision (2026-04-13): go on the revised simulator evidence. The
1-second mainnet candidate is confirmed for the next implementation gates:
target share spacing 1 second, share target ratio 120 at 120-second mainnet
block spacing, reward-window work 7200 target-spacing shares, max orphan shares
64, claim witness version 2, optional commitment tag `RNGS`, BIP9 period 2016,
and BIP9 threshold 1916 for 95%. This confirmation authorizes the pre-consensus
checkpoint and deployment-boundary implementation. Relay cost and orphan
behavior remain explicit POOL-06-GATE measurements before payout and claim code
can proceed.

Current live-code facts:

- `src/consensus/params.h` defines only `DEPLOYMENT_TESTDUMMY` and
  `DEPLOYMENT_TAPROOT`; there is no `DEPLOYMENT_SHAREPOOL`.
- `src/rpc/` does not implement `submitshare`, `getsharechaininfo`, or
  `getrewardcommitment`.
- `src/protocol.h` and `src/net_processing.cpp` do not define or process
  `shareinv`, `getshare`, or `share`.
- `src/node/internal_miner.{h,cpp}` mines classical blocks only.
- SegWit witness v0 and Taproot witness v1 are active; witness versions 2
  through 16 remain unassigned in the current code.
- Coinbase maturity remains the existing 100-block rule.
- `contrib/sharepool/simulate.py` implements the offline reward-window,
  payout-leaf, commitment-root, withholding, and 10% miner variance metrics.

The constants in this document are confirmed protocol inputs for subsequent
sharepool implementation tasks. They are not live consensus behavior until
future code lands and the deployment activates.

## Goals

After activation, RNG should replace the legacy "block finder receives the
entire block reward" contract with a deterministic pooled reward contract.
Recent publicly relayed shares define the reward split, each block commits to
that split, and miners later claim their committed amount after coinbase
maturity.

The protocol goals are:

- Shares are public, replayable RandomX work proofs.
- Any upgraded node with the same accepted share history computes the same
  reward window and payout commitment.
- Payout commitments remain compact and do not add one coinbase output per
  miner.
- Miners see pending pooled reward as soon as their accepted shares enter the
  reward window, but on-chain claim spends remain constrained by coinbase
  maturity.
- Pre-activation behavior remains classical RNG mining.

## Confirmed Constants

Constants in this section are `[CONFIRMED]` by the POOL-03R decision gate unless
they are explicitly marked as rejected historical candidates. The old 10-second
/ 720-share candidate and the measured 2-second / 3600-share candidate remain
only as rejected baselines.

| Name | Candidate value | Notes |
|------|-----------------|-------|
| Target share spacing | `[CONFIRMED]` 1 second | Mainnet has 120-second blocks. POOL-02R found that 1-second shares pass the required variance sweep. The 2-second candidate fails required stress seeds and the 10-second candidate remains rejected. A 60-second test chain may use a different ratio, but it must be reported separately. |
| Share target ratio | `[CONFIRMED]` `share_target = min(powLimit, block_target * (block_spacing / share_spacing))` | RNG accepts hashes `<= target`, so an easier share target must be larger than the block target. With 1-second shares and 120-second mainnet blocks, the confirmed mainnet ratio is 120. The older sketch saying `block_target / 12` is reversed for Bitcoin-style target arithmetic. |
| Reward window work | `[CONFIRMED]` 7200 target-spacing shares at 1-second spacing | The window is work-based, not count-based. POOL-02R found that 7200 at 1-second spacing is the only listed candidate that passes every required variance seed. This keeps roughly the same 60-block smoothing horizon as the rejected 10-second / 720-share baseline; at 120-second mainnet blocks that horizon is about two hours, not one hour. |
| Max orphan shares | `[CONFIRMED]` 64 | In-memory relay buffer for shares whose parents are not yet known. POOL-06-GATE still measures live orphan rate before payout/claim work proceeds. |
| Claim witness version | `[CONFIRMED]` witness version 2 | The next unassigned witness version after Taproot. |
| Commitment tag | `[CONFIRMED OPTIONAL]` `RNGS` | Used only if an auxiliary OP_RETURN discovery marker is kept. |
| BIP9 period | `[CONFIRMED]` 2016 blocks | Follows the current version-bits deployment shape. |
| BIP9 threshold | `[CONFIRMED]` 1916 of 2016 for a 95% threshold | Live chainparams comments use 1815 of 2016 as 90%. Do not copy the older `1815 (95%)` text. |

## Share Object

A share is a lower-difficulty RandomX proof built against a candidate block
header. It is valid work if the RandomX hash of the serialized 80-byte
candidate header is less than or equal to the share target.

Canonical share records must include enough data to verify the RandomX preimage.
The older planning phrase `candidate_header_hash` is not enough on its own.

Proposed `ShareRecord` fields:

- `version`: share serialization version, initially `1`.
- `parent_share`: 32-byte id of the previous accepted share, or null for the
  first share in a sharechain segment.
- `prev_block_hash`: 32-byte block tip hash the miner built on.
- `candidate_header`: serialized 80-byte block header candidate.
- `share_nBits`: compact target for the share proof.
- `payout_script`: serialized scriptPubKey that identifies the miner's payout
  destination.

Derived fields:

- `candidate_header_hash = Hash(candidate_header)`.
- `share_id = Hash(serialized ShareRecord)`.
- `share_work = GetBlockProof(share_nBits)` using the same target-to-work style
  as the block proof calculation.

Validity rules:

- The candidate header's `hashPrevBlock` must match `prev_block_hash`.
- `share_nBits` must decode to a positive target no easier than `powLimit`.
- The share target must be easier than or equal to the current block target and
  hard enough to satisfy the configured share ratio.
- `RandomXHash(candidate_header) <= share_target`.
- If `RandomXHash(candidate_header) <= block_target`, the share also represents
  a block-finding event and must count in the reward window.
- `payout_script` must be a supported claim destination for the initial claim
  verifier. The first implementation should prefer native SegWit v0 keyhash and
  Taproot v1 destinations unless POOL-07 proves broader script support.

## Sharechain Rules

The sharechain is an ordered graph of accepted shares linked by `parent_share`.
It is separate from the blockchain, but each share is anchored to a block tip by
`prev_block_hash`.

Tip selection:

- A node's best share tip is the known share with the highest cumulative
  `share_work` along its parent chain.
- If cumulative work ties, select the lower `share_id` bytewise so all nodes
  have a deterministic tie break.
- Shares whose `prev_block_hash` is not on the active block chain are not
  eligible for the current block's reward window. They may remain stored for
  reorg handling.

Orphan handling:

- A share whose parent is unknown is an orphan.
- Nodes buffer up to `[CONFIRMED]` 64 orphan shares.
- When the buffer is full, evict the oldest orphan by arrival time.
- Receiving a missing parent should trigger orphan resolution before tip
  selection is recomputed.

Reorg handling:

- The share store is not blindly deleted on block reorg.
- Reward-window reconstruction must filter shares by the active block chain.
- A block reorg can therefore change which accepted shares are eligible for a
  given block's payout commitment.

## Reward Window

For a block at height `H`, the reward window is the trailing set of eligible
shares ending at the share tip selected for that block.

Window construction:

1. Start from the block-producing share if available; otherwise start from the
   current best eligible share tip chosen by the block assembler.
2. Walk backward through `parent_share`.
3. Accumulate `share_work`.
4. Stop when accumulated work reaches the configured reward-window work
   threshold or the sharechain segment is exhausted.
5. Return shares ordered oldest to newest for deterministic replay.

The confirmed threshold is `[CONFIRMED]` enough work for 7200 target-spacing
shares at 1-second spacing. The measured 2-second / 3600-share candidate and
the 10-second / 720-share baseline are rejected for consensus constants because
they fail the POOL-03R or POOL-03 variance gates.

Reward calculation:

- `total_reward = block_subsidy(height) + transaction_fees`.
- Aggregate share work by `payout_script`.
- For each payout script:
  `amount_roshi = floor(total_reward * script_work / window_work)`.
- Distribute any remainder roshi deterministically by ascending
  `Hash(payout_script)` until the amounts sum exactly to `total_reward`.
- A solo miner is the degenerate case where the window contains one payout
  script and that script receives the full reward.

Pending pooled reward means deterministic accrued entitlement under this
formula. Claimable pooled reward means the same entitlement after the commitment
output has matured under the 100-block coinbase maturity rule.

## Payout Commitment

A reward leaf represents one payout script's share of one block reward.

Leaf fields:

- `payout_script`: serialized scriptPubKey.
- `amount_roshi`: int64 amount assigned to this payout script.
- `first_share_id`: oldest share for this payout script in the reward window.
- `last_share_id`: newest share for this payout script in the reward window.

Leaf serialization for hashing:

1. CompactSize length of `payout_script`.
2. Raw `payout_script` bytes.
3. `amount_roshi` as signed 64-bit little endian.
4. `first_share_id` as 32 bytes.
5. `last_share_id` as 32 bytes.

Leaf hash:

- `leaf_hash = Hash("RNGSharepoolLeaf" || serialized_leaf)`.

Merkle tree:

- Sort leaves by `Hash(payout_script)`, with raw `payout_script` bytes as the
  tie break.
- Build a binary Merkle tree using double-SHA256 in the same style as Bitcoin's
  block Merkle tree.
- If a level has an odd number of hashes, duplicate the final hash.
- The resulting 32-byte root is the payout commitment root.

Coinbase encoding:

- The active consensus proposal is a payout commitment output with
  `scriptPubKey = OP_2 <32-byte root>` and `nValue = total_reward`.
- The legacy finder output must not also receive `total_reward` after activation.
  A zero-value compatibility output may be preserved if block assembly needs it.
- The existing SegWit witness commitment OP_RETURN output remains separate and
  unchanged.
- An auxiliary zero-value `OP_RETURN <"RNGS"> <root>` marker is optional
  `[CONFIRMED OPTIONAL]` discovery metadata only. It is not a funding source.

Truthfulness note: older planning text described a "witness v2 OP_RETURN"
commitment. An output cannot be both an OP_RETURN output and a spendable witness
v2 program. This document treats the witness v2 output as the proposed funding
and claim surface, while an OP_RETURN marker is optional metadata.

## Claim Program

The proposed claim surface uses witness version 2 with a 32-byte program equal
to the payout commitment root.

Claim output:

- Script form: `OP_2 <32-byte payout_commitment_root>`.
- Output value: the total reward committed for that block.
- Maturity: claim spends are invalid until the coinbase output is mature under
  the existing 100-block coinbase maturity rule.

Claim witness stack:

1. `merkle_branch`: concatenated 32-byte sibling hashes.
2. `leaf_index`: CScriptNum-encoded leaf index.
3. `leaf_data`: serialized reward leaf.
4. `signature`: signature proving control of `leaf_data.payout_script`.

Verifier responsibilities:

- Deserialize `leaf_data`.
- Hash the leaf and reconstruct the Merkle root from `merkle_branch` and
  `leaf_index`.
- Require the reconstructed root to equal the witness program.
- Require the claimed output amount to match `leaf_data.amount_roshi`.
- Verify the signature against the payout script.

Open implementation constraint:

- A single shared reward UTXO can normally be spent only once. POOL-07 must
  prove the final claim accounting model before consensus code lands. Acceptable
  answers may include a carefully specified residual-output covenant model or
  explicit per-leaf claim-state accounting. This document intentionally does not
  pretend that an OP_RETURN alone can fund trustless claims.

## Activation Semantics

Sharepool rules are gated by a future BIP9 deployment named
`DEPLOYMENT_SHAREPOOL`.

Pre-activation:

- Blocks keep classical RNG coinbase reward semantics.
- Witness v2 outputs remain unknown witness programs under existing script
  rules.
- Sharepool P2P messages and RPCs should either be unavailable or return
  inactive-state errors.

Post-activation:

- Blocks must contain a valid payout commitment for the active reward window.
- Blocks with missing, mismatched, or wrong-value payout commitments are invalid.
- The internal miner must construct and relay shares.
- Wallets may track pending and claimable pooled reward.

Network parameters:

- Mainnet must remain `NEVER_ACTIVE` until simulator, regtest, and devnet gates
  pass.
- Regtest may be activatable with
  `-vbparams=sharepool:0:9999999999:0` after `DEPLOYMENT_SHAREPOOL` exists in
  code.
- Any mainnet activation threshold must use mathematically correct BIP9
  parameters. If the target is 95% over 2016 blocks, the confirmed threshold is
  `[CONFIRMED]` 1916, not 1815.

## P2P Relay

Share relay is peer-to-peer and not operator-gated.

New message types:

- `shareinv`: announces one or more share ids.
- `getshare`: requests one or more share records by id.
- `share`: delivers one or more serialized share records.

Relay rules:

- Nodes must validate share proof of work before accepting or forwarding a
  share.
- Nodes should request unknown parents when receiving an orphan share.
- Nodes should not relay shares before `DEPLOYMENT_SHAREPOOL` is active unless
  a future test-mode rule explicitly allows pre-activation relay on regtest.
- Invalid shares are rejected and should be discouraged the same way other
  malformed P2P objects are discouraged.

Payload limits:

- `shareinv` and `getshare` payloads should be bounded to avoid large-memory
  messages.
- `share` payloads must be bounded by serialized share size and batch count.
- The initial relay target is less than 10 KB/s per node at the proposed share
  rate; POOL-06-GATE must measure this on a multi-node regtest network.

## RPC, Miner, And Wallet Surface

The planned RPCs are:

- `submitshare <hex>`: validate, store, and relay one serialized share.
- `getsharechaininfo`: return best share tip, height, orphan count, and reward
  window size.
- `getrewardcommitment <blockhash>`: return commitment root, leaves, and
  amounts for an activated block.

Existing mining and wallet surfaces should be extended rather than replaced:

- `getmininginfo` may add `sharepool_active`, `share_tip`,
  `pending_pooled_reward`, and `accepted_shares`.
- `getinternalmininginfo` may add an internal `shares_found` counter.
- `getbalances` may add `pooled.pending` and `pooled.claimable`.
- `-mineaddress` remains the miner's payout destination; after activation it is
  the payout script used in produced shares.

## Simulator Gate

POOL-02R tested the revised candidate family against this spec. POOL-03R
confirmed the 1-second candidate before any consensus code was written.

The simulator must prove or reject:

- A 90/10 work split produces proportional reward leaves within the acceptance
  tolerance.
- Deterministic replay produces byte-identical commitment roots.
- Reorged share suffixes only affect outputs inside the changed window.
- Pending entitlement is visible before any block is found.
- Share withholding advantage stays below the decision threshold or forces a
  protocol revision.
- Reward variance for a 10% miner over 100 blocks stays below the decision
  threshold or forces constant revision.

Authoritative POOL-02R variance metric:

- Model a miner with 10% of total hash rate over 100 consecutive blocks.
- For each candidate, set `shares_per_block = block_spacing_seconds /
  target_share_spacing_seconds`, using the 120-second mainnet block spacing
  unless the sweep is explicitly labeled for a different network.
- Set `reward_window_work` to the candidate work threshold, with unit share work
  in the current simulator model.
- Generate candidate shares with a deterministic pseudorandom Bernoulli process
  using `miner_fraction = 0.10`.
- Pay each synthetic block at the final share generated for that block.
- Compute the miner's per-block rewards, the arithmetic mean, the population
  standard deviation with denominator `N = 100`, and
  `coefficient_of_variation_percent = 100 * stddev / mean_reward`.
- The continuity check uses seed `42` because it is the seed that failed
  POOL-03. The stress check uses seeds `1` through `20` inclusive.
- Passing requires `coefficient_of_variation_percent < 10.00` for seed `42`
  and for every seed in the required stress check.

POOL-02R reported at least:

| Candidate | `shares_per_block` | `reward_window_work` | Required status |
|-----------|--------------------|----------------------|-----------------|
| 10-second rejected baseline | 12 | 720 | Re-run only as comparison to the 25.10% POOL-03 failure. |
| 2-second secondary candidate | 60 | 3600 | Test seed `42` and a multi-seed sweep before promotion. |
| 1-second primary candidate | 120 | 7200 | Test seed `42` and a multi-seed sweep before promotion. |

The POOL-03 no-go report included a non-authoritative exploratory sweep where
the 2-second candidate passed seed `42` but had one seed above 10%, while the
1-second candidate stayed below 10% across seeds 1 through 20. POOL-02R replaced
that exploratory evidence with committed deterministic sweep output and did not
cherry-pick a seed that hides variance. Relay cost from shorter share spacing
remains a POOL-06-GATE input and cannot be ignored now that constants are
confirmed for implementation planning.

## Open Questions

- Does the claim accounting model for one compact commitment survive UTXO
  semantics without operator coordination?
- Should the first version include a finder bonus or publication incentive?
- Should 60-second test networks keep the same target share spacing as the
  revised mainnet family or the same target ratio as mainnet?
- Which payout script forms are supported by the first claim verifier?
- Should share relay be available pre-activation on regtest for testing?
- How does merged QSB policy interact with witness-v2 claim standardness?

## Verification

Specification-only verification:

```bash
grep "CONFIRMED" specs/sharepool.md | wc -l
```

The count must be at least 4 and must include the confirmed share spacing,
reward window, share target ratio, and max orphan constants.
