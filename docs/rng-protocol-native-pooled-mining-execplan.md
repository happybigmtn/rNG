# Implement Protocol-Native Default Pooled Mining In RNG

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds.

This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

The goal is to upgrade RNG from a classic Bitcoin-style “winner takes the whole block reward” chain into a protocol-native pooled mining chain. After this work, RNG nodes will no longer treat pooled mining as an off-chain operator service. Instead, each block will commit to a deterministic reward split over recent publicly relayed mining shares, so small miners can begin accumulating a visible, protocol-enforced portion of each block as soon as they contribute valid work to the live share network.

For users, the visible change is simple. A low-hashrate miner, game client, or agent can join the network, submit shares, and immediately see pending pooled reward accrual instead of waiting for a full block lottery win. For operators, the key proof is on regtest and then devnet: two miners with very different hashrates mine against the same share network, both accumulate reward entitlements from every block, and an independent node recomputes the same payout commitment from public share history without consulting any Zend-only database.

This plan uses Zend’s recent rBTC work as design input, not as the consensus implementation. Zend already proved useful patterns around replayable proof bundles, append-only witnessed share tails, remote miner onboarding, and truthful operator status. Those lessons should inform RNG’s observability and rollout, but the actual “fully decentralized, strongest-sense trustless” property must live inside RNG consensus, networking, mining, and wallet code.

## Requirements Trace

`R1`. After activation, RNG must replace the legacy “block finder receives the full coinbase reward” contract with a deterministic pooled reward contract. The replacement must be enforced by consensus, not by an operator database or external service.

`R2`. The pooled reward contract must be derived from a public share history. Any fully validating RNG node must be able to replay the same accepted share window and derive the same per-block payout commitment without Zend or any privileged coordinator.

`R3`. A miner that contributes low-difficulty shares must begin accruing a proportional share of every block immediately after those shares enter the accepted share history, even though final on-chain spends still respect coinbase maturity unless a later fork changes that rule.

`R4`. Share admission and share propagation must be peer-to-peer. The protocol must not rely on one operator-managed pool server, one admission proxy, or one relay host to decide which shares count.

`R5`. Block construction must remain miner-built. The protocol may extend `getblocktemplate` with sharepool context, but it must not reintroduce an operator-built template bottleneck.

`R6`. The consensus design must scale without emitting thousands of direct coinbase outputs per block. The block reward must therefore commit to a compact payout structure and allow deterministic post-maturity claims.

`R7`. The claim path must be trustless. A miner must be able to prove its entitlement from the payout commitment, its payout key, and the public share window, without requesting settlement from an operator.

`R8`. Before activation, existing RNG behavior remains unchanged. Existing blocks, mining RPCs, wallet flows, and tests must continue to work until the new deployment is explicitly activated.

`R9`. Activation must be staged through RNG’s existing version-bits infrastructure first on regtest, then on a dedicated devnet or signet-style test deployment, and only then on mainnet.

`R10`. Solo mining must remain possible, but it becomes a special case of pooled mining where one miner contributes essentially all of the accepted work in the active reward window.

`R11`. The plan must preserve user truthfulness. If immediate wallet spendability is still constrained by the current 100-confirmation coinbase maturity rule in `specs/consensus.md`, the implementation and docs must say so explicitly while still surfacing pending pooled accrual immediately.

## Scope Boundaries

This plan does not port Zend’s current typed-HTTP `rbtcpool` runtime into RNG. Zend’s pool runtime is useful operationally, but it is still an application-layer pool shape. The target here is a chain-level mining protocol.

This plan does not promise that Bitino, Zend, or any other application will ship polished UX in the same change set. It defines the RNG-side protocol, node, wallet, and RPC work required so those applications can later build on a real consensus surface.

This plan does not reduce RNG’s existing 100-confirmation coinbase maturity in its first version. Miners should see pending pooled reward accrual immediately after each block, but actual claim spends remain mature only after the current coinbase maturity unless a separate consensus change is planned and approved.

This plan does not preserve legacy post-activation reward semantics. Before activation, the full block reward still goes to the classical coinbase destination. After activation, that contract is intentionally replaced by the pooled reward commitment plus claim path.

This plan does not attempt anonymous or private pooled mining. Shares and payout keys are public protocol objects.

This plan does not attempt to solve every possible withholding or eclipse attack in the first patch before proving the basic mechanism. It does, however, require simulation, regtest, and devnet validation specifically to reject designs that obviously fail under realistic adversarial conditions.

## Progress

- [x] (2026-04-13 01:28Z) Read `PLANS.md` and `EXECPLAN.md` to match RNG’s ExecPlan contract and style.
- [x] (2026-04-13 01:28Z) Reviewed the RNG mining, activation, and consensus seams in `README.md`, `specs/consensus.md`, `specs/activation.md`, `src/pow.cpp`, `src/node/miner.cpp`, `src/node/internal_miner.cpp`, `src/rpc/mining.cpp`, `src/validation.cpp`, `src/protocol.h`, `src/net_processing.cpp`, and `src/kernel/chainparams.cpp`.
- [x] (2026-04-13 01:28Z) Reviewed the relevant Zend rBTC pool work and distilled the useful lessons: replayable proof bundles, append-only witnessed share tails, truthful onboarding, and the limits of multi-host coordination that is still not protocol-native.
- [x] (2026-04-13 01:28Z) Chose the core architecture for RNG: a peer-to-peer sharechain, consensus-enforced payout commitment, and post-maturity trustless claims, with activation through RNG’s existing version-bits path.
- [x] (2026-04-13 01:28Z) Wrote this initial ExecPlan in `docs/rng-protocol-native-pooled-mining-execplan.md`.
- [ ] Formalize the new protocol spec in repository-owned spec files and lock the first implementation constants after simulation.
- [ ] Implement the sharechain data model, storage, and peer relay on regtest behind a new deployment flag.
- [ ] Implement the compact reward commitment plus trustless claim program and prove end-to-end deterministic replay on regtest.
- [ ] Integrate the internal miner, template RPCs, and wallet claim tracking so low-hashrate miners can use the feature without external pool infrastructure.
- [ ] Activate the protocol on devnet, observe real share propagation and claim behavior, and only then prepare a mainnet deployment plan.

## Surprises & Discoveries

- Observation: Zend’s recent “trustless” pool work is valuable, but it still stops short of the strongest decentralized claim because its live trust model is satisfied by multiple hosts in one coordinated administrative domain.
  Evidence: Zend’s operator work centers on proof publication, sharechain publication, mirrored replay, and admission-peer witnessing. Those patterns help with observability and rollout, but they do not by themselves replace chain-level reward semantics.

- Observation: RNG already has most of the activation machinery needed for a consensus upgrade without inventing a new deployment system.
  Evidence: `src/consensus/params.h`, `src/deploymentinfo.cpp`, `src/versionbits.cpp`, `src/rpc/blockchain.cpp`, and `src/kernel/chainparams.cpp` already define and expose version-bits deployments. `specs/activation.md` explicitly says future upgrades should use BIP9 version bits.

- Observation: RNG’s current mining path is still entirely classical Bitcoin mining even though the proof of work is RandomX.
  Evidence: `src/node/miner.cpp` still constructs one classical coinbase output script, `src/node/internal_miner.cpp` mines directly against `interfaces::Mining::createNewBlock`, and `src/rpc/mining.cpp` still exposes Bitcoin-shaped mining RPCs such as `getblocktemplate`, `submitblock`, and `generatetoaddress`.

- Observation: RNG’s own agent-facing documentation already anticipates pooled mining for low-resource participants, but only as a future external pool.
  Evidence: `specs/agent-integration.md` includes a “pool” mining mode and an example `rng-cli pool-mine --pool ...` flow. That means the product intent is already present, but the implementation is currently imagined as an off-chain pool rather than a protocol-native default.

- Observation: A “pay everyone directly in the coinbase” approach is not realistic for RNG if pooled mining becomes the default mode.
  Evidence: `src/node/miner.cpp` and `specs/blocks.md` follow standard Bitcoin coinbase construction. Expanding every block into a large fanout transaction would create block-weight and UTXO-set pressure immediately. A compact commitment plus claim path is the safer design.

## Decision Log

- Decision: Implement protocol-native pooled mining inside RNG consensus instead of porting Zend’s current pool runtime.
  Rationale: The user goal is strongest-sense trustless pooled mining. Zend’s current work is useful operationally, but it remains an application-layer pool design. Consensus must own reward splitting if pooled mining is meant to be the chain’s default mode.
  Date/Author: 2026-04-13 / Codex

- Decision: Use a public peer-to-peer sharechain plus a deterministic reward window rather than an operator-maintained share ledger.
  Rationale: The sharechain makes non-winning work visible to every validating node. That is the minimum structure required to pay miners proportionally without trusting a pool operator.
  Date/Author: 2026-04-13 / Codex

- Decision: Commit each block’s pooled reward through a compact payout commitment and use post-maturity trustless claims instead of a giant multi-output coinbase.
  Rationale: Directly paying all miners in the coinbase does not scale. A compact commitment plus claim proofs preserves consensus determinism while keeping block size bounded.
  Date/Author: 2026-04-13 / Codex

- Decision: Treat immediate reward visibility and immediate spendability as different things.
  Rationale: The product intent is that small miners should start receiving rewards right away. In the first protocol version, that means they immediately accrue deterministic pending entitlements after each block, while on-chain spends still respect the current coinbase maturity rule.
  Date/Author: 2026-04-13 / Codex

- Decision: Extend the existing mining surfaces where practical instead of inventing a wholly parallel mining API.
  Rationale: RNG already has `getblocktemplate`, `getmininginfo`, `submitblock`, the internal miner, and version-bits activation surfaces. The protocol should extend those with sharepool-specific context and add only the missing share-specific calls such as `submitshare`.
  Date/Author: 2026-04-13 / Codex

- Decision: Plan for deployment through version bits first, while treating “fallback to hard fork” as an explicit contingency only if the claim-program prototype proves impossible under the chosen witness-program design.
  Rationale: RNG’s existing deployment machinery already supports BIP9-style activation and regtest overrides. Starting there minimizes rollout novelty. If the implementation spike shows the claim semantics cannot be expressed cleanly enough in that model, the plan should be revised explicitly rather than pretending the question is unresolved.
  Date/Author: 2026-04-13 / Codex

## Outcomes & Retrospective

At this stage the outcome is a research-grounded implementation plan, not working code. The main result is conceptual clarity. The right next step for RNG is not “embed Zend’s pool” and not “ship a better central pool.” The right next step is a new protocol layer: public shares, deterministic reward windows, compact payout commitments, and trustless claims.

The main remaining risk is economic and protocol complexity rather than raw coding volume. The repo already has the right starting points for proof of work, block assembly, version-bits activation, P2P relay, and wallet/RPC integration. What it lacks is the sharechain model and the reward-commitment machinery. That is why the first milestone in this plan is a spec-plus-simulator phase rather than immediate mainnet code.

## Context and Orientation

RNG is a Bitcoin Core-derived chain that replaced SHA256d mining with RandomX but otherwise kept most Bitcoin node structure. For this plan, five existing code areas matter.

First, `src/pow.cpp` and `src/pow.h` define RandomX proof of work. They currently only care about full block validity. There is no concept of a lower-difficulty accepted share.

Second, `src/node/miner.cpp` and `src/node/internal_miner.cpp` implement block assembly and the built-in miner. They currently assume that mining is a search for one winning block header whose coinbase pays the block reward to one script. That assumption is exactly what this plan changes after activation.

Third, `src/rpc/mining.cpp` is the public mining RPC surface. It already exposes the Bitcoin-derived concepts of mining info, block templates, and manual block generation on test chains. That makes it the right place to add sharepool context and any new share submission RPCs.

Fourth, `src/validation.cpp` enforces block acceptance. Any claim that pooled mining is “default” must eventually be visible here, because block reward commitments and claim-spend validation are consensus rules.

Fifth, `src/protocol.h` and `src/net_processing.cpp` are the peer-to-peer network seam. A “share” in this plan means a lower-difficulty RandomX proof that is difficult enough to count toward pooled reward accounting but may be too weak to create a block. Those shares must be serializable, relayable, storable, and replayable by ordinary RNG peers.

Three new terms appear throughout this plan:

A “share” is a publicly relayed proof of work weaker than a full block but strong enough to count toward the pooled reward window. A share has its own identifier, parent share reference, payout destination, candidate block context, and RandomX work proof.

A “sharechain” is the best known chain of accepted shares. It is analogous to the block chain, but it moves faster and exists to measure who contributed recent work, not to settle ordinary transactions directly.

A “payout commitment” is a compact cryptographic commitment to the deterministic reward split for one block. The block contains the commitment, and miners later prove that they own one committed payout leaf when they create a claim spend after maturity.

The most important lesson imported from Zend is not a code module. It is an architecture boundary. Zend proved that replayable share proofs, mirrored verification, and truthful miner onboarding are useful. RNG should reuse those ideas later for operator tooling and external auditing, but consensus must be able to stand alone without Zend. In this design, Zend becomes an optional control plane and verifier around RNG’s sharechain rather than the pool itself.

## Plan of Work

Start by writing the protocol down in RNG’s own specifications. Add a new `specs/sharepool.md` that defines the share object, sharechain tip-selection rule, reward-window rule, payout-commitment encoding, claim format, and activation semantics in plain English. Update `specs/consensus.md`, `specs/activation.md`, and `specs/agent-integration.md` so the written contract matches the intended product behavior. Add a small deterministic simulator under `contrib/` that can replay synthetic share traces and emit the exact payout commitment a block would carry. Do not start consensus coding before this simulator exists, because the simulator is the fastest way to reject bad economics and bad constants.

Once the protocol constants are settled, wire a new version-bits deployment into the node. Extend `src/consensus/params.h`, `src/deploymentinfo.cpp`, `src/kernel/chainparams.cpp`, and the related deployment reporting surfaces with a `DEPLOYMENT_SHAREPOOL` flag. Add sharepool-specific consensus parameters such as target share spacing, reward-window work, and claim-program version. Regtest must keep legacy behavior by default, and the new rules should only turn on under explicit `-vbparams` or the eventual deployed network parameters.

After the deployment skeleton exists, add the sharechain modules. Create a new `src/sharechain/` subtree that owns share serialization, share validation, cumulative-work scoring, persistent storage, and payout-window reconstruction. Extend `src/protocol.h` and `src/net_processing.cpp` so peers can announce, request, relay, and persist accepted shares. Extend `src/rpc/mining.cpp` so mining callers can learn the current share tip and submit new shares without inventing a separate operator-only pool API.

With public shares relayable, change block assembly. `src/node/miner.cpp` must stop emitting a single classical finder coinbase after activation. Instead, it should derive the current reward window from the accepted sharechain, compute a compact payout commitment, and encode that commitment into the coinbase in a consensus-defined way. At the same time, `src/validation.cpp` must start rejecting activated blocks whose pooled reward commitment does not match the accepted share window at that height.

Then implement trustless claims. Introduce a new claim output form for the pooled reward commitment, most likely by reserving a new witness-program version interpreted by RNG after activation. Claim spends must prove three things: the payout leaf exists under the committed root, the claim amount and payout destination match the leaf, and the spender controls that payout destination. Add wallet tracking so miners can see pending pooled rewards and later build claim transactions as soon as the coinbase matures.

Finally, adapt the mining and product surfaces. `src/node/internal_miner.cpp`, `src/init.cpp`, `README.md`, and the user-facing mining docs must describe mining as share submission by default after activation. Low-resource agents should no longer need an external “pool URL” to participate. They should talk to ordinary RNG nodes and join the sharechain directly.

## Implementation Units

### Unit 1: Protocol Spec And Economic Simulator

Goal: lock the share object, reward-window formula, and payout-commitment design before any irreversible consensus code is written.

Requirements advanced: `R1`, `R2`, `R3`, `R6`, `R7`, `R11`.

Dependencies: none.

Files to create or modify:

- `specs/sharepool.md` (new)
- `specs/consensus.md`
- `specs/activation.md`
- `specs/agent-integration.md`
- `README.md`
- `contrib/sharepool/simulate.py` (new)
- `contrib/sharepool/README.md` (new)

Tests to add or modify:

- `test/functional/feature_sharepool_simulator.py` (new)

Approach:

Write the protocol in repository-owned prose first. The simulator should accept a machine-readable share trace, reconstruct the reward window, and emit the exact payout leaves and commitment root that the node is expected to produce. Use this phase to settle key constants such as target share spacing, reward-window work, and the exact leaf hash format. The simulator is also where to evaluate whether a zero finder bonus is viable or whether the protocol needs an explicit, small, documented publication incentive before implementation proceeds.

Specific test scenarios:

- Given a synthetic trace where miner A contributes 90% of the accepted work and miner B contributes 10%, the simulator outputs a reward commitment whose leaf amounts match that ratio for subsidy plus fees.
- Given the same accepted-share prefix replayed twice, the simulator emits the exact same commitment root byte-for-byte.
- Given a reorged share suffix, the simulator changes only the affected window outputs and leaves the surviving prefix accounting unchanged.
- Given a trace where one miner submits shares but no full block is found yet, the simulator still reports pending entitlement accumulation for that miner.

### Unit 2: Sharepool Deployment Skeleton

Goal: add a clean activation boundary and chain parameters for the new protocol without changing pre-activation behavior.

Requirements advanced: `R8`, `R9`.

Dependencies: Unit 1.

Files to create or modify:

- `src/consensus/params.h`
- `src/deploymentinfo.cpp`
- `src/deploymentinfo.h`
- `src/kernel/chainparams.cpp`
- `src/kernel/chainparams.h`
- `src/rpc/blockchain.cpp`
- `src/versionbits.cpp`
- `specs/activation.md`

Tests to add or modify:

- `src/test/versionbits_tests.cpp`
- `test/functional/feature_sharepool_activation.py` (new)

Approach:

Add `DEPLOYMENT_SHAREPOOL` to the existing version-bits deployment list. Extend consensus parameters with the values Unit 1 locked: share target spacing, reward-window work, payout-commitment witness version, and any explicit maximum share lag tolerated at block creation. Make `getdeploymentinfo` and any human-readable activation status surfaces report the new deployment. Regtest should remain pre-activation by default and only enter sharepool mode through explicit `-vbparams`.

Specific test scenarios:

- On default regtest with no `-vbparams`, classical block generation still works and payout semantics remain unchanged.
- On regtest with `-vbparams=sharepool:0:9999999999:0`, deployment transitions to active and the node reports the sharepool activation in `getdeploymentinfo`.
- Blocks created before activation are still accepted after the code lands.

### Unit 3: Sharechain Objects, Storage, Relay, And RPC

Goal: make accepted shares first-class public protocol objects.

Requirements advanced: `R2`, `R4`, `R5`, `R10`.

Dependencies: Unit 2.

Files to create or modify:

- `src/sharechain/share.h` (new)
- `src/sharechain/share.cpp` (new)
- `src/sharechain/store.h` (new)
- `src/sharechain/store.cpp` (new)
- `src/sharechain/window.h` (new)
- `src/sharechain/window.cpp` (new)
- `src/protocol.h`
- `src/net_processing.cpp`
- `src/rpc/mining.cpp`
- `src/init.cpp`
- `src/node/interfaces.cpp`
- `src/CMakeLists.txt`

Tests to add or modify:

- `src/test/sharechain_tests.cpp` (new)
- `src/test/net_tests.cpp`
- `test/functional/feature_sharepool_relay.py` (new)

Approach:

Introduce a share record type that includes the parent share id, previous block hash, payout destination, share difficulty bits, timestamp, nonce, and candidate block context needed to validate the RandomX work proof. Persist accepted shares in a dedicated sharechain store rather than in the mempool. Add P2P relay messages for inventory, request, and announcement of shares. Extend the mining RPC surface so callers can discover the current share tip and submit a share, while extending `getblocktemplate` with a `sharepool` section instead of inventing a wholly disconnected template API.

Specific test scenarios:

- Two regtest nodes relay a valid share and agree on the same best share tip.
- A share that fails the declared share target is rejected and never enters the sharechain store.
- A share referencing an unknown parent is kept as an orphan share only until the parent arrives or the configured orphan limit is hit.
- `getblocktemplate` on an activated node includes the current share tip, current share target, and reward-window parameters.
- `submitshare` returns the share id for a valid share and a descriptive rejection for an invalid share.

### Unit 4: Payout Commitment And Claim Program

Goal: encode pooled rewards compactly in each block and make them independently claimable after maturity.

Requirements advanced: `R1`, `R2`, `R3`, `R6`, `R7`, `R11`.

Dependencies: Unit 3.

Files to create or modify:

- `src/sharechain/payout.h` (new)
- `src/sharechain/payout.cpp` (new)
- `src/script/interpreter.cpp`
- `src/script/interpreter.h`
- `src/script/standard.cpp`
- `src/script/standard.h`
- `src/node/miner.cpp`
- `src/validation.cpp`
- `src/policy/policy.cpp`
- `src/policy/policy.h`

Tests to add or modify:

- `src/test/shareclaim_tests.cpp` (new)
- `src/test/miner_tests.cpp`
- `test/functional/feature_sharepool_claims.py` (new)

Approach:

Define a compact payout commitment encoding, ideally as a dedicated new witness-program version interpreted by RNG only after activation. Change block assembly so an activated block computes the current reward window from the accepted sharechain, builds payout leaves, hashes them into a commitment root, and inserts one pooled reward output into the coinbase instead of a single winner-take-all payout. Add consensus validation so activated blocks are rejected when their pooled reward commitment does not match the local replay of the accepted share window. Add claim-spend validation so a later transaction can prove membership in that reward root and transfer the correct amount to the claimant’s payout script after coinbase maturity.

Specific test scenarios:

- Given a known share window and a mined block, the node derives the exact payout root that the simulator from Unit 1 computed.
- A block with a tampered payout root is rejected after activation.
- A valid claim transaction spending one committed payout leaf is accepted after maturity.
- A claim with an invalid Merkle branch, wrong amount, or wrong payout key is rejected.
- Pre-activation blocks and ordinary transactions still validate exactly as before.

### Unit 5: Internal Miner, External Miner, And Wallet Claim Tracking

Goal: make the new protocol usable by ordinary miners and visible in the wallet and CLI.

Requirements advanced: `R3`, `R5`, `R10`, `R11`.

Dependencies: Unit 4.

Files to create or modify:

- `src/node/internal_miner.h`
- `src/node/internal_miner.cpp`
- `src/rpc/mining.cpp`
- `src/init.cpp`
- `src/wallet/wallet.h`
- `src/wallet/wallet.cpp`
- `src/wallet/rpc/spend.cpp`
- `src/wallet/rpc/transactions.cpp`
- `README.md`
- `doc/JSON-RPC-interface.md`
- `doc/man/rngd.1`

Tests to add or modify:

- `src/wallet/test/wallet_tests.cpp`
- `test/functional/feature_sharepool_wallet.py` (new)
- `test/functional/feature_sharepool_mining.py` (new)

Approach:

Teach the internal miner to produce shares continuously after activation instead of only hunting for full blocks. Reinterpret `-mineaddress` as the payout destination for pooled claims after activation while keeping the flag name for continuity. Extend mining info RPCs with pending share counts, current share tip, and pending pooled accrual. Add wallet support to recognize payout leaves addressed to local scripts, expose those pending entitlements, and build claim transactions automatically once the pooled reward output matures.

Specific test scenarios:

- Two internal miners with different thread counts mine on activated regtest and both accumulate pending pooled rewards after the first few blocks.
- The wallet shows pending pooled rewards before maturity and claimable pooled rewards once maturity is reached.
- A wallet claim transaction built by the node spends only its own committed payout leaf and lands successfully after maturity.
- A low-resource miner participating at a much lower share rate still receives a non-zero pending balance rather than nothing.

### Unit 6: Devnet Rollout, Observability, And Operator Proof Surfaces

Goal: prove the protocol on a live non-mainnet network and preserve the best parts of Zend’s observability work.

Requirements advanced: `R2`, `R4`, `R9`, `R11`.

Dependencies: Units 1 through 5.

Files to create or modify:

- `docs/devnet-sharepool-rollout.md` (new)
- `docs/devnet-mining-summary-2026-02-27.md`
- `specs/agent-integration.md`
- `README.md`
- `src/rpc/mining.cpp`
- `src/rpc/blockchain.cpp`
- `contrib/sharepool/export_proof.py` (new)

Tests to add or modify:

- `test/functional/feature_sharepool_reorg.py` (new)
- `test/functional/feature_sharepool_devnet_smoke.py` (new)

Approach:

Bring forward the useful Zend lessons without making Zend part of consensus. Add node-native export surfaces for the current reward window, payout commitment, and recent accepted shares so external tools can replay them. Write a devnet rollout document that explains how to activate the deployment on a controlled network, how to run a few miners with very different hashrates, and how to verify reward fairness from an independent node. Only after this devnet phase succeeds should the mainnet activation plan be written.

Specific test scenarios:

- A three-node devnet with at least two miners converges on the same share tip and payout commitment.
- An independent observer node can export the accepted share window and recompute the latest payout commitment from scratch.
- A short reorg on devnet rewinds and rebuilds the share window deterministically, and the repaired payout commitment matches all upgraded nodes.
- The exported proof data is sufficient for an external verifier to reproduce the node’s answer without any private database.

## Concrete Steps

Work from the repository root for every step below.

1. Write and review the protocol spec first.

       python3 contrib/sharepool/simulate.py --scenario contrib/sharepool/examples/two-miner-basic.json

   Expected outcome: the command prints a deterministic commitment root plus per-miner leaf amounts. The exact root value becomes part of the simulator fixture for Unit 1.

2. Build the node after the deployment skeleton and sharechain code land.

       cmake -S . -B build
       cmake --build build -j"$(nproc)" --target rngd rng-cli test_bitcoin

   Expected outcome: the build completes without adding a second mining binary or a parallel pool daemon.

3. Run the focused unit and functional coverage while the deployment is still regtest-only.

       test/functional/test_runner.py feature_sharepool_activation.py feature_sharepool_relay.py feature_sharepool_claims.py feature_sharepool_wallet.py feature_sharepool_mining.py feature_sharepool_reorg.py
       build/src/test/test_bitcoin --run_test=versionbits_tests,sharechain_tests,shareclaim_tests,miner_tests,wallet_tests

   Expected outcome: pre-activation tests continue to pass, and the new sharepool tests prove deterministic share replay, payout-root validation, and claim behavior.

4. Exercise the activated protocol manually on regtest.

       build/src/rngd -regtest -daemon -vbparams=sharepool:0:9999999999:0 -mine -mineaddress=<regtest-address> -minethreads=2
       build/src/rng-cli -regtest getdeploymentinfo
       build/src/rng-cli -regtest getblocktemplate '{"rules":["segwit"]}'
       build/src/rng-cli -regtest submitshare <serialized-share>
       build/src/rng-cli -regtest getmininginfo

   Expected outcome: `getdeploymentinfo` shows the sharepool deployment active, `getblocktemplate` includes a `sharepool` section, `submitshare` returns a share id, and `getmininginfo` reports pending pooled accrual rather than only classical block-finder rewards.

5. Run the devnet smoke once regtest is stable.

       test/functional/test_runner.py feature_sharepool_devnet_smoke.py

   Expected outcome: multiple devnet nodes converge on one share tip, mine pooled blocks, and export replayable payout proof data.

## Validation and Acceptance

The implementation is accepted only when all of the following are demonstrably true.

An activated regtest network with at least two miners of unequal hashrate produces blocks whose rewards are split proportionally across the recent accepted share window instead of paying the entire reward to the block finder.

An independent upgraded node can replay the public accepted share history and compute the same payout commitment root for the latest block without consulting Zend or any operator service.

A wallet that owns one payout destination sees pending pooled rewards accumulate after the first few activated blocks, then successfully creates claim transactions after maturity using only on-chain and sharechain data.

Legacy pre-activation behavior remains unchanged. Existing non-sharepool tests continue to pass when the deployment is inactive.

The protocol is live-testable on devnet before any mainnet activation begins. No “mainnet first” rollout is acceptable here.

## Idempotence and Recovery

All early milestones should be repeatable from a clean regtest or devnet datadir. Use dedicated datadirs for activated test networks so legacy node data is never mutated accidentally. If a sharechain schema or claim-program prototype changes incompatibly during implementation, wipe only the dedicated test datadir and rerun the simulator fixtures before retesting the node.

If the version-bits deployment code lands but the sharepool logic is not ready, keep the deployment parameters inactive on every public network and rely on regtest `-vbparams` for continued development. That preserves safety while code is still moving.

If the claim-program prototype proves impossible or excessively brittle under the initial witness-program design, stop and revise this ExecPlan before continuing. Do not paper over a broken reward-claim path with operator-side settlement, because that would violate the purpose of the work.

For wallet work, always keep existing wallet backups valid. New claim-tracking metadata should be additive and reconstructible from chain data wherever possible so that a partial wallet-db failure does not destroy claimability.

## Artifacts and Notes

The core data flow intended by this plan is:

    miner constructs candidate block context
      -> miner hashes RandomX nonces until the work meets share target
      -> miner relays accepted share to peers
      -> peers extend their best sharechain tip
      -> block finder eventually produces a share that also meets block target
      -> block assembler derives reward window from accepted sharechain
      -> block coinbase commits one compact payout root
      -> after maturity, each miner claims its own committed leaf

The minimal reward leaf shape for the simulator and the node should be:

    {
      "payout_script": "<serialized scriptPubKey bytes>",
      "amount_roshi": <int64>,
      "window_start_share": "<share id>",
      "window_end_share": "<share id>"
    }

That leaf shape is intentionally verbose. It keeps enough context for replay tools and later operator surfaces, and it is still compact enough to hash into a Merkle-style payout commitment.

The most important truthfulness note to preserve in docs and UX is:

    “Pending pooled reward” means deterministic accrued entitlement derived from accepted shares.
    “Claimable pooled reward” means the same entitlement after the pooled reward output has matured under RNG’s coinbase maturity rules.

## Interfaces and Dependencies

The implementation should introduce the following stable surfaces.

In `src/consensus/params.h`, add a new deployment and the sharepool consensus parameters:

    enum DeploymentPos {
        DEPLOYMENT_TESTDUMMY,
        DEPLOYMENT_TAPROOT,
        DEPLOYMENT_SHAREPOOL,
        MAX_VERSION_BITS_DEPLOYMENTS
    };

    struct SharePoolParams {
        uint32_t target_share_spacing;
        uint32_t reward_window_work;
        uint8_t claim_witness_version;
        uint16_t max_orphan_shares;
    };

    struct Params {
        ...
        SharePoolParams sharepool;
    };

In `src/sharechain/share.h`, define the new public share record and best-tip scoring API:

    namespace sharechain {

    struct ShareRecord {
        uint256 parent_share;
        uint256 prev_block_hash;
        uint256 candidate_header_hash;
        uint32_t nTime;
        uint32_t nBits;
        uint32_t nNonce;
        CScript payout_script;
    };

    uint256 GetShareId(const ShareRecord&);
    arith_uint256 GetShareWork(const ShareRecord&);

    } // namespace sharechain

In `src/sharechain/payout.h`, define deterministic reward reconstruction:

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

    RewardCommitment ComputeRewardCommitment(const ShareChainView&, const CBlockIndex& prev_block, CAmount total_reward);

    } // namespace sharechain

In `src/rpc/mining.cpp`, extend the existing mining surfaces rather than replacing them:

- `getblocktemplate` gains a `sharepool` object when the deployment is active.
- `getmininginfo` gains `sharepool_active`, `share_tip`, `pending_pooled_reward`, and `accepted_shares`.
- add `submitshare`
- add `getsharechaininfo`
- add `getrewardcommitment`

In `src/script/interpreter.cpp`, reserve a new witness-program version for pooled reward claims and make validation call into a dedicated shareclaim verifier rather than scattering claim logic across unrelated script paths.

In `src/node/internal_miner.cpp`, keep the internal miner entrypoint but change its post-activation behavior from “search only for full blocks” to “submit shares continuously and produce blocks when a share also meets block difficulty.”

This plan depends on RNG’s existing RandomX PoW implementation, version-bits deployment machinery, wallet, and Bitcoin-derived P2P relay code. It deliberately does not depend on Zend at consensus time. Zend remains optional tooling that can later consume the replay/export surfaces this plan adds.

Change note: Created this ExecPlan on 2026-04-13 after reviewing RNG’s mining and activation seams plus Zend’s recent rBTC pool/sharechain work. The purpose of this first revision is to lock the architecture before implementation starts so later revisions can update progress against a single self-contained protocol plan.
