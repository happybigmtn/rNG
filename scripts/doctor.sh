#!/usr/bin/env bash

set -euo pipefail

EXPECTED_GENESIS_HASH="83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4"
RNG_DAEMON="${RNG_DAEMON:-rngd}"
RNG_CLI="${RNG_CLI:-rng-cli}"
RNG_DATADIR="${RNG_DATADIR:-}"
RNG_CONF="${RNG_CONF:-}"
CLI_ARGS=()
HEALTHY=1
CONFIG_PATH=""
OUTPUT_JSON=0
STRICT=0
EXPECT_PUBLIC=0
EXPECT_MINER=0
WARNINGS=()

usage() {
    cat <<'EOF'
Verify that this node is pointed at the live RNG mainnet and ready to mine.

Usage:
  ./scripts/doctor.sh [--datadir DIR] [--conf PATH] [--json] [--strict] [--expect-public] [--expect-miner]
  rng-doctor [--datadir DIR] [--conf PATH] [--json] [--strict] [--expect-public] [--expect-miner]

Environment:
  RNG_DAEMON   rngd binary path (default: rngd)
  RNG_CLI      rng-cli binary path (default: rng-cli)
  RNG_DATADIR  Optional datadir to pass to rng-cli
  RNG_CONF     Optional config path to pass to rng-cli
EOF
}

info() {
    if [ "$OUTPUT_JSON" -eq 0 ]; then
        printf '[INFO] %s\n' "$1"
    fi
}
warn() {
    if [ "$OUTPUT_JSON" -eq 0 ]; then
        printf '[WARN] %s\n' "$1"
    fi
    HEALTHY=0
    WARNINGS+=("$1")
}
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
            --json)
                OUTPUT_JSON=1
                shift
                ;;
            --strict)
                STRICT=1
                shift
                ;;
            --expect-public)
                EXPECT_PUBLIC=1
                shift
                ;;
            --expect-miner)
                EXPECT_MINER=1
                shift
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
    resolve_config_path

    if [ ! -f "$CONFIG_PATH" ]; then
        warn "Config not found at $CONFIG_PATH"
        return
    fi

    if [ "$OUTPUT_JSON" -eq 1 ]; then
        return
    fi

    info "Configured addnode peers from $CONFIG_PATH:"
    grep '^addnode=' "$CONFIG_PATH" || warn "No addnode entries found in $CONFIG_PATH"
}

resolve_config_path() {
    CONFIG_PATH="$HOME/.rng/rng.conf"
    if [ -n "$RNG_CONF" ]; then
        CONFIG_PATH="$RNG_CONF"
    elif [ -n "$RNG_DATADIR" ] && [ -f "$RNG_DATADIR/rng.conf" ]; then
        CONFIG_PATH="$RNG_DATADIR/rng.conf"
    fi
}

config_value() {
    local key

    key="$1"
    resolve_config_path
    if [ ! -f "$CONFIG_PATH" ]; then
        return 1
    fi

    sed -n "s/^[[:space:]]*$key[[:space:]]*=[[:space:]]*//p" "$CONFIG_PATH" | tail -1
}

count_localaddresses() {
    awk '
        /"localaddresses"[[:space:]]*:/ {in_local=1; next}
        in_local && /"address"[[:space:]]*:/ {count++}
        in_local && /^[[:space:]]*]/ {in_local=0}
        END {print count + 0}
    '
}

warnings_json() {
    python3 - <<'PY' "${WARNINGS[@]}"
import json
import sys
print(json.dumps(sys.argv[1:]))
PY
}

print_json_status() {
    local chain_ok="$1"
    local rpc_ok="$2"
    local public_reachable="$3"
    local miner_configured="$4"
    local miner_running="$5"
    local ready="$6"
    local warning_json="$7"
    local services_json="$8"

    python3 - "$chain_ok" "$rpc_ok" "$public_reachable" "$miner_configured" \
        "$miner_running" "$ready" "${connection_count:-0}" \
        "${inbound:-0}" "${outbound:-0}" "${localaddresses:-0}" \
        "${blocks:-0}" "${headers:-0}" "${chain_name:-unknown}" \
        "${version_line:-}" "${fast_mode:-unknown}" "${threads:-0}" \
        "$warning_json" "$services_json" <<'PY'
import json
import sys

payload = {
    "chain_ok": sys.argv[1] == "true",
    "rpc_ok": sys.argv[2] == "true",
    "public_reachable": sys.argv[3] == "true",
    "miner_configured": sys.argv[4] == "true",
    "miner_running": sys.argv[5] == "true",
    "ready": sys.argv[6] == "true",
    "peer_count": int(sys.argv[7]),
    "connections_in": int(sys.argv[8]),
    "connections_out": int(sys.argv[9]),
    "advertised_local_addresses": int(sys.argv[10]),
    "blocks": int(sys.argv[11]),
    "headers": int(sys.argv[12]),
    "chain": sys.argv[13],
    "version": sys.argv[14],
    "randomx_fast_mode": sys.argv[15],
    "mining_threads": int(sys.argv[16]),
    "warnings": json.loads(sys.argv[17]),
    "services": json.loads(sys.argv[18]),
}
print(json.dumps(payload, indent=2))
PY
}

main() {
    local version_line genesis_hash connection_count blockchaininfo mininginfo
    local chain_name blocks headers ibd running fast_mode threads networkinfo
    local inbound outbound localaddresses listen_value
    local chain_ok rpc_ok public_reachable miner_configured miner_running
    local ready services_json warning_json

    parse_args "$@"
    append_common_args

    command -v "$RNG_DAEMON" >/dev/null 2>&1 || error "rngd binary not found: $RNG_DAEMON"
    command -v "$RNG_CLI" >/dev/null 2>&1 || error "rng-cli binary not found: $RNG_CLI"

    version_line="$("$RNG_DAEMON" --version 2>/dev/null | head -1 || true)"
    if [ -n "$version_line" ]; then
        info "$version_line"
    fi

    rpc_ok=true
    if ! cli getblockcount >/dev/null 2>&1; then
        rpc_ok=false
        chain_ok=false
        public_reachable=false
        miner_configured=false
        miner_running=false
        ready=false
        warn "RPC is not reachable. Start the daemon first:"
        if [ "$OUTPUT_JSON" -eq 0 ]; then
            printf '       %s -daemon\n' "$RNG_DAEMON"
        fi
        show_config_seeds
        warning_json="$(warnings_json)"
        services_json='{"rngd":"unreachable"}'
        if [ "$OUTPUT_JSON" -eq 1 ]; then
            print_json_status "$chain_ok" "$rpc_ok" "$public_reachable" \
                "$miner_configured" "$miner_running" "$ready" \
                "$warning_json" "$services_json"
        fi
        exit 1
    fi

    genesis_hash="$(cli getblockhash 0 2>/dev/null || true)"
    if [ "$genesis_hash" = "$EXPECTED_GENESIS_HASH" ]; then
        chain_ok=true
        info "Genesis hash matches live mainnet: $genesis_hash"
    else
        chain_ok=false
        warn "Unexpected genesis hash: ${genesis_hash:-<empty>}"
        printf '       expected: %s\n' "$EXPECTED_GENESIS_HASH"
    fi

    connection_count="$(cli getconnectioncount 2>/dev/null || echo 0)"
    if [ "${connection_count:-0}" -gt 0 ]; then
        info "Peer connections: $connection_count"
        if [ "${connection_count:-0}" -le 4 ]; then
            info "Low peer count is normal on the current operator-seeded network"
        fi
    else
        warn "No peer connections yet. This can be normal if the operator seed fleet is down or you are bringing the chain back with very few public nodes."
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

    networkinfo="$(cli getnetworkinfo 2>/dev/null || true)"
    inbound="$(printf '%s\n' "$networkinfo" | extract_json_number connections_in | head -1)"
    outbound="$(printf '%s\n' "$networkinfo" | extract_json_number connections_out | head -1)"
    localaddresses="$(printf '%s\n' "$networkinfo" | count_localaddresses)"
    listen_value="$(config_value listen || true)"

    [ -n "$inbound" ] && info "Inbound peers: $inbound"
    [ -n "$outbound" ] && info "Outbound peers: $outbound"
    info "Advertised local addresses: ${localaddresses:-0}"

    if [ "${listen_value:-1}" = "0" ]; then
        warn "Config sets listen=0. This node can mine, but it will not accept inbound peers."
    elif [ "${localaddresses:-0}" -eq 0 ] || [ "${inbound:-0}" -eq 0 ]; then
        info "This node is not currently visible as a public peer. If this is a public VPS, keep listen=1 and open TCP/8433 to help decentralize the network."
    fi

    if [ "${listen_value:-1}" != "0" ] && [ "${localaddresses:-0}" -gt 0 ] && [ "${inbound:-0}" -gt 0 ]; then
        public_reachable=true
    else
        public_reachable=false
        if [ "$EXPECT_PUBLIC" -eq 1 ]; then
            warn "Expected a public node, but inbound reachability is not yet proven"
        fi
    fi

    mininginfo="$(cli getinternalmininginfo 2>/dev/null || true)"
    if [ -n "$mininginfo" ]; then
        running="$(printf '%s\n' "$mininginfo" | extract_json_bool running | head -1)"
        fast_mode="$(printf '%s\n' "$mininginfo" | extract_json_bool fast_mode | head -1)"
        threads="$(printf '%s\n' "$mininginfo" | extract_json_number mining_threads | head -1)"
        miner_configured=true

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
        if [ "${running:-false}" = "true" ]; then
            miner_running=true
        else
            miner_running=false
            warn "Internal miner is not running. Start it with rng-start-miner"
        fi
    else
        miner_configured=false
        miner_running=false
        warn "Could not read getinternalmininginfo"
    fi

    if [ "$EXPECT_MINER" -eq 0 ] && [ "$miner_running" = false ]; then
        miner_running=false
    fi

    ready=false
    if [ "$chain_ok" = true ] && [ "$rpc_ok" = true ] && \
        [ "${connection_count:-0}" -gt 0 ] && \
        { [ "$EXPECT_PUBLIC" -eq 0 ] || [ "$public_reachable" = true ]; } && \
        { [ "$EXPECT_MINER" -eq 0 ] || [ "$miner_running" = true ]; }; then
        ready=true
    fi

    services_json="$(python3 - <<'PY' "${running:-unknown}" "${threads:-0}"
import json
import sys
print(json.dumps({"rngd": {"mining_running": sys.argv[1], "mining_threads": sys.argv[2]}}))
PY
)"
    warning_json="$(warnings_json)"

    if [ "$OUTPUT_JSON" -eq 1 ]; then
        print_json_status "$chain_ok" "$rpc_ok" "$public_reachable" \
            "$miner_configured" "$miner_running" "$ready" \
            "$warning_json" "$services_json"
    fi

    if [ "$ready" = true ]; then
        info "Node looks healthy for the live RNG network"
        exit 0
    fi

    if [ "$STRICT" -eq 1 ] || [ "$EXPECT_PUBLIC" -eq 1 ] || [ "$EXPECT_MINER" -eq 1 ]; then
        exit 1
    fi

    warn "Node needs attention before it is fully ready to mine"
    exit 1
}

main "$@"
