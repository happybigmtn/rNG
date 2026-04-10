# QSB Tooling

This directory holds the in-repo QSB builder and operator tooling for RNG.

The current state is the first end-to-end toy-QSB operator slice from [EXECPLAN.md](/EXECPLAN.md): it proves the bare-script funding path, the external spend assembly path, the node-side `submitqsbtransaction` queue, and miner-local inclusion on top of the Bitcoin Core 30.2 port.

## What It Does Today

`qsb.py` currently supports three builder commands:

- `fixture`
  - Deterministically derives a versioned bare-script template from a seed and writes a JSON fixture.
- `toy-funding`
  - Builds a raw transaction with a large bare `scriptPubKey`.
  - Uses `fundrawtransaction` to attach ordinary wallet inputs and change.
  - Uses `signrawtransactionwithwallet` only for those ordinary wallet inputs.
  - Writes a state file containing the one-time secret, the funding script, and the funded transaction hex.
- `toy-spend`
  - Builds the matching spend entirely outside the wallet.
  - Uses the state file’s one-time secret as the unlocking `scriptSig`.
  - Marks the state file as consumed so the tool refuses a second spend.

`submit_fleet.py` currently supports five operator commands:

- `info`
  - Probes one or more remote validators over SSH and reports chain tip plus QSB queue status.
- `list`
  - Lists the locally queued QSB candidates on one or more remote validators.
- `submit`
  - Reads raw hex directly, from a hex file, or from a QSB state file and queues it with `submitqsbtransaction`.
- `remove`
  - Removes a queued tx by txid from one or more validators.
- `wait-mined`
  - Polls recent blocks on one or more validators until the requested txid is mined.

## What It Does Not Do Yet

- This is still the strict `rng-qsb-v1-toy` template, not the paper-faithful HORS-based construction from `Quantum-Safe-Bitcoin-Transactions`.
- The QSB queue is still in-memory only; queued candidates disappear on restart.
- `qsb.py` still talks to a wallet RPC endpoint directly, so live funding normally uses an SSH port-forward or runs on the validator host itself.
- There is not yet a broader wallet UX, descriptor integration, or persistent operator state inside the node.

Those pieces belong to later implementation units in `EXECPLAN.md`.

## Why This Spike Exists

The risky unknowns are operational, not cryptographic:

- Can RNG carry a consensus-valid bare script larger than 520 bytes directly in `scriptPubKey`?
- Can the wallet fund that output without owning the custom secret state?
- Can the matching spend be assembled entirely outside the wallet?
- Can both transactions be mined directly even though public mempool policy rejects them?

This implementation answers those questions with a minimal template that preserves the important mechanics:

- the locking script is bare and larger than 520 bytes,
- the spend requires external secret material,
- the wallet only funds the output,
- the state file lives outside the wallet, and
- the transactions remain non-standard for public relay,
- the operator queues them locally instead of relaying them publicly, and
- the miner includes them directly from the local QSB queue.

## Usage

Deterministic fixture:

```bash
python3 contrib/qsb/qsb.py fixture \
  --seed 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f \
  --state-file /tmp/qsb-fixture.json
```

Toy funding:

```bash
python3 contrib/qsb/qsb.py toy-funding \
  --rpc-url http://127.0.0.1:18443 \
  --rpc-cookie-file /path/to/regtest/.cookie \
  --wallet default_wallet \
  --amount 1.0 \
  --fee-rate-sat-vb 1 \
  --state-file /tmp/qsb-state.json
```

The funding path passes `fee_rate` to `fundrawtransaction` so low-history private chains do not depend on wallet fee estimation or `fallbackfee`.

Toy spend:

```bash
python3 contrib/qsb/qsb.py toy-spend \
  --rpc-url http://127.0.0.1:18443 \
  --rpc-cookie-file /path/to/regtest/.cookie \
  --state-file /tmp/qsb-state.json \
  --destination-address trng1... \
  --fee-sats 1000
```

Inspect a validator's QSB status over SSH:

```bash
python3 contrib/qsb/submit_fleet.py info \
  --host contabo-validator-01
```

Queue the funding transaction from a saved state file:

```bash
python3 contrib/qsb/submit_fleet.py submit \
  --host contabo-validator-01 \
  --state-file /tmp/qsb-state.json \
  --kind funding
```

Wait for that funding transaction to be mined:

```bash
python3 contrib/qsb/submit_fleet.py wait-mined \
  --host contabo-validator-01 \
  --state-file /tmp/qsb-state.json \
  --kind funding
```

## File Layout

- `template_v1.py`
  - versioned bare-script template generation
- `state.py`
  - state-file and fixture-file helpers
- `qsb.py`
  - CLI entrypoint
- `submit_fleet.py`
  - SSH wrapper around validator-local `rng-cli` for queue inspection, submission, and confirmation polling
- `fixtures/`
  - deterministic fixtures for future C++ matcher and validator tests
