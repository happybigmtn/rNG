# RNG Bootstrap Snapshot

This directory contains a bundled assumeutxo snapshot for fast RNG mainnet bootstrap.

Snapshot metadata:

| Field | Value |
|-------|-------|
| Height | `15091` |
| Base hash | `2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb` |
| Serialized UTXO hash | `9ca1b551b9837c0b0e9158436bac5051e4984d39f691e1374c4786a6c0ed5393` |
| Chain tx count | `15107` |
| File SHA256 | `622cd6255b8f44380fb9fa51809f783665d54e2d10d2e74135f00aa9ca34c882` |

Usage:

```bash
# from a repo checkout
./scripts/load-bootstrap.sh
./scripts/doctor.sh

# or after install
rng-load-bootstrap
rng-doctor
```

If the datadir starts downloading blocks before the snapshot loads, wipe `blocks/`
and `chainstate/` and retry on a fresh datadir.
