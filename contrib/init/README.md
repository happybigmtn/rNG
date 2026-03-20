Sample configuration files for:
```
systemd: rngd.service (recommended)
legacy upstream examples:
         bitcoind.service
Upstart: bitcoind.conf
OpenRC:  bitcoind.openrc
         bitcoind.openrcconf
CentOS:  bitcoind.init
macOS:   org.bitcoin.bitcoind.plist
```
have been made available to assist packagers in creating node packages here.

RNG operators should start with `rngd.service` and `rng.conf.example`. The remaining files are inherited
upstream examples that may still be useful as references for non-systemd packagers.

See [doc/init.md](../../doc/init.md) for more information.
