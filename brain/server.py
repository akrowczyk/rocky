#!/usr/bin/env python3
"""Desk Rocky brain FastAPI WebSocket server."""
from __future__ import annotations
import asyncio, json, os, sys
from pathlib import Path
from typing import Any
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse, PlainTextResponse
from llm import complete, llm_configured
from personality import fallback_for_event

app = FastAPI(title="Desk Rocky Brain", version="0.1.0")
_clients: set[WebSocket] = set()

@app.get("/", response_class=PlainTextResponse)
def root() -> str:
    mode = "llm" if llm_configured() else "fallback"
    return f"Desk Rocky brain ok ({mode}). ws=/ws\n"

@app.get("/health")
def health() -> JSONResponse:
    return JSONResponse({"ok": True, "llm": llm_configured(), "clients": len(_clients)})

async def send_commands(ws: WebSocket, cmds: list[dict[str, Any]]) -> None:
    for i, cmd in enumerate(cmds):
        await ws.send_text(json.dumps(cmd))
        print(f"  -> {json.dumps(cmd, ensure_ascii=False)}")
        if i + 1 < len(cmds):
            await asyncio.sleep(1.6)

@app.websocket("/ws")
async def robot_socket(ws: WebSocket) -> None:
    await ws.accept()
    _clients.add(ws)
    print("[brain] robot connected")
    try:
        while True:
            raw = await ws.receive_text()
            print(f"  <- {raw}")
            try:
                payload = json.loads(raw)
            except json.JSONDecodeError:
                payload = {"event": "chat", "text": raw}
            event = str(payload.get("event") or "chat")
            cmds = complete(event, payload) or fallback_for_event(event)
            await send_commands(ws, cmds)
    except WebSocketDisconnect:
        print("[brain] robot disconnected")
    finally:
        _clients.discard(ws)

def main() -> None:
    import uvicorn
    port = int(os.environ.get("ROCKY_PORT", "8080"))
    print(f"Desk Rocky brain on 0.0.0.0:{port}  llm={llm_configured()}")
    uvicorn.run(app, host="0.0.0.0", port=port, log_level="info")

if __name__ == "__main__":
    main()
