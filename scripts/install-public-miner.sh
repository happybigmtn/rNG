#!/usr/bin/env bash

set -euo pipefail

SERVICE_NAME="${RNG_SYSTEMD_SERVICE_NAME:-rngd.service}"
SERVICE_DIR="${RNG_SYSTEMD_DIR:-/etc/systemd/system}"
INSTALL_BIN_DIR="${RNG_SYSTEM_BIN_DIR:-/usr/local/bin}"
CONFIG_DIR="${RNG_SYSTEM_CONFIG_DIR:-/etc/rng}"
DATA_DIR="${RNG_SYSTEM_DATA_DIR:-/var/lib/rngd}"
MINE_ADDRESS="${RNG_MINEADDRESS:-}"
THREADS="${RNG_MINETHREADS:-}"
RANDOMX_MODE="${RNG_RANDOMX_MODE:-fast}"
MINE_PRIORITY="${RNG_MINEPRIORITY:-low}"
ENABLE_NOW=0
REMOVE_OVERRIDE=0

usage() {
    cat <<'EOF'
Install or remove a persistent RNG mining override for the systemd node service.

Usage:
  sudo ./scripts/install-public-miner.sh --address rng1... [--threads N] [--randomx fast|light] [--priority low|normal] [--enable-now]
  sudo rng-install-public-miner --address rng1... [same flags]
  sudo rng-install-public-miner --remove

Environment:
  RNG_SYSTEMD_SERVICE_NAME  Base service name (default: rngd.service)
  RNG_SYSTEMD_DIR           systemd unit dir (default: /etc/systemd/system)
  RNG_SYSTEM_BIN_DIR        Directory containing rngd (default: /usr/local/bin)
  RNG_SYSTEM_CONFIG_DIR     Config directory (default: /etc/rng)
  RNG_SYSTEM_DATA_DIR       Datadir (default: /var/lib/rngd)
  RNG_MINEADDRESS           Payout address if not passed via --address
  RNG_MINETHREADS           Mining threads (default: CPU count minus one)
  RNG_RANDOMX_MODE          fast or light (default: fast)
  RNG_MINEPRIORITY          low or normal (default: low)
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
            --address)
                [ $# -ge 2 ] || error "--address requires a value"
                MINE_ADDRESS="$2"
                shift 2
                ;;
            --threads)
                [ $# -ge 2 ] || error "--threads requires a value"
                THREADS="$2"
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
            --enable-now)
                ENABLE_NOW=1
                shift
                ;;
            --remove)
                REMOVE_OVERRIDE=1
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

require_root() {
    [ "$(id -u)" -eq 0 ] || error "Run this script as root (for example with sudo)"
}

validate_args() {
    if [ "$REMOVE_OVERRIDE" -eq 1 ]; then
        return
    fi

    [ -n "$MINE_ADDRESS" ] || error "A payout address is required (--address rng1...)"

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

    case "$THREADS" in
        ''|*[!0-9]*)
            error "Thread count must be a positive integer"
            ;;
    esac
    [ "$THREADS" -ge 1 ] || error "Thread count must be at least 1"

    case "$MINE_ADDRESS" in
        *[!a-z0-9]*|'')
            error "Payout address must be a bech32 RNG address without whitespace"
            ;;
    esac
}

dropin_dir() {
    printf '%s/%s.d\n' "$SERVICE_DIR" "$SERVICE_NAME"
}

dropin_path() {
    printf '%s/mining.conf\n' "$(dropin_dir)"
}

base_service_path() {
    printf '%s/%s\n' "$SERVICE_DIR" "$SERVICE_NAME"
}

remove_override() {
    if [ -f "$(dropin_path)" ]; then
        find "$(dropin_dir)" -maxdepth 1 -name 'mining.conf' -delete
        rmdir --ignore-fail-on-non-empty "$(dropin_dir)" 2>/dev/null || true
        systemctl daemon-reload
        info "Removed mining override for $SERVICE_NAME"
    else
        info "No mining override present for $SERVICE_NAME"
    fi

    if [ "$ENABLE_NOW" -eq 1 ]; then
        systemctl restart "$SERVICE_NAME"
    fi
}

install_override() {
    local daemon_path

    [ -f "$(base_service_path)" ] || error "Base service $(base_service_path) not found. Run rng-install-public-node first."
    daemon_path="$INSTALL_BIN_DIR/rngd"
    [ -x "$daemon_path" ] || error "rngd not found at $daemon_path"

    install -d -m 0755 "$(dropin_dir)"
    cat > "$(dropin_path)" <<EOF
[Service]
Nice=19
ExecStart=
ExecStart=$daemon_path -pid=/run/rngd/rngd.pid -conf=$CONFIG_DIR/rng.conf -datadir=$DATA_DIR -mine -mineaddress=$MINE_ADDRESS -minethreads=$THREADS -minerandomx=$RANDOMX_MODE -minepriority=$MINE_PRIORITY -startupnotify='systemd-notify --ready' -shutdownnotify='systemd-notify --stopping'
EOF

    chmod 644 "$(dropin_path)"
    systemctl daemon-reload
    info "Installed mining override at $(dropin_path)"
    info "Threads: $THREADS"
    info "RandomX mode: $RANDOMX_MODE"
    info "Priority: $MINE_PRIORITY"
    info "Payout address: $MINE_ADDRESS"

    if [ "$ENABLE_NOW" -eq 1 ]; then
        systemctl enable --now "$SERVICE_NAME"
    fi
}

main() {
    parse_args "$@"
    require_root
    validate_args

    if [ "$REMOVE_OVERRIDE" -eq 1 ]; then
        remove_override
        exit 0
    fi

    install_override
}

main "$@"
