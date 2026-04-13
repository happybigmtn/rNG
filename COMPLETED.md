# COMPLETED

## POOL-07F Witness-v2 Settlement Program Verification

- Completed witness-v2 settlement program verification and interpreter dispatch.
- Added `SCRIPT_VERIFY_SHAREPOOL` and sharepool-specific script errors.
- Added `VerifySharepoolSettlement()` for descriptor, leaf-index, payout-branch,
  status-branch, and state-hash verification.
- Added `sharepool_verification_tests` coverage for valid solo/multi-leaf
  claims, malformed stack/version/index/leaf/branch cases, double-claim state,
  pre-activation compatibility, activated malformed-v2 failure, and other future
  witness-version compatibility.
- Fixed `sharepool_commitment_tests` vector path discovery so the sharepool
  regression suite runs from the CTest build working directory.
- Validation:
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build -R sharepool_verification --output-on-failure`
  - `ctest --test-dir build -R sharepool --output-on-failure`
  - `ctest --test-dir build -R 'sharepool|script_tests|transaction_tests' --output-on-failure`
- Commit: `2a41cb846e434ce36e957cd99beeff7ef8189c4c`

## CHKPT-07A Solo Settlement Claim Verification Checkpoint

- Confirmed the witness-v2 settlement verifier and interpreter dispatch are
  present while `ConnectBlock` still does not enable `SCRIPT_VERIFY_SHAREPOOL`.
- Repaired stale fork-drift tests that blocked the checkpoint suite: CLI help
  examples now expect `rng-cli`, Taproot address vectors expect RNG HRPs,
  message verification uses live RNG addresses/signatures, and regtest
  assumeutxo metadata matches this chain.
- Recorded the live monetary-schedule mismatch in `WORKLIST.md` instead of
  widening this checkpoint into a consensus-parameter change.
- Manually exercised regtest solo settlement claiming: activated sharepool,
  mined a solo settlement output, reconstructed the descriptor/leaf/state hash,
  spent the matured settlement output with a five-element witness stack, and
  confirmed the claim transaction entered the mempool.
- Validation:
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - `python3 build/test/functional/feature_sharepool_relay.py --configfile=/home/r/Coding/RNG/build/test/config.ini --timeout-factor=4`
  - `python3 - --configfile=/home/r/Coding/RNG/build/test/config.ini --timeout-factor=4` (stdin manual solo settlement claim script)
- Notes:
  - The repository-root `test/functional/test_runner.py feature_sharepool_relay`
    invocation looked for a missing root `test/config.ini`, and the build-tree
    runner did not register `feature_sharepool_relay`. The direct generated
    functional script was used and passed.
  - One existing full-suite skip remains: `script_assets_tests`.
- Commit: `8ba5c596049309355b7984e85feb98dc1e260125`
