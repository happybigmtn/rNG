# Worklist

## Operations

- Repair `contabo-validator-01` startup: `/root/.rng/settings.json` is zero bytes as of 2026-04-13 03:52Z, causing `rngd.service` to crash-loop before RPC starts. After repair, verify the daemon reaches the fleet tip before allowing `mine=1` to resume active mining.
