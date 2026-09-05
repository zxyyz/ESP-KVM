# P4+C5 AI KVM

> Working name: `p4-c5-ai-kvm`. This repository is being initialized in the existing empty GitHub repository `zxyyz/-`; rename the repository when convenient.

An ESP32-P4 + ESP32-C5 firmware project for a self-contained IP-KVM with HDMI capture, USB keyboard/mouse emulation, VPN-only remote access, and a lightweight Codex-compatible agent that authenticates with a user's ChatGPT/Codex account instead of a third-party GPT API.

## Product goal

One appliance should provide:

- HDMI input through an external HDMI-to-MIPI-CSI bridge (`TC358743` first target; `LT6911` as a planned alternative).
- USB Device HID output to the controlled computer: keyboard, relative/absolute mouse, optional consumer-control keys.
- Optional read-only USB mass-storage virtual media.
- An additional 256 MB bulk-data flash for virtual media, update staging and bounded diagnostics; it is not treated as the P4 boot/security flash.
- H.264/MJPEG video streaming to a browser.
- ESP32-C5 as Wi-Fi coprocessor via ESP-Hosted (prefer SDIO; SPI fallback).
- WireGuard as the primary remote-access VPN. Tailscale-compatible networking is an optional later backend.
- No public exposure of the KVM web service by default.
- A small on-device AI agent that can observe KVM state and invoke explicit KVM tools.
- ChatGPT/Codex device-code sign-in and token lifecycle modeled after the upstream open-source Codex client, rather than an API-key / third-party API flow.

## Important architecture decision

This is **not** an attempt to port the complete Codex CLI to ESP32-P4. The firmware only carries the minimal pieces needed for this product:

1. device-code authentication and token lifecycle;
2. streaming model transport;
3. a constrained agent loop;
4. explicit KVM tools;
5. local policy and approval gates.

Desktop-only Codex features such as shell execution, PTY, Git, SQLite, sandbox processes, MCP servers, and general-purpose filesystem agents are intentionally out of scope.

## Repository status

The repository currently contains an architecture-first ESP-IDF skeleton. Hardware-specific drivers are deliberately behind interfaces until the exact P4+C5 board pinout, P4 silicon revision, HDMI bridge, PSRAM/main-flash size, 256 MB bulk-flash part/interface, and USB wiring are confirmed.

Initial implementation priorities:

1. bootable ESP-IDF component graph and state model;
2. board abstraction and capability registry;
3. network state machine for C5/ESP-Hosted + VPN;
4. HID command safety layer;
5. video pipeline interface;
6. secure/main storage vs bulk-storage separation;
7. Codex authentication/agent interfaces;
8. integration of proven upstream components only after dependency/licensing review.

## High-level data flow

```text
                         Internet
                            |
                      ESP32-C5 Wi-Fi
                            |
                      ESP-Hosted SDIO
                            |
+---------------------------v----------------------------+
|                       ESP32-P4                         |
|                                                        |
|  HDMI -> HDMI/CSI -> Video -> H264/MJPEG -> Web/VPN   |
|                                                        |
|  ChatGPT/Codex auth -> Agent -> Policy -> KVM tools    |
|                                      |                 |
|                               +------+-------+         |
|                               |              |         |
|                              HID            ATX        |
|                                                        |
|  Main boot/security flash      256 MB bulk-data flash  |
+-------------------------------+--------------+---------+
                                |
                              USB
                                |
                           Target computer
```

## AI permission model

The AI layer is not allowed to generate arbitrary USB reports directly. It must call typed tools checked by a local policy layer.

Planned modes:

- `observe`: screen/status access only;
- `assist`: navigation/input commands require local or web-session approval;
- `autonomous`: approved bounded workflows may send HID without per-action confirmation;
- destructive actions such as power/reset/virtual-media changes remain separately gated.

## Build target

Target: ESP32-P4 with ESP-IDF 6.x. The exact pinned IDF version will be selected after validating the target board and the HDMI bridge driver. The current source skeleton avoids version-sensitive peripheral calls so architecture can settle before hardware binding.

```bash
idf.py set-target esp32p4
idf.py build
```

## Documentation

- `docs/ARCHITECTURE.md` - firmware architecture and FreeRTOS task ownership
- `docs/HARDWARE.md` - required hardware and board-porting checklist
- `docs/STORAGE.md` - main secure flash vs 256 MB bulk-flash architecture
- `docs/NETWORKING.md` - C5, ESP-Hosted, VPN and service exposure
- `docs/CODEX.md` - ChatGPT/Codex authentication and agent strategy
- `docs/SECURITY.md` - threat model and security requirements
- `docs/ROADMAP.md` - milestones and acceptance criteria
- `docs/PROTOCOL.md` - internal KVM/AI command contracts
- `docs/MEMORY_BUDGET.md` - memory, buffering and bandwidth plan
- `docs/UPSTREAM.md` - pinned upstream revisions and integration notes

## Upstream projects to evaluate

- `espkvm/espkvm`: ESP32-P4 KVM implementation and useful P4-specific lessons.
- `espressif/esp-hosted-mcu`: P4 host / C5 Wi-Fi coprocessor transport.
- `openai/codex`: source of truth for device-code login, token handling and Codex client protocol behavior.

No upstream source has been blindly copied into this initial scaffold. Any later reuse must preserve its license and notices.
