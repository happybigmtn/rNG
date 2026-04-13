# Focus Brief

## Raw Focus String

> Revise the RNG planning corpus around one goal: protocol-native trustless pool
> mining as the default mining mode on a Bitcoin-derived RandomX chain.
>
> Treat the target end state as strongest-sense trustless pooled mining, not a
> centralized or merely trust-minimized operator pool. The protocol should make
> public share work first-class, smooth block-finder lumpiness, and let small CPU
> miners begin accruing deterministic reward entitlement as soon as they
> participate, with trustless claimability after the required maturity rules.
>
> Use Zend only as a reference repo for lessons learned from miner onboarding,
> pool proofs, share accounting, operational control planes, explorer/wallet UX,
> remote deployment, and the practical failure modes we hit while building the
> rBTC pool. Do not port Zend's current pool design into RNG as the end state.
> Instead, extract what should remain tooling in Zend versus what must move into
> RNG consensus, miner, wallet, RPC, networking, and activation logic.

## Normalized Focus Themes

1. **Protocol-native pooled mining** -- pooled mining is consensus-enforced, not an overlay service.
2. **Trustless by default** -- no operator ledger, no operator-controlled payout, no single control plane for share admission. The protocol itself enforces reward splits.
3. **Default mining mode** -- after activation, every miner participates in the sharepool automatically. Solo mining is the degenerate case where a single miner fills the reward window.
4. **Small-miner accessibility** -- casual CPU miners (e.g. Bitino game miners) see pending reward accumulation immediately after submitting valid shares, without running separate pool infrastructure.
5. **Deterministic reward entitlement** -- reward splits are computable by any node from the public share history. Claims are trustless after coinbase maturity.
6. **Zend as lesson source, not code source** -- extract operational lessons, not runtime coupling.

## Likely Code, Product, and Operational Surfaces

### Consensus layer (highest priority)
- `src/consensus/sharepool.{h,cpp}` -- settlement helpers (exists, partially complete)
- `src/consensus/params.h` -- `DEPLOYMENT_SHAREPOOL` (exists, dormant)
- `src/script/interpreter.cpp` -- witness v2 settlement verification (not yet implemented)
- `src/validation.cpp` -- `ConnectBlock` sharepool commitment enforcement (not yet implemented)
- `src/node/miner.cpp` -- activated coinbase construction (solo case wired, multi-leaf not yet)

### Sharechain and P2P layer
- `src/node/sharechain.{h,cpp}` -- share storage and tip selection (exists, working)
- `src/net_processing.cpp` -- `shareinv`/`getshare`/`share` relay (exists, activation-gated)
- `src/protocol.h` -- share message types (exists)

### Internal miner
- `src/node/internal_miner.{h,cpp}` -- dual-target share+block production (not yet implemented)
- Share construction and relay on share-meeting hash (not yet implemented)

### Wallet and RPC
- `src/wallet/` -- pooled reward tracking, auto-claim (not yet implemented)
- `src/rpc/mining.cpp` -- `submitshare`, `getsharechaininfo`, `getrewardcommitment` (not yet implemented)
- Extended `getmininginfo`, `getbalances` with sharepool fields (not yet implemented)

### Activation and migration
- `src/kernel/chainparams.cpp` -- BIP9 sharepool activation parameters (exists, `NEVER_ACTIVE`)
- Regtest activation via `-vbparams=sharepool:0:9999999999:0` (exists, working)

### Tooling and simulation
- `contrib/sharepool/simulate.py` -- offline economic simulator (exists, working)
- `contrib/sharepool/settlement_model.py` -- reference settlement transition model (exists, working)
- `test/functional/feature_sharepool_*.py` -- relay tests (exists), commitment/e2e (not yet)

### Specs
- `specs/sharepool.md` -- top-level protocol spec (exists, current)
- `specs/sharepool-settlement.md` -- settlement state machine (exists, current)

## What Still Requires Repo-Wide Review Despite the Focus

1. **Validator fleet health** -- `contabo-validator-01` is crash-looping on a zero-byte `settings.json`. Three validators are healthy and mining. This is an operational risk independent of sharepool work.
2. **QSB policy interaction** -- the existing QSB operator path uses non-standard mempool admission. The focus raises the question of how QSB policy and witness-v2 claim standardness interact after activation.
3. **CI stability** -- the PR #2 CI stabilization work was extensive. Any consensus changes (witness-v2 enforcement) must not regress CI coverage.
4. **Documentation staleness** -- several `specs/120426-*.md` files carry dated claims. The focus should not block documentation truthfulness cleanup.
5. **Bitcoin Core 30.2 port maintenance** -- RNG is based on Bitcoin Core 30.2. Upstream security patches must still be tracked.
6. **Test coverage gaps** -- functional tests exist for relay but not for commitment, claim, or end-to-end sharepool lifecycle.
7. **Reproducible release pipeline** -- release tooling is working but only covers linux-x86_64 reproducibility proof. Cross-platform release workflow exists in CI.

## Main Questions the Focus Should Answer

1. **Is the settlement state machine implementable?** The spec exists and reference vectors pass, but no C++ witness-v2 verifier or `ConnectBlock` enforcement exists yet.
2. **Can the reward window produce fair splits at 1-second share spacing on mainnet?** The simulator says yes (CV < 10% for 10% miner), but no live regtest multi-miner proof exists.
3. **What is the actual bandwidth and storage cost of 1-second shares?** POOL-06-GATE measured relay at 10-second intervals. The confirmed 1-second cadence needs its own measurement.
4. **How does a casual miner start accruing reward?** The UX path from "install rngd, start mining" to "see pending pooled reward" is not implemented yet.
5. **What attack surfaces does the settlement model create?** Withholding, claim abuse, settlement draining, sybil admission -- these are specified in the open questions but not adversarially tested.
6. **What belongs in RNG consensus vs what stays in Zend tooling?** Share accounting, payout commitment, and claim verification belong in RNG. Operator fleet management, mobile UX, and HTTP pool protocols stay in Zend.

## How the Focus Changed Priority Ordering

### Before focus
The `IMPLEMENTATION_PLAN.md` already had sharepool as the primary work track (POOL-01 through POOL-08, CHKPT-03, FUTURE-01/02). QSB, operations, and release work filled Tiers 1-2 and 5.

### After focus
The priority ordering remains structurally similar because the existing plan already centered on sharepool. The focus sharpens emphasis in three ways:

1. **POOL-07 (commitment/claim) is the critical path.** Settlement helpers (POOL-07D) and solo coinbase wiring (POOL-07E) are done. The remaining POOL-07 work -- witness-v2 interpreter enforcement, `ConnectBlock` validation, multi-leaf commitment, claim transaction acceptance -- is the single highest-priority code slice.

2. **POOL-08 (miner + wallet integration) moves from "blocked" to "next after POOL-07."** Dual-target share production and wallet auto-claim are where the "default mining mode" and "small miner accrual" goals become real.

3. **Decision gates remain mandatory.** The focus does not skip the regtest proof (CHKPT-03), devnet adversarial testing (FUTURE-01), or mainnet activation (FUTURE-02). These gates are where the trustless and decentralization claims get validated.

### Items that dropped in relative priority
- Agent wallet / MCP server (FUTURE-04) -- still future work, deprioritized behind proven sharepool.
- Atomic swap protocol (FUTURE-06) -- explicitly blocked until sharepool is stable.
- Validator-01 repair -- still required but does not block sharepool development on regtest/devnet.
