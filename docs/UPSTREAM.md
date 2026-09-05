# Upstream Baseline

This project intentionally tracks a few upstream projects closely. Record the reviewed revisions here before integrating code or protocol behavior.

## ESP-KVM

Repository: `espkvm/espkvm`

Reviewed revision:

- commit: `7b61cce54aec8717b64dca52ef2f9db906147aba`
- date: 2026-09-03
- release message: `Release 0.42.2: wake a sleeping target`

Important architectural lessons reviewed from its current `AGENTS.md`:

- ESP32-P4 + TC358743 is a proven KVM path.
- Its component split separates video, HID, storage, web, networking and board configuration.
- Current upstream uses ESP-IDF 6.1-rc1.
- WireGuard and native Tailscale share one `wireguard_lwip` implementation; linking separate copies is unsafe.
- P4 silicon revision affects H.264 behavior/performance and must be treated as a runtime/board capability.
- USB virtual media needs deliberate TinyUSB integration rather than assuming the managed component fits every use case.

Do not copy board-specific hacks without verifying them on our hardware.

## OpenAI Codex

Repository: `openai/codex`

Reviewed revision:

- commit: `588b781ab4924ce7352488394028e63d74cf807f`
- date: 2026-09-05

Relevant current paths include:

- `codex-rs/login/src/device_code_auth.rs`
- `codex-rs/login/src/lib.rs`
- `codex-rs/cli/src/login.rs`
- `codex-rs/app-server/README.md`
- `codex-rs/app-server-protocol/src/protocol/v2/account.rs`

The current upstream exposes a ChatGPT device-code login flow suitable for headless clients. Our firmware must pin the exact upstream behavior used by each release instead of treating internal constants as a forever-stable API.

Before implementing `akvm_codex_transport`:

1. review the current Codex license/notice requirements;
2. identify the minimum auth + model transport subset;
3. port behavior, not desktop dependencies;
4. add protocol fixtures without secrets;
5. document any endpoint/header constant with the upstream commit that justified it.

## ESP-Hosted MCU

Repository: `espressif/esp-hosted-mcu`

Reviewed revision:

- commit: `471900d45c3b9208da10a01beb200b97f15a161a`
- date: 2026-09-02

Integration requirements:

- confirm the chosen release supports ESP32-P4 host + ESP32-C5 coprocessor for our transport;
- prefer SDIO if the board exposes it;
- pin both P4-side and C5-side compatible versions;
- record throughput and recovery behavior with video and VPN active.

## Update policy

Do not casually track `main` for production releases. Once hardware bring-up starts, use known-good upstream revisions and update intentionally with a short compatibility note in this file.
