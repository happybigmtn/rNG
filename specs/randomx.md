# RandomX Proof-of-Work Specification

## Topic
The proof-of-work algorithm that secures the RNG blockchain using CPU-optimized hashing.

## Behavioral Requirements

### Algorithm Selection
RNG uses **RandomX** instead of Bitcoin's SHA-256d because:
- CPU-optimized: AI agents run on standard servers, not ASICs
- ASIC-resistant: Random code execution prevents specialized hardware
- Memory-hard: Large dataset prevents GPU advantage
- Battle-tested: Monero has used it successfully since November 2019
- Audited: Four independent security audits (Trail of Bits, X41 D-SEC, Kudelski, QuarksLab)

### Memory Requirements

| Mode | Memory | Purpose | Performance |
|------|--------|---------|-------------|
| Fast | **2080 MiB** | Mining | ~500-700 H/s per core |
| Light | **256 MiB** | Validation | ~10-15x slower than fast |

**Note**: These are the default RandomX parameters. Fast mode is required for competitive mining; light mode is sufficient for block validation.

### Seed Policy
RNG mainnet currently uses a **fixed genesis seed** for all block heights.

**Live policy:**
- Seed phrase: **`RNG Genesis Seed`**
- Seed hash: `Hash("RNG Genesis Seed")` (Bitcoin-style double-SHA256)
- Seed height: always `0`

This preserves compatibility with the deployed v3.0.0 chain.

### RandomX Configuration

RNG mainnet currently uses a **custom deployed salt**:

| Parameter | Default | RNG | Purpose |
|-----------|---------|---------|---------|
| `RANDOMX_ARGON_SALT` | `"RandomX\x03"` | `"RNGCHAIN01"` | Mainnet compatibility salt |

### Performance Expectations

Based on official RandomX benchmarks (fast mode):

| CPU | Threads | Hashrate |
|-----|---------|----------|
| Intel Core i9-9900K | 8 | ~5,770 H/s |
| AMD Ryzen 7 1700 | 8 | ~4,100 H/s |
| Intel Core i7-8550U | 4 | ~1,700 H/s |
| Server (16 cores) | 16 | ~8,000-10,000 H/s |

**Per-core average: 500-700 H/s** (varies by CPU architecture and memory bandwidth)

### Hash Computation

1. Input: Block header template (hashing blob) with nonce
2. Initialize RandomX VM with current seed hash
3. Execute RandomX program (256 instructions, 2048 iterations, 8 programs)
4. Output: 256-bit hash
5. Compare hash against difficulty target

### Nonce Handling
- Primary nonce: 4 bytes in block header
- Extra nonce: Additional bytes in coinbase transaction for extended search space
- Combined nonce space is sufficient for RandomX's requirements

### Difficulty Encoding
- Use Bitcoin's compact "bits" encoding (unchanged)
- Target calculation: `target = coefficient * 2^(8*(exponent-3))`
- Block valid if: `RandomX_hash(header) <= target`

### JIT Compilation
- RandomX includes JIT compiler for x86-64, ARM64, RISCV64
- Other architectures use portable interpreter (significantly slower)
- JIT provides ~5-10x speedup over interpreter

## Acceptance Criteria

1. [ ] RandomX library compiles and links with RNG
2. [ ] Block validation uses RandomX hash (not SHA-256d)
3. [ ] Valid RandomX proof-of-work is accepted
4. [ ] Invalid RandomX proof-of-work is rejected
5. [ ] Light mode validation works with 256 MiB memory
6. [ ] Fast mode mining works with 2080 MiB memory
7. [ ] Seed hash remains fixed at the genesis seed across all heights
8. [ ] Dataset regenerates when seed hash changes
9. [ ] Mining RPC produces valid RandomX proofs
10. [ ] ARGON_SALT is set to `"RNGCHAIN01"` across builds
11. [ ] Difficulty adjustment works correctly with RandomX
12. [ ] CPU mining achieves expected hashrate (~500-700 H/s per core)

## Test Scenarios

- Mine block with known seed, verify RandomX hash meets target
- Submit block with invalid hash, verify rejection
- Verify headers and full blocks remain valid across arbitrary heights with the fixed seed policy
- Benchmark mining hashrate against expected values
- Verify light mode validation on memory-constrained system (256 MiB)
- Verify fast mode requires ~2 GiB memory allocation
- Test with different CPU architectures (x86-64, ARM64 if available)
- Verify RNG blocks rejected by standard RandomX configurations (different salt)

## References

- RandomX specification: https://github.com/tevador/RandomX/blob/master/doc/specs.md
- RandomX configuration guide: https://github.com/tevador/RandomX/blob/master/doc/configuration.md
- Monero integration: https://github.com/monero-project/monero/blob/master/src/crypto/rx-slow-hash.c
