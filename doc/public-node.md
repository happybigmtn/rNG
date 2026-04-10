# Public RNG Node Guide

This guide is for operators who want to do more than solo-mine privately.
Public miners that accept inbound peers improve RNG's resilience and make first
sync easier for everyone else.

## Minimum host expectations

- 64-bit Linux VPS or bare-metal host
- at least 2 CPU cores
- at least 4 GiB RAM if you want RandomX `fast` mode with headroom
- a stable public IPv4 address is preferred

## Install and bootstrap

Use the tagged-release install path unless you are intentionally testing
unreleased source changes:

```bash
curl -fsSLO https://github.com/happybigmtn/RNG/releases/latest/download/install.sh
less install.sh
bash install.sh --add-path --bootstrap
rng-load-bootstrap
rng-start-miner
rng-doctor
```

If `rngd` is already installed on the host, the fastest systemd path is:

```bash
sudo rng-public-apply --address rng1... --enable-now
```

For a new public VPS, the simplest sequence is:

```bash
curl -fsSLO https://github.com/happybigmtn/RNG/releases/latest/download/install.sh
bash install.sh --add-path --bootstrap
sudo rng-public-apply --address rng1... --enable-now
sudo ufw allow 8433/tcp
```

## Required config for a public peer

Make sure `~/.rng/rng.conf` or `/etc/rng/rng.conf` includes:

```ini
server=1
daemon=0
listen=1
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
minerandomx=fast
```

`listen=1` is the key difference between a private miner and a public peer.

## Open the public port

Expose `8433/TCP` to the host:

- `ufw allow 8433/tcp`
- your cloud security group or firewall must also allow inbound `8433/TCP`
- keep RPC port `8432` private; it should stay bound to localhost

## Verify public reachability

After the node is synced, check:

```bash
rng-cli getnetworkinfo
rng-doctor --json --strict --expect-public --expect-miner
```

Healthy public peers usually show:

- nonzero `connections_out`
- eventually nonzero `connections_in`
- a non-empty `localaddresses` list once the node has a reachable external address

If mining is running but `rng-doctor` warns that the node is not reachable as a
public peer, check:

- `listen=1` is still set
- `8433/TCP` is open on both the host and the cloud firewall
- no reverse proxy or container network is hiding the daemon from the public interface

## Run under systemd

For long-running VPS nodes, use the packaged helper or install the assets manually.

Fast path:

```bash
sudo rng-public-apply --address rng1... --enable-now
```

Manual path:

```bash
sudo useradd --system --create-home --home-dir /var/lib/rngd --shell /usr/sbin/nologin rng
sudo install -d -o rng -g rng -m 0710 /etc/rng /var/lib/rngd
sudo install -m 0644 contrib/init/rngd.service /etc/systemd/system/rngd.service
sudo install -m 0600 contrib/init/rng.conf.example /etc/rng/rng.conf
sudo systemctl daemon-reload
sudo systemctl enable --now rngd
```

Important: RandomX uses JIT-generated code. Do not add
`MemoryDenyWriteExecute=true` to the service unit or RandomX fast mode may fail.

## Run mining persistently under systemd

If you want to manage the pieces manually instead of using `rng-public-apply`, add the mining override after the base node is installed:

```bash
sudo rng-install-public-miner --address rng1...
sudo systemctl restart rngd
```

By default the helper:

- keeps RandomX in `fast` mode
- uses `CPU count - 1` mining threads
- sets `Nice=19` in the service override

Optional flags:

```bash
sudo rng-install-public-miner --address rng1... --threads 7 --priority low --randomx fast
```

To remove the mining override and return the host to node-only mode:

```bash
sudo rng-install-public-miner --remove
sudo systemctl restart rngd
```
