# QSB Operator Runbook

This runbook documents the operator-only QSB path that now exists in the Bitcoin Core `30.2` RNG port. It matches the current toy implementation, not the full paper-faithful HORS construction.

## Current Constraints

- The only supported template is `rng-qsb-v1-toy`.
- Public relay policy is unchanged. QSB transactions must be queued locally with `submitqsbtransaction`.
- The local QSB queue is in-memory only. Restarting `rngd` drops any queued-but-unmined QSB candidates.
- Live funding still requires wallet RPC access, so the simplest live workflow is an SSH port-forward into the validator's localhost RPC port.

## Local Build

Build the canary binaries from the `30.2` worktree:

```bash
cmake -B build -DENABLE_WALLET=ON -DBUILD_TESTING=ON -DWITH_ZMQ=OFF -DENABLE_IPC=OFF
cmake --build build --target rngd rng-cli test_bitcoin -j"$(nproc)"
python3 test/functional/feature_qsb_builder.py --configfile=build/test/config.ini
python3 test/functional/feature_qsb_rpc.py --configfile=build/test/config.ini
python3 test/functional/feature_qsb_mining.py --configfile=build/test/config.ini
./build/bin/test_bitcoin --run_test=qsb_tests --catch_system_error=no --log_level=test_suite
```

## Canary Deployment

Preserve the current root-managed validator layout for the first rollout. The known live hosts use `/root/rngd`, `/root/rng-cli`, and `/root/.rng`.

Back up the current canary binaries and config:

```bash
ssh contabo-validator-01 'cp /root/rngd /root/rngd.pre-qsb.$(date -u +%Y%m%dT%H%M%SZ)'
ssh contabo-validator-01 'cp /root/rng-cli /root/rng-cli.pre-qsb.$(date -u +%Y%m%dT%H%M%SZ)'
ssh contabo-validator-01 'cp /root/.rng/rng.conf /root/.rng/rng.conf.pre-qsb.$(date -u +%Y%m%dT%H%M%SZ)'
```

Deploy the new binaries:

```bash
scp build/bin/rngd contabo-validator-01:/root/rngd.new
scp build/bin/rng-cli contabo-validator-01:/root/rng-cli.new
ssh contabo-validator-01 'install -m 0755 /root/rngd.new /root/rngd'
ssh contabo-validator-01 'install -m 0755 /root/rng-cli.new /root/rng-cli'
```

If the host still uses the older root-managed `Type=forking` service unit, install a `30.2` compatibility drop-in before startup. The newer daemon path expects systemd notifications instead of the old fork-and-PID contract:

```bash
ssh contabo-validator-01 "install -d /etc/systemd/system/rngd.service.d && cat > /etc/systemd/system/rngd.service.d/30.2-compat.conf <<'EOF'
[Service]
Type=notify
NotifyAccess=all
TimeoutStartSec=infinity
ExecStart=
ExecStart=/root/rngd -pid=/root/.rng/rngd.pid -conf=/root/.rng/rng.conf -datadir=/root/.rng -walletcrosschain=1 -wallet=miner -startupnotify='systemd-notify --ready' -shutdownnotify='systemd-notify --stopping'
EOF"
ssh contabo-validator-01 'systemctl daemon-reload'
```

Enable the operator surface in `/root/.rng/rng.conf` if it is not already present:

```bash
ssh contabo-validator-01 "grep -q '^enableqsboperator=1$' /root/.rng/rng.conf || echo 'enableqsboperator=1' >> /root/.rng/rng.conf"
```

Set an explicit miner thread count before the canary proof. The first live proof used eight fast-mode RandomX workers on a 12-core Contabo host; leaving `minethreads` unset fell back to one worker and made the canary block wait unnecessarily long:

```bash
ssh contabo-validator-01 "grep -q '^minethreads=' /root/.rng/rng.conf && sed -i 's/^minethreads=.*/minethreads=8/' /root/.rng/rng.conf || echo 'minethreads=8' >> /root/.rng/rng.conf"
```

For non-canary validators that are still catching up, keep mining disabled until `initialblockdownload=false`. This avoids wasting CPU on stale local templates while the upgraded node replays the current chain:

```bash
ssh contabo-validator-02 "sed -i 's/^mine=.*/mine=0/' /root/.rng/rng.conf"
```

Start the canary and verify the node identity:

```bash
ssh contabo-validator-01 'systemctl enable --now rngd'
python3 contrib/qsb/submit_fleet.py info --host contabo-validator-01
ssh contabo-validator-01 '/root/rng-cli -conf=/root/.rng/rng.conf -datadir=/root/.rng getblockhash 0'
```

The expected mainnet genesis hash is:

```text
83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4
```

## Live Funding And Submission

Open an SSH tunnel so the local builder can talk to the validator's wallet RPC:

```bash
ssh -f -N -L 28435:127.0.0.1:18435 -o ExitOnForwardFailure=yes contabo-validator-01
```

Build the toy funding transaction locally against the forwarded RPC:

```bash
python3 contrib/qsb/qsb.py toy-funding \
  --rpc-url http://127.0.0.1:28435 \
  --rpc-user agent \
  --rpc-password <rpcpassword> \
  --wallet miner \
  --amount 0.10000000 \
  --fee-rate-sat-vb 1 \
  --state-file /tmp/qsb-canary.json
```

`--fee-rate-sat-vb` is intentionally explicit. Live RNG chains may not have enough fee history for wallet fee estimation, and production validator configs should not need `fallbackfee` just to build the canary transaction.

Queue it on the canary:

```bash
python3 contrib/qsb/submit_fleet.py submit \
  --host contabo-validator-01 \
  --state-file /tmp/qsb-canary.json \
  --kind funding
```

Watch the queue and confirmation:

```bash
python3 contrib/qsb/submit_fleet.py list --host contabo-validator-01
python3 contrib/qsb/submit_fleet.py wait-mined \
  --host contabo-validator-01 \
  --state-file /tmp/qsb-canary.json \
  --kind funding
```

## Live Spend And Submission

Once the funding transaction is mined, build the spend:

```bash
python3 contrib/qsb/qsb.py toy-spend \
  --state-file /tmp/qsb-canary.json \
  --destination-address rng1q... \
  --fee-sats 1000
```

Queue the spend:

```bash
python3 contrib/qsb/submit_fleet.py submit \
  --host contabo-validator-01 \
  --state-file /tmp/qsb-canary.json \
  --kind spend
```

Wait for the spend to be mined:

```bash
python3 contrib/qsb/submit_fleet.py wait-mined \
  --host contabo-validator-01 \
  --state-file /tmp/qsb-canary.json \
  --kind spend
```

## Fleet Rollout

After the canary has mined both funding and spend successfully, roll the same binaries to the remaining reachable validators one at a time:

```bash
for host in contabo-validator-02 contabo-validator-04 contabo-validator-05; do
  ssh "$host" 'cp /root/rngd /root/rngd.pre-qsb.$(date -u +%Y%m%dT%H%M%SZ)'
  ssh "$host" 'cp /root/rng-cli /root/rng-cli.pre-qsb.$(date -u +%Y%m%dT%H%M%SZ)'
  scp build/bin/rngd "$host":/root/rngd.new
  scp build/bin/rng-cli "$host":/root/rng-cli.new
  ssh "$host" 'install -m 0755 /root/rngd.new /root/rngd'
  ssh "$host" 'install -m 0755 /root/rng-cli.new /root/rng-cli'
  ssh "$host" "grep -q '^enableqsboperator=1$' /root/.rng/rng.conf || echo 'enableqsboperator=1' >> /root/.rng/rng.conf"
  ssh "$host" "grep -q '^minethreads=' /root/.rng/rng.conf && sed -i 's/^minethreads=.*/minethreads=8/' /root/.rng/rng.conf || echo 'minethreads=8' >> /root/.rng/rng.conf"
  ssh "$host" "sed -i 's/^mine=.*/mine=0/' /root/.rng/rng.conf"
  ssh "$host" 'systemctl enable --now rngd'
  python3 contrib/qsb/submit_fleet.py info --host "$host"
done
```

After each host reports `initialblockdownload=false` and matches the canary tip family, re-enable mining with `sed -i 's/^mine=.*/mine=1/' /root/.rng/rng.conf` and restart `rngd`.

## Rollback

If the updated daemon fails to start, diverges, or drops a queued QSB candidate before it is mined:

```bash
python3 contrib/qsb/submit_fleet.py list --host contabo-validator-01
python3 contrib/qsb/submit_fleet.py remove --host contabo-validator-01 --txid <queued-txid>
ssh contabo-validator-01 'install -m 0755 /root/rngd.pre-qsb.<timestamp> /root/rngd'
ssh contabo-validator-01 'install -m 0755 /root/rng-cli.pre-qsb.<timestamp> /root/rng-cli'
ssh contabo-validator-01 'install -m 0600 /root/.rng/rng.conf.pre-qsb.<timestamp> /root/.rng/rng.conf'
ssh contabo-validator-01 'systemctl restart rngd'
```

The queue is not persistent, so any queued-but-unmined QSB transactions must be resubmitted from the saved state file after restart:

```bash
python3 contrib/qsb/submit_fleet.py submit \
  --host contabo-validator-01 \
  --state-file /tmp/qsb-canary.json \
  --kind funding
```
