# Internal Control / AI Tool Protocol

This document describes product-level tool contracts. It is intentionally independent of any model provider wire format.

## Common envelope

Conceptual JSON representation:

```json
{
  "id": "call-123",
  "tool": "send_key",
  "args": {},
  "scope": "workflow-abc"
}
```

The firmware converts the provider-specific tool call into an internal typed request before authorization.

## Read-only tools

### `get_status`

Returns bounded product state only:

```json
{
  "video": {"present": true, "width": 1920, "height": 1080},
  "hid": {"ready": true},
  "network": {"uplink": true, "vpn": true},
  "policy": "assist"
}
```

No keys/tokens/credentials.

### `get_screen_text`

Returns text extracted from the current screen with a configured byte limit. Include generation/timestamp later to identify stale observations.

## HID tools

### `send_key`

Arguments:

- USB HID usage code;
- modifier mask;
- press duration (bounded).

Implementation should send press then release unless an explicit low-level diagnostic mode is active.

### `send_hotkey`

Higher-level convenience tool converted locally to one or more keyboard reports.

### `mouse_move`

Use signed bounded deltas. Rate limit repeated movement.

### `mouse_click`

Button + press/release semantics. Never permit an indefinitely held button without a timeout.

### `release_input`

Always safe to call and should release keyboard modifiers, keys and mouse buttons.

## Destructive tools

### `power_press`

Arguments include bounded press duration. Separate approval required.

### `reset_press`

Separate approval required.

### `mount_virtual_media`

Only files already present/approved in the local media store may be mounted. The AI cannot provide an arbitrary URL for the firmware to fetch and mount.

## Tool execution limits

Planned defaults:

- 32 tool calls / AI turn;
- 30 s default turn deadline, extendable for explicit workflows;
- 16 KiB maximum generic tool result;
- 8 KiB maximum screen text;
- HID event queue length 32;
- destructive approval single-use unless explicitly scoped.

## Idempotency

Read tools are idempotent. HID and power tools are not. Provider retries must therefore carry a call ID and the firmware should keep a short replay cache to avoid duplicate key/power actions after reconnect.
