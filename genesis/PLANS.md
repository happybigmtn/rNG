# Genesis Corpus Plan Index

This file indexes the generated plan set under `genesis/plans/`. The repo-root `PLANS.md` remains the governing ExecPlan standard for document structure and maintenance.

## Relationship To Repo-Root Planning Docs

The inspected working tree already contains multiple root planning artifacts:

- `PLANS.md` — the ExecPlan authoring standard
- `docs/rng-protocol-native-pooled-mining-execplan.md` — an existing pooled-mining proposal
- `EXECPLAN.md` — a separate local operational plan
- `IMPLEMENTATION_PLAN.md` — a stale legacy plan

No inspected repo-root instruction file designated `genesis/` as the sole active control plane. This `genesis/` corpus is therefore a generated supplemental decomposition and review layer. Before coding starts, one root-owned ExecPlan should be chosen as the active implementation surface and updated with any material kept from this corpus.

## Plan Inventory

| Plan | Title | Role |
|---|---|---|
| 001 | Master Plan: Protocol-Native Pooled Mining | Corpus master plan |
| 002 | Sharepool Protocol Spec and Economic Simulator | Spec + simulator |
| 003 | Decision Gate: Simulator Results and Protocol Constants | Checkpoint |
| 004 | Sharepool Version-Bits Deployment Skeleton | Activation boundary |
| 005 | Sharechain Data Model, Storage, and P2P Relay | Core implementation |
| 006 | Decision Gate: Share Relay Viability on Small Network | Checkpoint |
| 007 | Compact Payout Commitment and Claim Program | Core implementation |
| 008 | Internal Miner, External Miner, and Wallet Claim Integration | Product integration |
| 009 | Regtest End-to-End Proof | End-to-end proof |
| 010 | Decision Gate: Regtest Proof Review Before Devnet | Checkpoint |
| 011 | Devnet Deployment, Observability, and Adversarial Testing | Multi-node validation |
| 012 | Mainnet Activation Preparation | Operator prep |

## Sequencing Rationale

The sequence remains sensible once grounded in the actual repo:

1. Lock the protocol contract and constants first with a spec-plus-simulator pass.
2. Stop for an explicit decision gate before any consensus coding.
3. Add the activation boundary before adding share data and relay.
4. Stop again after relay work because a 4-10 node network is a real design constraint for RNG.
5. Add payout commitment, claims, miner/wallet integration, and then prove the flow end to end on regtest.
6. Stop again before devnet.
7. Only after devnet results exist should mainnet preparation be written.

This preserves explicit checkpoints after risky clusters at 003, 006, and 010.

## Control-Plane Rule

Treat `genesis/` as an index and review corpus, not a substitute for repo-root planning ownership. If implementation starts from these files, first reconcile them back into the chosen root ExecPlan so the repository does not run two competing planning surfaces in parallel.
