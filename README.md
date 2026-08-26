# Desk Rocky

M5Stack **StackChan** (CoreS3, kit SKU **K151-R**) talks to an **OpenAI-compatible LLM over HTTPS itself**. No laptop required.

Owner: **Andrew Krowczyk**. License: MIT. Personality: **Rocky** from *Project Hail Mary*.

The Python laptop brain is optional (simulator / local Ollama / WebSocket fallback).

Kit: [M5StackChan AI Desktop Robot Kit](https://shop.m5stack.com/products/m5stackchan-ai-desktop-robot-kit-with-remote-control-esp32-s3)

## Boot priority

1. Wi-Fi from LittleFS `/wifi.json`
2. If `llm_base_url` + `llm_api_key` set: **LLM mode**. Robot POSTs `/chat/completions`. Person at the desk → head snap + LLM greeting. API fail → canned `Friend! Friend! Friend!` + fist. Still no laptop.
3. Else if `brain_host` set: optional WebSocket brain.
4. Else / Wi-Fi down: 12-second offline clip.

## wifi.json

Copy `firmware/data/wifi.json.example` to `firmware/data/wifi.json` (gitignored). Never commit a real key.

| Field | Role |
|---|---|
| `ssid` / `password` | 2.4 GHz Wi-Fi |
| `llm_base_url` | e.g. `https://api.openai.com/v1` |
| `llm_api_key` | Bearer token |
| `llm_model` | default `gpt-4o-mini` |
| `brain_host` | leave `""` for LLM-only |

## TLS (v1)

`WiFiClientSecure::setInsecure()`. Weekend prototype. Host varies (OpenAI, Groq, LAN proxy). Not production cert pinning. `http://` URLs (local Ollama) skip TLS.

## Servo safety

**Pitch 5° to 85° only.** `motionLook()` clamps. Do not rotate the head by hand while powered. UART TX=GPIO6 RX=GPIO7, yaw ID=1, pitch ID=2.

## Flash

```bash
cd firmware
cp data/wifi.json.example data/wifi.json
# edit ssid, password, llm_base_url, llm_api_key
pio run -t uploadfs
pio run -t upload
pio device monitor
```

Power via the **base** USB-C. Hold RST ~3 s for download mode.

Compiled 2026-08-26 PlatformIO espressif32@6.7.0 — SUCCESS (~1.24 MB flash). Compile-only, no hardware attached.

## Optional laptop brain

```bash
cd brain && python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python server.py                 # terminal 1
python simulator.py              # terminal 2
```

## 12-second clip

Wi-Fi off: sleep → ping → head snap → chord → Friend! Friend! Friend! → Fist my bump!

Film desk, eye-level, landscape. Boot, crop those 12 seconds.
