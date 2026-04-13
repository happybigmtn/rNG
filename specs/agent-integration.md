# Agent Integration Specification

## Current Status

This document separates the agent-facing interfaces that exist today from
agent-first features that are still planned. The current codebase exposes the
standard Bitcoin Core-derived wallet RPC surface, RNG's internal miner flags,
the `getinternalmininginfo` RPC, and the operator-only QSB RPC. It does not yet
include a dedicated agent wallet RPC, MCP server, webhook system, autonomy
budget framework, agent identity registry, or external pool-mining command.

Verified implemented surfaces:

| Surface | Status | Code evidence |
|---------|--------|---------------|
| `createwallet` | Implemented | `src/wallet/rpc/wallet.cpp:347`, registered at `src/wallet/rpc/wallet.cpp:918` |
| `getnewaddress` | Implemented | `src/wallet/rpc/addresses.cpp:21`, registered at `src/wallet/rpc/wallet.cpp:926` |
| `getbalance` | Implemented | `src/wallet/rpc/coins.cpp:164`, registered at `src/wallet/rpc/wallet.cpp:924` |
| `getbalances` | Implemented | `src/wallet/rpc/coins.cpp:402`, registered at `src/wallet/rpc/wallet.cpp:931` |
| `sendtoaddress` | Implemented | `src/wallet/rpc/spend.cpp:238`, registered at `src/wallet/rpc/wallet.cpp:954` |
| `sendmany` | Implemented | `src/wallet/rpc/spend.cpp:336`, registered at `src/wallet/rpc/wallet.cpp:953` |
| `getmininginfo` | Implemented | `src/rpc/mining.cpp:426`, registered at `src/rpc/mining.cpp:1210` |
| `getinternalmininginfo` | Implemented | `src/rpc/mining.cpp:510`, registered at `src/rpc/mining.cpp:1211` |
| `submitqsbtransaction` | Implemented, operator-only | `src/rpc/qsb.cpp:83`, registered at `src/rpc/qsb.cpp:266` |
| `-mine` | Implemented | `src/init.cpp:690`, startup handling at `src/init.cpp:2181` |
| `-mineaddress=<addr>` | Implemented | `src/init.cpp:691`, validation at `src/init.cpp:2182`, used by `scripts/start-miner.sh:154` |
| `-minethreads=<n>` | Implemented | `src/init.cpp:692`, validation at `src/init.cpp:2183`, used by `scripts/start-miner.sh:154` |
| `-minerandomx=fast|light` | Implemented | `src/init.cpp:693`, validation at `src/init.cpp:2197`, used by `scripts/start-miner.sh:154` |
| `-minepriority=low|normal` | Implemented | `src/init.cpp:694`, validation at `src/init.cpp:2203`, used by `scripts/start-miner.sh:154` |

Explicitly not implemented surfaces:

| Surface | Status |
|---------|--------|
| `createagentwallet` RPC | `[NOT YET IMPLEMENTED]` |
| `startmining` RPC or CLI subcommand | `[NOT YET IMPLEMENTED]` |
| `pool-mine` CLI subcommand | `[NOT YET IMPLEMENTED]` |
| `register-agent` CLI subcommand | `[NOT YET IMPLEMENTED]` |
| `rng-mcp` server binary | `[NOT YET IMPLEMENTED]` |
| MCP tools `rng_balance`, `rng_send`, `rng_receive`, `rng_history`, `rng_mine_start`, `rng_mine_stop`, `rng_mine_status`, `rng_price` | `[NOT YET IMPLEMENTED]` |
| Webhook or callback notification infrastructure | `[NOT YET IMPLEMENTED]` |
| Autonomy spending/mining budget enforcement | `[NOT YET IMPLEMENTED]` |
| Audit log for agent actions | `[NOT YET IMPLEMENTED]` |
| On-chain agent identity registration | `[NOT YET IMPLEMENTED]` |

Verification used for this status:

```bash
rg -n "createagentwallet|startmining|pool-mine|register-agent|rng_balance|rng_send|rng-mcp|webhook|autonomy" \
  src/rpc src/wallet/rpc src/node src/init.cpp contrib scripts test
# Expected today: no matches.
```

## Topic

The interfaces and patterns that enable AI agents to participate in the RNG
economy without accidentally relying on APIs that do not exist yet.

## Design Philosophy

RNG remains agent-first as a product direction. Current implementation support is
lower-level than the historical agent-first sketch:

- Agents can use standard RPC wallet commands directly.
- Agents can mine with the built-in miner by starting `rngd` with explicit miner
  flags.
- Agents can inspect mining status with `getinternalmininginfo`.
- Higher-level autonomy, MCP, webhooks, pool-mining, and identity APIs are
  future work.

## Current Implemented Agent Flow

### Wallet Creation

Use the standard wallet RPC. There is no single-purpose agent wallet RPC today.

```bash
rng-cli createwallet "agent-clawd"
rng-cli -rpcwallet=agent-clawd getnewaddress "" bech32
```

Source evidence:

- `createwallet`: `src/wallet/rpc/wallet.cpp:347`
- `getnewaddress`: `src/wallet/rpc/addresses.cpp:21`
- Wallet RPC registration: `src/wallet/rpc/wallet.cpp:918` and `src/wallet/rpc/wallet.cpp:926`

### Sending And Balance Queries

Use the standard wallet RPCs:

```bash
rng-cli -rpcwallet=agent-clawd getbalances
rng-cli -rpcwallet=agent-clawd sendtoaddress rng1q... 10.5
```

Source evidence:

- `getbalance`: `src/wallet/rpc/coins.cpp:164`
- `getbalances`: `src/wallet/rpc/coins.cpp:402`
- `sendtoaddress`: `src/wallet/rpc/spend.cpp:238`
- `sendmany`: `src/wallet/rpc/spend.cpp:336`

### Mining

There is no `startmining` command. Mining is enabled by daemon startup flags:

```bash
rngd -daemon \
  -mine \
  -mineaddress=rng1q... \
  -minethreads=2 \
  -minerandomx=fast \
  -minepriority=low
```

The helper script wraps this flow:

```bash
scripts/start-miner.sh --threads 2 --randomx fast
```

Source evidence:

- Internal miner public behavior: `src/node/internal_miner.h`
- `getinternalmininginfo`: `src/rpc/mining.cpp:510`
- Helper startup command: `scripts/start-miner.sh:154`

## Planned Agent Wallet Convenience RPC

`[NOT YET IMPLEMENTED]`

Historical agent-facing docs proposed a one-command wallet flow:

```bash
rng-cli createagentwallet "agent-name"
```

Proposed response shape:

```json
{
  "wallet_name": "agent-clawd",
  "address": "rng1q...",
  "mnemonic": "word1 word2...",
  "wallet_path": "~/.rng/wallets/agent-clawd"
}
```

This is not present in `src/rpc/` or `src/wallet/rpc/`. Agents must currently
compose the implemented `createwallet`, `getnewaddress`, wallet encryption,
backup, and descriptor-wallet RPCs themselves.

### Planned Key Storage Options

`[NOT YET IMPLEMENTED]`

| Method | Intended security | Intended agent access |
|--------|-------------------|-----------------------|
| Encrypted file | Medium | Via password |
| Environment variable | Low | Direct |
| Hardware wallet | High | Via signing requests |
| Custodial API | Medium | Via API key |

The table is a product design sketch only. The current codebase inherits
Bitcoin Core wallet encryption, descriptor wallets, external signer support, and
backup RPCs, but does not add agent-specific key policy.

## Planned Mining Convenience Interface

`[NOT YET IMPLEMENTED]`

Historical docs proposed:

```bash
rng-cli startmining --address rng1q... --cores 2 --memory 2200
rng-cli startmining --config agent-mining.json
```

No `startmining` RPC or CLI subcommand exists. Current mining uses daemon flags
documented in `specs/120426-internal-miner.md`.

### Planned Resource Budget Configuration

`[NOT YET IMPLEMENTED]`

```json
{
  "mining": {
    "enabled": true,
    "max_cores": 2,
    "max_memory_mb": 2200,
    "schedule": {
      "type": "idle",
      "min_idle_percent": 50
    },
    "auto_stop": {
      "on_high_load": true,
      "load_threshold": 80
    }
  }
}
```

RNG currently has `-minethreads`, `-minerandomx=fast|light`, and
`-minepriority=low|normal`. It does not enforce JSON resource budgets, idle
schedules, or high-load auto-stop rules.

### Planned Mining Modes

`[NOT YET IMPLEMENTED]`

| Mode | Historical description | Current status |
|------|------------------------|----------------|
| `background` | Low priority, yields to other tasks | Use `-minepriority=low`; no named mode exists |
| `dedicated` | Full allocated resources | Use explicit `-minethreads`; no named mode exists |
| `burst` | Mine only when idle | Not implemented |
| `pool` | Connect to mining pool | Not implemented |

## Planned Transaction Convenience Interface

`[NOT YET IMPLEMENTED]`

Historical docs proposed simplified commands:

```bash
rng-cli send rng1q... 10.5 RNG --memo "Payment for services"
rng-cli balance
rng-cli watch --address rng1q... --callback "curl https://agent/webhook"
```

These exact commands are not implemented. Current equivalents are standard
wallet RPCs:

```bash
rng-cli -rpcwallet=agent-clawd sendtoaddress rng1q... 10.5
rng-cli -rpcwallet=agent-clawd getbalances
```

The callback/watch flow is not implemented.

## Planned Autonomy Framework

`[NOT YET IMPLEMENTED]`

Historical docs proposed agent spending, mining, and action budgets:

```json
{
  "autonomy": {
    "spending": {
      "per_transaction_max": "10 RNG",
      "daily_max": "100 RNG",
      "require_approval_above": "50 RNG"
    },
    "mining": {
      "cores_budget": 2,
      "memory_budget_mb": 4096,
      "daily_hours_max": 12
    },
    "actions": {
      "can_send": true,
      "can_receive": true,
      "can_mine": true,
      "can_create_addresses": true,
      "can_export_keys": false
    }
  }
}
```

No code currently parses this config, enforces these limits, requests human
approval, or records budget state. Any agent using RNG today must implement
spending limits and supervision outside `rngd`.

### Planned Approval Flow

`[NOT YET IMPLEMENTED]`

```text
Transaction requested: 75 RNG
  -> Check per_transaction_max: exceeds 10 RNG
  -> Check require_approval_above: exceeds 50 RNG
  -> Action: request human approval
```

This approval flow is not present in the wallet, RPC server, CLI, or scripts.

### Planned Agent Audit Log

`[NOT YET IMPLEMENTED]`

```json
{
  "timestamp": "2026-01-31T09:30:00Z",
  "agent": "clawd",
  "action": "send",
  "amount": "5 RNG",
  "to": "rng1q...",
  "reason": "Purchased compute credits",
  "approved_by": "autonomous",
  "budget_remaining": "95 RNG daily"
}
```

The current wallet records transactions using inherited wallet metadata and
transaction history RPCs, but it does not maintain an agent action audit log.

## Planned MCP Server

`[NOT YET IMPLEMENTED]`

RNG does not currently provide an MCP server binary.

### Planned MCP Tools

`[NOT YET IMPLEMENTED]`

| Tool | Historical description |
|------|------------------------|
| `rng_balance` | Check wallet balance |
| `rng_send` | Send RNG to address |
| `rng_receive` | Generate receiving address |
| `rng_history` | Transaction history |
| `rng_mine_start` | Start mining |
| `rng_mine_stop` | Stop mining |
| `rng_mine_status` | Mining statistics |
| `rng_price` | Current RNG price if exchanges exist |

### Planned MCP Configuration

`[NOT YET IMPLEMENTED]`

```json
{
  "mcpServers": {
    "rng": {
      "command": "rng-mcp",
      "args": ["--wallet", "agent-clawd"],
      "env": {
        "RNG_WALLET_PASSWORD": "${WALLET_PASSWORD}"
      }
    }
  }
}
```

No `rng-mcp` command, MCP transport, or MCP tool registry exists in this
repository.

## Planned Pool Mining For Low-Resource Agents

`[NOT YET IMPLEMENTED]`

Historical docs proposed external pool mining:

```bash
rng-cli pool-mine --pool stratum+tcp://pool.rng.network:3333 \
  --user rng1q... \
  --cores 1
```

No `pool-mine` CLI command, Stratum client, external pool URL support, or
sharepool implementation exists today. The current sharepool design is
protocol-native future work and is documented separately in
`specs/120426-sharepool-protocol.md`.

### Light Validation Mode

Partially implemented through RandomX validation modes, not as an agent product
mode.

- `-minerandomx=light` exists for mining mode selection.
- RandomX light validation concepts are documented in `specs/randomx.md`.
- There is no agent-specific "light validation mode" command or profile.

## Planned Agent Identity

`[NOT YET IMPLEMENTED]`

Historical docs proposed on-chain agent registration:

```bash
rng-cli register-agent \
  --name "Clawd" \
  --type "assistant" \
  --human "geo@example.com" \
  --pubkey "rng1q..."
```

No `register-agent` command exists. No OP_RETURN agent identity schema,
registry, discovery mechanism, reputation tracking, or service-advertisement
layer exists in the codebase.

## Planned Webhooks And Notifications

`[NOT YET IMPLEMENTED]`

Historical docs proposed webhook-driven notifications:

```json
{
  "notifications": {
    "on_receive": {
      "webhook": "https://agent.example/rng/received",
      "min_amount": "0.01 RNG"
    },
    "on_confirm": {
      "webhook": "https://agent.example/rng/confirmed",
      "confirmations": 6
    },
    "on_mining_reward": {
      "webhook": "https://agent.example/rng/mined"
    }
  }
}
```

No webhook delivery, callback registration, HTTP client, retry queue, or
notification config parser exists in the current source tree.

## Acceptance Criteria

### Current System

- Agents can create standard wallets with `createwallet`.
- Agents can derive addresses with `getnewaddress`.
- Agents can inspect balances with `getbalance` and `getbalances`.
- Agents can send transactions with `sendtoaddress`, `sendmany`, and other
  inherited wallet spend RPCs.
- Agents can start mining by launching `rngd` with explicit `-mine`,
  `-mineaddress`, `-minethreads`, `-minerandomx`, and `-minepriority` flags.
- Agents can query miner state with `getinternalmininginfo`.
- The spec labels every unimplemented agent-specific RPC, MCP tool, webhook, and
  autonomy feature as `[NOT YET IMPLEMENTED]`.

### Planned System

`[NOT YET IMPLEMENTED]`

The historical acceptance criteria below remain future work:

1. `createagentwallet` creates an agent wallet in one command.
2. Mining can start through a convenience RPC or CLI command with three or fewer
   parameters.
3. Resource budgets are enforced.
4. Spending limits trigger an approval flow.
5. MCP server exposes all core functions.
6. Audit log captures all agent actions.
7. Pool mining or protocol-native pooled mining works for low-resource agents.
8. Webhooks fire on configured events.
9. Agent-specific light mode works with an explicit low-memory profile.
10. Agent can go from zero to mining in less than five minutes through a
    dedicated agent onboarding path.

## Test Scenarios

### Current Implemented Scenarios

- Create a wallet, get an address, receive funds, and send funds through the
  standard wallet RPC surface.
- Start `rngd` with `-mine` flags and query `getinternalmininginfo`.
- Use external agent code to enforce any spending budgets before calling wallet
  RPCs.

### Planned Scenarios

`[NOT YET IMPLEMENTED]`

- New agent creates wallet, receives RNG, sends RNG, all via MCP.
- Agent starts mining through a convenience command, earns reward, and logs the
  action.
- Agent attempts an over-budget transaction and receives an approval request.
- Low-resource agent joins a pool or future protocol-native sharepool and earns
  proportional reward.
- Agent registers identity and other agents discover it.
- Webhook fires when agent receives payment.

## Example: Current Agent Onboarding Flow

```text
1. Agent obtains RPC credentials for a local RNG node.
2. Agent or operator creates a wallet:
   rng-cli createwallet "agent-clawd"
3. Agent derives a receiving address:
   rng-cli -rpcwallet=agent-clawd getnewaddress "" bech32
4. Human or supervisor decides whether mining may run.
5. Operator starts the daemon with explicit mining flags, or uses
   scripts/start-miner.sh.
6. Agent monitors balance with getbalances and mining state with
   getinternalmininginfo.
```

## Example: Planned Agent Onboarding Flow

`[NOT YET IMPLEMENTED]`

```text
1. Agent reads about RNG.
2. Agent asks its human for an approved budget.
3. Agent calls createagentwallet.
4. Agent starts mining through a convenience command or MCP tool.
5. Agent enforces local budgets, monitors balance, and reports earnings.
```
