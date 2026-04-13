# Specification: RandomX Proof-of-Work Integration

## Objective

Define how RNG integrates RandomX as its proof-of-work algorithm, replacing Bitcoin's SHA256d. Covers the vendored library, seed policy, hash computation interface, JIT support, and operational configuration.

## Evidence Status

### Verified Facts (grounded in source code and documentation)

- **Algorithm**: RandomX v1.2.1 vendored at `src/crypto/randomx/` (cloned from `https://github.com/tevador/RandomX.git`, branch `v1.2.1`)
- **Hash interface**: `src/crypto/randomx_hash.h` and `src/crypto/randomx_hash.cpp` (~346 lines) wrap RandomX for block header hashing
- **Hash input**: 80-byte block header (version + prev_hash + merkle_root + timestamp + nBits + nNonce)
- **Hash output**: 256-bit hash, compared against difficulty target
- **Seed policy**: Fixed genesis seed at all block heights — seed phrase `"RNG Genesis Seed"`, double-SHA256'd to produce the 32-byte key
- **Custom Argon salt**: `"RNGCHAIN01"` (RNG-specific, ensures incompatibility with Monero's RandomX cache)
- **Seed schedule**: Fixed genesis seed at all block heights. `GetRandomXSeedHeight()` returns 0 for every input, so no height-based schedule changes the seed.
- **RandomX modes**:
  - `fast`: ~2 GiB RAM, full dataset in memory, ~500–700 H/s per CPU core
  - `light`: ~256 MiB RAM, dataset computed on-the-fly, significantly slower
- **Mode selection**: `-minerandomx=fast|light` daemon flag (default varies by context; `fast` is the live mainnet setting)
- **JIT compilation**: Supported on x86-64, ARM64, RISCV64; portable interpreter fallback for other architectures
- **Validation path**: `src/pow.cpp` calls into RandomX hash for block validation; every full node verifies RandomX PoW
- **Security audits**: RandomX has 4 independent audits (Trail of Bits, X41 D-SEC, Kudelski, QuarksLab) — these are upstream RandomX audits, not RNG-specific
- **Performance benchmarks** (historical notes, not independently measured):
  - Intel i9-9900K 8 threads: ~5,770 H/s
  - AMD Ryzen 7 1700 8 threads: ~4,100 H/s
  - Intel i7-8550U 4 threads: ~1,700 H/s
  - Server 16-core: ~8,000–10,000 H/s
- **Nonce space**: 4-byte nonce in block header (0 to 2^32-1), plus extra nonce in coinbase transaction for additional entropy

### Recommendations (intended system)

- If protocol-native pooled mining is implemented, shares would use the same RandomX hash with a lower difficulty target. The active sharepool spec confirms `share_target = min(powLimit, block_target * 120)` for 1-second mainnet shares; the older `block_target / 12` sketch is rejected.
- The fixed-seed policy avoids cache invalidation overhead but forecloses periodically changing the RandomX key; a future review may reconsider

### Hypotheses / Unresolved Questions

- Whether the custom Argon salt `RNGCHAIN01` is sufficient to prevent cross-chain dataset reuse attacks (likely yes, but not formally analyzed for RNG)
- Long-term implications of never changing the seed (caching the full dataset once is permanent advantage for fast-mode miners; no periodic recalculation cost)

## Acceptance Criteria

- `CheckProofOfWork()` in `src/pow.cpp` validates blocks using RandomX hash, not SHA256d
- The RandomX cache/dataset is initialized from the fixed seed `SHA256d("RNG Genesis Seed")` with Argon salt `"RNGCHAIN01"`
- The same seed is used at all block heights; the seed does not change
- Blocks whose RandomX hash exceeds the nBits target are rejected
- Both `fast` and `light` modes produce identical hash outputs for the same input
- The `-minerandomx` flag selects between `fast` (2 GiB dataset) and `light` (256 MiB cache) modes
- JIT compilation is automatically used when available on the platform (x86-64, ARM64, RISCV64)
- On architectures without JIT support, the portable interpreter produces correct results
- RandomX library version matches v1.2.1 at `src/crypto/randomx/`

## Verification

```bash
# Verify RandomX is used for PoW (not SHA256d)
grep -r "RandomX" src/pow.cpp src/crypto/randomx_hash.cpp
# Should show RandomX hash calls in PoW validation

# Verify fixed seed policy
grep -r "RNG Genesis Seed" src/
# Should find the seed phrase in chainparams or randomx_hash

# Verify Argon salt
grep -r "RNGCHAIN01" src/
# Should find the custom salt

# Verify RandomX version
head -5 src/crypto/randomx/CMakeLists.txt
# Or check git submodule state

# Verify mining modes work
rngd -mine -minerandomx=fast -mineaddress=... -minethreads=1 &
rng-cli getinternalmininginfo | jq '.hashrate'
# Should show non-zero hashrate

# Run unit tests
./build/src/test/test_bitcoin --run_test=randomx_tests
```

## Open Questions

1. Is the fixed genesis seed the permanent design, or should a future soft fork define a height-based seed schedule?
2. Should RNG-specific RandomX benchmarks be collected and published, or are upstream benchmarks sufficient?
3. What is the validation performance difference between `fast` and `light` mode for non-mining full nodes? Full nodes must verify every block's PoW — the mode choice affects sync speed.
