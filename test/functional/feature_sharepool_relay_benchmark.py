#!/usr/bin/env python3
# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Measure activated sharepool P2P relay viability on regtest."""

import json
import math
import os
from pathlib import Path
import platform
import subprocess
import time

from test_framework.messages import (
    CBlockHeader,
    CShareRecord,
    msg_getshare,
    msg_share,
)
from test_framework.p2p import P2PInterface, P2P_SUBVERSION, p2p_lock
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


SHARE_MESSAGE_TYPES = ("shareinv", "getshare", "share")
LATENCY_P50_THRESHOLD_SECONDS = 5.0
LATENCY_P99_THRESHOLD_SECONDS = 10.0
BANDWIDTH_THRESHOLD_KB_PER_SECOND = 10.0
ORPHAN_RATE_THRESHOLD_PERCENT = 20.0
COMPLETENESS_THRESHOLD_PERCENT = 100.0


class ShareObserver(P2PInterface):
    def __init__(self, node_index):
        super().__init__()
        self.node_index = node_index
        self.inv_times = {}
        self.share_times = {}
        self.shares = {}

    def on_shareinv(self, message):
        now = time.monotonic()
        for share_id in message.share_ids:
            self.inv_times.setdefault(share_id, now)
        self.send_without_ping(msg_getshare(message.share_ids))

    def on_share(self, message):
        now = time.monotonic()
        for share in message.shares:
            self.share_times.setdefault(share.share_id, now)
            self.shares.setdefault(share.share_id, share)


class ShareSeeder(P2PInterface):
    def __init__(self):
        super().__init__()
        self.shares = {}
        self.getshare_requests = []

    def add_share(self, share):
        self.shares[share.share_id] = share

    def on_getshare(self, message):
        now = time.monotonic()
        self.getshare_requests.extend((share_id, now) for share_id in message.share_ids)
        requested = [self.shares[share_id] for share_id in message.share_ids if share_id in self.shares]
        if requested:
            self.send_without_ping(msg_share(requested))


def uint256_from_rpc_hex(value):
    return int(value, 16)


def percentile(values, percentile_value):
    if not values:
        return None
    ordered = sorted(values)
    rank = math.ceil((percentile_value / 100.0) * len(ordered)) - 1
    return ordered[max(0, min(rank, len(ordered) - 1))]


def hex_share_id(share_id):
    return f"{share_id:064x}"


class SharepoolRelayBenchmark(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        self.extra_args = [["-vbparams=sharepool:0:9999999999:0"] for _ in range(self.num_nodes)]

    def add_options(self, parser):
        parser.add_argument(
            "--share-interval",
            dest="share_interval",
            type=float,
            default=10.0,
            help="Seconds between generated shares. Default: %(default)s",
        )
        parser.add_argument(
            "--duration",
            dest="duration",
            type=float,
            default=None,
            help="Measurement window in seconds. Defaults to shares * interval, or 1800 seconds.",
        )
        parser.add_argument(
            "--shares",
            dest="shares",
            type=int,
            default=None,
            help="Number of shares to generate. Defaults to duration / interval.",
        )
        parser.add_argument(
            "--propagation-timeout",
            dest="propagation_timeout",
            type=float,
            default=10.0,
            help="Seconds to wait for each share to reach all observers. Default: %(default)s",
        )
        parser.add_argument(
            "--output",
            dest="output",
            default=None,
            help="Path for the JSON measurement report. Default: tmpdir/pool-06-relay-viability.json",
        )

    def setup_network(self):
        self.setup_nodes()
        self.connect_full_mesh()
        self.sync_all()

    def connect_full_mesh(self):
        for high in range(self.num_nodes):
            for low in range(high):
                if not self.nodes_are_connected(high, low):
                    self.connect_nodes(high, low)

    def nodes_are_connected(self, a, b):
        needle = f"testnode{b}"
        return any(needle in peer["subver"] for peer in self.nodes[a].getpeerinfo())

    def run_test(self):
        share_interval = self.options.share_interval
        if share_interval <= 0:
            raise AssertionError("--share-interval must be positive")

        share_count = self.options.shares
        duration = self.options.duration
        if share_count is None and duration is None:
            duration = 1800.0
        if share_count is None:
            share_count = max(1, int(duration // share_interval))
        if duration is None:
            duration = share_count * share_interval
        if share_count <= 0:
            raise AssertionError("--shares must be positive")
        if duration <= 0:
            raise AssertionError("--duration must be positive")

        output_path = Path(self.options.output or Path(self.options.tmpdir) / "pool-06-relay-viability.json")
        output_path.parent.mkdir(parents=True, exist_ok=True)

        self.log.info("Activate sharepool deployment on 4-node regtest mesh")
        self.activate_sharepool()

        self.log.info("Attach one observer and one share seeder to each node")
        observers = [self.nodes[i].add_p2p_connection(ShareObserver(i)) for i in range(self.num_nodes)]
        seeders = [self.nodes[i].add_p2p_connection(ShareSeeder()) for i in range(self.num_nodes)]

        self.log.info("Build reusable PoW-valid candidate header for share records")
        header = self.make_valid_candidate_header()

        self.log.info("Start relay measurement: %d shares, %.3fs interval, %.3fs window", share_count, share_interval, duration)
        start_peerinfo = self.snapshot_peerinfo()
        measurement_start = time.monotonic()

        shares = []
        send_times = {}
        parent_share = 0
        for sequence in range(share_count):
            origin = sequence % self.num_nodes
            share = self.make_share(header=header, parent_share=parent_share, sequence=sequence, origin=origin)
            parent_share = share.share_id
            shares.append({
                "sequence": sequence,
                "origin_node": origin,
                "share_id": hex_share_id(share.share_id),
                "parent_share": hex_share_id(share.parent_share) if share.parent_share else None,
            })

            seeders[origin].add_share(share)
            send_times[share.share_id] = time.monotonic()
            seeders[origin].send_without_ping(msg_share([share]))
            self.wait_for_observers(observers, share.share_id, timeout=self.options.propagation_timeout)

            next_send = measurement_start + ((sequence + 1) * share_interval)
            if sequence != share_count - 1 and next_send > time.monotonic():
                time.sleep(next_send - time.monotonic())

        measurement_end_target = measurement_start + duration
        if measurement_end_target > time.monotonic():
            time.sleep(measurement_end_target - time.monotonic())
        measurement_end = time.monotonic()
        end_peerinfo = self.snapshot_peerinfo()

        result = self.build_report(
            start_peerinfo=start_peerinfo,
            end_peerinfo=end_peerinfo,
            observers=observers,
            seeders=seeders,
            shares=shares,
            send_times=send_times,
            measurement_start=measurement_start,
            measurement_end=measurement_end,
            configured_duration=duration,
            share_interval=share_interval,
            share_count=share_count,
        )

        output_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf8")
        self.log.info("Wrote relay benchmark report to %s", output_path)
        self.log_summary(result)
        self.assert_thresholds(result)

    def activate_sharepool(self):
        address = self.nodes[0].get_deterministic_priv_key().address
        for _ in range(6):
            self.generatetoaddress(self.nodes[0], 72, address, sync_fun=self.sync_all)
        for node in self.nodes:
            self.wait_until(lambda n=node: n.getdeploymentinfo()["deployments"]["sharepool"]["active"])

    def make_valid_candidate_header(self):
        address = self.nodes[0].get_deterministic_priv_key().address
        block_hash = self.generatetoaddress(self.nodes[0], 1, address, sync_fun=self.sync_all)[0]
        block = self.nodes[0].getblock(block_hash)

        header = CBlockHeader()
        header.nVersion = block["version"]
        header.hashPrevBlock = uint256_from_rpc_hex(block["previousblockhash"])
        header.hashMerkleRoot = uint256_from_rpc_hex(block["merkleroot"])
        header.nTime = block["time"]
        header.nBits = int(block["bits"], 16)
        header.nNonce = block["nonce"]
        return header

    def make_share(self, *, header, parent_share, sequence, origin):
        share = CShareRecord()
        share.parent_share = parent_share
        share.prev_block_hash = header.hashPrevBlock
        share.candidate_header = header
        share.share_nBits = header.nBits
        share.payout_script = bytes.fromhex("0014") + origin.to_bytes(1, "little") + sequence.to_bytes(4, "little") + bytes(15)
        return share

    def wait_for_observers(self, observers, share_id, *, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with p2p_lock:
                if all(share_id in observer.share_times for observer in observers):
                    return
            time.sleep(0.02)

    def snapshot_peerinfo(self):
        snapshots = []
        for node_index, node in enumerate(self.nodes):
            peer_rows = []
            for peer in node.getpeerinfo():
                if peer.get("subver") == P2P_SUBVERSION:
                    continue
                if "testnode" not in peer.get("subver", ""):
                    continue
                peer_rows.append({
                    "peer_id": peer["id"],
                    "subver": peer["subver"],
                    "bytessent": peer["bytessent"],
                    "bytesrecv": peer["bytesrecv"],
                    "bytessent_per_msg": dict(peer["bytessent_per_msg"]),
                    "bytesrecv_per_msg": dict(peer["bytesrecv_per_msg"]),
                })
            snapshots.append({"node": node_index, "peers": peer_rows})
        return snapshots

    def build_report(self, *, start_peerinfo, end_peerinfo, observers, seeders, shares, send_times,
                     measurement_start, measurement_end, configured_duration, share_interval, share_count):
        share_ids = [int(share["share_id"], 16) for share in shares]
        share_by_id = {int(share["share_id"], 16): share for share in shares}
        receipt_rows = []
        latency_to_all = []
        missing_receipts = 0
        parent_observations = 0
        orphan_order_events = 0

        with p2p_lock:
            for share_id in share_ids:
                node_latencies = []
                for observer in observers:
                    received_at = observer.share_times.get(share_id)
                    if received_at is None:
                        missing_receipts += 1
                        receipt_rows.append({
                            "share_id": hex_share_id(share_id),
                            "node": observer.node_index,
                            "latency_ms": None,
                            "missing": True,
                        })
                        continue
                    latency = received_at - send_times[share_id]
                    node_latencies.append(latency)
                    receipt_rows.append({
                        "share_id": hex_share_id(share_id),
                        "node": observer.node_index,
                        "latency_ms": round(latency * 1000.0, 3),
                        "missing": False,
                    })

                    parent_hex = share_by_id[share_id]["parent_share"]
                    if parent_hex is not None:
                        parent_observations += 1
                        parent_received_at = observer.share_times.get(int(parent_hex, 16))
                        if parent_received_at is None or received_at < parent_received_at:
                            orphan_order_events += 1
                if len(node_latencies) == len(observers):
                    latency_to_all.append(max(node_latencies))

        bandwidth_rows = self.compute_bandwidth(start_peerinfo, end_peerinfo, configured_duration)
        max_bandwidth_kb_s = max((row["share_relay_kb_s"] for row in bandwidth_rows), default=0.0)
        completeness_denominator = share_count * self.num_nodes
        completeness_percent = 100.0 * (completeness_denominator - missing_receipts) / completeness_denominator
        orphan_rate_percent = 0.0 if parent_observations == 0 else 100.0 * orphan_order_events / parent_observations

        p50 = percentile(latency_to_all, 50)
        p99 = percentile(latency_to_all, 99)
        p50_ms = None if p50 is None else round(p50 * 1000.0, 3)
        p99_ms = None if p99 is None else round(p99 * 1000.0, 3)

        return {
            "task_id": "POOL-06-GATE",
            "decision": "GO" if (
                p50 is not None
                and p99 is not None
                and p50 < LATENCY_P50_THRESHOLD_SECONDS
                and p99 < LATENCY_P99_THRESHOLD_SECONDS
                and max_bandwidth_kb_s < BANDWIDTH_THRESHOLD_KB_PER_SECOND
                and orphan_rate_percent < ORPHAN_RATE_THRESHOLD_PERCENT
                and completeness_percent >= COMPLETENESS_THRESHOLD_PERCENT
            ) else "NO-GO",
            "environment": {
                "platform": platform.platform(),
                "python": platform.python_version(),
                "cpu_count": os.cpu_count(),
                "git_head": self.git_head(),
                "transport": "v2" if self.options.v2transport else "v1",
            },
            "methodology": {
                "node_count": self.num_nodes,
                "topology": "full mesh",
                "activation": "-vbparams=sharepool:0:9999999999:0; six 72-block regtest periods mined before measurement",
                "share_generation": (
                    "P2P test harness seeded serialized CShareRecord messages because POOL-08 miner/RPC share "
                    "production does not exist yet. Each record used one PoW-valid mined block header and a "
                    "distinct parent/payout script so the relay path handled unique linked shares."
                ),
                "configured_duration_seconds": configured_duration,
                "actual_duration_seconds": round(measurement_end - measurement_start, 3),
                "share_interval_seconds": share_interval,
                "share_count": share_count,
            },
            "thresholds": {
                "latency_p50_ms_lt": LATENCY_P50_THRESHOLD_SECONDS * 1000.0,
                "latency_p99_ms_lt": LATENCY_P99_THRESHOLD_SECONDS * 1000.0,
                "bandwidth_kb_s_per_node_lt": BANDWIDTH_THRESHOLD_KB_PER_SECOND,
                "orphan_rate_percent_lt": ORPHAN_RATE_THRESHOLD_PERCENT,
                "propagation_completeness_percent_gte": COMPLETENESS_THRESHOLD_PERCENT,
            },
            "metrics": {
                "latency_to_all_p50_ms": p50_ms,
                "latency_to_all_p99_ms": p99_ms,
                "latency_to_all_max_ms": None if not latency_to_all else round(max(latency_to_all) * 1000.0, 3),
                "max_share_relay_bandwidth_kb_s_per_node": round(max_bandwidth_kb_s, 6),
                "orphan_order_events": orphan_order_events,
                "parent_observations": parent_observations,
                "orphan_order_rate_percent": round(orphan_rate_percent, 6),
                "missing_receipts": missing_receipts,
                "propagation_completeness_percent": round(completeness_percent, 6),
                "origin_getshare_requests": sum(len(seeder.getshare_requests) for seeder in seeders),
            },
            "bandwidth_by_node": bandwidth_rows,
            "shares": shares,
            "receipts": receipt_rows,
            "limitations": [
                "No node-native submitshare/getsharechaininfo RPC exists in this slice, so the benchmark injects shares over P2P and observes propagation through test peers.",
                "The benchmark measures child-before-parent observer arrival order as the externally visible orphan-order proxy; the live node does not expose share orphan counters through RPC yet.",
                "Observers request each announced share to timestamp full payload receipt. Observer traffic is excluded from the per-node bandwidth calculation by filtering getpeerinfo to node-to-node peers.",
            ],
        }

    def compute_bandwidth(self, start_peerinfo, end_peerinfo, duration):
        start_by_node = {entry["node"]: entry for entry in start_peerinfo}
        rows = []
        for end_entry in end_peerinfo:
            node_index = end_entry["node"]
            start_peers = {peer["subver"]: peer for peer in start_by_node[node_index]["peers"]}
            share_sent = 0
            share_recv = 0
            total_sent = 0
            total_recv = 0
            peer_count = 0
            for end_peer in end_entry["peers"]:
                start_peer = start_peers.get(end_peer["subver"])
                if start_peer is None:
                    continue
                peer_count += 1
                total_sent += end_peer["bytessent"] - start_peer["bytessent"]
                total_recv += end_peer["bytesrecv"] - start_peer["bytesrecv"]
                for msg_type in SHARE_MESSAGE_TYPES:
                    share_sent += end_peer["bytessent_per_msg"].get(msg_type, 0) - start_peer["bytessent_per_msg"].get(msg_type, 0)
                    share_recv += end_peer["bytesrecv_per_msg"].get(msg_type, 0) - start_peer["bytesrecv_per_msg"].get(msg_type, 0)
            share_bytes = share_sent + share_recv
            rows.append({
                "node": node_index,
                "node_peer_count": peer_count,
                "share_relay_bytes_sent": share_sent,
                "share_relay_bytes_received": share_recv,
                "share_relay_bytes_total": share_bytes,
                "share_relay_kb_s": round(share_bytes / 1024.0 / duration, 6),
                "all_node_peer_bytes_sent": total_sent,
                "all_node_peer_bytes_received": total_recv,
                "all_node_peer_kb_s": round((total_sent + total_recv) / 1024.0 / duration, 6),
            })
        return rows

    def git_head(self):
        try:
            return subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
        except (OSError, subprocess.CalledProcessError):
            return "unknown"

    def log_summary(self, result):
        metrics = result["metrics"]
        self.log.info("=== Share Relay Viability Report ===")
        self.log.info("Latency p50: %.3f ms", metrics["latency_to_all_p50_ms"])
        self.log.info("Latency p99: %.3f ms", metrics["latency_to_all_p99_ms"])
        self.log.info("Max bandwidth: %.6f KB/s per node", metrics["max_share_relay_bandwidth_kb_s_per_node"])
        self.log.info("Orphan-order rate: %.6f%%", metrics["orphan_order_rate_percent"])
        self.log.info("Propagation completeness: %.6f%%", metrics["propagation_completeness_percent"])
        self.log.info("Overall: %s", result["decision"])

    def assert_thresholds(self, result):
        metrics = result["metrics"]
        assert metrics["latency_to_all_p50_ms"] is not None
        assert metrics["latency_to_all_p99_ms"] is not None
        assert metrics["latency_to_all_p50_ms"] < LATENCY_P50_THRESHOLD_SECONDS * 1000.0
        assert metrics["latency_to_all_p99_ms"] < LATENCY_P99_THRESHOLD_SECONDS * 1000.0
        assert metrics["max_share_relay_bandwidth_kb_s_per_node"] < BANDWIDTH_THRESHOLD_KB_PER_SECOND
        assert metrics["orphan_order_rate_percent"] < ORPHAN_RATE_THRESHOLD_PERCENT
        assert_equal(metrics["propagation_completeness_percent"], COMPLETENESS_THRESHOLD_PERCENT)
        assert_equal(result["decision"], "GO")


if __name__ == "__main__":
    SharepoolRelayBenchmark(__file__).main()
