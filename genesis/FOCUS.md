# Focus Brief

## Raw Focus String

> Target the new ExecPlan at docs/rng-protocol-native-pooled-mining-execplan.md. Review whether RNG can be upgraded from classic block-finder rewards to protocol-native default pooled mining with a public sharechain, deterministic reward window, compact payout commitment, and trustless post-maturity claims. Stress-test consensus coherence against RNGs Bitcoin-derived mining, validation, wallet, networking, and RandomX code paths. Focus on whether the proposed share object, share relay, and sharechain tip rule are sufficient and attack-aware; whether version-bits activation is realistic or whether the design actually implies a harder fork boundary; whether compact payout commitments plus claim spends survive Bitcoin-style UTXO and script constraints without hidden operator trust; whether immediate pending accrual but delayed claimability under the current 100-block coinbase maturity is the right product contract for small miners and Bitino-style game loops; whether internal miner, getblocktemplate, wallet, and RPC changes are correctly scoped; and what minimum implementation sequence would de-risk this before any mainnet rollout. Use Zends recent rBTC sharechain, proof, and onboarding work only as reference material and explicitly distinguish what should stay tooling versus what must move into RNG consensus.

## Normalized Focus Themes

1. Can RNG support protocol-native pooled mining without breaking its Bitcoin-derived consensus and node model?
2. Is a public sharechain plus deterministic reward window sufficient and attack-aware?
3. Is version-bits activation realistic, or does the design hide a harder fork boundary?
4. Can compact payout commitments plus delayed trustless claims fit existing UTXO and script constraints?
5. Is “pending now, claimable after maturity” the right user contract for small miners and agent/game integrations?
6. What is the minimum implementation sequence that proves feasibility before any mainnet decision?

## Repo-Grounded Code Surfaces

The focus most directly touches these verified target-repo seams:

- `src/consensus/params.h`
- `src/deploymentinfo.cpp`
- `src/kernel/chainparams.cpp`
- `src/pow.cpp`
- `src/node/miner.cpp`
- `src/node/internal_miner.cpp`
- `src/protocol.h`
- `src/net_processing.cpp`
- `src/rpc/mining.cpp`
- `src/validation.cpp`
- wallet and script surfaces that would need future extension if the design proceeds

## What Still Needs Broad Review

1. Documentation truthfulness: several specs still describe future features as if they exist today.
2. Activation realism: current code exposes only `testdummy` and `taproot`; sharepool would be new machinery.
3. RandomX and difficulty details: fixed seed policy and mainnet `fPowAllowMinDifficultyBlocks=true` both matter to mining economics and attack analysis.
4. Operator/developer experience: pooled mining changes CLI/RPC information architecture even without a GUI.
5. Parallel local plans: the root `EXECPLAN.md` describes QSB rollout work, but corresponding source files were not present in the inspected checkout, so pooled-mining planning must not assume those integration points are already available.

## Main Questions To Answer

1. What is the smallest truthful spec and simulator needed to validate economics before code?
2. What must be consensus-enforced versus what can remain tooling or observability?
3. What exact regtest proof would show that the design works before devnet?
4. What should stay explicitly out of scope until pooled mining itself is real?

## Planning Consequence

The focus still justifies a pooled-mining-first research plan, but not a rewrite of repository reality. The corpus should guide toward a simulator-first, decision-gated protocol effort while remaining explicit that pooled mining is proposed work layered on top of an already-live RandomX node, not a feature already present in the current checkout.
