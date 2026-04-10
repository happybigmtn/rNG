#!/usr/bin/env python3
"""RNG QSB builder utilities.

This file currently implements the Milestone 1 feasibility spike:

- create a large consensus-valid bare script directly in scriptPubKey,
- fund it with ordinary wallet inputs via fundrawtransaction,
- build the matching spend outside the wallet, and
- persist one-time secret state outside wallet storage.
"""

from __future__ import annotations

import argparse
import base64
from decimal import Decimal, InvalidOperation, ROUND_DOWN
import hashlib
import json
from pathlib import Path
import sys
import urllib.error
import urllib.parse
import urllib.request

from state import load_json, new_fixture, new_state, write_json
from template_v1 import (
    DEFAULT_CHUNK_SIZE,
    DEFAULT_PAYLOAD_BYTES,
    TEMPLATE_VERSION,
    build_funding_script,
    derive_material,
    parse_seed,
    push_data,
    script_sha256,
    sha256,
    split_payload,
)


COIN = 100_000_000
DEFAULT_FUNDING_FEE_RATE_SAT_VB = "1"
DEFAULT_SPEND_FEE_SATS = 1_000


class QSBError(RuntimeError):
    """User-facing error."""


class RpcClient:
    def __init__(self, url: str, username: str, password: str) -> None:
        token = f"{username}:{password}".encode("utf-8")
        self._auth_header = base64.b64encode(token).decode("ascii")
        self._request_id = 0
        self.url = url

    @classmethod
    def from_args(cls, args: argparse.Namespace, *, wallet: str | None = None) -> "RpcClient":
        if not getattr(args, "rpc_url", None):
            raise QSBError("rpc-url is required for this command")
        parsed = urllib.parse.urlsplit(args.rpc_url)
        if not parsed.scheme:
            raise QSBError("rpc-url must include a scheme such as http://")

        username = args.rpc_user or (urllib.parse.unquote(parsed.username) if parsed.username else "")
        password = args.rpc_password or (urllib.parse.unquote(parsed.password) if parsed.password else "")

        if args.rpc_cookie_file:
            cookie = Path(args.rpc_cookie_file).read_text(encoding="utf-8").strip()
            username, password = cookie.split(":", 1)

        if not username or not password:
            raise QSBError("missing RPC credentials; pass rpc-url with user:pass, rpc-user/rpc-password, or rpc-cookie-file")

        path = parsed.path or ""
        if wallet and not path.startswith("/wallet/"):
            path = path.rstrip("/")
            path = f"{path}/wallet/{urllib.parse.quote(wallet)}" if path else f"/wallet/{urllib.parse.quote(wallet)}"

        host = parsed.hostname or ""
        if parsed.port is not None:
            host = f"{host}:{parsed.port}"

        url = urllib.parse.urlunsplit((parsed.scheme, host, path, parsed.query, parsed.fragment))
        return cls(url=url, username=username, password=password)

    def call(self, method: str, params: list | None = None) -> object:
        self._request_id += 1
        payload = json.dumps(
            {"jsonrpc": "1.0", "id": self._request_id, "method": method, "params": params or []}
        ).encode("utf-8")
        request = urllib.request.Request(self.url, data=payload, method="POST")
        request.add_header("Authorization", f"Basic {self._auth_header}")
        request.add_header("Content-Type", "application/json")

        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                body = response.read()
        except urllib.error.HTTPError as exc:
            body = exc.read()
            raise self._rpc_error_from_body(body) from exc
        except urllib.error.URLError as exc:
            raise QSBError(f"RPC connection failed: {exc.reason}") from exc

        decoded = json.loads(body.decode("utf-8"))
        if decoded["error"] is not None:
            raise QSBError(f"RPC {method} failed: {decoded['error']['message']}")
        return decoded["result"]

    def _rpc_error_from_body(self, body: bytes) -> QSBError:
        if not body:
            return QSBError("RPC request failed with no response body")
        try:
            decoded = json.loads(body.decode("utf-8"))
        except json.JSONDecodeError:
            return QSBError(body.decode("utf-8", errors="replace"))
        if isinstance(decoded, dict) and decoded.get("error"):
            return QSBError(decoded["error"]["message"])
        return QSBError(json.dumps(decoded))


def parse_amount_to_sats(amount_str: str) -> int:
    try:
        amount = Decimal(amount_str)
    except InvalidOperation as exc:
        raise QSBError(f"invalid amount: {amount_str}") from exc
    if amount <= 0:
        raise QSBError("amount must be positive")
    sats = int((amount * COIN).to_integral_value(rounding=ROUND_DOWN))
    if Decimal(sats) / COIN != amount:
        raise QSBError("amount must have no more than 8 decimal places")
    return sats


def format_amount(amount_sats: int) -> str:
    return f"{Decimal(amount_sats) / COIN:.8f}"


def parse_positive_decimal_arg(value: str, *, field_name: str) -> str:
    try:
        parsed = Decimal(value)
    except InvalidOperation as exc:
        raise QSBError(f"invalid {field_name}: {value}") from exc
    if not parsed.is_finite() or parsed <= 0:
        raise QSBError(f"{field_name} must be positive")
    return format(parsed.normalize(), "f")


def hash256(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def serialize_varint(value: int) -> bytes:
    if value < 0xFD:
        return bytes([value])
    if value <= 0xFFFF:
        return b"\xFD" + value.to_bytes(2, "little")
    if value <= 0xFFFFFFFF:
        return b"\xFE" + value.to_bytes(4, "little")
    return b"\xFF" + value.to_bytes(8, "little")


def create_raw_transaction(*, inputs: list[dict], outputs: list[tuple[int, bytes]], version: int = 2, locktime: int = 0) -> str:
    raw = bytearray()
    raw.extend(version.to_bytes(4, "little", signed=True))
    raw.extend(serialize_varint(len(inputs)))
    for txin in inputs:
        raw.extend(bytes.fromhex(txin["txid"])[::-1])
        raw.extend(int(txin["vout"]).to_bytes(4, "little"))
        script_sig = txin.get("script_sig", b"")
        raw.extend(serialize_varint(len(script_sig)))
        raw.extend(script_sig)
        raw.extend(int(txin.get("sequence", 0xFFFFFFFF)).to_bytes(4, "little"))
    raw.extend(serialize_varint(len(outputs)))
    for value_sats, script_pubkey in outputs:
        raw.extend(int(value_sats).to_bytes(8, "little", signed=True))
        raw.extend(serialize_varint(len(script_pubkey)))
        raw.extend(script_pubkey)
    raw.extend(locktime.to_bytes(4, "little"))
    return raw.hex()


def txid_from_hex(tx_hex: str) -> str:
    return hash256(bytes.fromhex(tx_hex))[::-1].hex()


def write_hex_file(path: str | None, hex_value: str) -> None:
    if path is None:
        return
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(hex_value + "\n", encoding="utf-8")


def build_fixture_dict(args: argparse.Namespace) -> dict:
    seed = parse_seed(args.seed)
    material = derive_material(seed, payload_bytes=args.payload_bytes)
    script = build_funding_script(material, chunk_size=args.payload_chunk_size)
    chunks = split_payload(material.payload, args.payload_chunk_size)
    return new_fixture(
        template_version=TEMPLATE_VERSION,
        seed_hex=material.seed.hex(),
        secret_preimage_hex=material.secret_preimage.hex(),
        secret_hash_hex=material.secret_hash.hex(),
        metadata_commitment_hex=material.metadata_commitment.hex(),
        payload_sha256_hex=sha256(material.payload).hex(),
        payload_bytes=len(material.payload),
        payload_chunk_size=args.payload_chunk_size,
        payload_chunk_count=len(chunks),
        funding_script_pubkey_hex=script.hex(),
        funding_script_pubkey_sha256=script_sha256(script),
        funding_script_size=len(script),
    )


def command_fixture(args: argparse.Namespace) -> None:
    fixture = build_fixture_dict(args)
    write_json(args.state_file, fixture)
    print(f"Wrote fixture file {args.state_file}")
    print(f"Template version: {fixture['template_version']}")
    print(f"Funding script size: {fixture['funding']['script_size']} bytes")
    print(f"Funding script sha256: {fixture['funding']['script_pubkey_sha256']}")


def command_toy_funding(args: argparse.Namespace) -> None:
    rpc = RpcClient.from_args(args, wallet=args.wallet)
    chain = rpc.call("getblockchaininfo")["chain"]
    seed = parse_seed(args.seed)
    material = derive_material(seed, payload_bytes=args.payload_bytes)
    script = build_funding_script(material, chunk_size=args.payload_chunk_size)
    amount_sats = parse_amount_to_sats(args.amount)
    fee_rate_sat_vb = parse_positive_decimal_arg(args.fee_rate_sat_vb, field_name="fee-rate-sat-vb")

    raw_shell = create_raw_transaction(inputs=[], outputs=[(amount_sats, script)])
    funded = rpc.call("fundrawtransaction", [raw_shell, {"change_position": 1, "fee_rate": fee_rate_sat_vb}])
    signed = rpc.call("signrawtransactionwithwallet", [funded["hex"]])
    if not signed["complete"]:
        raise QSBError("signrawtransactionwithwallet did not complete")

    decoded = rpc.call("decoderawtransaction", [signed["hex"]])
    qsb_vout = None
    for vout in decoded["vout"]:
        if vout["scriptPubKey"]["hex"] == script.hex():
            qsb_vout = vout["n"]
            break
    if qsb_vout is None:
        raise QSBError("funded transaction does not contain the expected QSB output")

    chunks = split_payload(material.payload, args.payload_chunk_size)
    state = new_state(
        template_version=TEMPLATE_VERSION,
        network=chain,
        seed_hex=material.seed.hex(),
        secret_preimage_hex=material.secret_preimage.hex(),
        secret_hash_hex=material.secret_hash.hex(),
        metadata_commitment_hex=material.metadata_commitment.hex(),
        payload_sha256_hex=sha256(material.payload).hex(),
        payload_bytes=len(material.payload),
        payload_chunk_size=args.payload_chunk_size,
        payload_chunk_count=len(chunks),
        funding={
            "amount": format_amount(amount_sats),
            "amount_sats": amount_sats,
            "script_pubkey_hex": script.hex(),
            "script_pubkey_sha256": script_sha256(script),
            "script_size": len(script),
            "tx_hex": signed["hex"],
            "txid": decoded["txid"],
            "vout": qsb_vout,
            "fundrawtransaction_fee": funded["fee"],
            "fundrawtransaction_fee_rate_sat_vb": fee_rate_sat_vb,
            "change_position": funded.get("changepos"),
        },
    )

    write_json(args.state_file, state)
    write_hex_file(args.tx_out, signed["hex"])
    print(f"Wrote state file {args.state_file}")
    if args.tx_out:
        print(f"Funding tx hex written to {args.tx_out}")
    else:
        print("Funding tx hex written to state file")
    print(f"Funding txid: {decoded['txid']}")
    print(f"Bare script size: {len(script)} bytes")


def command_toy_spend(args: argparse.Namespace) -> None:
    state = load_json(args.state_file)
    if state.get("template_version") != TEMPLATE_VERSION:
        raise QSBError(f"unsupported template_version: {state.get('template_version')}")
    if state["spend"]["consumed"]:
        raise QSBError("QSB state already spent")

    if bool(args.destination_address) == bool(args.destination_script_hex):
        raise QSBError("pass exactly one of --destination-address or --destination-script-hex")

    destination_script_hex = args.destination_script_hex
    if args.destination_address:
        rpc = RpcClient.from_args(args)
        info = rpc.call("validateaddress", [args.destination_address])
        if not info["isvalid"]:
            raise QSBError(f"invalid destination address: {args.destination_address}")
        destination_script_hex = info["scriptPubKey"]

    fee_sats = int(args.fee_sats)
    funding_amount_sats = int(state["funding"]["amount_sats"])
    spend_amount_sats = funding_amount_sats - fee_sats
    if spend_amount_sats <= 0:
        raise QSBError("fee exceeds funding amount")

    spend_tx_hex = create_raw_transaction(
        inputs=[{
            "txid": state["funding"]["txid"],
            "vout": state["funding"]["vout"],
            "script_sig": push_data(bytes.fromhex(state["secret_preimage_hex"])),
            "sequence": 0xFFFFFFFF,
        }],
        outputs=[(spend_amount_sats, bytes.fromhex(destination_script_hex))],
        version=2,
        locktime=0,
    )
    spend_txid = txid_from_hex(spend_tx_hex)

    state["spend"] = {
        "consumed": True,
        "destination_address": args.destination_address,
        "destination_script_pubkey_hex": destination_script_hex,
        "fee_sats": fee_sats,
        "tx_hex": spend_tx_hex,
        "txid": spend_txid,
    }
    write_json(args.state_file, state)
    write_hex_file(args.tx_out, spend_tx_hex)

    print(f"Updated state file {args.state_file}")
    if args.tx_out:
        print(f"Spend tx hex written to {args.tx_out}")
    else:
        print("Spend tx hex written to state file")
    print(f"Spend txid: {spend_txid}")
    print(f"Spend amount: {format_amount(spend_amount_sats)}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="RNG QSB tooling")
    subparsers = parser.add_subparsers(dest="command", required=True)

    fixture = subparsers.add_parser("fixture", help="Write a deterministic template fixture")
    fixture.add_argument("--seed", required=True, help="Hex seed for deterministic fixture generation")
    fixture.add_argument("--payload-bytes", type=int, default=DEFAULT_PAYLOAD_BYTES)
    fixture.add_argument("--payload-chunk-size", type=int, default=DEFAULT_CHUNK_SIZE)
    fixture.add_argument("--state-file", required=True, help="Path to write the fixture JSON")
    fixture.set_defaults(func=command_fixture)

    funding = subparsers.add_parser("toy-funding", help="Build a wallet-funded large bare-script transaction")
    add_rpc_args(funding, include_wallet=True)
    funding.add_argument("--seed", help="Optional deterministic seed in hex")
    funding.add_argument("--payload-bytes", type=int, default=DEFAULT_PAYLOAD_BYTES)
    funding.add_argument("--payload-chunk-size", type=int, default=DEFAULT_CHUNK_SIZE)
    funding.add_argument("--amount", required=True, help="Funding amount in RNG, for example 1.0")
    funding.add_argument(
        "--fee-rate-sat-vb",
        default=DEFAULT_FUNDING_FEE_RATE_SAT_VB,
        help="Explicit fundrawtransaction fee rate in sat/vB; default: %(default)s",
    )
    funding.add_argument("--state-file", required=True, help="Path to write the QSB state JSON")
    funding.add_argument("--tx-out", help="Optional file to write the funded transaction hex")
    funding.set_defaults(func=command_toy_funding)

    spend = subparsers.add_parser("toy-spend", help="Build the matching spend from a saved QSB state file")
    spend.add_argument("--rpc-url", help="Base RPC URL, required when using --destination-address")
    spend.add_argument("--rpc-user", help="RPC username")
    spend.add_argument("--rpc-password", help="RPC password")
    spend.add_argument("--rpc-cookie-file", help="Path to the RPC auth cookie file")
    spend.add_argument("--state-file", required=True, help="Path to the QSB state JSON")
    spend.add_argument("--destination-address", help="Address to pay from the QSB output")
    spend.add_argument("--destination-script-hex", help="Destination scriptPubKey in hex")
    spend.add_argument("--fee-sats", type=int, default=DEFAULT_SPEND_FEE_SATS)
    spend.add_argument("--tx-out", help="Optional file to write the spend transaction hex")
    spend.set_defaults(func=command_toy_spend)

    return parser


def add_rpc_args(parser: argparse.ArgumentParser, *, include_wallet: bool) -> None:
    parser.add_argument("--rpc-url", required=True, help="Base RPC URL, for example http://127.0.0.1:18443")
    parser.add_argument("--rpc-user", help="RPC username")
    parser.add_argument("--rpc-password", help="RPC password")
    parser.add_argument("--rpc-cookie-file", help="Path to the RPC auth cookie file")
    if include_wallet:
        parser.add_argument("--wallet", required=True, help="Wallet name to use for wallet RPCs")


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        args.func(args)
    except (QSBError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
