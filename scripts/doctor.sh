#!/usr/bin/env bash

set -euo pipefail

EXPECTED_GENESIS_HASH="83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4"
RNG_DAEMON="${RNG_DAEMON:-rngd}"
RNG_CLI="${RNG_CLI:-rng-cli}"
RNG_DATADIR="${RNG_DATADIR:-}"
RNG_CONF="${RNG_CONF:-}"
CLI_ARGS=()
HEALTHY=1

usage() {
    cat <<'EOF'
Verify that this node is pointed at the live RNG mainnet and ready to mine.

Usage:
  ./scripts/doctor.sh [--datadir DIR] [--conf PATH]
  rng-doctor [--datadir DIR] [--conf PATH]

Environment:
  RNG_DAEMON   rngd binary path (default: rngd)
  RNG_CLI      rng-cli binary path (default: rng-cli)
  RNG_DATADIR  Optional datadir to pass to rng-cli
  RNG_CONF     Optional config path to pass to rng-cli
EOF
}

info() { printf '[INFO] %s\n' "$1"; }
warn() { printf '[WARN] %s\n' "$1"; HEALTHY=0; }
error() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
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
        CLI_ARGS+=("-datadir=$RNG_DATADIR")
    fi
    if [ -n "$RNG_CONF" ]; then
        CLI_ARGS+=("-conf=$RNG_CONF")
    fi
}

cli() {
    "$RNG_CLI" "${CLI_ARGS[@]}" "$@"
}

extract_json_string() {
    sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\\([^\"]*\\)\".*/\\1/p"
}

extract_json_number() {
    sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\\([-0-9][0-9]*\\).*/\\1/p"
}

extract_json_bool() {
    sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\\(true\\|false\\).*/\\1/p"
}

show_config_seeds() {
    local conf_path

    conf_path="$HOME/.rng/rng.conf"
    if [ -n "$RNG_CONF" ]; then
        conf_path="$RNG_CONF"
    elif [ -n "$RNG_DATADIR" ] && [ -f "$RNG_DATADIR/rng.conf" ]; then
        conf_path="$RNG_DATADIR/rng.conf"
    fi

    if [ ! -f "$conf_path" ]; then
        warn "Config not found at $conf_path"
        return
    fi

    info "Configured addnode peers from $conf_path:"
    grep '^addnode=' "$conf_path" || warn "No addnode entries found in $conf_path"
}

main() {
    local version_line genesis_hash connection_count blockchaininfo mininginfo
    local chain_name blocks headers ibd running fast_mode threads

    parse_args "$@"
    append_common_args

    command -v "$RNG_DAEMON" >/dev/null 2>&1 || error "rngd binary not found: $RNG_DAEMON"
    command -v "$RNG_CLI" >/dev/null 2>&1 || error "rng-cli binary not found: $RNG_CLI"

    version_line="$("$RNG_DAEMON" --version 2>/dev/null | head -1 || true)"
    if [ -n "$version_line" ]; then
        info "$version_line"
    fi

    if ! cli getblockcount >/dev/null 2>&1; then
        warn "RPC is not reachable. Start the daemon first:"
        printf '       %s -daemon\n' "$RNG_DAEMON"
        show_config_seeds
        exit 1
    fi

    genesis_hash="$(cli getblockhash 0 2>/dev/null || true)"
    if [ "$genesis_hash" = "$EXPECTED_GENESIS_HASH" ]; then
        info "Genesis hash matches live mainnet: $genesis_hash"
    else
        warn "Unexpected genesis hash: ${genesis_hash:-<empty>}"
        printf '       expected: %s\n' "$EXPECTED_GENESIS_HASH"
    fi

    connection_count="$(cli getconnectioncount 2>/dev/null || echo 0)"
    if [ "${connection_count:-0}" -gt 0 ]; then
        info "Peer connections: $connection_count"
    else
        warn "No peer connections yet. Check firewall/routing and addnode peers."
    fi

    blockchaininfo="$(cli getblockchaininfo 2>/dev/null || true)"
    chain_name="$(printf '%s\n' "$blockchaininfo" | extract_json_string chain | head -1)"
    blocks="$(printf '%s\n' "$blockchaininfo" | extract_json_number blocks | head -1)"
    headers="$(printf '%s\n' "$blockchaininfo" | extract_json_number headers | head -1)"
    ibd="$(printf '%s\n' "$blockchaininfo" | extract_json_bool initialblockdownload | head -1)"

    [ -n "$chain_name" ] && info "Chain: $chain_name"
    [ -n "$blocks" ] && info "Blocks: $blocks"
    [ -n "$headers" ] && info "Headers: $headers"
    [ -n "$ibd" ] && info "Initial block download: $ibd"

    mininginfo="$(cli getinternalmininginfo 2>/dev/null || true)"
    if [ -n "$mininginfo" ]; then
        running="$(printf '%s\n' "$mininginfo" | extract_json_bool running | head -1)"
        fast_mode="$(printf '%s\n' "$mininginfo" | extract_json_bool fast_mode | head -1)"
        threads="$(printf '%s\n' "$mininginfo" | extract_json_number mining_threads | head -1)"

        if [ -n "$running" ]; then
            info "Mining running: $running"
        fi
        if [ -n "$fast_mode" ]; then
            info "RandomX fast mode: $fast_mode"
            if [ "$fast_mode" != "true" ]; then
                warn "Live miners currently use RandomX fast mode"
            fi
        fi
        if [ -n "$threads" ]; then
            info "Mining threads: $threads"
        fi
        if [ "$running" != "true" ]; then
            warn "Internal miner is not running. Start it with rng-start-miner"
        fi
    else
        warn "Could not read getinternalmininginfo"
    fi

    if [ "$HEALTHY" -eq 1 ]; then
        info "Node looks healthy for the live RNG network"
        exit 0
    fi

    warn "Node needs attention before it is fully ready to mine"
    exit 1
}

main "$@"
