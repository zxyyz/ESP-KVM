# Memory and Bandwidth Budget

Exact numbers depend on the board PSRAM and video format. This file defines design ceilings to prevent PC-style assumptions from entering the firmware.

## Memory classes

### Internal RAM

Reserve for:

- FreeRTOS/task stacks that need low latency;
- lwIP/TLS critical allocations;
- USB descriptors/queues;
- ISR/DMA descriptors where required;
- small control events.

### PSRAM

Prefer for:

- video frame/encoded rings where DMA/cache rules permit;
- web assets;
- large bounded JSON/SSE scratch areas;
- optional screen analysis buffers.

Do not rely on PSRAM for secrets merely because it is large.

## Initial control-plane limits

| Item | Limit |
|---|---:|
| generic AI tool payload | 16 KiB |
| extracted screen text | 8 KiB |
| single SSE/event body | 32 KiB hard ceiling |
| accumulated assistant text | 64 KiB per turn |
| device-code URL/code | fixed structs |
| access token buffer | 4 KiB |
| refresh token buffer | 4 KiB |
| HID queue | 32 fixed events |
| event bus messages | fixed-size, no frame payload |

These are ceilings, not allocation targets. Streaming parsers should use smaller chunks.

## Video buffering

Avoid multiple raw 1080p RGB frame copies. Prefer the bridge/CSI format and hardware encoder path that minimizes color-conversion copies.

Encoded streaming should use a small frame ring and drop old frames under network backpressure. KVM latency matters more than perfect frame delivery.

## Bandwidth priorities

1. HID/control latency;
2. VPN keepalive/control;
3. web control messages;
4. video stream;
5. AI traffic;
6. OTA/background transfer.

Video bitrate should adapt or drop frames before control traffic starves.

## AI bandwidth

Do not continuously upload video to the model. Prefer:

- state JSON;
- extracted screen text;
- manually/on-demand captured image later.

A model turn must not share the same large frame buffer ownership path as live streaming.

## Measurement plan

For each milestone record:

- free internal RAM after boot;
- largest free internal block;
- free PSRAM;
- per-task stack high-water marks;
- TLS peak during Codex login/turn;
- video ring occupancy;
- end-to-end video bitrate;
- HID latency p50/p99;
- P4<->C5 throughput while video + VPN + AI are active.
