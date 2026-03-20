# RNG Bootstrap Assets

This directory contains bundled live-mainnet bootstrap assets.

Bundled datadir archive metadata:

| Field | Value |
|-------|-------|
| Height | `15244` |
| File | `rng-mainnet-15244-datadir.tar.gz` |
| File SHA256 | `bf0bfad8054c73dc732391f2420d8b9f20f3c8276360745706783079a004c73d` |

Assumeutxo snapshot metadata:

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

On a fresh datadir, the helper prefers the bundled datadir archive first. That
bundle alone is enough to bring a node up near tip. If the bundle is unavailable
or you are retrying on a partially initialized datadir, it falls back to the
assumeutxo snapshot path. On a tagged binary install, `rng-load-bootstrap`
will download the release-matched assets automatically if they are not already
present under `~/.rng/bootstrap`.

If you are bootstrapping a public VPS miner, keep `listen=1` in `rng.conf` and
open `8433/TCP` after the node is up so it can contribute inbound connectivity.
