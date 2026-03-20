RNG init and service configuration
==================================

RNG's recommended long-running Linux service path is the systemd unit in
[`contrib/init/rngd.service`](/contrib/init/rngd.service). The inherited
`bitcoind.*` files remain in the tree as upstream reference material, but
`rngd.service` is the supported public-node example.

## Recommended Linux paths

The packaged service unit assumes:

    Binary:              /usr/bin/rngd
    Configuration file:  /etc/rng/rng.conf
    Data directory:      /var/lib/rngd
    PID file:            /run/rngd/rngd.pid

The service should run as a dedicated `rng` user and group.

## Public-node configuration

At minimum, make sure your config contains:

```ini
server=1
daemon=1
listen=1
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
minerandomx=fast
```

If you want the node to help the network, keep `listen=1` and open `8433/TCP`
through any host firewall and cloud security group. Additional public peers make
mainnet more resilient.

RNG's internal miner uses RandomX JIT. That means the daemon needs executable
memory mappings, so hardening profiles must not enable `MemoryDenyWriteExecute=true`.

## Install the systemd unit

```bash
sudo useradd --system --create-home --home-dir /var/lib/rngd --shell /usr/sbin/nologin rng
sudo install -d -o rng -g rng -m 0710 /etc/rng /var/lib/rngd
sudo install -m 0644 contrib/init/rngd.service /etc/systemd/system/rngd.service
sudo install -m 0600 /path/to/rng.conf /etc/rng/rng.conf
sudo systemctl daemon-reload
sudo systemctl enable --now rngd
```

Verify the node after startup:

```bash
sudo -u rng rng-cli -conf=/etc/rng/rng.conf -datadir=/var/lib/rngd getblockhash 0
sudo -u rng rng-cli -conf=/etc/rng/rng.conf -datadir=/var/lib/rngd getnetworkinfo
```

## Legacy upstream examples

For non-systemd packagers, the inherited upstream examples remain under
[`contrib/init/`](/contrib/init/). They still use Bitcoin naming and paths and
should be treated as templates rather than drop-in RNG units.
