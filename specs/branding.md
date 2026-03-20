# Branding Specification

## Topic
The user-facing identity of RNG software, ensuring consistent naming throughout.

## Behavioral Requirements

### Product Name
- **Full name**: RNG Core
- **Currency name**: RNG
- **Ticker symbol**: RNG
- **Smallest unit**: roshi (1 RNG = 100,000,000 roshi)

### Binary Names
Users interact with these executables:
| Binary | Purpose |
|--------|---------|
| `rngd` | Full node daemon |
| `rng-cli` | Command-line RPC client |
| `rng-qt` | GUI wallet (optional) |
| `rng-tx` | Transaction utility |
| `rng-wallet` | Wallet utility |

### Data Directory
Default locations where RNG stores data:
| OS | Path |
|----|------|
| Linux | `~/.rng/` |
| macOS | `~/Library/Application Support/RNG/` |
| Windows | `%APPDATA%\RNG\` |

### Configuration
- Config file: `rng.conf`
- PID file: `rngd.pid`

### Version Identity
- Version string: `RNG Core daemon version v3.0.0 rngd`
- User agent: `/RNG:3.0.0/`
- Protocol: RNG protocol

### User-Facing Text
All user-visible text must reference "RNG":
- Help messages (`--help`)
- Error messages
- RPC responses
- Log output
- Documentation

### No Bitcoin References
Users must never see "Bitcoin" in normal operation:
- No "BTC" in amount displays
- No "bitcoin" in paths or filenames
- No "satoshi" in unit names (use "roshi")

## Acceptance Criteria

1. [ ] `rngd --version` shows "RNG Core"
2. [ ] `rngd --help` contains no "Bitcoin" references
3. [ ] Default data directory is `~/.rng` (Linux)
4. [ ] Config file is `rng.conf`
5. [ ] All RPC help text references "RNG"
6. [ ] Amount displays use "RNG" not "BTC"
7. [ ] Unit conversion uses "roshi" not "satoshi"
8. [ ] Log files reference "RNG"
9. [ ] Error messages reference "RNG"
10. [ ] Man pages document "rng" commands

## Test Scenarios

- Run `rngd --help`, grep for "bitcoin" (should find none)
- Check data directory creation path
- Verify config file naming
- Check RPC `getnetworkinfo` output for user agent
- Verify amount formatting in `getbalance` response
- Review all error messages for correct branding
