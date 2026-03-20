RNG Security Review - 2026-03-19
================================

Scope: RNG-specific consensus, RandomX proof-of-work, and internal miner paths introduced on top of Bitcoin Core.

## Findings fixed in this pass

### 1. Invalid miner mode and priority values were silently coerced

Status: fixed in [`src/init.cpp`](/src/init.cpp)

Previously:

- any `-minerandomx` value other than `fast` was treated as `light`
- any `-minepriority` value other than `low` was treated as `normal`

Impact:

- operators could believe they were running one mining mode while actually running another
- typoed configs were accepted instead of failing closed

Current behavior:

- `-minerandomx` must be `fast` or `light`
- `-minepriority` must be `low` or `normal`
- invalid values now abort startup with an explicit init error

### 2. `-mineaddress` validation was hardcoded to mainnet HRP

Status: fixed in [`src/init.cpp`](/src/init.cpp)

Previously, internal miner startup required addresses beginning with `rng1`, which breaks regtest/testnet/signet where the HRP differs.

Current behavior:

- address validation uses `Params().Bech32HRP()`
- the miner can start correctly on non-mainnet chains

### 3. RandomX initialization failures could terminate the process

Status: fixed in [`src/crypto/randomx_hash.cpp`](/src/crypto/randomx_hash.cpp), [`src/pow.cpp`](/src/pow.cpp), [`src/validation.cpp`](/src/validation.cpp), and [`src/rpc/mining.cpp`](/src/rpc/mining.cpp)

Previously, dataset/cache/VM allocation failures from RandomX setup could escape miner and validation call paths as exceptions.

Impact:

- worker threads could hit `std::terminate`
- block/header validation or RPC mining helpers could crash the daemon instead of failing safely

Current behavior:

- miner VM initialization catches setup failures
- validation and RPC PoW paths convert RandomX failures into rejection/error paths
- the miner now falls back from fast mode to light mode if fast mode initialization fails

### 4. Miner shutdown could abort during late node teardown

Status: fixed in [`src/init.cpp`](/src/init.cpp)

Previously, `InternalMiner` was often destroyed after `Shutdown()` had already reset node infrastructure such as validation signals and chainstate references.

Impact:

- clean daemon shutdown could abort with `std::system_error`

Current behavior:

- the internal miner is explicitly stopped and reset during `Shutdown()`
- teardown now happens while dependent node objects are still alive

### 5. Internal miner status could misreport active RandomX mode

Status: fixed in [`src/crypto/randomx_hash.h`](/src/crypto/randomx_hash.h), [`src/crypto/randomx_hash.cpp`](/src/crypto/randomx_hash.cpp), and [`src/node/internal_miner.cpp`](/src/node/internal_miner.cpp)

Previously, the miner advertised automatic fast-to-light fallback but did not surface the actual mode used by worker VMs.

Current behavior:

- worker initialization records the mode actually in use
- `getinternalmininginfo` can now reflect real fast/light mode state after fallback

## New coverage

- Added [`test/functional/feature_internal_miner.py`](/test/functional/feature_internal_miner.py) to cover:
  - regtest internal-miner startup
  - chain-aware address handling
  - explicit rejection of invalid `-minerandomx` values
  - explicit rejection of invalid `-minepriority` values

## Remaining risks

These are not consensus bugs found in code review, but they still block a strong "production-ready public chain" claim:

- the network remains operator-seeded and low-peer
- release trust was previously branch-first rather than tag-first; the repo now includes the release workflow, but it still depends on maintainers actually cutting tags
- the RandomX vendoring flow still deserves follow-up so normal configure/build cycles do not dirty the vendored checkout
