# AGENTS.md

Guidance for coding agents and contributors.

## Product boundary

This repository targets a single-purpose ESP32-P4 + ESP32-C5 IP-KVM appliance. Do not turn it into a general-purpose shell agent. AI actions must pass through explicit KVM tools and a local policy gate.

## Architecture invariants

- P4 owns application logic, video, USB device, security, VPN and the AI agent.
- C5 is primarily a Wi-Fi coprocessor through ESP-Hosted. Prefer SDIO; SPI is fallback.
- KVM services must remain usable when AI is signed out or unavailable.
- AI must remain usable in `observe` mode when HID is unavailable.
- Web/KVM services are never intentionally exposed to WAN by default.
- WireGuard is the primary remote-access VPN. If Tailscale support is later added, do not link two independent WireGuard/lwIP implementations.
- Never store ChatGPT refresh/access tokens, VPN private keys, session secrets, or Wi-Fi credentials in source code.
- Destructive tools (power/reset/virtual media, credential entry, irreversible firmware actions) require separate policy capability checks.
- Board pin numbers belong in the board component/Kconfig, never in protocol, AI, web, or service modules.

## Upstream policy

Use upstream implementations as references, but review licenses and dependency cost before copying source. In particular:

- `espkvm/espkvm` provides proven ESP32-P4 KVM patterns and hard-won P4 notes.
- `espressif/esp-hosted-mcu` is the expected C5 transport.
- `openai/codex` is the source of truth for ChatGPT/Codex device login and protocol behavior.

Do not reverse-engineer a frozen private endpoint and call it stable. The Codex integration must track upstream source/protocol changes and fail closed when authentication/protocol expectations diverge.

## Build discipline

- Keep hardware-specific integrations optional until the board profile is known.
- A component should expose a small typed interface and own its FreeRTOS task(s).
- Avoid unbounded allocations. Every network/tool/frame buffer needs an explicit maximum.
- No long blocking operation while holding lwIP core locks, global KVM state locks, or HID queues.
- Treat video frame data as bulk data and agent/event data as control data; they should not share queues.
- Prefer static task stacks or explicitly budgeted dynamic allocations once the implementation stabilizes.

## Definition of done for hardware features

A feature is not considered available merely because it compiled. It needs:

1. capability probe result;
2. startup log with concrete reason on failure;
3. bounded retry/recovery behavior;
4. measurable resource/bandwidth impact;
5. hardware validation notes in `docs/HARDWARE.md`.
