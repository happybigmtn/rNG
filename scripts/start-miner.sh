#!/usr/bin/env bash

set -euo pipefail

RNG_DAEMON="${RNG_DAEMON:-rngd}"
RNG_CLI="${RNG_CLI:-rng-cli}"
RNG_DATADIR="${RNG_DATADIR:-}"
RNG_CONF="${RNG_CONF:-}"
WALLET_NAME="${RNG_WALLET:-miner}"
MINE_ADDRESS="${RNG_MINEADDRESS:-}"
RANDOMX_MODE="${RNG_RANDOMX_MODE:-fast}"
MINE_PRIORITY="${RNG_MINEPRIORITY:-low}"
THREADS="${RNG_MINETHREADS:-}"
DAEMON_ARGS=()
CLI_ARGS=()

usage() {
    cat <<'EOF'
Create or load a miner wallet, derive a payout address, and restart rngd in mining mode.

Usage:
  ./scripts/start-miner.sh [--wallet NAME] [--threads N] [--address ADDR]
                           [--randomx fast|light] [--priority low|normal]
                           [--datadir DIR] [--conf PATH]
  rng-start-miner [same flags]

Environment:
  RNG_DAEMON        rngd binary path (default: rngd)
  RNG_CLI           rng-cli binary path (default: rng-cli)
  RNG_DATADIR       Optional datadir to pass to both commands
  RNG_CONF          Optional config path to pass to both commands
  RNG_WALLET        Wallet name (default: miner)
  RNG_MINEADDRESS   Optional payout address to reuse
  RNG_RANDOMX_MODE  fast or light (default: fast)
  RNG_MINEPRIORITY  low or normal (default: low)
  RNG_MINETHREADS   Mining thread count (default: CPU count minus one, minimum 1)
EOF
}

info() { printf '[INFO] %s\n' "$1"; }
error() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }

cpu_count() {
    nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 1
}

default_threads() {
    local threads
    threads="$(cpu_count)"
    if [ "$threads" -gt 1 ]; then
        threads=$((threads - 1))
    fi
    printf '%s\n' "$threads"
}

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --wallet)
                [ $# -ge 2 ] || error "--wallet requires a name"
                WALLET_NAME="$2"
                shift 2
                ;;
            --threads)
                [ $# -ge 2 ] || error "--threads requires a value"
                THREADS="$2"
                shift 2
                ;;
            --address)
                [ $# -ge 2 ] || error "--address requires a value"
                MINE_ADDRESS="$2"
                shift 2
                ;;
            --randomx)
                [ $# -ge 2 ] || error "--randomx requires fast or light"
                RANDOMX_MODE="$2"
                shift 2
                ;;
            --priority)
                [ $# -ge 2 ] || error "--priority requires low or normal"
                MINE_PRIORITY="$2"
                shift 2
                ;;
            --datadir)
                [ $# -ge 2 ] || error "--datadir requires a path"
                RNG_DATADIR="$2"
                shift 2
                ;;
            --conf)
                [ $# -ge 2 ] || error "--conf requires a path"
                RNG_CONF="$2"
                shift 2
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                error "Unknown argument: $1"
                ;;
        esac
    done
}

append_common_args() {
    if [ -n "$RNG_DATADIR" ]; then
        DAEMON_ARGS+=("-datadir=$RNG_DATADIR")
        CLI_ARGS+=("-datadir=$RNG_DATADIR")
    fi
    if [ -n "$RNG_CONF" ]; then
        DAEMON_ARGS+=("-conf=$RNG_CONF")
        CLI_ARGS+=("-conf=$RNG_CONF")
    fi
}

cli() {
    "$RNG_CLI" "${CLI_ARGS[@]}" "$@"
}

daemon() {
    "$RNG_DAEMON" "${DAEMON_ARGS[@]}" "$@"
}

wait_for_rpc() {
    local rpc_output

    for _ in $(seq 1 30); do
        if rpc_output="$(cli getblockcount 2>&1)"; then
            return 0
        fi
        case "$rpc_output" in
            *"Incorrect rpcuser or rpcpassword"*)
                error "RPC endpoint rejected this node's credentials. Another rngd is probably already bound to this rpcport; stop it or change rpcport in rng.conf."
                ;;
        esac
        sleep 1
    done
    return 1
}

wait_for_shutdown() {
    for _ in $(seq 1 30); do
        if ! cli getblockcount >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
    done
    return 1
}

start_mining_daemon() {
    for _ in $(seq 1 60); do
        if daemon -daemon -mine -mineaddress="$MINE_ADDRESS" -minethreads="$THREADS" -minerandomx="$RANDOMX_MODE" -minepriority="$MINE_PRIORITY" >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
    done

    return 1
}

ensure_wallet_loaded() {
    if cli -rpcwallet="$WALLET_NAME" getwalletinfo >/dev/null 2>&1; then
        return 0
    fi

    cli createwallet "$WALLET_NAME" >/dev/null 2>&1 || true
    if cli -rpcwallet="$WALLET_NAME" getwalletinfo >/dev/null 2>&1; then
        info "Using wallet $WALLET_NAME"
        return 0
    fi

    cli loadwallet "$WALLET_NAME" >/dev/null 2>&1 || true
    if cli -rpcwallet="$WALLET_NAME" getwalletinfo >/dev/null 2>&1; then
        info "Loaded wallet $WALLET_NAME"
        return 0
    fi

    error "Unable to create or load wallet $WALLET_NAME"
}

validate_address() {
    cli validateaddress "$MINE_ADDRESS" | grep -q '"isvalid"[[:space:]]*:[[:space:]]*true'
}

main() {
    parse_args "$@"
    append_common_args

    case "$RANDOMX_MODE" in
        fast|light) ;;
        *) error "RandomX mode must be fast or light" ;;
    esac

    case "$MINE_PRIORITY" in
        low|normal) ;;
        *) error "Priority must be low or normal" ;;
    esac

    if [ -z "$THREADS" ]; then
        THREADS="$(default_threads)"
    fi

    if ! cli getblockcount >/dev/null 2>&1; then
        info "Starting daemon to create or load the miner wallet"
        daemon -daemon
    fi

    wait_for_rpc || error "RPC did not become ready"
    ensure_wallet_loaded

    if [ -n "$MINE_ADDRESS" ]; then
        validate_address || error "Invalid payout address: $MINE_ADDRESS"
    else
        MINE_ADDRESS="$(cli -rpcwallet="$WALLET_NAME" getnewaddress)"
    fi

    info "Using wallet: $WALLET_NAME"
    info "Using payout address: $MINE_ADDRESS"
    info "Restarting daemon with mining enabled"

    cli stop >/dev/null || true
    wait_for_shutdown || true
    start_mining_daemon || error "Timed out restarting rngd in mining mode"

    wait_for_rpc || error "RPC did not become ready after mining restart"

    info "Mining started with RandomX mode $RANDOMX_MODE on $THREADS thread(s)"
    cli getinternalmininginfo
}

main "$@"
