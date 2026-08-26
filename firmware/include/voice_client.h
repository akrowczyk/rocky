#pragma once

#include <Arduino.h>
#include "wifi_config.h"

// Grok Voice Agent (Speech-to-Speech) client for the StackChan body.
// WSS: wss://api.x.ai/v1/realtime?model=grok-voice-latest
// Event names from https://docs.x.ai/developers/rest-api-reference/inference/voice
//
// CoreS3 ES7210 mic + AW88298 speaker share I2S_NUM_1, so this client is
// half-duplex: listen (mic uplink) XOR talk (speaker downlink). Pitch still
// goes through motionLook() which clamps 5..85.

void voiceBegin(const WifiConfig& cfg);
bool voiceWanted();      // wifi.json voice!=false and llm_base_url+llm_api_key set
bool voiceConnected();   // WSS is up
bool voiceReady();       // session.updated received
bool voiceFailed();      // never got a session after the connect timeout
void voiceTick();        // pump WS + I2S; never blocks for seconds

// Documented nudge: conversation.item.create (input_text) + response.create.
// Does not wait for the user to speak. No-op if the socket is down.
void voiceNudge(const char* user_text);
void voiceGreetBoot();
void voiceGreetPerson(int range_mm);

// REST POST /v1/tts PCM fallback (same llm_voice, default rex). Blocking ~few seconds.
// Used only when the realtime socket is down. Returns false on HTTP failure.
bool voiceTtsSpeak(const char* text);
