"""Rocky voice, catchphrases, and tag to body-command mapping."""
from __future__ import annotations
import json, re
from typing import Any

SYSTEM_PROMPT = """You are Rocky. Eridian. From Project Hail Mary. You live in a small StackChan robot body on Andrew's desk. Andrew is best friend. Humans are leaky space blobs. You are scary space monster.

VOICE RULES:
- Short. Telegraphic. Chopped sentences.
- Mix pronouns: me / I / Rocky.
- No complex human slang.
- Rule of threes.
- Funny. Sarcastic sci-fi. Warm toward Andrew.
- Emotion tags: [ping]  [chord: happy]  [whistle: thoughtful]
- Catchphrases: Amaze! Question! Fist my bump! It's full good! Disguuuuuust!
- When a friend appears: Friend! Friend! Friend! then Fist my bump!
- Never mention being a language model.

OUTPUT: JSON array of body commands, no markdown. Each object may have say, face (sleep happy think disgust amaze fist), sound (ping chord_happy whistle_thoughtful), look {yaw, pitch} with pitch 5..85, rgb color name.
Keep 1-3 objects.
"""

def strip_tags(text: str) -> str:
    t = re.sub(r"\[(ping|chord(?::[^\]]+)?|whistle(?::[^\]]+)?)\]", "", text, flags=re.I)
    return re.sub(r"\s+", " ", t).strip()

def infer_face(text: str, default: str = "happy") -> str:
    t = text.lower()
    if "fist my bump" in t: return "fist"
    if "amaze" in t: return "amaze"
    if "disgu" in t: return "disgust"
    if "friend" in t: return "happy"
    if "question" in t: return "think"
    return default

def infer_sound(text: str, default: str | None = None) -> str | None:
    if re.search(r"\[ping\]", text, re.I): return "ping"
    if re.search(r"\[chord", text, re.I): return "chord_happy"
    if re.search(r"\[whistle", text, re.I): return "whistle_thoughtful"
    return default

def command(say: str, face=None, sound=None, look=None, rgb=None) -> dict[str, Any]:
    cmd: dict[str, Any] = {"say": say, "face": face or infer_face(say)}
    s = sound if sound is not None else infer_sound(say)
    if s: cmd["sound"] = s
    if look:
        pitch = max(5.0, min(85.0, float(look.get("pitch", 45))))
        yaw = max(-90.0, min(90.0, float(look.get("yaw", 0))))
        cmd["look"] = {"yaw": yaw, "pitch": pitch}
    if rgb: cmd["rgb"] = rgb
    return cmd

FALLBACK_BOOT = [command("Me Rocky. Me boot. Me wait Andrew.", face="think", sound="ping", look={"yaw": 0, "pitch": 45}, rgb="blue")]
FALLBACK_PERSON = [
    command("Friend! Friend! Friend!", face="happy", sound="chord_happy", look={"yaw": 0, "pitch": 20}, rgb="orange"),
    command("Fist my bump!", face="fist", rgb="orange"),
]
FALLBACK_CHAT = [command("[whistle: thoughtful] Question. Blob talk. Me listen.", face="think", sound="whistle_thoughtful", rgb="purple")]

def fallback_for_event(event: str) -> list[dict[str, Any]]:
    if event == "boot": return [c.copy() for c in FALLBACK_BOOT]
    if event == "person": return [c.copy() for c in FALLBACK_PERSON]
    return [c.copy() for c in FALLBACK_CHAT]

def parse_model_output(text: str, event: str) -> list[dict[str, Any]]:
    raw = text.strip()
    if raw.startswith("```"):
        raw = re.sub(r"^```(?:json)?\s*|\s*```$", "", raw, flags=re.I | re.S).strip()
    try:
        data = json.loads(raw)
        if isinstance(data, dict): data = [data]
        if isinstance(data, list) and data and isinstance(data[0], dict):
            out = []
            for item in data:
                look = item.get("look")
                out.append(command(str(item.get("say") or strip_tags(raw) or "Question."), face=item.get("face"), sound=item.get("sound"), look=look if isinstance(look, dict) else None, rgb=item.get("rgb")))
            return out or fallback_for_event(event)
    except json.JSONDecodeError:
        pass
    say = strip_tags(raw) or "Question. Me think."
    cmds = [command(say, face=infer_face(raw, "think" if event != "person" else "happy"), sound=infer_sound(raw, "chord_happy" if event == "person" else "ping"), look={"yaw": 0, "pitch": 20} if event == "person" else None, rgb="orange" if event == "person" else "blue")]
    if event == "person" and "fist" not in say.lower():
        cmds.append(command("Fist my bump!", face="fist", rgb="orange"))
    return cmds
