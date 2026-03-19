#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SNAPSHOT_BASE_HASH="2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb"
SNAPSHOT_BASE_HEIGHT="15091"
CHAIN_BUNDLE_ARCHIVE="rng-mainnet-15244-datadir.tar.gz"
SNAPSHOT_PATH=""
CHAIN_BUNDLE_PATH=""
RNG_DAEMON="${RNG_DAEMON:-rngd}"
RNG_CLI="${RNG_CLI:-rng-cli}"
RNG_DATADIR="${RNG_DATADIR:-}"
RNG_CONF="${RNG_CONF:-}"
RNG_BOOTSTRAP_HEADER_WAIT_SECONDS="${RNG_BOOTSTRAP_HEADER_WAIT_SECONDS:-900}"
CURRENT_HEIGHT=0
DAEMON_ARGS=()
CLI_ARGS=()

usage() {
    cat <<'EOF'
Load the bundled RNG assumeutxo snapshot.

Usage:
  ./scripts/load-bootstrap.sh [--snapshot PATH] [--bundle PATH] [--datadir DIR] [--conf PATH]
  rng-load-bootstrap [--snapshot PATH] [--bundle PATH] [--datadir DIR] [--conf PATH]

Environment:
  RNG_DAEMON   rngd binary path (default: rngd)
  RNG_CLI      rng-cli binary path (default: rng-cli)
  RNG_DATADIR  Optional datadir to pass to both commands
  RNG_CONF     Optional config path to pass to both commands
  RNG_BOOTSTRAP_HEADER_WAIT_SECONDS  Max wait for snapshot base header (default: 900)
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
            --bundle)
                [ $# -ge 2 ] || error "--bundle requires a path"
                CHAIN_BUNDLE_PATH="$2"
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

default_data_dir() {
    if [ -n "$RNG_DATADIR" ]; then
        printf '%s\n' "$RNG_DATADIR"
        return
    fi
    printf '%s\n' "$HOME/.rng"
}

detect_bootstrap_asset() {
    local filename data_dir candidate

    filename="$1"
    data_dir="$(default_data_dir)"

    for candidate in \
        "$data_dir/bootstrap/$filename" \
        "$ROOT_DIR/bootstrap/$filename"
    do
        if [ -f "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
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

fresh_datadir() {
    local data_dir

    data_dir="$(default_data_dir)"
    [ ! -d "$data_dir/blocks" ] && [ ! -d "$data_dir/chainstate" ]
}

extract_chain_bundle() {
    local data_dir

    [ -f "$CHAIN_BUNDLE_PATH" ] || return 1
    data_dir="$(default_data_dir)"
    mkdir -p "$data_dir"
    info "Extracting bundled chain bundle from $CHAIN_BUNDLE_PATH"
    tar -xzf "$CHAIN_BUNDLE_PATH" -C "$data_dir"
}

main() {
    local header_wait_loops wait_step current_headers

    parse_args "$@"
    append_common_args

    if [ -z "$CHAIN_BUNDLE_PATH" ]; then
        CHAIN_BUNDLE_PATH="$(detect_bootstrap_asset "$CHAIN_BUNDLE_ARCHIVE" || true)"
    fi

    if [ -n "$CHAIN_BUNDLE_PATH" ] && fresh_datadir; then
        extract_chain_bundle
    fi

    if [ -z "$SNAPSHOT_PATH" ]; then
        SNAPSHOT_PATH="$(detect_bootstrap_asset "rng-mainnet-15091.utxo" || true)"
    fi

    if ! cli getblockcount >/dev/null 2>&1; then
        info "Starting daemon for snapshot load"
        daemon -daemon -listen=0 -discover=0
    fi

    wait_for_rpc || error "RPC did not become ready"

    CURRENT_HEIGHT="$(cli getblockcount)"
    if [ "$CURRENT_HEIGHT" -gt 0 ]; then
        info "Datadir already has blocks at height $CURRENT_HEIGHT; snapshot load is not needed"
        cli getchainstates
        exit 0
    fi

    [ -f "$SNAPSHOT_PATH" ] || error "Snapshot not found: $SNAPSHOT_PATH"

    info "Waiting for snapshot base header $SNAPSHOT_BASE_HASH"
    header_wait_loops=$((RNG_BOOTSTRAP_HEADER_WAIT_SECONDS / 2))
    if [ "$header_wait_loops" -lt 1 ]; then
        header_wait_loops=1
    fi

    for wait_step in $(seq 1 "$header_wait_loops"); do
        if cli getblockheader "$SNAPSHOT_BASE_HASH" false >/dev/null 2>&1; then
            break
        fi
        if [ $((wait_step % 15)) -eq 0 ]; then
            current_headers="$(cli getchainstates 2>/dev/null | sed -n 's/.*"headers"[[:space:]]*:[[:space:]]*\\([0-9][0-9]*\\).*/\\1/p' | head -1)"
            current_headers="${current_headers:-0}"
            info "Still waiting for snapshot base header; headers=$current_headers/$SNAPSHOT_BASE_HEIGHT"
        fi
        sleep 2
    done

    if ! cli getblockheader "$SNAPSHOT_BASE_HASH" false >/dev/null 2>&1; then
        warn "Snapshot base header is not available yet after ${RNG_BOOTSTRAP_HEADER_WAIT_SECONDS}s; wait for headers to sync and retry"
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
