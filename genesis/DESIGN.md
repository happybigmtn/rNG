# RNG Design Document

## Design Pass Applicability

RNG has no meaningful GUI, website, or block explorer in this repository. The design review for this corpus is therefore not a visual design pass. It is an information-architecture pass over CLI, RPC, wallet, operator, and documentation surfaces.

## Current User-Facing Surfaces

- CLI binaries: `rngd`, `rng-cli`
- Operator scripts: `rng-start-miner`, `rng-doctor`, bootstrap and public-node helpers
- Config file: `~/.rng/rng.conf`
- RPC responses for mining, wallet, and node health
- Log output and systemd/service behavior

## Current Core Journeys

### New miner

1. Install or build RNG.
2. Bootstrap the chain state if desired.
3. Start mining with the internal miner.
4. Check health with `rng-doctor` and mining RPCs.

### Public node operator

1. Install the daemon and helper scripts.
2. Apply public-node configuration.
3. Open the P2P port and verify sync/mining state.

## Design Implications Of Proposed Pooled Mining

If pooled mining proceeds, the biggest design change is not visual. It is state clarity.

The user would need clear distinctions between:

- classical pre-activation mining vs pooled post-activation mining
- pending pooled reward vs claimable pooled reward
- share tip / share acceptance state vs block tip / block acceptance state
- wallet-visible accrual vs actually spendable balance

Those distinctions should appear consistently in:

- `getmininginfo`
- any new sharepool RPCs
- wallet balance reporting
- operator docs and runbooks

## Required State Coverage

The future pooled-mining design should explicitly cover:

- deployment inactive
- deployment active but miner has submitted no accepted shares yet
- accepted shares present and pending reward visible
- claimable reward reached maturity
- claim transaction built, pending, confirmed, or failed
- share rejection, orphan-share, and relay-failure states

## Error And Empty States

The generated corpus should preserve these requirements:

- When sharepool is inactive, current RPC output should remain unchanged unless a new field explicitly reports inactivity.
- When a miner has no accepted shares yet, the future UX should report zero or empty state clearly rather than implying broken mining.
- Claim-related errors must explain whether the problem is maturity, proof data, wallet ownership, or chain reorg.

## Accessibility And Responsive Design

Not applicable for this repo review. No web or mobile UI was inspected, so no responsive or visual accessibility rewrite was performed.

## AI-Slop Risk

The main design-quality risk in this repository is not layout. It is documentation slop:

- aspirational specs presented as complete
- local planning documents read as source truth
- generated corpus docs overstating what exists in-tree

The revised corpus addresses that by making status explicit and by separating verified behavior from proposed design.
