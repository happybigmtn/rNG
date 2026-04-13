# RandomX Proof-of-Work

## Current Behavior

RNG uses RandomX for block proof-of-work. The live consensus behavior is a fixed
genesis seed for every block height.

## Seed Policy

- The seed phrase is `"RNG Genesis Seed"`.
- `GetRandomXSeedHash()` in `src/pow.cpp` hashes that phrase when checking
  genesis, when no previous block index is available, and whenever the seed
  height resolves to `0`.
- `GetRandomXSeedHeight()` in `src/crypto/randomx_hash.cpp` returns `0` for
  every input height.
- The same genesis seed is therefore used for all block heights.
- No height-based schedule changes the seed under the current consensus rules.

## RandomX Parameters

- RandomX v1.2.1 is vendored in `src/crypto/randomx/`.
- Block headers are serialized as the standard 80-byte header before hashing.
- RandomX returns a 256-bit proof-of-work hash, which is compared with the
  target derived from `nBits`.
- RNG uses the custom Argon salt `"RNGCHAIN01"` in the patched RandomX build.
- Mining supports `fast` and `light` modes through `-minerandomx=fast|light`.

## Code Evidence

- `src/pow.cpp` contains `GetRandomXSeedHash()`, `GetBlockPoWHash()`, and
  `CheckBlockProofOfWork()`.
- `src/crypto/randomx_hash.cpp` contains the fixed-height seed helper and the
  RandomX context wrappers.
- `src/test/randomx_tests.cpp` asserts that the seed height is `0` across
  representative block heights.

## Verification

```bash
grep "fixed\|genesis.*seed\|all.*height" specs/randomx.md
build/bin/test_bitcoin --run_test=randomx_tests
```
