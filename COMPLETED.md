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
