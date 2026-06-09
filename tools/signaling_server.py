#!/usr/bin/env python3
"""
Minimal WebSocket signaling server for flutter_datachannel.

Protocol (JSON over WebSocket):
  register      — {type, peer_id, role: client|server|hybrid}
  list_peers    — {type, from} → server responds with {type: peers, servers: [...]}
  connect_request — {type, from, to} → forwarded to target peer
  offer/answer/candidate — SDP relay between peers

Usage:
  python3 tools/signaling_server.py [--host 0.0.0.0] [--port 8765]
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
from typing import Any

try:
    import websockets
    from websockets.server import WebSocketServerProtocol
except ImportError:
    raise SystemExit(
        "Install dependencies: pip install websockets\n"
        "Or: pip install -r tools/requirements.txt"
    )

log = logging.getLogger("signaling")

# peer_id -> websocket
peers: dict[str, WebSocketServerProtocol] = {}
# peer_id -> role
roles: dict[str, str] = {}


async def send_json(ws: WebSocketServerProtocol, payload: dict[str, Any]) -> None:
    await ws.send(json.dumps(payload))


async def broadcast_peers(requester: str | None = None) -> None:
    servers = [pid for pid, role in roles.items() if role in ("server", "hybrid")]
    msg = {"type": "peers", "servers": servers}
    if requester and requester in peers:
        await send_json(peers[requester], msg)
    else:
        for ws in peers.values():
            try:
                await ws.send(json.dumps(msg))
            except Exception:
                pass


async def relay_to(target_id: str, msg: dict[str, Any]) -> bool:
    ws = peers.get(target_id)
    if not ws:
        return False
    await send_json(ws, msg)
    return True


async def handler(ws: WebSocketServerProtocol) -> None:
    peer_id: str | None = None
    try:
        async for raw in ws:
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                await send_json(ws, {"type": "error", "message": "invalid json"})
                continue

            mtype = msg.get("type", "")

            if mtype == "register":
                peer_id = msg.get("peer_id")
                role = msg.get("role", "client")
                if not peer_id:
                    await send_json(ws, {"type": "error", "message": "peer_id required"})
                    continue
                if peer_id in peers and peers[peer_id] != ws:
                    await send_json(ws, {"type": "error", "message": "peer_id taken"})
                    continue
                peers[peer_id] = ws
                roles[peer_id] = role
                log.info("registered %s as %s", peer_id, role)
                await send_json(ws, {"type": "registered", "peer_id": peer_id})
                await broadcast_peers()
                continue

            if not peer_id:
                await send_json(ws, {"type": "error", "message": "register first"})
                continue

            if mtype == "list_peers":
                await broadcast_peers(peer_id)
                continue

            if mtype in ("offer", "answer", "candidate", "connect_request"):
                target = msg.get("to")
                if not target:
                    await send_json(ws, {"type": "error", "message": "to required"})
                    continue
                msg.setdefault("from", peer_id)
                if not await relay_to(target, msg):
                    await send_json(ws, {"type": "error", "message": f"peer {target} offline"})
                continue

            await send_json(ws, {"type": "error", "message": f"unknown type {mtype}"})

    except websockets.ConnectionClosed:
        pass
    finally:
        if peer_id and peers.get(peer_id) is ws:
            del peers[peer_id]
            roles.pop(peer_id, None)
            log.info("disconnected %s", peer_id)
            await broadcast_peers()


async def main(host: str, port: int) -> None:
    log.info("signaling server on ws://%s:%d", host, port)
    async with websockets.serve(handler, host, port):
        await asyncio.Future()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="flutter_datachannel signaling server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
    )

    asyncio.run(main(args.host, args.port))
