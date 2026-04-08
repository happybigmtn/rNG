#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ADDRESS="${RNG_MINEADDRESS:-}"
THREADS="${RNG_MINETHREADS:-}"
CPU_PERCENT="${RNG_MINER_CPU_PERCENT:-}"
ENABLE_NOW=0
OUTPUT_JSON=0
STRICT=0

usage() {
    cat <<'EOF'
Converge this host onto a healthy public RNG node plus miner.

Usage:
  sudo ./scripts/public-apply.sh --address rng1... [--threads N | --cpu-percent P] [--enable-now] [--json] [--strict]
  sudo rng-public-apply --address rng1... [same flags]
EOF
}

error() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }

print_next_steps() {
    cat <<EOF

RNG public apply complete

Installed:
- config: /etc/rng/rng.conf
- data dir: /var/lib/rngd
- service: rngd.service

Next steps:
- verify health: rng-doctor --json --strict --expect-public --expect-miner
- open inbound P2P: sudo ufw allow 8433/tcp
- inspect network state: rng-cli getnetworkinfo
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --address)
            [ $# -ge 2 ] || error "--address requires a value"
            ADDRESS="$2"
            shift 2
            ;;
        --threads)
            [ $# -ge 2 ] || error "--threads requires a value"
            THREADS="$2"
            shift 2
            ;;
        --cpu-percent)
            [ $# -ge 2 ] || error "--cpu-percent requires a value"
            CPU_PERCENT="$2"
            shift 2
            ;;
        --enable-now)
            ENABLE_NOW=1
            shift
            ;;
        --json)
            OUTPUT_JSON=1
            shift
            ;;
        --strict)
            STRICT=1
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

[ "$(id -u)" -eq 0 ] || error "Run this script as root"
[ -n "$ADDRESS" ] || error "--address is required"

node_args=()
miner_args=(--address "$ADDRESS")
doctor_args=(--expect-public --expect-miner)

if [ "$ENABLE_NOW" -eq 1 ]; then
    node_args+=(--enable-now)
    miner_args+=(--enable-now)
fi
if [ -n "$THREADS" ]; then
    miner_args+=(--threads "$THREADS")
fi
if [ -n "$CPU_PERCENT" ]; then
    export RNG_MINER_CPU_PERCENT="$CPU_PERCENT"
fi
if [ "$OUTPUT_JSON" -eq 1 ]; then
    doctor_args+=(--json)
fi
if [ "$STRICT" -eq 1 ]; then
    doctor_args+=(--strict)
fi

"$SCRIPT_DIR/install-public-node.sh" "${node_args[@]}"
"$SCRIPT_DIR/install-public-miner.sh" "${miner_args[@]}"
"$SCRIPT_DIR/doctor.sh" "${doctor_args[@]}"

if [ "$OUTPUT_JSON" -eq 0 ]; then
    print_next_steps
fi
