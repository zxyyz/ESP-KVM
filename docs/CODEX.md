# Codex / ChatGPT Integration

## Product requirement

The intended user experience is similar to Codex CLI login: authenticate with the user's own ChatGPT/Codex account and use the account-backed Codex service, rather than requiring an API key or a third-party GPT proxy.

This integration is **upstream-coupled**. It follows behavior observed in a pinned `openai/codex` revision; it is not treated as a permanently stable generic HTTP API.

## Current implementation status

The ESP32-P4 target now contains a compile-proven minimal account-backed transport split into two components:

- `akvm_codex_auth`: device-code authentication and token refresh;
- `akvm_codex_model`: ChatGPT account/workspace extraction, model discovery and streaming text turns.

Reviewed upstream Codex revision:

```text
588b781ab4924ce7352488394028e63d74cf807f
```

Compatibility client version currently advertised by the model transport:

```text
0.153.4
```

The current code builds successfully under ESP-IDF 6.1 for `esp32p4` in GitHub CI.

### Implemented authentication path

```text
request device code
  -> verification URL + short user code
  -> poll device authorization
  -> receive authorization code + PKCE verifier
  -> OAuth token exchange
  -> id_token + access_token + refresh_token
  -> refresh-token flow on 401 / explicit refresh
```

`akvm_auth` owns the state machine. The Codex-specific HTTP details remain isolated in `akvm_codex_auth` so protocol changes do not leak into the rest of the KVM firmware.

### Implemented account/model path

After authentication, `akvm_codex_model` currently:

1. decodes the ID-token JWT payload locally;
2. obtains the ChatGPT account/workspace ID expected by the Codex client path;
3. calls the account model catalog;
4. selects the highest-priority model marked usable through the API path, unless a Kconfig model override is set;
5. opens a streaming Responses request;
6. parses SSE incrementally and emits `response.output_text.delta` text to the local AI callback;
7. refreshes authentication and retries once on HTTP 401.

Model names are not hard-coded by default. The account model catalog is bounded in memory and dynamically selected.

### Current intentional limitation

The current model request is **text-only**. No KVM tools are exposed to the model yet.

That means the present milestone proves the software shape for:

```text
ChatGPT/Codex login
  -> account identity
  -> account model discovery
  -> streaming text response
```

but it deliberately does not yet allow the model to send keyboard, mouse, power, reset or virtual-media actions.

This separation is intentional: first validate the account-backed Codex path on real hardware, then add tool calling behind the existing local KVM policy boundary.

## What is ported from Codex conceptually

Only product-required behavior:

- device-code challenge request;
- verification URL + user code presentation;
- polling state machine;
- access/refresh token lifecycle;
- ChatGPT account/workspace identity;
- model catalog discovery;
- streaming model turn transport;
- bounded SSE parsing;
- later: bounded product-specific tool-call loop.

Do not port:

- shell/PTY execution;
- Git;
- SQLite thread store;
- desktop sandboxing;
- MCP servers;
- generic filesystem agent behavior;
- TUI.

## Token handling

The current implementation keeps OAuth tokens in RAM only.

Persistent token storage remains blocked until the protected main-flash secret store is implemented. When persistence is enabled it must:

- use encrypted/protected main-flash storage;
- never use the external 256 MB bulk flash for plaintext OAuth tokens;
- erase/replace old token material atomically;
- never log token bodies;
- keep refresh tokens out of crash diagnostics;
- support explicit sign-out/wipe;
- invalidate AI readiness immediately on unrecoverable auth failure.

The public KVM UI may eventually receive the verification URL, short user code and coarse login state. It must never receive access or refresh tokens.

## Protocol compatibility strategy

All Codex-specific routes, headers and compatibility constants stay in the Codex transport components.

For every firmware release that changes this layer:

1. record the reviewed upstream Codex commit SHA;
2. compare current device-login, account-ID and model transport behavior;
3. update protocol fixtures without secrets;
4. run ESP32-P4 compile CI;
5. run a real device-code login and one text turn on hardware;
6. fail closed on unexpected protocol shape instead of guessing.

If the upstream behavior changes unexpectedly, the product should report a compatibility/authentication error while preserving human KVM access.

## Agent loop target

The eventual on-device agent is product-specific:

```text
user instruction
  -> model turn
  -> text and/or tool call
  -> convert provider call to internal typed KVM request
  -> local policy authorization
  -> execute KVM tool
  -> bounded tool result
  -> next model turn
  -> final response
```

Hard limits:

- maximum tool calls per turn;
- maximum turn duration;
- maximum SSE event size;
- maximum accumulated text;
- maximum screen-text payload;
- cancellation on sign-out, session revoke or unrecoverable network loss;
- duplicate-call replay protection for non-idempotent actions;
- no recursive/unbounded subagents.

## Tool rollout

### Phase 1: read-only

- `get_status`
- `get_screen_text`

These are the first tools to add after real account-backed text turns are validated.

### Phase 2: normal KVM input

- `send_key`
- `send_hotkey`
- `mouse_move`
- `mouse_click`
- `release_input`

All mutations pass through `akvm_tools` and the active `observe / assist / autonomous` policy.

### Phase 3: separately gated destructive tools

- `power_press`
- `reset_press`
- `mount_virtual_media`

These retain explicit destructive-action approval even inside an otherwise authorized autonomous workflow.

## Screen understanding

Prefer structured/text screen observations for BIOS, bootloader and terminal-style screens. Images/screenshots should be a separate bounded capability because they consume substantially more bandwidth and memory.

Never continuously upload the KVM video stream to the model.

## Validation still required

Compile success does **not** prove account entitlement or runtime compatibility. Real-hardware acceptance still requires:

- C5/ESP-Hosted network bring-up;
- time/TLS validation;
- successful device-code login with a real ChatGPT/Codex account;
- successful model catalog lookup for that account;
- successful streamed Responses turn using the account-backed path;
- token refresh after expiry/401;
- logout/wipe behavior;
- memory and TLS peak measurements on the actual P4/PSRAM configuration.
