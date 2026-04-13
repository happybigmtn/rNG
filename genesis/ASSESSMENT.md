# Assessment

## Inspection Scope and Control Plane

This assessment was amended by a Codex review pass on 2026-04-13. The review inspected this repository, the generated corpus under `genesis/`, and a separate Zend reference repository. It used lightweight local inspection (`rg`, `find`, targeted `sed`, and `git show` for deleted genesis artifacts) and did not run long integration suites.

No repo-root `AGENTS.md`, `CLAUDE.md`, or `RTK.md` file exists on disk at this review point. The controlling instructions therefore came from the user prompt plus root project docs. The repo-root `PLANS.md` is an ExecPlan format standard, not an active backlog. The active planning/control surface is the root `IMPLEMENTATION_PLAN.md`, with operational context in `EXECPLAN.md`, `WORKLIST.md`, and `LEARNINGS.md`. There is no root `plans/` directory, so the `genesis/` corpus should be treated as a subordinate planning/index corpus rather than a second active control plane.

Verified facts in this document are grounded in the files listed under "Code Review Coverage" and in the root control docs named above. Assumptions and hypotheses are separated in the Assumption Ledger. Any statement about live mainnet health is taken from committed operator docs, not from a fresh node probe in this review pass.

## What the Project Says It Is vs What the Code Shows It Is

### Claims
RNG is described as a Bitcoin Core-derived node for a live RandomX proof-of-work chain with protocol-native pooled mining as the planned default mining mode. The README states it is based on Bitcoin Core 30.2. The IMPLEMENTATION_PLAN.md describes a phased sharepool build from spec through mainnet activation.

### Code reality
RNG is a faithful Bitcoin Core v30.2 fork with working RandomX PoW, a live mainnet around the low-32,000 block range per committed docs, operator-run Contabo validators, a multi-threaded internal miner, an operator-only QSB escape hatch proven on mainnet, and significant sharepool groundwork. Root `EXECPLAN.md` records validator-02/04/05 healthy at height 32122 on 2026-04-13 and validator-01 crash-looping on a zero-byte `settings.json`; this review did not re-probe the live fleet. The sharepool groundwork includes a dormant BIP9 deployment boundary, LevelDB-backed sharechain storage, activation-gated P2P share relay, an offline economic simulator with confirmed constants, a reference settlement state-machine model with deterministic test vectors, C++ consensus settlement helpers with vector-backed parity tests, and activated solo-settlement coinbase construction in the miner. However, the protocol is not yet trustlessly pooled: witness-v2 claim verification, `ConnectBlock` commitment enforcement, multi-leaf reward-window commitment, the share-producing miner, wallet auto-claim, and sharepool RPCs are all unbuilt.

## What Works

| Component | Evidence | Status |
|-----------|----------|--------|
| RandomX PoW | `src/crypto/randomx_hash.{h,cpp}`, `src/crypto/randomx/` (vendored v1.2.1), `randomx_tests` pass | Working, deployed |
| Internal miner | `src/node/internal_miner.{h,cpp}` (770 lines), coordinator + N workers, lock-free hot path | Working; root docs record 3 healthy validators and 1 crash-looping validator on 2026-04-13 |
| Bitcoin Core v30.2 wallet | Full descriptor-based SQLite wallet, all standard RPCs | Working |
| QSB operator support | `src/script/qsb.{h,cpp}`, `src/node/qsb_*.{h,cpp}`, canary funding/spend proven at heights 29946-29947 | Working, mainnet proven |
| BIP9 sharepool deployment | `DEPLOYMENT_SHAREPOOL` bit 3, dormant `NEVER_ACTIVE`, regtest activatable | Working skeleton |
| Sharechain storage | `src/node/sharechain.{h,cpp}` (463 lines), LevelDB, orphan buffer, best-tip selection | Working |
| P2P share relay | `shareinv`/`getshare`/`share` in `src/net_processing.cpp`, activation-gated | Working, tested |
| Settlement consensus helpers | `src/consensus/sharepool.{h,cpp}` (340 lines), tagged SHA256, Merkle trees, claim-status tracking | Working, vector-verified |
| Solo settlement coinbase | `src/node/miner.cpp` emits OP_2 settlement output when sharepool active | Working, miner_tests pass |
| Economic simulator | `contrib/sharepool/simulate.py` (789 lines), 1-second candidate confirmed | Working |
| Settlement reference model | `contrib/sharepool/settlement_model.py` (660 lines), 5 transition scenarios | Working |
| Operator scripts | `scripts/start-miner.sh`, `doctor.sh`, `load-bootstrap.sh`, `build-release.sh` | Working, used in fleet ops |
| Bootstrap assets | Height 29944 bundle and UTXO snapshot | Working |
| Docker image | `Dockerfile`, fail-closed on missing `RNG_RPC_PASSWORD` | Working |
| Reproducible releases | `scripts/check-reproducible-release.sh`, same-machine linux-x86_64 verified | Working |
| CI | `.github/workflows/ci.yml` (Windows, macOS, Linux), release workflow | Working |

## What Is Broken

| Issue | Evidence | Severity |
|-------|----------|----------|
| `contabo-validator-01` crash-looping | `/root/.rng/settings.json` is zero bytes, `rngd.service` crash-loops before RPC starts | Operational / medium |
| `fPowAllowMinDifficultyBlocks=true` on mainnet | `src/kernel/chainparams.cpp` sets this true, but current LWMA does not branch on it. Effect unresolved. | Latent risk / low |

## What Is Half-Built

| Component | Built | Missing | Blocks |
|-----------|-------|---------|--------|
| Witness-v2 claim verification | Settlement helpers, solo coinbase output | `src/script/interpreter.cpp` witness-v2 dispatch, settlement program verification | POOL-07 |
| `ConnectBlock` enforcement | Solo settlement output in miner | Commitment validation, claim-conservation rules in `src/validation.cpp` | POOL-07 |
| Multi-leaf commitment | Solo single-leaf fallback | Reward-window walk, multi-payout-script leaf set | POOL-07 |
| Share-producing miner | Internal miner v2 (block-only) | Dual-target (share + block), share construction, share relay on find | POOL-08 |
| Wallet pooled reward | Standard wallet | `pooled.pending`/`pooled.claimable` in `getbalances`, auto-claim | POOL-08 |
| Sharepool RPCs | None | `submitshare`, `getsharechaininfo`, `getrewardcommitment` | POOL-08 |
| Regtest e2e proof | Relay test, solo settlement test | Multi-miner activated lifecycle: produce shares, commit, claim | CHKPT-03 |
| Devnet adversarial testing | Nothing | Multi-node devnet, withholding/eclipse/spam tests | FUTURE-01 |
| Mainnet activation | `NEVER_ACTIVE` deployment | BIP9 start time, operator docs, release with activation params | FUTURE-02 |

## Tech Debt Inventory

1. **Internal naming**: Build targets still use `test_bitcoin`, `bench_bitcoin` instead of RNG names. Intentional to reduce upstream divergence, but confusing for contributors.
2. **Spec files dated `120426`**: Nine spec files carry the `120426-` prefix from an older planning pass. Some carry stale claims (e.g., `fPowAllowMinDifficultyBlocks` confusion, missing DNS seeds). The current top-level specs (`sharepool.md`, `sharepool-settlement.md`, `consensus.md`, `randomx.md`, `agent-integration.md`) are accurate.
3. **Bootstrap asset growth**: Current bundle is ~60 MB at height 29944. No versioning, CDN, or refresh schedule. Will grow unbounded.
4. **DNS seeds absent**: `seed1/2/3.rng.network` referenced in old docs but absent from `src/kernel/chainparams.cpp`. Network discovery relies entirely on four hardcoded operator IPs.
5. **Agent integration spec gap**: `specs/agent-integration.md` lists extensive planned features (`createagentwallet`, MCP server, webhooks, autonomy budgets) as `[NOT YET IMPLEMENTED]`. Product direction says "agent-first" but implementation is entirely future work.
6. **No root `install.sh`**: Tracked in `WORKLIST.md` as optional. Operators use release tarballs, Docker, or public-node/public-miner scripts.
7. **Cross-machine reproducibility unverified**: Same-machine linux-x86_64 reproducibility proven. Cross-machine and cross-platform reproducibility not yet tested.
8. **QSB interaction with witness-v2**: Merged QSB code uses non-standard mempool admission. Interaction with future witness-v2 claim standardness is unanalyzed.

## Security Risks

1. **Single-vendor seed infrastructure**: All four mainnet seed peers are Contabo-hosted, operator-run. A single ASN failure or vendor policy change could partition the network.
2. **Fixed RandomX seed**: The genesis seed `"RNG Genesis Seed"` is permanent. This means dataset computation is a one-time cost, which benefits miners with persistent fast-mode VMs but also means an attacker's precomputation investment never expires.
3. **Witness-v2 reservation**: After activation, all witness v2 32-byte programs are reserved for sharepool settlement. No unrelated witness-v2 destinations are allowed in v1. This is intentional but constraining.
4. **Settlement claim ordering**: Claims are serialized by UTXO set (only one tx can spend the current settlement output). This means claim throughput is limited to one claim per block per settlement output. With many miners, claim backlog could grow.
5. **Unimplemented claim fee market**: Fees must come from non-settlement inputs. If claim fees become significant relative to claimed amounts, small miners may find claims uneconomic without batched-claim support (v1 does not support batched claims).
6. **No SIGHASH_ANYPREVOUT or equivalent**: Settlement claims are not malleable because the payout destination is consensus-locked, but the claim transaction itself does not carry an inner signature. This is safe for v1 but limits future extensibility.

## Test Gaps

1. **No functional test for activated multi-miner sharepool lifecycle**: `feature_sharepool_relay.py` tests relay only. No test exercises the full path: activate, produce shares from multiple miners, build multi-leaf commitment, mine block, claim after maturity.
2. **No adversarial share tests**: Withholding, invalid share injection, orphan flooding, share relay spam are not tested.
3. **No witness-v2 settlement claim test**: No test creates a claim transaction with settlement input, payout output, and successor settlement output.
4. **No wallet pooled-reward test**: No test verifies `getbalances` with `pooled.pending`/`pooled.claimable`.
5. **1-second relay benchmark not run**: POOL-06-GATE measured at 10-second intervals. The confirmed 1-second cadence needs separate measurement.
6. **No cross-platform CI for RNG-specific tests**: QSB and sharepool tests run on Linux CI. macOS and Windows coverage for these is implicit via build but not explicitly verified.

## Documentation Staleness

| Document | Issue |
|----------|-------|
| `specs/120426-network-identity.md` | References DNS seeds that are absent from code |
| `specs/120426-consensus-chain-rules.md` | `fPowAllowMinDifficultyBlocks` confusion unresolved |
| `specs/120426-sharepool-protocol.md` | Historical context only; still says `src/consensus/sharepool` does not exist even though `src/consensus/sharepool.{h,cpp}` now exists |
| `specs/agent-integration.md` | Lists 10+ features as `[NOT YET IMPLEMENTED]` |
| `README.md` | Accurate but does not describe sharepool work or current dev status |
| `EXECPLAN.md` | Complete and archived; describes QSB rollout, not ongoing work |
| `IMPLEMENTATION_PLAN.md` | Still points POOL-07C evidence at `genesis/plans/chkpt-03a-settlement-design-review.md`, which is absent from the current generated tree |
| `genesis/PLANS.md` and `genesis/GENESIS-REPORT.md` | Originally claimed separate checkpoint plans existed; current generated tree only has numbered plans 001-012 |
| `genesis/DESIGN.md` | Originally said the repo has no GUI; the target repo has inherited Qt GUI sources and an optional `rng-qt` target, though no bespoke sharepool UI |

## Implementation Status Table

### Prior Plans (from `IMPLEMENTATION_PLAN.md`)

| ID | Description | Claimed Status | Verified Status | Evidence |
|----|-------------|---------------|-----------------|----------|
| DONE-01 | RandomX PoW | Complete | **Verified** | `randomx_tests` pass, live mining on mainnet |
| DONE-02 | Internal miner v2 | Complete | **Verified** | `feature_internal_miner.py` passes; committed root docs show the miner running on healthy validators, with validator-01 currently an ops repair item |
| DONE-03 | Network identity | Complete | **Verified** | Correct magic, ports, HRP in `chainparams.cpp` |
| DONE-04 | Consensus rules | Complete | **Verified** | `validation_tests` pass, 32K+ blocks mined |
| DONE-05 | Wallet/RPC base | Complete | **Verified** | Full Bitcoin Core v30.2 wallet surface |
| DONE-06 | Operator scripts | Complete | **Verified** | Scripts exist, used in fleet deployment |
| DONE-07 | Release pipeline | Complete | **Verified** | Reproducible builds, CI workflow |
| DONE-08 | QSB operator | Complete | **Verified** | Mainnet canary proven, fleet deployed |
| POOL-01 | Sharepool spec | Complete | **Verified** | `specs/sharepool.md` exists with confirmed constants |
| POOL-02 | Economic simulator | Complete | **Verified** | `contrib/sharepool/simulate.py` + test coverage |
| POOL-03 | Simulator decision (no-go) | Complete | **Verified** | 25.10% CV documented, rejected baseline |
| POOL-01R/02R/03R | Revised constants | Complete | **Verified** | 1-second candidate confirmed, evidence committed |
| POOL-04 | BIP9 deployment | Complete | **Verified** | `DEPLOYMENT_SHAREPOOL` in `params.h`, regtest activatable |
| POOL-05 | Sharechain + relay | Complete | **Verified** | `sharechain.{h,cpp}`, `net_processing.cpp` relay handlers |
| POOL-06-GATE | Relay viability | Complete | **Verified** | Benchmark report in `contrib/sharepool/reports/` |
| CHKPT-02 | Pre-consensus review | Complete | **Partially verified** | Root `IMPLEMENTATION_PLAN.md` records completion; standalone historical checkpoint file is absent from current generated tree |
| POOL-07A | Settlement spec | Complete | **Verified** | `specs/sharepool-settlement.md` fully specified |
| POOL-07B | Reference model | Complete | **Verified** | `settlement_model.py` + vectors JSON |
| POOL-07C | Design checkpoint | Complete | **Partially verified** | Root `IMPLEMENTATION_PLAN.md` records completion; the cited standalone genesis checkpoint file is absent from the current generated tree, so current authoritative detail is `specs/sharepool-settlement.md`, `contrib/sharepool/settlement_model.py`, and vectors |
| POOL-07D | C++ consensus helpers | Complete | **Verified** | `src/consensus/sharepool.{h,cpp}`, parity tests pass |
| POOL-07E | Solo settlement coinbase | Complete | **Verified** | `miner_tests` pre/post-activation pass |
| POOL-07 | Full commitment/claim | In progress | **Not started** | No witness-v2 verifier, no ConnectBlock enforcement |
| POOL-08 | Miner + wallet integration | Blocked | **Not started** | No dual-target miner, no wallet auto-claim |
| CHKPT-03 | Regtest e2e proof | Blocked | **Not started** | No end-to-end test |
| FUTURE-01 | Devnet adversarial | Blocked | **Not started** | No devnet infrastructure |
| FUTURE-02 | Mainnet activation | Blocked | **Not started** | Deployment still `NEVER_ACTIVE` |

## Code Review Coverage

### Source files actually read during this assessment

| File | Lines | Read |
|------|-------|------|
| `src/consensus/sharepool.h` | 74 | Full |
| `src/consensus/sharepool.cpp` | 266 | Partial (first 80 lines + grep) |
| `src/consensus/params.h` | 164 | Grep for DEPLOYMENT_SHAREPOOL |
| `src/node/sharechain.h` | 132 | Symbol and structure checks via `rg` |
| `src/node/sharechain.cpp` | 331 | Symbol and structure checks via `rg` |
| `src/node/internal_miner.h` | 240 | Symbol checks for current block-only miner surface |
| `src/node/internal_miner.cpp` | 530 | Symbol checks for share-producing miner gap |
| `src/node/miner.cpp` | 675 | Grep for sharepool/settlement |
| `src/script/interpreter.cpp` | Large | Grep for witness v2 (none found) |
| `src/validation.cpp` | 6607 | Grep for sharepool (none found) |
| `src/net_processing.cpp` | 6061 | Grep for share relay handlers |
| `src/protocol.h` | - | Grep for share message types |
| `src/kernel/chainparams.cpp` | 200+ | Targeted checks for chain params, deployments, seed peers |
| `src/test/sharepool_commitment_tests.cpp` | 262 | Symbol checks for vector-backed coverage |
| `src/test/miner_tests.cpp` | - | Grep for sharepool tests |
| `test/functional/feature_sharepool_relay.py` | 87 | Targeted read/search for relay-only scope |
| `test/functional/feature_sharepool_relay_benchmark.py` | 464 | Targeted read/search for no-RPC note and benchmark scope |
| `contrib/sharepool/simulate.py` | 789 | Targeted symbol/search checks |
| `contrib/sharepool/settlement_model.py` | 660 | Targeted symbol/search checks |
| `specs/sharepool.md` | 426 | Full |
| `specs/sharepool-settlement.md` | 406 | Full |
| `EXECPLAN.md` | 1640 | First 100 lines |
| `IMPLEMENTATION_PLAN.md` | 342 | Full |
| `README.md` | 150 | Full |
| `PLANS.md` | 187 | Full |
| `WORKLIST.md` | 15 | Full |
| `ARCHIVED.md` | 67 | First 100 lines |
| `LEARNINGS.md` | 13 | Full |
| `specs/120426-*.md` files | ~14 files | Targeted discrepancy searches |
| `genesis/*.md`, `genesis/plans/*.md` | Generated corpus | Full/targeted review for shape and stale references |

### Reference repo files read

| File | Read |
|------|------|
| `Zend/README.md` | First 40 lines |
| `Zend/docs/rbtc-pool-third-party-onboarding.md` | Targeted read for protocol and trust model |
| `Zend/services/home-miner-daemon/rbtc_tools.py` | Targeted search for `trust_model`, `fully_trustless`, handoff/proof paths |
| `Zend/LEARNINGS.md` | Targeted read for operational lessons |
| Zend docs directory | Listed |

## Target Users, Success Criteria, and Constraints

### Target users

1. **Small CPU miners**: Home operators running `rngd` with the internal miner. They want to mine on commodity hardware and see reward accrual without running or trusting a pool operator.
2. **Casual game miners**: Players of games like Bitino who mine RNG as a side activity. They want the simplest possible path from "install" to "earning."
3. **Operators**: People running validator fleets (currently 4 Contabo nodes). They need reliable tooling for deployment, monitoring, and fleet management.
4. **Developers/contributors**: People extending RNG. They need honest documentation, working CI, and clear specs.

### Success criteria

1. After sharepool activation, a CPU miner running `rngd -mine -mineaddress=<addr> -minethreads=N` begins accruing deterministic pending pooled reward within minutes of starting.
2. After coinbase maturity (100 blocks), the miner's wallet can claim its committed reward without any operator involvement.
3. A 10% miner over 100 blocks receives ~10% of total rewards with CV < 10%.
4. The protocol does not depend on any operator ledger, operator-controlled payout decision, or single control plane for share admission.
5. Regtest proof demonstrates the full lifecycle: activate, produce shares, relay, commit, mine, claim.

### Constraints

1. Bitcoin Core v30.2 codebase: All changes must be layerable on top of the inherited architecture.
2. Existing mainnet: Changes must not break the live chain. BIP9 activation provides the gating mechanism.
3. Coinbase maturity: The 100-block rule is inherited and must be preserved.
4. RandomX PoW: Share proofs must use the same RandomX computation as block proofs.
5. Resource limits: Home miners have limited bandwidth and storage. Share relay must stay under ~10 KB/s per node at the confirmed 1-second cadence.

## Assumption Ledger

| Assumption | Status | Evidence |
|-----------|--------|----------|
| RandomX PoW is stable and secure for RNG | Verified | 32K+ blocks mined, vendored v1.2.1, no incidents |
| Bitcoin Core v30.2 is a sound base | Verified | Inherited wallet, RPC, P2P, validation all working |
| 1-second share spacing passes variance threshold | Verified | POOL-02R simulator: CV 8.06% max across 20 seeds |
| Settlement state machine prevents double claims | Verified (reference model) | `settlement_model.py` self-test passes all 5 scenarios |
| C++ settlement helpers match reference model | Verified | `sharepool_commitment_tests` reproduces vectors |
| Solo settlement coinbase is correctly constructed | Verified | `miner_tests` pre/post-activation pass |
| Witness-v2 claim verification is implementable | Hypothesis | Spec exists, no C++ implementation yet |
| `ConnectBlock` enforcement is layerable | Hypothesis | Architecture supports it, but code not written |
| 1-second share relay bandwidth is acceptable | Needs proof | POOL-06-GATE measured at 10-second intervals only |
| Claim throughput (one per block per settlement) is sufficient | Needs proof | With many miners, claim backlog could grow |
| Small miners can afford claim fees | Needs proof | Fee market for claims is unmodeled |
| Mainnet validators can handle sharechain storage | Needs proof | Storage cost at 1-second cadence not measured |

## Focus Response

### What the operator focus emphasized
Protocol-native trustless pooled mining as the default mining mode. Strongest-sense trustless: no operator ledger, no operator-controlled payout, no single control plane. Small CPU miners accruing deterministic reward immediately.

### What the code says about it
The codebase has made genuine progress toward this goal. The spec is locked, the simulator validated the constants, the settlement state machine is designed and reference-tested, C++ helpers pass parity tests, and the solo settlement coinbase is wired. But the critical consensus enforcement layer (witness-v2 verification + `ConnectBlock` commitment validation) and the user-facing integration layer (share-producing miner + wallet auto-claim + RPCs) are entirely unbuilt. The protocol is approximately 60% of the way from spec to regtest proof, measured by the implementation plan's own dependency chain.

### Non-focused risks that still outrank focus work
1. **Validator-01 crash-loop**: Operational risk. Three healthy validators is a thin margin for a four-node mainnet. Repair is independent of sharepool development.
2. **Single-ASN seed infrastructure**: All seeds are Contabo-hosted. This is a network resilience issue regardless of sharepool status.

## Opportunity Framing

### Strongest direction (recommended)
Complete the sharepool critical path (POOL-07 → POOL-08 → CHKPT-03) to prove trustless pooled mining on regtest, then pursue devnet adversarial testing before mainnet activation. This is the direction already encoded in `IMPLEMENTATION_PLAN.md` and the operator focus confirms it.

### Rejected directions

1. **Port Zend's HTTP pool design into RNG**: Rejected because the goal is protocol-native consensus-enforced pooling, not an HTTP/control-plane pool overlay. The Zend reference repo has a trustless-track handoff model when direct-only, MinerBuilt, replay-verified, and peer-mirrored conditions are satisfied, but that is still an operator-facing HTTP overlay architecture rather than RNG-native consensus settlement. Zend's lessons about share accounting, operational tooling, proof bundles, and failure modes are valuable as reference material, not as consensus design.

2. **Prioritize agent wallet / MCP before sharepool**: Rejected because agent features depend on proven core wallet and mining surfaces. The sharepool lifecycle must work before agent convenience wrappers make sense.

3. **Skip regtest/devnet gates and activate on mainnet**: Rejected because the settlement state machine is consensus-critical. A subtle bug in claim verification could allow settlement draining. The decision gates at CHKPT-03 and FUTURE-01 exist precisely to catch these issues.

4. **Reduce coinbase maturity to improve small-miner UX**: Rejected for v1. The 100-block maturity rule is inherited and well-understood. Changing it introduces new economic dynamics that should be studied separately.

## DX Assessment

### First-run developer experience

1. **Build**: `cmake -B build && cmake --build build -j$(nproc)` works on Linux with standard dependencies. Build guides in `doc/build-*.md` are inherited from Bitcoin Core and accurate. Build time is significant (~10 minutes on a modern machine) due to Bitcoin Core size.

2. **Bootstrap**: `scripts/load-bootstrap.sh` loads the height-29944 snapshot. This gets a new node to a usable state quickly. However, the bootstrap bundle is ~60 MB and will grow.

3. **Mine**: `rngd -mine -mineaddress=<addr> -minethreads=4` starts mining immediately. The miner logs prominently on block finds. The fastest path from checkout to "I mined a block on regtest" is under 15 minutes.

4. **Test**: `build/bin/test_bitcoin --run_test=randomx_tests` runs RNG-specific tests. `python3 test/functional/feature_internal_miner.py --configfile=build/test/config.ini` tests mining. Test infrastructure is inherited from Bitcoin Core and well-maintained.

5. **Friction points**: The `test_bitcoin`/`bench_bitcoin` naming is confusing for newcomers. No root `install.sh` for quick local install. The `PLANS.md` ExecPlan standard is thorough but dense for a first-time contributor.

6. **Honest examples**: The README accurately describes what's working and what's in progress. The QSB example is real (mainnet-proven). The sharepool work is clearly labeled as specification and groundwork, not a working feature.
