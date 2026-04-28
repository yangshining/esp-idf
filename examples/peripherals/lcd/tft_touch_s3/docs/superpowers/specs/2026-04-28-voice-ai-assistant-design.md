# ESP32-S3 Voice AI Assistant Design

## Context

The current `tft_touch_s3` example is an ESP32-S3 Edge AI Lab demo with an ST7789 SPI TFT display, XPT2046 touch input, LVGL 9.3 UI, BLE WiFi provisioning, and a simulated AI result page. The AI page currently shows a random classification result and confidence bar.

The desired direction is to turn the AI page into a real voice AI assistant. The user wants to wake the device, ask a spoken question, have a configured large model answer, hear the result spoken back, and see a dynamic cartoon digital avatar rather than a text-heavy chat page.

## Goals

- Replace the simulated AI result UI with a voice assistant page centered on a dynamic digital avatar.
- Keep OpenAI or other model provider API keys off the ESP32-S3 firmware.
- Use a backend proxy as the model gateway and orchestration layer.
- Support a practical MVP using button or touch wake first.
- Preserve an upgrade path for keyword wake and lower-latency realtime voice later.
- Keep LVGL calls on the LVGL task side, following the existing example's threading model.
- Keep changes scoped to `examples/peripherals/lcd/tft_touch_s3`.

## Non-Goals

- Do not put the OpenAI API key directly into firmware, NVS, serial logs, or Kconfig defaults.
- Do not implement always-on realtime streaming in the MVP.
- Do not require a full keyboard or text chat UI on the 240x320 display.
- Do not call LVGL directly from WiFi, HTTP, audio, or backend response tasks.
- Do not change ESP-IDF framework-level components unless a later implementation task explicitly requires it.

## Recommended Product Shape

The first version should be a turn-based voice assistant:

1. The user taps the AI page or presses a wake button.
2. The avatar enters `Listening`.
3. The ESP32-S3 records a short voice clip from an I2S microphone.
4. The device uploads the clip to the backend proxy over HTTPS.
5. The backend performs speech-to-text, model reasoning, and text-to-speech.
6. The ESP32-S3 downloads or streams back the response audio.
7. The device plays the response through an I2S speaker path while the avatar animates in `Speaking`.

This is less natural than full realtime speech-to-speech, but it is much easier to debug on ESP32-S3 and fits the existing demo structure. The firmware interfaces should be shaped so a later backend realtime bridge can replace the turn-based backend without rewriting the UI state model.

## Hardware Recommendation

The MVP should assume add-on audio modules:

- I2S MEMS microphone module, such as INMP441 or a similar ESP32-S3-compatible module.
- I2S amplifier/speaker module, such as MAX98357A or a similar small speaker driver.
- Optional physical wake button. If no button is installed, the AI page can expose a touch wake button.

The exact GPIOs should be configurable in `main/Kconfig.projbuild` to avoid conflicts with the existing LCD and touch SPI wiring.

## Firmware Architecture

Add an `assistant` area under `main/` with small, explicit modules:

- `main/assistant/assistant_state.c/.h`
  - Stores assistant status behind a FreeRTOS-safe boundary.
  - Exposes snapshots for LVGL rendering.
  - Tracks state, last error, short caption, request trace ID, and optional audio progress.

- `main/assistant/assistant_audio.c/.h`
  - Owns I2S microphone capture and I2S speaker playback.
  - Provides blocking or task-friendly functions for recording a bounded clip and playing backend audio.
  - Keeps audio buffers bounded and configurable.

- `main/assistant/assistant_client.c/.h`
  - Owns HTTPS communication with the backend proxy.
  - Sends device token authentication to the backend.
  - Does not know any OpenAI API key.
  - Returns backend metadata such as caption, trace ID, and response audio location or stream handle.

- `main/assistant/assistant_service.c/.h`
  - Coordinates wake, record, upload, wait, download, playback, and state transitions.
  - Runs outside the LVGL task.
  - Updates `assistant_state` rather than touching UI objects.

- `main/ui/ui_page_ai.c`
  - Becomes the digital avatar page.
  - Uses an LVGL timer to poll `assistant_state`.
  - Renders states such as idle, listening, uploading, thinking, speaking, muted, and error.
  - Shows only short captions/status text, not long chat transcripts.

- `main/ui/ui_config.h`
  - Adds UI constants for avatar size, animation period, status label width, wake button dimensions, and related layout values.

## Assistant State Machine

The MVP state machine should include:

- `Idle`: avatar is waiting; touch or button can start a turn.
- `Listening`: microphone capture is active.
- `Uploading`: recorded audio is being sent to the backend.
- `Thinking`: backend is transcribing, calling the model, or synthesizing speech.
- `Speaking`: response audio is playing.
- `Muted`: microphone or speaker has been disabled by the user.
- `Error`: a recoverable error occurred; the UI shows a short reason and then can return to `Idle`.

Expected transition:

```text
Idle -> Listening -> Uploading -> Thinking -> Speaking -> Idle
```

Error transition:

```text
Listening/Uploading/Thinking/Speaking -> Error -> Idle
```

The UI can animate the avatar based on the state:

- Idle: subtle blinking or breathing.
- Listening: microphone pulse or listening ring.
- Uploading/Thinking: compact loading animation.
- Speaking: mouth or waveform animation synchronized loosely to playback.
- Error: soft error expression plus short text.

## Backend API

Use a backend proxy instead of calling OpenAI directly from the ESP32-S3.

Recommended MVP endpoint:

```text
POST /v1/assistant/turn
```

Recommended request metadata:

- `device_id`
- `session_id`
- `sample_rate`
- `audio_format`
- `language`
- optional device/network status summary

Authentication:

```text
Authorization: Bearer <device-token>
```

The backend should:

1. Validate the device token.
2. Accept the uploaded audio clip.
3. Run speech-to-text.
4. Call the configured LLM with the assistant prompt and optional tools.
5. Generate speech from the text response.
6. Return response metadata and audio in a device-friendly way.

Recommended response shape for the MVP:

```json
{
  "trace_id": "turn_...",
  "caption": "Short text summary for the screen",
  "audio_url": "https://backend.example.com/v1/assistant/audio/...",
  "audio_format": "wav",
  "expires_in": 60
}
```

Avoid returning large base64 audio in JSON unless the response is guaranteed to be small. A URL or controlled chunked download is friendlier to ESP32-S3 memory limits.

## Backend Model Strategy

The backend should keep provider details configurable. For an OpenAI-backed implementation, there are two viable approaches:

- MVP: chain speech-to-text, LLM, and text-to-speech. This is reliable and easy to inspect in logs.
- Later: bridge device audio to a realtime speech-to-speech backend session for lower latency and more natural conversation.

The backend should own:

- OpenAI API key.
- Model names.
- TTS voice.
- System prompt.
- Safety and rate-limit policy.
- Per-device authorization.
- Logs and trace IDs.

## Kconfig Additions

Add project configuration options for:

- `CONFIG_EXAMPLE_ASSISTANT_BACKEND_URL`
- `CONFIG_EXAMPLE_ASSISTANT_DEVICE_TOKEN`
- `CONFIG_EXAMPLE_ASSISTANT_RECORD_MS`
- `CONFIG_EXAMPLE_ASSISTANT_SAMPLE_RATE`
- `CONFIG_EXAMPLE_PIN_NUM_I2S_MIC_BCLK`
- `CONFIG_EXAMPLE_PIN_NUM_I2S_MIC_WS`
- `CONFIG_EXAMPLE_PIN_NUM_I2S_MIC_DIN`
- `CONFIG_EXAMPLE_PIN_NUM_I2S_SPK_BCLK`
- `CONFIG_EXAMPLE_PIN_NUM_I2S_SPK_WS`
- `CONFIG_EXAMPLE_PIN_NUM_I2S_SPK_DOUT`
- `CONFIG_EXAMPLE_ASSISTANT_WAKE_BUTTON_GPIO`

If token handling becomes more sensitive, prefer provisioning or NVS entry flows over committing real tokens in defaults.

## Error Handling

Device UI errors should be short and actionable:

- `WiFi offline`
- `Mic failed`
- `No speech heard`
- `Server busy`
- `Request timeout`
- `Playback failed`

Serial logs should include detailed ESP-IDF error codes and backend `trace_id` values. The device should never remain indefinitely in `Listening`, `Thinking`, or `Speaking`; each step needs a timeout and a transition back to `Idle` or `Error`.

## Threading Rules

The existing LVGL rule remains:

- Audio, HTTP, WiFi, and backend tasks must not call LVGL directly.
- Those tasks update `assistant_state`.
- `ui_page_ai.c` reads `assistant_state` from an LVGL timer and updates widgets while running under the LVGL task model.
- If any external task must call an existing UI function, it must acquire `lvgl_api_lock` from `lcd_touch.h`.

## Implementation Phases

### Phase 1: Avatar UI Prototype

- Replace the classification page with a digital avatar page.
- Add a local simulated state cycle for `Listening`, `Thinking`, `Speaking`, and `Error`.
- No real audio or backend dependency yet.

### Phase 2: Backend Mock Loop

- Add `assistant_state`, `assistant_client`, and `assistant_service`.
- Call a mock backend endpoint from the wake action.
- Return fixed metadata/audio and exercise download/playback path if audio hardware is ready.

### Phase 3: Real Audio Turn

- Add I2S microphone capture and speaker playback.
- Upload short audio clips to the backend.
- Backend performs speech-to-text, LLM response, and text-to-speech.
- Device plays returned audio and updates avatar state.

### Phase 4: Keyword Wake

- Add local keyword wake if memory and CPU budget allow.
- Keep touch/button wake as a fallback.
- Evaluate privacy and power tradeoffs before considering backend wake detection.

### Phase 5: Realtime Upgrade

- Add a backend realtime bridge only if the UX needs interruption, lower latency, or continuous conversation.
- Preserve the same high-level `assistant_state` interface so the AI page does not need a major rewrite.

## Documentation Updates

When implementation begins, keep these files aligned:

- `README.md`: hardware modules, wiring, backend setup, usage, troubleshooting.
- `AGENTS.md`: assistant module responsibilities and workflow rules.
- `CLAUDE.md`: same workflow updates if present.
- `main/Kconfig.projbuild`: audio/backend options and help text.
- `main/idf_component.yml`: managed dependencies, if new components are added.

## Open Questions

- Exact microphone and speaker modules.
- Final I2S GPIO assignments for the target board.
- Whether audio response should be WAV, MP3, or another format the ESP32 path can decode reliably.
- Whether the backend is Node.js, Python, or another stack.
- Whether device tokens are configured through Kconfig for demos or provisioned into NVS for safer field use.
