#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SNAPSHOT_BASE_HASH="2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb"
SNAPSHOT_PATH=""
RNG_DAEMON="${RNG_DAEMON:-rngd}"
RNG_CLI="${RNG_CLI:-rng-cli}"
RNG_DATADIR="${RNG_DATADIR:-}"
RNG_CONF="${RNG_CONF:-}"
CURRENT_HEIGHT=0
DAEMON_ARGS=()
CLI_ARGS=()

usage() {
    cat <<'EOF'
Load the bundled RNG assumeutxo snapshot.

Usage:
  ./scripts/load-bootstrap.sh [--snapshot PATH] [--datadir DIR] [--conf PATH]
  rng-load-bootstrap [--snapshot PATH] [--datadir DIR] [--conf PATH]

Environment:
  RNG_DAEMON   rngd binary path (default: rngd)
  RNG_CLI      rng-cli binary path (default: rng-cli)
  RNG_DATADIR  Optional datadir to pass to both commands
  RNG_CONF     Optional config path to pass to both commands
EOF
}

info() { printf '[INFO] %s\n' "$1" >&2; }
warn() { printf '[WARN] %s\n' "$1" >&2; }
error() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --snapshot)
                [ $# -ge 2 ] || error "--snapshot requires a path"
                SNAPSHOT_PATH="$2"
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
                if [ -z "$SNAPSHOT_PATH" ]; then
                    SNAPSHOT_PATH="$1"
                    shift
                else
                    error "Unknown argument: $1"
                fi
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
    for _ in $(seq 1 30); do
        if cli getblockcount >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
    done
    return 1
}

main() {
    parse_args "$@"
    append_common_args

    if [ -z "$SNAPSHOT_PATH" ]; then
        SNAPSHOT_PATH="$ROOT_DIR/bootstrap/rng-mainnet-15091.utxo"
    fi

    [ -f "$SNAPSHOT_PATH" ] || error "Snapshot not found: $SNAPSHOT_PATH"

    if ! cli getblockcount >/dev/null 2>&1; then
        info "Starting daemon for snapshot load"
        daemon -daemon
    fi

    wait_for_rpc || error "RPC did not become ready"

    CURRENT_HEIGHT="$(cli getblockcount)"
    if [ "$CURRENT_HEIGHT" -gt 0 ]; then
        warn "Datadir already has blocks at height $CURRENT_HEIGHT; skipping snapshot load"
        cli getchainstates
        exit 0
    fi

    info "Waiting for snapshot base header $SNAPSHOT_BASE_HASH"
    for _ in $(seq 1 120); do
        if cli getblockheader "$SNAPSHOT_BASE_HASH" false >/dev/null 2>&1; then
            break
        fi
        sleep 2
    done

    if ! cli getblockheader "$SNAPSHOT_BASE_HASH" false >/dev/null 2>&1; then
        warn "Snapshot base header is not available yet; wait for headers to sync and retry"
        cli getconnectioncount >&2 || true
        exit 1
    fi

    CURRENT_HEIGHT="$(cli getblockcount)"
    if [ "$CURRENT_HEIGHT" -gt 0 ]; then
        warn "Datadir advanced to height $CURRENT_HEIGHT before the snapshot could be loaded"
        warn "Stop the node, wipe blocks/chainstate, and retry the snapshot on a fresh datadir"
        exit 1
    fi

    info "Loading snapshot $SNAPSHOT_PATH"
    cli -rpcclienttimeout=0 loadtxoutset "$SNAPSHOT_PATH"
    cli getchainstates
}

main "$@"
