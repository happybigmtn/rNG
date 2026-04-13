# Specification: QSB Operator Support

## Objective

Define the Quantum-Safe Bitcoin (QSB) operator transaction support as documented in `EXECPLAN.md`. QSB adds narrow policy hooks allowing operator-controlled validators to mine special funding and spend transactions with post-quantum properties. This is an operator-only feature — it does not change public consensus policy.

## Evidence Status

### Verified Facts (grounded in EXECPLAN.md and repository state)

**Implementation exists in the inspected checkout on `main`:**
- `src/script/qsb.h` and `src/script/qsb.cpp`
- `src/node/qsb_pool.h`, `src/node/qsb_pool.cpp`, `src/node/qsb_validation.h`, and `src/node/qsb_validation.cpp`
- `src/rpc/qsb.cpp`
- `contrib/qsb/` Python builder and fleet submission helpers
- `test/functional/feature_qsb_builder.py`, `test/functional/feature_qsb_rpc.py`, `test/functional/feature_qsb_mining.py`, and `src/test/qsb_tests.cpp`

**Live canary proof (from EXECPLAN.md progress log)**:
- Canary validator: `contabo-validator-01` at `95.111.227.14`
- QSB funding transaction: `363a3e5063d34c1ca775fdf5e93aeb18567d17489fce2ccdef14e6fdcdfec2e3`
- Funding block: `b3268a02f1613a8020a79d86db8482516fcf1593c88a7b859a74fbe611810ac9` (height 29946)
- QSB spend transaction: `e562d60c7601e120742483cdd7f737383c424e4f55d65bc64f87fe24648fe2b8`
- Spend block: `b755640d4309a4869cc0bd70947221250d16709fa40fc46f1df37596570a5d2e` (height 29947)
- Both funding and spend transactions mined and confirmed on live mainnet

**Fleet rollout status (from EXECPLAN.md, as of 2026-04-10)**:
- Canary (validator-01): QSB proven, mining with 8 threads
- Wave 2 (validators 02, 04, 05): Binary deployed, syncing with catch-up overrides (`minimumchainwork=0`, `mine=0` during IBD)
- Binary hashes:
  - `rngd`: SHA256 `36eb7509a17c15fbca062dc3427bb36d0d19cb24ec4fb299fcea09e20a5ad054`
  - `rng-cli`: SHA256 `eff7e8d116b8143f4182197e482804b74a49d8885915e24ab23eec6b3f67b92a`
  - Both report: RNG Core `v3.0.0`

**Operator infrastructure details**:
- Validator RPC port: varies (`18435` on validator-01, may differ on others)
- Binary paths: `/root/rngd`, `/root/rng-cli`
- Data directory: `/root/.rng`
- Config: `/root/.rng/rng.conf`
- Systemd service: `rngd.service` (upgraded from `Type=forking` to `Type=notify`)
- Systemd drop-in for notify: `-startupnotify='systemd-notify --ready' -shutdownnotify='systemd-notify --stopping'`
- Binary backup pattern: `/root/rngd.pre-qsb.<timestamp>`
- Config backup pattern: `/root/.rng/rng.conf.pre-*.<timestamp>`

**Activation mechanism** (from EXECPLAN.md):
- Flag: `-enableqsboperator` (daemon flag to enable QSB policy hooks)
- RPC: `submitqsbtransaction` (submit QSB candidate transactions)
- QSB candidate pool: local-only (not relayed to non-QSB peers)
- Policy hooks in `src/validation.cpp` — narrow acceptance rules for QSB patterns

### Recommendations (from EXECPLAN.md intent)

- QSB is scoped as operator-only and must NOT change public consensus policy
- Fleet deployment should normalize only after all validators sync to the same best block and exit IBD
- Catch-up overrides (`minimumchainwork=0`) must be reverted before production mining resumes
- Keep QSB scoped as operator-only after merge; future review should continue to verify that public consensus policy is unchanged for standard transactions

### Hypotheses / Unresolved Questions

- Whether QSB-enabled nodes accept QSB transactions into the regular mempool or only the local QSB candidate pool
- Whether non-QSB nodes can validate blocks containing QSB transactions (soft-fork compatibility)
- Whether the Bitcoin Core v30.2 port introduces breaking changes relative to the prior v29.0-based consensus
- Whether `contrib/qsb/` Python builder is a production tool or a test harness
- Timeline for QSB code to land on main branch

## Acceptance Criteria

These criteria are based on EXECPLAN.md claims. They are testable only on validators running the QSB-enabled branch.

- `-enableqsboperator` flag enables QSB policy hooks without affecting standard transaction processing
- `submitqsbtransaction` RPC accepts valid QSB candidate transactions into the local candidate pool
- QSB funding transactions are accepted into blocks mined by QSB-enabled operators
- QSB spend transactions are accepted into blocks mined by QSB-enabled operators
- Non-QSB nodes accept blocks containing QSB transactions (they appear as standard/anyone-can-spend from the non-QSB node's perspective)
- The QSB candidate pool is local-only — QSB candidates are not relayed via standard P2P inventory messages
- Enabling QSB does not change behavior for standard (non-QSB) transactions
- All four Contabo validators report the same best block hash after fleet synchronization
- All validators exit IBD (`initialblockdownload=false`) before mining is re-enabled
- Catch-up overrides are reverted after sync completes

## Verification

Verification requires access to the QSB-enabled branch and validator fleet. From EXECPLAN.md:

```bash
# On canary validator (contabo-validator-01)
ssh root@95.111.227.14

# Verify QSB funding tx exists
rng-cli getrawtransaction 363a3e5063d34c1ca775fdf5e93aeb18567d17489fce2ccdef14e6fdcdfec2e3 1
# Should return decoded transaction with QSB funding pattern

# Verify QSB spend tx exists
rng-cli getrawtransaction e562d60c7601e120742483cdd7f737383c424e4f55d65bc64f87fe24648fe2b8 1
# Should return decoded transaction with QSB spend pattern

# Verify binary version
rngd --version
# Expected: RNG Core v3.0.0

# Verify binary hash
sha256sum /root/rngd
# Expected: 36eb7509a17c15fbca062dc3427bb36d0d19cb24ec4fb299fcea09e20a5ad054

# Verify fleet health (all validators same best block)
for host in 95.111.227.14 95.111.229.108 161.97.83.147 161.97.97.83; do
  ssh root@$host "rng-cli getbestblockhash"
done
# All should return the same hash

# Verify IBD complete
rng-cli getblockchaininfo | jq '.initialblockdownload'
# Expected: false
```

## Open Questions

1. **Interaction with sharepool**: If sharepool claims use witness v2 and QSB uses its own script patterns, are there conflict scenarios?
2. **Rollback plan**: What is the procedure if QSB transactions cause issues on mainnet? Can they be orphaned without chain reorg?
3. **Public documentation**: Should QSB be documented in public-facing specs, or remain operator-internal?
4. **Bitcoin Core v30.2 delta**: What consensus or P2P changes from Bitcoin Core v30.2 (vs v29.0) are included in the port? Are any of them independently valuable beyond QSB?
5. **Validator RPC port inconsistency**: Validator-01 uses port `18435` while the standard RPC port is `8432`. Is this intentional per-validator customization?
