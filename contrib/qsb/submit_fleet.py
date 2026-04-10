#!/usr/bin/env python3
"""Operate the QSB queue on remote RNG validators over SSH."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys
import time

from state import load_json


DEFAULT_CLI_PATH = "/root/rng-cli"
DEFAULT_CONF_PATH = "/root/.rng/rng.conf"
DEFAULT_DATADIR = "/root/.rng"
DEFAULT_LOOKBACK_BLOCKS = 12
DEFAULT_TIMEOUT_SECS = 900
DEFAULT_POLL_SECS = 5


class FleetError(RuntimeError):
    """User-facing operator error."""


def read_tx_hex(args: argparse.Namespace) -> str:
    sources = sum(bool(value) for value in (args.tx_hex, args.tx_hex_file, args.state_file))
    if sources != 1:
        raise FleetError("pass exactly one of --tx-hex, --tx-hex-file, or --state-file")

    if args.tx_hex:
        return args.tx_hex.strip()
    if args.tx_hex_file:
        return Path(args.tx_hex_file).read_text(encoding="utf-8").strip()

    state = load_json(args.state_file)
    section = state.get(args.kind)
    if not isinstance(section, dict):
        raise FleetError(f"state file is missing the {args.kind} section")
    tx_hex = section.get("tx_hex")
    if not tx_hex:
        raise FleetError(f"state file does not contain {args.kind}.tx_hex")
    return tx_hex


def read_txid(args: argparse.Namespace) -> str:
    if args.txid:
        return args.txid
    if not args.state_file:
        raise FleetError("pass --txid or --state-file")

    state = load_json(args.state_file)
    section = state.get(args.kind)
    if not isinstance(section, dict):
        raise FleetError(f"state file is missing the {args.kind} section")
    txid = section.get("txid")
    if not txid:
        raise FleetError(f"state file does not contain {args.kind}.txid")
    return txid


def target_host(host: str, ssh_user: str | None) -> str:
    return f"{ssh_user}@{host}" if ssh_user else host


def run_ssh_command(host: str, args: argparse.Namespace, remote_command: list[str]) -> str:
    ssh_cmd = ["ssh"]
    for option in args.ssh_option:
        ssh_cmd.extend(["-o", option])
    ssh_cmd.append(target_host(host, args.ssh_user))
    ssh_cmd.append(shlex.join(remote_command))

    result = subprocess.run(ssh_cmd, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or f"ssh exited with code {result.returncode}"
        raise FleetError(f"{host}: {detail}")
    return result.stdout.strip()


def run_remote_cli(host: str, args: argparse.Namespace, method: str, *params: str, parse_json: bool = True):
    remote_command = [
        args.cli_path,
        f"-conf={args.conf}",
        f"-datadir={args.datadir}",
        method,
        *params,
    ]
    output = run_ssh_command(host, args, remote_command)
    if not parse_json:
        return output
    if not output:
        return None
    try:
        return json.loads(output)
    except json.JSONDecodeError as exc:
        raise FleetError(f"{host}: expected JSON output from {method}, got: {output}") from exc


def emit(args: argparse.Namespace, payload) -> None:
    if args.json:
        print(json.dumps(payload, indent=2, sort_keys=True))
        return

    if isinstance(payload, list):
        for item in payload:
            print(format_host_payload(item))
        return

    print(format_host_payload(payload))


def format_host_payload(payload: dict) -> str:
    host = payload.get("host", "<unknown>")
    body = {key: value for key, value in payload.items() if key != "host"}
    lines = [f"[{host}]"]
    for key, value in body.items():
        rendered = json.dumps(value, sort_keys=True) if isinstance(value, (dict, list)) else str(value)
        lines.append(f"{key}: {rendered}")
    return "\n".join(lines)


def command_info(args: argparse.Namespace) -> None:
    results = []
    for host in args.host:
        network = run_remote_cli(host, args, "getnetworkinfo")
        blockchain = run_remote_cli(host, args, "getblockchaininfo")
        queue_enabled = True
        queue_size = 0
        queue_error = None
        try:
            queued = run_remote_cli(host, args, "listqsbtransactions")
            queue_size = len(queued)
        except FleetError as exc:
            queue_enabled = False
            queue_error = str(exc).split(": ", 1)[-1]

        results.append(
            {
                "host": host,
                "subversion": network.get("subversion"),
                "version": network.get("version"),
                "chain": blockchain.get("chain"),
                "blocks": blockchain.get("blocks"),
                "headers": blockchain.get("headers"),
                "bestblockhash": blockchain.get("bestblockhash"),
                "initialblockdownload": blockchain.get("initialblockdownload"),
                "qsb_operator_enabled": queue_enabled,
                "queued_qsb_transactions": queue_size,
                "queue_probe_error": queue_error,
            }
        )
    emit(args, results)


def command_list(args: argparse.Namespace) -> None:
    results = []
    for host in args.host:
        queued = run_remote_cli(host, args, "listqsbtransactions")
        results.append({"host": host, "queued": queued})
    emit(args, results)


def command_submit(args: argparse.Namespace) -> None:
    tx_hex = read_tx_hex(args)
    results = []
    for host in args.host:
        result = run_remote_cli(host, args, "submitqsbtransaction", tx_hex)
        result["host"] = host
        results.append(result)
    emit(args, results)


def command_remove(args: argparse.Namespace) -> None:
    results = []
    for host in args.host:
        result = run_remote_cli(host, args, "removeqsbtransaction", args.txid)
        result["host"] = host
        results.append(result)
    emit(args, results)


def find_tx_in_recent_blocks(host: str, args: argparse.Namespace, txid: str) -> dict | None:
    blockchain = run_remote_cli(host, args, "getblockchaininfo")
    tip_height = int(blockchain["blocks"])
    start_height = max(0, tip_height - args.lookback_blocks + 1)

    for height in range(tip_height, start_height - 1, -1):
        blockhash = run_remote_cli(host, args, "getblockhash", str(height), parse_json=False)
        block = run_remote_cli(host, args, "getblock", blockhash, "1")
        if txid in block.get("tx", []):
            return {
                "height": block["height"],
                "hash": block["hash"],
                "confirmations": block["confirmations"],
            }
    return None


def command_wait_mined(args: argparse.Namespace) -> None:
    txid = read_txid(args)
    deadline = time.monotonic() + args.timeout_secs
    pending = set(args.host)
    confirmations: dict[str, dict] = {}

    while pending:
        finished_hosts = []
        for host in pending:
            confirmation = find_tx_in_recent_blocks(host, args, txid)
            if confirmation is None:
                continue
            confirmations[host] = confirmation
            finished_hosts.append(host)

        for host in finished_hosts:
            pending.remove(host)

        if not pending:
            break
        if time.monotonic() >= deadline:
            pending_hosts = ", ".join(sorted(pending))
            raise FleetError(f"timed out waiting for {txid} on: {pending_hosts}")
        time.sleep(args.poll_secs)

    results = []
    for host in args.host:
        results.append(
            {
                "host": host,
                "txid": txid,
                "mined": True,
                **confirmations[host],
            }
        )
    emit(args, results)


def add_host_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--host", action="append", required=True, help="SSH host or alias. Repeat for multiple validators.")
    parser.add_argument("--ssh-user", help="Optional SSH user override.")
    parser.add_argument(
        "--ssh-option",
        action="append",
        default=[],
        help="Additional ssh -o option, for example StrictHostKeyChecking=no.",
    )
    parser.add_argument("--cli-path", default=DEFAULT_CLI_PATH, help=f"Remote rng-cli path. Default: {DEFAULT_CLI_PATH}")
    parser.add_argument("--conf", default=DEFAULT_CONF_PATH, help=f"Remote rng.conf path. Default: {DEFAULT_CONF_PATH}")
    parser.add_argument("--datadir", default=DEFAULT_DATADIR, help=f"Remote datadir. Default: {DEFAULT_DATADIR}")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON.")


def add_tx_source_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--tx-hex", help="Raw transaction hex to submit.")
    parser.add_argument("--tx-hex-file", help="File containing raw transaction hex.")
    parser.add_argument("--state-file", help="QSB state file containing tx_hex and txid.")
    parser.add_argument(
        "--kind",
        choices=("funding", "spend"),
        default="funding",
        help="Which transaction to read from --state-file. Default: funding.",
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Operate remote RNG QSB queues over SSH")
    subparsers = parser.add_subparsers(dest="command", required=True)

    info = subparsers.add_parser("info", help="Show remote node and QSB queue status")
    add_host_args(info)
    info.set_defaults(func=command_info)

    list_cmd = subparsers.add_parser("list", help="List queued QSB candidates on each host")
    add_host_args(list_cmd)
    list_cmd.set_defaults(func=command_list)

    submit = subparsers.add_parser("submit", help="Submit a raw QSB transaction to each host")
    add_host_args(submit)
    add_tx_source_args(submit)
    submit.set_defaults(func=command_submit)

    remove = subparsers.add_parser("remove", help="Remove a queued QSB transaction by txid")
    add_host_args(remove)
    remove.add_argument("--txid", required=True, help="Queued transaction id to remove.")
    remove.set_defaults(func=command_remove)

    wait_mined = subparsers.add_parser("wait-mined", help="Wait for a txid to appear in a recent block on each host")
    add_host_args(wait_mined)
    wait_mined.add_argument("--txid", help="Transaction id to monitor.")
    wait_mined.add_argument("--state-file", help="Optional QSB state file containing the txid.")
    wait_mined.add_argument(
        "--kind",
        choices=("funding", "spend"),
        default="funding",
        help="Which transaction to read from --state-file. Default: funding.",
    )
    wait_mined.add_argument("--lookback-blocks", type=int, default=DEFAULT_LOOKBACK_BLOCKS)
    wait_mined.add_argument("--timeout-secs", type=int, default=DEFAULT_TIMEOUT_SECS)
    wait_mined.add_argument("--poll-secs", type=int, default=DEFAULT_POLL_SECS)
    wait_mined.set_defaults(func=command_wait_mined)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        args.func(args)
    except (FleetError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
