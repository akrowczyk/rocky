#!/usr/bin/env python3
"""Pretend to be the StackChan body."""
from __future__ import annotations
import argparse, asyncio, json, sys
from pathlib import Path
from typing import Any
sys.path.insert(0, str(Path(__file__).resolve().parent))

async def run(url: str, send_person: bool, range_mm: int) -> int:
    try:
        import websockets
    except ImportError:
        print("pip install -r brain/requirements.txt")
        return 2
    print(f"[sim] connecting {url}")
    try:
        async with websockets.connect(url, open_timeout=5) as ws:
            await ws.send(json.dumps({"event": "boot"}))
            await _drain(ws, 2.5)
            if send_person:
                await ws.send(json.dumps({"event": "person", "range_mm": range_mm}))
                await _drain(ws, 4.0)
            print("[sim] done")
            return 0
    except Exception as exc:
        print(f"[sim] failed: {exc}")
        return 1

async def _drain(ws: Any, seconds: float) -> None:
    end = asyncio.get_event_loop().time() + seconds
    while True:
        remaining = end - asyncio.get_event_loop().time()
        if remaining <= 0:
            return
        try:
            msg = await asyncio.wait_for(ws.recv(), timeout=remaining)
        except asyncio.TimeoutError:
            return
        print(f"  <- {msg}")

def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--url", default="ws://127.0.0.1:8080/ws")
    p.add_argument("--no-person", action="store_true")
    p.add_argument("--range-mm", type=int, default=420)
    args = p.parse_args()
    raise SystemExit(asyncio.run(run(args.url, not args.no_person, args.range_mm)))

if __name__ == "__main__":
    main()
