"""Optional OpenAI-compatible chat. Deterministic Rocky if no key."""
from __future__ import annotations
import json, os
from typing import Any
from personality import SYSTEM_PROMPT, fallback_for_event, parse_model_output

def _base_url() -> str:
    return os.environ.get("OPENAI_COMPATIBLE_BASE_URL", "").rstrip("/")

def _api_key() -> str:
    return os.environ.get("OPENAI_API_KEY") or os.environ.get("OPENAI_COMPATIBLE_API_KEY") or ""

def _model() -> str:
    return os.environ.get("OPENAI_COMPATIBLE_MODEL") or os.environ.get("OPENAI_MODEL") or "gpt-4o-mini"

def llm_configured() -> bool:
    return bool(_base_url() and _api_key())

def build_user_message(event: str, payload: dict[str, Any]) -> str:
    if event == "boot":
        return "Event: boot. Body just powered on. Greet. Short. You are in the desk robot."
    if event == "person":
        mm = payload.get("range_mm", "?")
        return f"Event: person. A leaky space blob is close. range_mm={mm}. Be happy. Fist bump."
    text = payload.get("text") or payload.get("say") or ""
    return f"Event: {event}. Blob says: {text!r}" if text else f"Event: {event}. React."

def complete(event: str, payload: dict[str, Any]) -> list[dict[str, Any]]:
    if not llm_configured():
        return fallback_for_event(event)
    url = _base_url() + "/chat/completions"
    body = {
        "model": _model(),
        "temperature": 0.8,
        "messages": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": build_user_message(event, payload)},
        ],
    }
    try:
        import urllib.request
        req = urllib.request.Request(
            url,
            data=json.dumps(body).encode("utf-8"),
            headers={"Content-Type": "application/json", "Authorization": f"Bearer {_api_key()}"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=20) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        return parse_model_output(data["choices"][0]["message"]["content"], event)
    except Exception as exc:
        print(f"[llm] fallback after error: {exc}")
        return fallback_for_event(event)
