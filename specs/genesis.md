# Genesis Block Specification

This document reflects the live RNG mainnet chain as of March 19, 2026.

## Mainnet Genesis

| Field | Value |
|-------|-------|
| Height | `0` |
| Version | `0x20000000` |
| Previous hash | `0x00...00` |
| Timestamp message | `Life is a random number generator` |
| Hash | `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4` |
| Merkle root | `b713a92ad8104e5a1650d02f96df9cb18bd6a39a222829ba4e4b5e79e4de7232` |
| Initial reward | `50 * COIN` |
| Output type | `OP_RETURN` commitment |

## RandomX Parameters

The live chain uses RandomX for proof of work.

Externally verifiable mainnet constants:

- Genesis hash: `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`
- Merkle root: `b713a92ad8104e5a1650d02f96df9cb18bd6a39a222829ba4e4b5e79e4de7232`
- RandomX genesis seed phrase: `RNG Genesis Seed`
- RandomX ARGON salt: `RNGCHAIN01`
- RandomX seed policy: fixed genesis seed for all heights
- Bundled assumeutxo snapshot height: `15091`

## Difficulty Parameters

Mainnet currently uses:

- Target block interval: `120` seconds
- Per-block retargeting
- Difficulty window: `720` blocks
- Timestamp cut: `60`

## Coinbase Structure

The genesis coinbase:

- uses the standard null prevout
- embeds the message `Life is a random number generator`
- creates a single `OP_RETURN` output

The output is intentionally unspendable.

## Reset History

RNG mainnet was restarted from genesis on February 26, 2026.

Operational checks for any new node:

```bash
rngd -daemon
sleep 10
rng-cli getblockhash 0
```

Expected result:

```text
83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4
```

## Supported Snapshot

| Field | Value |
|-------|-------|
| Height | `15091` |
| Base hash | `2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb` |
| Serialized UTXO hash | `9ca1b551b9837c0b0e9158436bac5051e4984d39f691e1374c4786a6c0ed5393` |
| Chain tx count | `15107` |
