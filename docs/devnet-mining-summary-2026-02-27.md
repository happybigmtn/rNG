# RNG Devnet Mining Summary (Public)

This is a public-safe summary of the devnet mining rollout and recovery work.
Private operational details (host IPs, wallet file locations, exact payout
addresses, and machine-local paths) are intentionally excluded.

## Scope

- 10-node devnet mining fleet
- RNG and rBTC mining validation
- Wallet ownership validation and mining config hardening

## Final Outcome

- RNG mining is active across all 10 nodes with one mining thread per node.
- rBTC mining is active across all 10 nodes with one mining thread per node.
- RNG miners were migrated from a shared payout pattern to per-node wallet-owned
  payout addresses.
- Post-rollout wallet backups were refreshed and checksum-verified.

## Incident Summary

### Symptom

Some wallet backups appeared to have zero spendable balance.

### Root Cause

Mining outputs were being paid to an address that was not owned by the wallet
being inspected on several nodes.

### Resolution

- Verified on-chain UTXO presence for the payout address.
- Identified a key-owning wallet backup and confirmed funds were recoverable.
- Rolled out per-node payout addresses owned by each node's local miner wallet.
- Restarted nodes in a rolling pattern and re-verified ownership (`ismine=true`).

## Lessons Learned

1. Do not use one shared payout address across a mining fleet.
2. After config changes, always verify payout address ownership from the active
   wallet context.
3. Do not treat zero `trusted` balance alone as proof of fund loss.
   Cross-check with chain-level UTXO scans and key ownership.
4. Run rolling changes with per-node validation gates.
5. Refresh and verify wallet backups immediately after wallet/config migrations.

## Future-Agent Runbook (Public)

1. Baseline each node: height, mining flags, wallet loaded state.
2. Apply change on one node at a time.
3. Restart daemon and verify:
   - mining enabled,
   - expected thread count,
   - payout address owned by local wallet.
4. Re-check fleet convergence after rollout.
5. Produce checksum-verified backup artifacts.

## Privacy Note

This document is intentionally public-safe. Detailed operational records belong
in the private documentation area and must not be committed to public remotes.
