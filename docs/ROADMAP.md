# Roadmap

## M0 - Architecture scaffold

Status: **complete for the software scaffold**.

Completed:

- ESP-IDF component graph exists;
- board-specific assumptions are isolated;
- runtime service state and AI policy code exists;
- typed KVM tool authorization exists;
- bounded SSE parser exists;
- secure-vs-bulk storage abstraction exists;
- GitHub CI builds successfully with ESP-IDF 6.1 for `esp32p4`;
- documentation captures hardware, networking, Codex, storage and security constraints.

Hardware-dependent components intentionally remain unbound.

## M1 - Board bring-up

Status: **blocked on exact board information**.

Inputs required:

- exact P4+C5 board model/revision and pinout;
- P4 silicon revision;
- PSRAM and main-flash configuration;
- P4<->C5 transport pins;
- USB-OTG wiring;
- HDMI bridge + CSI wiring.

Acceptance:

- P4 revision/flash/PSRAM logged;
- C5 reset/transport verified;
- board profile selected from Kconfig;
- watchdog and crash logging stable.

### M1b - 256 MB secondary bulk flash

Status: **architecture complete, driver blocked on exact flash part/interface**.

The extra 256 MB flash is a secondary bulk store, not the boot/security flash.

Acceptance:

- exact NOR/NAND part and geometry detected;
- backend bound below `akvm_storage`;
- power-cut-safe metadata/index;
- full-device development test;
- erase/program stress does not break HID/video latency budgets;
- protected secrets do not appear in bulk-flash dumps.

### M1c - Main boot/security flash layout

Status: **planned; blocked on final main-flash capacity/security profile**.

Acceptance:

- custom main-flash partition table;
- adequate bootloader headroom with Secure Boot + Flash Encryption;
- A/B OTA slots with firmware growth margin;
- protected NVS/secret store;
- signed OTA and rollback tested.

## M2 - C5 uplink

Status: **not bound yet**.

Acceptance:

- ESP-Hosted SDIO preferred backend works;
- DHCP/DNS/NTP stable;
- reconnect after AP loss;
- C5-only reset/recovery path;
- 30 min bidirectional throughput soak with memory metrics.

## M3 - USB HID KVM output

Status: **typed HID and policy interfaces exist; TinyUSB hardware backend pending**.

Acceptance:

- keyboard + relative mouse enumerate on BIOS, Linux and Windows;
- `release_all` on disconnect/reboot;
- 10k report stress without stuck keys;
- AI policy test suite proves observe mode cannot send input.

## M4 - HDMI capture

Status: **video interface exists; bridge/CSI backend pending**.

Acceptance:

- bridge probe/EDID;
- CSI capture stable at 720p then 1080p;
- unplug/replug recovery;
- BIOS/UEFI/OS timing transitions;
- MJPEG debug stream.

## M5 - H.264 + web console

Status: **not implemented**.

Acceptance:

- bounded frame ring;
- HTTPS UI;
- authenticated WebSocket input;
- measured glass-to-glass and HID latency;
- H.264 capability based on actual P4 revision.

## M6 - WireGuard remote access

Status: **network/VPN policy state exists; WireGuard backend pending**.

Acceptance:

- one compatible WireGuard/lwIP stack;
- protected key storage;
- VPN reconnect;
- remote KVM listeners restricted according to policy;
- remote session revoked on VPN loss;
- video soak through VPN.

## M7 - Codex device login and account-backed text transport

Status: **implemented and compile-validated; real account/hardware validation pending**.

Implemented:

- device-code request/poll flow;
- OAuth code exchange;
- access/refresh/id-token handling in RAM;
- refresh flow;
- ID-token account/workspace extraction;
- dynamic account model-catalog lookup;
- account-backed Responses SSE text transport;
- upstream Codex revision pin;
- one retry after auth refresh on HTTP 401.

Still required for acceptance:

- expose device-code challenge through serial/web UI;
- real device-code login on P4+C5 hardware;
- real account model lookup;
- real streamed text turn using the user's ChatGPT/Codex account;
- refresh after expiry/401 on hardware;
- protected persistent refresh-token storage;
- sign-out/wipe validation.

## M8 - Read-only AI assistant

Status: **partial**.

Already present:

- bounded SSE parser;
- account-backed text transport;
- local read-only tools `get_status` and `get_screen_text`;
- local policy layer prevents AI mutation in observe mode.

Still required:

- expose read-only KVM tools through the model tool schema;
- bounded tool-call loop;
- max turn/tool-call deadlines;
- provider call-ID replay tracking;
- real screen-text backend;
- graceful cancellation on auth/session/network loss.

## M9 - Assisted AI KVM

Status: **not model-exposed yet by design**.

Tools:

- keyboard/hotkeys;
- mouse;
- release input.

Acceptance:

- explicit approval UX;
- provider tool calls converted to internal typed requests;
- duplicate tool-call replay protection;
- cancellation and emergency human override;
- no path from model transport directly to raw USB reports.

## M10 - Autonomous bounded workflows

Status: **planned**.

Examples: navigate BIOS, select boot entry, perform a known installation/bootstrap sequence.

Acceptance:

- explicit workflow scope + expiry;
- destructive actions separately gated;
- full action audit log without leaking typed secrets;
- immediate revoke/human takeover.

## Later

- compact virtual media from 256 MB bulk flash;
- optional multi-GB media via SD/eMMC/remote block backend;
- ATX power/reset;
- Tailscale-compatible backend;
- Ethernet uplink;
- optional one-shot image understanding;
- production OTA/update channel.
