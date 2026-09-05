# Roadmap

## M0 - Architecture scaffold

Status: in progress.

Acceptance:

- ESP-IDF component graph exists;
- board-specific assumptions are isolated;
- runtime state and AI policy code exists;
- documentation captures hardware/network/security constraints.

## M1 - Board bring-up

Inputs required: exact P4+C5 board model/pinout.

Acceptance:

- P4 revision/flash/PSRAM logged;
- C5 reset/transport verified;
- board profile selected from Kconfig;
- watchdog and crash logging stable.

## M2 - C5 uplink

Acceptance:

- ESP-Hosted SDIO preferred backend works;
- DHCP/DNS/NTP stable;
- reconnect after AP loss;
- 30 min bidirectional throughput soak with memory metrics.

## M3 - USB HID KVM output

Acceptance:

- keyboard + relative mouse enumerate on BIOS, Linux and Windows;
- `release_all` on disconnect/reboot;
- 10k report stress without stuck keys;
- AI policy test suite proves observe mode cannot send input.

## M4 - HDMI capture

Acceptance:

- bridge probe/EDID;
- CSI capture stable at 720p then 1080p;
- unplug/replug recovery;
- BIOS/UEFI/OS timing transitions;
- MJPEG debug stream.

## M5 - H.264 + web console

Acceptance:

- bounded frame ring;
- HTTPS UI;
- authenticated WebSocket input;
- measured glass-to-glass and HID latency;
- H.264 capability based on actual P4 revision.

## M6 - WireGuard remote access

Acceptance:

- protected key storage;
- VPN reconnect;
- remote KVM listeners restricted according to policy;
- remote session revoked on VPN loss;
- video soak through VPN.

## M7 - Codex device login

Acceptance:

- device-code challenge visible in KVM UI/serial;
- account login succeeds using upstream-compatible flow;
- refresh works after access expiry;
- sign-out securely wipes local token state;
- protocol constants pinned to a documented upstream Codex revision.

## M8 - Read-only AI assistant

Tools:

- `get_status`;
- `get_screen_text`.

Acceptance:

- bounded SSE parser;
- max turn/tool limits;
- no HID path reachable in observe mode;
- graceful service/auth/network failure.

## M9 - Assisted AI KVM

Tools:

- keyboard/hotkeys;
- mouse;
- release input.

Acceptance:

- explicit approval UX;
- duplicate tool-call replay protection;
- cancellation and emergency human override.

## M10 - Autonomous bounded workflows

Examples: navigate BIOS, select boot entry, perform a known installation sequence.

Acceptance:

- explicit workflow scope + expiry;
- destructive actions separately gated;
- full action audit log without leaking typed secrets;
- immediate revoke/human takeover.

## Later

- virtual media;
- ATX power/reset;
- Tailscale-compatible backend;
- Ethernet uplink;
- optional one-shot image understanding;
- OTA release/update channel.
