#!/usr/bin/env bash
# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C
set -euo pipefail

is_metadata_command() {
    for arg in "$@"; do
        case "$arg" in
            -version|--version|-help|--help|-h)
                return 0
                ;;
        esac
    done
    return 1
}

write_default_config() {
    local data_dir rpc_user config_file

    data_dir="${RNG_DATA_DIR:-/home/rng/.rng}"
    rpc_user="${RNG_RPC_USER:-agent}"
    config_file="${data_dir}/rng.conf"

    mkdir -p "$data_dir"
    if [ -f "$config_file" ]; then
        return
    fi

    umask 077
    {
        printf 'server=1\n'
        printf 'rpcbind=127.0.0.1\n'
        printf 'rpcallowip=127.0.0.1\n'
        printf 'rpcuser=%s\n' "$rpc_user"
        printf 'rpcpassword=%s\n' "$RNG_RPC_PASSWORD"
        printf 'minerandomx=fast\n'
        printf 'addnode=144.91.87.251:8433\n'
        printf 'addnode=198.244.177.144:8433\n'
        printf 'addnode=89.58.57.243:8433\n'
        printf 'addnode=89.58.32.181:8433\n'
    } > "$config_file"
}

if [ "${1:-}" = "rngd" ] && ! is_metadata_command "$@"; then
    if [ -z "${RNG_RPC_PASSWORD:-}" ]; then
        printf 'ERROR: RNG_RPC_PASSWORD must be set to start rngd in the container.\n' >&2
        exit 64
    fi
    write_default_config
fi

exec "$@"
