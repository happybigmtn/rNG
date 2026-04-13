# Specification: BIP9 Activation and Deployment Sequence

Plans 004 (skeleton), 009 (regtest proof), 010 (decision gate), 011 (devnet testing),
012 (mainnet activation).

## Objective

Define the complete activation and deployment path for the sharepool feature,
from the existing BIP9 skeleton through regtest proof, devnet adversarial testing,
and mainnet activation. This specification covers the deployment parameters, gating
logic, testing requirements, and release process.

## Evidence Status

### Verified Facts

**DEPLOYMENT_SHAREPOOL enum (src/consensus/params.h:36)**:
- Position: after `DEPLOYMENT_TAPROOT`, before `MAX_VERSION_BITS_DEPLOYMENTS`
- Bit: 3

**Chain parameters (src/kernel/chainparams.cpp, all verified)**:

| Network | Class | Bit | nStartTime | nTimeout | Threshold | Period | Line |
|---------|-------|-----|------------|----------|-----------|--------|------|
| Mainnet | CMainParams | 3 | NEVER_ACTIVE | NO_TIMEOUT | 1916 (95%) | 2016 | 134-139 |
| Testnet | CTestNetParams | 3 | NEVER_ACTIVE | NO_TIMEOUT | 1916 (95%) | 2016 | 256-261 |
| Testnet4 | CTestNet4Params | 3 | NEVER_ACTIVE | NO_TIMEOUT | 1916 (95%) | 2016 | 358-363 |
| Signet (devnet) | SigNetParams | 3 | NEVER_ACTIVE | NO_TIMEOUT | 1916 (95%) | 2016 | 497-502 |
| Regtest | CRegTestParams | 3 | NEVER_ACTIVE | NO_TIMEOUT | 108 (75%) | 144 | 583-588 |

Note: There is no separate devnet chain type. "Devnet" in project plans refers to
a signet deployment used for pre-mainnet adversarial testing.

**Activation gating functions (verified in code)**:

| Function | Location | Usage |
|----------|----------|-------|
| `SharepoolRelayActive()` | `src/net_processing.cpp:2161` | Gates P2P share relay (share_inv, share_getdata, share messages). Called at lines 3973, 3997, 4025. |
| `SharepoolDeploymentActiveAfter()` | `src/node/miner.cpp:82` | Gates miner settlement commitment in block templates. Called at line 183. |

Both use the standard `DeploymentActiveAt()` / `DeploymentActiveAfter()` BIP9
infrastructure. All sharepool code paths are gated behind these checks.

**Regtest activation (verified)**:
- Command-line flag: `-vbparams=sharepool:0:9999999999:0`
- Used in test suite: `src/test/miner_tests.cpp:77`
- Sets nStartTime=0 (epoch), nTimeout=9999999999 (far future), min_activation_height=0

**Comment in mainnet params (line 133)**:
> "Sharepool remains dormant on mainnet until regtest and devnet gates pass."

### Recommendations

- Keep mainnet at `NEVER_ACTIVE` until Plan 010 (decision gate) explicitly
  approves promotion. The activation timeline depends on devnet results.
- When setting mainnet nStartTime, allow a minimum 4-week upgrade window between
  binary release and the earliest possible activation signal start.
- Signet (devnet) should be activated before mainnet but after regtest proof
  passes. Activation can use a fixed nStartTime in the past (immediate signaling)
  for controlled testing.

### Hypotheses / Unresolved Questions

- **Mainnet threshold**: Currently set at 95% (1916/2016). This matches Bitcoin's
  BIP9 default. Whether to lower it (e.g., 90%) depends on miner coordination
  dynamics on the RNG chain. Decision deferred to Plan 012.
- **Speedy trial**: Bitcoin used height-based "speedy trial" for Taproot (BIP 341).
  Whether RNG should use a similar mechanism for faster activation is TBD.
- **LOT=true vs LOT=false**: Whether to enforce mandatory signaling after timeout
  is a governance decision outside this spec's scope.

## BIP9 Deployment Skeleton

### Current State: BUILT AND WORKING

The BIP9 deployment infrastructure is complete:

1. `DEPLOYMENT_SHAREPOOL` registered in the version bits enum.
2. All five chain types have deployment parameters configured.
3. Activation gating functions exist and are called at all sharepool entry points.
4. Regtest activation via `-vbparams` is tested and working.
5. Mainnet is intentionally `NEVER_ACTIVE` pending proof.

### Version Bit Signaling

Miners signal readiness by setting bit 3 in the block version field.
The BIP9 state machine transitions:

```
DEFINED -> STARTED -> LOCKED_IN -> ACTIVE
                  \-> FAILED
```

- **DEFINED**: Default state. nStartTime not yet reached.
- **STARTED**: Current MTP >= nStartTime. Miners can signal.
- **LOCKED_IN**: Threshold met in a retarget period. Activation guaranteed.
- **ACTIVE**: min_activation_height reached after LOCKED_IN. Sharepool rules enforced.
- **FAILED**: Current MTP >= nTimeout without threshold. Deployment abandoned.

Since all networks currently use `NEVER_ACTIVE`, the state machine stays in
DEFINED until parameters are updated.

## Activation Sequence

### Phase 1: Regtest End-to-End Proof (Plan 009)

**Prerequisites**: Plans 007 (consensus) and 008 (miner/wallet/RPC) complete.

**Test environment**:
- 4 regtest nodes connected in a mesh
- BIP9 activated via `-vbparams=sharepool:0:9999999999:0`

**Required demonstrations**:

1. **BIP9 lifecycle**: Signal, lock-in, activate across a retarget period (144 blocks).
   Verify state transitions via `getblockchaininfo` `softforks.sharepool` field.
2. **Share lifecycle**: Submit shares via `submitshare`, verify relay to all 4 nodes,
   query via `getsharechaininfo`.
3. **Settlement lifecycle**: Mine blocks with settlement commitments, verify
   `getrewardcommitment` returns correct data.
4. **Claim lifecycle**: Wait for settlement maturity (100 blocks), verify auto-claim
   constructs and broadcasts claim tx, verify `getbalances` transitions from
   `pooled.claimable` to `mine.trusted`.
5. **Negative tests**: Submit invalid shares (bad PoW, bad parent), attempt relay
   before activation, verify rejection.

**Deliverable**: Regtest proof report with pass/fail for each demonstration.

### Phase 2: Decision Gate (Plan 010)

**Input**: Regtest proof report from Phase 1.

**Decision criteria** (GO/NO-GO):

| Criterion | Required for GO |
|-----------|-----------------|
| All 5 regtest demonstrations pass | Yes |
| No consensus rule violations observed | Yes |
| Settlement amounts match expected reward distribution | Yes |
| Claim transactions are valid and confirm | Yes |
| No memory leaks or resource exhaustion during test | Yes |
| Share relay latency acceptable (< 2s p99 across 4 nodes) | Yes |

**NO-GO actions**: Identify failing components, create fix plan, re-run Phase 1.

### Phase 3: Devnet Adversarial Testing (Plan 011)

**Prerequisites**: Phase 2 GO decision.

**Test environment**:
- Signet (devnet) deployment with 4+ nodes
- Activate sharepool on signet by setting `nStartTime` to a past timestamp
- Run for minimum 48 continuous hours

**Adversarial scenarios**:

1. **Stale share flooding**: One node submits shares referencing old/invalid parents.
   Verify other nodes reject without resource exhaustion.
2. **Double-claim attempt**: Attempt to claim the same settlement output twice.
   Verify consensus rejection.
3. **Reorg across settlement boundary**: Force a 3+ block reorg that invalidates
   a settlement. Verify wallet state recovers correctly.
4. **Withholding attack**: One miner withholds shares, then releases a burst.
   Verify sharechain handles out-of-order delivery.
5. **Malformed commitment**: Mine a block with an invalid reward commitment.
   Verify all nodes reject the block.
6. **Peer disconnection**: Kill and restart nodes mid-settlement. Verify
   sharechain and wallet state recover on reconnection.

**Monitoring**: Log share relay latency, memory usage, peer connection stability,
sharechain height progression, and settlement accuracy throughout the 48-hour run.

**Deliverable**: Devnet test report with metrics and pass/fail for each scenario.

### Phase 4: Mainnet Activation Preparation (Plan 012)

**Prerequisites**: Phase 3 devnet report reviewed, no blocking issues.

**Steps**:

1. **Set mainnet parameters**:
   - `nStartTime`: Chosen date >= 4 weeks after binary release
   - `nTimeout`: nStartTime + 1 year (standard BIP9 timeout)
   - `threshold`: 1916 (95%) unless decision gate recommends change
   - `min_activation_height`: 0 (or a specific height if coordination requires it)

2. **Set testnet/testnet4 parameters**:
   - Activate with `nStartTime` in the past for immediate signaling
   - Allows testnet miners to exercise the feature before mainnet

3. **Set signet parameters**:
   - Already activated from Phase 3; keep active

4. **Documentation**:
   - Release notes describing sharepool feature, activation timeline, upgrade instructions
   - Mining guide for pool operators and solo miners

5. **Binary release**:
   - Tag release with version bump
   - Reproducible builds
   - Publish binaries and release notes

6. **Upgrade window**:
   - Minimum 4 weeks between binary availability and nStartTime
   - Monitor node version distribution via network crawling
   - If insufficient upgrade penetration, consider delaying nStartTime

## Deployment Parameter Changes Required

### Current (all networks):
```cpp
consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
```

### Phase 3 (signet/devnet activation):
```cpp
// SigNetParams only:
consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nStartTime = 0; // Immediate
consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
```

### Phase 4 (mainnet activation):
```cpp
// CMainParams:
consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nStartTime = <TBD>;
consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nTimeout = <TBD>;

// CTestNetParams, CTestNet4Params:
consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nStartTime = 0; // Immediate
consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
```

Regtest parameters remain unchanged (NEVER_ACTIVE by default, activatable via
`-vbparams` flag for testing).

## Acceptance Criteria

- Regtest 4-node proof passes all 5 demonstration categories (Plan 009).
- Decision gate (Plan 010) issues GO recommendation.
- Devnet adversarial testing runs for 48+ hours with 4+ nodes without
   consensus failures, crashes, or resource exhaustion (Plan 011).
- All 6 adversarial scenarios are tested and pass.
- Mainnet nStartTime is set with >= 4-week upgrade window after binary release.
- Testnet and testnet4 activate successfully with nStartTime in the past.
- Release binary is reproducible and matches tagged source.
- Release notes document the feature, activation timeline, and upgrade path.

## Verification

- **Regtest (automated)**: Python functional test that:
  1. Starts 4 regtest nodes with `-vbparams=sharepool:0:9999999999:0`
  2. Mines 144 blocks to trigger BIP9 activation
  3. Verifies `getblockchaininfo` shows `sharepool` as `active`
  4. Runs full share-commit-claim lifecycle
  5. Verifies final balances

- **Devnet (manual + monitoring)**:
  1. Deploy 4+ signet nodes with sharepool activated
  2. Run adversarial test scripts for each scenario
  3. Collect and review metrics after 48 hours
  4. Generate devnet report

- **Mainnet (monitoring)**:
  1. After release, monitor BIP9 signaling progress via `getblockchaininfo`
  2. Track network upgrade percentage
  3. Verify LOCKED_IN -> ACTIVE transition at expected height

## Open Questions

1. **Mainnet nStartTime**: Depends entirely on devnet results (Plan 011). Cannot
   be determined until Phase 3 completes successfully.
2. **Activation height guarantee**: Should `min_activation_height` be set to a
   specific height to ensure all miners have time to upgrade, even if threshold
   is met quickly? Bitcoin used this for Taproot (height 709632).
3. **Emergency deactivation**: If a critical bug is discovered post-activation,
   the only recourse is a soft fork to disable. Should a "kill switch" deployment
   bit be pre-allocated? This adds complexity and may not be worth it for v1.
4. **Testnet coordination**: Testnet and testnet4 have no mining ecosystem yet.
   Should activation be deferred until there are active testnet miners, or
   activate immediately to enable developer testing?
5. **Signet challenge script**: The current signet uses a standard challenge
   script. Does devnet testing require a custom challenge script to control
   block production, or is the default sufficient?
