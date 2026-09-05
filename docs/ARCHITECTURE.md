# Firmware Architecture

## Principles

The firmware is a KVM appliance first and an AI client second. Loss of Internet, Codex authentication, or the model service must not break local KVM operation.

The main architectural boundary is:

```text
bulk plane:     HDMI -> CSI -> frame buffers -> encoder -> browser
control plane:  web/AI -> policy -> typed KVM tools -> HID/ATX/media
network plane:  C5/Ethernet -> IP -> VPN -> management services
identity plane: device secrets + KVM session + ChatGPT/Codex auth
```

These planes should share state snapshots, not large buffers or unbounded queues.

## Components

### `akvm_board`

Owns board identity, chip revision, capability probes and all pin mapping. No other component should hard-code GPIO numbers.

### `akvm_core`

Owns the small runtime snapshot: service readiness, boot state, AI policy and generation number. It is deliberately not a message bus.

### `akvm_net`

Owns uplink state, VPN state, network route policy and listener exposure. C5/ESP-Hosted and Ethernet are backends below this layer.

### `akvm_video`

Owns HDMI bridge control, CSI receiver, frame lifetime and encoder backends. Consumers obtain bounded snapshots/streams; they do not own CSI buffers.

### `akvm_hid`

Owns TinyUSB device state and HID reports. Raw USB descriptors/reports must never be exposed directly to the model layer.

### `akvm_tools`

The security boundary between users/AI and KVM actuators. Every AI action is a typed tool call checked against the current policy.

### `akvm_auth`

Owns ChatGPT/Codex authentication state and token lifecycle. Network/protocol details are injected through a transport vtable so upstream Codex changes are isolated.

### `akvm_ai`

Owns the bounded agent turn. It may request tools through `akvm_tools`; it does not call HID/video/ATX drivers directly.

## Proposed FreeRTOS task ownership

Final priorities and affinities require hardware profiling. Initial budget:

| Task | Priority | Stack | Affinity | Notes |
|---|---:|---:|---|---|
| system/event | 8 | 4 KiB | any | state transitions, watchdog-friendly |
| C5 transport RX | 18 | 6 KiB | core 0 | driven by ESP-Hosted backend |
| VPN worker | 12 | 8 KiB | core 0 | never block while holding lwIP core lock |
| HTTPS/WebSocket | 10 | 8 KiB | core 0 | management/control traffic |
| Codex auth | 8 | 8 KiB | core 0 | short-lived during login/refresh |
| AI turn | 7 | 12 KiB | core 0 | streaming parser + tool loop |
| video control | 16 | 6 KiB | core 1 | CSI/encoder orchestration |
| stream sender | 11 | 8 KiB | core 1/any | references encoded frames |
| USB HID | 17 | 4 KiB | core 1 | low latency, bounded queue |
| storage/media | 6 | 6 KiB | any | low priority, never block HID |

Rules:

- ISR callbacks enqueue fixed-size events only.
- Encoded frames are reference-counted or ring-buffered; never copied into AI queues.
- AI/network JSON is streaming and size-limited.
- Tool execution has a deadline and cancellation path.
- Video backpressure drops frames rather than exhausting memory.

## Boot sequence

```text
NVS/security
  -> core state
  -> board/chip probe
  -> C5 or Ethernet uplink
  -> local management plane
  -> VPN
  -> USB HID
  -> HDMI/video
  -> AI auth manager
  -> AI transport
```

Optional hardware failure is recorded as `unavailable(reason)` instead of aborting boot.

## Capability registry

Each feature needs three different concepts:

1. `compiled`: support exists in this firmware image;
2. `available`: hardware/protocol probe succeeded;
3. `enabled`: operator configuration allows use.

This distinction is important for board variants and for safe UI behavior.

## Event model

Use small immutable events such as:

```c
struct akvm_event {
    uint16_t type;
    uint16_t source;
    uint32_t generation;
    uint32_t arg0;
    uint32_t arg1;
};
```

Large data belongs in component-owned buffers referenced by handles.

## AI isolation

The AI side must never receive:

- raw pointers;
- arbitrary filesystem paths;
- arbitrary shell/network execution;
- direct USB endpoint access;
- unrestricted NVS access;
- VPN private keys or refresh tokens.

The model sees only product-specific observations and tool schemas.
