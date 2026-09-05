# Codex / ChatGPT Integration Plan

## Product requirement

The desired user experience is similar to Codex CLI login: authenticate with the user's own ChatGPT/Codex account and use the entitlement associated with that account, rather than requiring an API key or a third-party GPT proxy.

## Upstream source of truth

Track `openai/codex`, specifically the device-code login and account/protocol layers. The upstream project currently contains an OAuth device-code flow and exposes device-code login in its CLI/app-server/SDK surfaces.

The firmware should treat this integration as **upstream-coupled**, not as a permanently stable generic API.

## What is ported

Only product-required behavior:

- device-code challenge request;
- verification URL + user code presentation;
- polling state machine;
- access/refresh token lifecycle;
- account/session status;
- streaming model turn transport;
- bounded tool-call loop.

Do not port:

- shell/PTY execution;
- Git;
- SQLite thread store;
- desktop sandboxing;
- MCP servers;
- generic filesystem agent behavior;
- TUI.

## Authentication architecture

`akvm_auth` owns state and secrets. A Codex-specific transport implements:

```text
request_device_code()
  -> verification_url + user_code + device_code + expiry
poll_device_code()
  -> pending | tokens | error
refresh_tokens()
  -> replacement token set
```

The public KVM UI only receives the verification URL/code and coarse status. It must never receive refresh tokens.

## Token persistence

The current scaffold keeps tokens in RAM only. Persistent implementation must:

- use an encrypted secret store;
- erase/replace old tokens atomically;
- never log token bodies;
- keep refresh tokens out of crash dumps;
- support explicit sign-out/wipe;
- invalidate AI readiness immediately on auth failure.

## Protocol compatibility strategy

Do not scatter endpoint URLs and headers throughout the firmware.

Create one component, planned as `akvm_codex_transport`, containing:

- issuer/account constants derived from a pinned upstream Codex revision;
- HTTP request construction;
- SSE/event parser;
- token refresh behavior;
- model/tool serialization;
- protocol version marker.

Record the upstream Codex commit SHA used for each firmware release. Add fixture tests generated from non-secret upstream protocol examples.

If the upstream protocol changes unexpectedly, fail closed and show `Codex transport incompatible` rather than attempting guessed requests.

## Agent loop

The on-device agent is product-specific:

```text
user instruction
  -> model turn
  -> text and/or tool call
  -> local policy authorization
  -> execute typed KVM tool
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
- cancellation on VPN/session loss;
- no recursive/unbounded subagents.

## Initial tools

Phase 1:

- `get_status`
- `get_screen_text`

Phase 2:

- `send_key`
- `send_hotkey`
- `mouse_move`
- `mouse_click`
- `release_input`

Phase 3, separately gated:

- `power_press`
- `reset_press`
- `mount_virtual_media`

## Screen understanding

Prefer structured/text screen observations for BIOS, bootloader and terminal-style screens. Images/screenshots should be a separate bounded capability because they consume substantially more bandwidth and memory.

Never send the video stream continuously to the model.

## Compatibility risk

This is the highest-maintenance subsystem in the project. Device-code sign-in is a good headless UX, but using ChatGPT/Codex account-backed behavior requires following the supported behavior of the upstream Codex client over time. Treat protocol update work as part of releases, not as a one-time port.
