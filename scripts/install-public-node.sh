#!/usr/bin/env bash

export LC_ALL=C
set -euo pipefail

SERVICE_USER="${RNG_SERVICE_USER:-rng}"
SERVICE_GROUP="${RNG_SERVICE_GROUP:-$SERVICE_USER}"
INSTALL_BIN_DIR="${RNG_SYSTEM_BIN_DIR:-/usr/local/bin}"
CONFIG_DIR="${RNG_SYSTEM_CONFIG_DIR:-/etc/rng}"
DATA_DIR="${RNG_SYSTEM_DATA_DIR:-/var/lib/rngd}"
SERVICE_DIR="${RNG_SYSTEMD_DIR:-/etc/systemd/system}"
DAEMON_PATH="${RNG_DAEMON_PATH:-}"
CLI_PATH="${RNG_CLI_PATH:-}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENABLE_NOW=0

usage() {
    cat <<'EOF'
Install RNG as a long-running public systemd node on this host.

Usage:
  sudo ./scripts/install-public-node.sh [--enable-now]
  sudo rng-install-public-node [--enable-now]

Environment:
  RNG_SERVICE_USER       Service user (default: rng)
  RNG_SERVICE_GROUP      Service group (default: same as user)
  RNG_SYSTEM_BIN_DIR     Target directory for rng binaries (default: /usr/local/bin)
  RNG_SYSTEM_CONFIG_DIR  Config directory (default: /etc/rng)
  RNG_SYSTEM_DATA_DIR    Datadir (default: /var/lib/rngd)
  RNG_SYSTEMD_DIR        systemd unit dir (default: /etc/systemd/system)
  RNG_DAEMON_PATH        Explicit source path for rngd
  RNG_CLI_PATH           Explicit source path for rng-cli
EOF
}

info() { printf '[INFO] %s\n' "$1"; }
warn() { printf '[WARN] %s\n' "$1" >&2; }
error() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --enable-now)
                ENABLE_NOW=1
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

resolve_asset() {
    local explicit_name command_name sibling_name resolved

    explicit_name="$1"
    command_name="$2"
    sibling_name="$3"

    if [ -n "$explicit_name" ]; then
        [ -x "$explicit_name" ] || error "Not executable: $explicit_name"
        printf '%s\n' "$explicit_name"
        return
    fi

    if [ -x "$SCRIPT_DIR/$sibling_name" ]; then
        printf '%s\n' "$SCRIPT_DIR/$sibling_name"
        return
    fi

    if resolved="$(command -v "$command_name" 2>/dev/null || true)"; then
        if [ -n "$resolved" ]; then
            printf '%s\n' "$resolved"
            return
        fi
    fi

    error "Could not locate $command_name"
}

require_file() {
    local path

    path="$1"
    [ -f "$path" ] || error "Required file not found: $path"
}

write_default_config() {
    local rpcpass config_path template_path

    config_path="$CONFIG_DIR/rng.conf"
    if [ -f "$config_path" ]; then
        warn "Config already exists at $config_path; leaving it unchanged"
        return
    fi

    template_path="$SCRIPT_DIR/rng.conf.example"
    rpcpass="$(openssl rand -hex 16 2>/dev/null || head -c 32 /dev/urandom | xxd -p | head -c 32)"
    sed "s#replace-this-password#$rpcpass#g" "$template_path" > "$config_path"
    chmod 600 "$config_path"
    chown root:"$SERVICE_GROUP" "$config_path"
    info "Wrote $config_path"
}

install_service() {
    local daemon_target cli_target service_template config_template

    daemon_target="$INSTALL_BIN_DIR/rngd"
    cli_target="$INSTALL_BIN_DIR/rng-cli"
    service_template="$SCRIPT_DIR/rngd.service"
    config_template="$SCRIPT_DIR/rng.conf.example"

    require_file "$service_template"
    require_file "$config_template"

    install -d -m 0755 "$INSTALL_BIN_DIR"
    install -m 0755 "$DAEMON_PATH" "$daemon_target"
    install -m 0755 "$CLI_PATH" "$cli_target"

    if [ -x "$SCRIPT_DIR/rng-load-bootstrap" ]; then
        install -m 0755 "$SCRIPT_DIR/rng-load-bootstrap" "$INSTALL_BIN_DIR/rng-load-bootstrap"
    fi
    if [ -x "$SCRIPT_DIR/rng-start-miner" ]; then
        install -m 0755 "$SCRIPT_DIR/rng-start-miner" "$INSTALL_BIN_DIR/rng-start-miner"
    fi
    if [ -x "$SCRIPT_DIR/rng-doctor" ]; then
        install -m 0755 "$SCRIPT_DIR/rng-doctor" "$INSTALL_BIN_DIR/rng-doctor"
    fi

    install -d -o root -g "$SERVICE_GROUP" -m 0710 "$CONFIG_DIR"
    install -d -o "$SERVICE_USER" -g "$SERVICE_GROUP" -m 0710 "$DATA_DIR"
    install -d -m 0755 "$SERVICE_DIR"

    write_default_config

    sed "s#/usr/bin/rngd#$daemon_target#g" "$service_template" > "$SERVICE_DIR/rngd.service"
    chmod 644 "$SERVICE_DIR/rngd.service"
    info "Installed systemd unit at $SERVICE_DIR/rngd.service"
}

ensure_service_user() {
    if ! getent group "$SERVICE_GROUP" >/dev/null 2>&1; then
        groupadd --system "$SERVICE_GROUP"
    fi

    if ! id "$SERVICE_USER" >/dev/null 2>&1; then
        useradd --system --home-dir "$DATA_DIR" --create-home --gid "$SERVICE_GROUP" \
            --shell /usr/sbin/nologin "$SERVICE_USER"
    fi
}

main() {
    parse_args "$@"
    require_root

    DAEMON_PATH="$(resolve_asset "$DAEMON_PATH" rngd rngd)"
    CLI_PATH="$(resolve_asset "$CLI_PATH" rng-cli rng-cli)"

    ensure_service_user
    install_service

    systemctl daemon-reload
    if [ "$ENABLE_NOW" -eq 1 ]; then
        systemctl enable --now rngd
    fi

    info "Public-node assets are installed."
    info "Next steps:"
    printf '       sudo systemctl enable --now rngd\n'
    printf '       sudo ufw allow 8433/tcp\n'
    printf '       sudo -u %s %s/rng-doctor --conf %s/rng.conf --datadir %s\n' \
        "$SERVICE_USER" "$INSTALL_BIN_DIR" "$CONFIG_DIR" "$DATA_DIR"
}

main "$@"
