# Networking and VPN

## Goal

Remote KVM access should be through a private VPN overlay, not a directly exposed Internet-facing HTTPS port.

## Uplink model

Preferred:

```text
ESP32-P4 -- SDIO --> ESP32-C5 -- Wi-Fi --> LAN/Internet
```

Fallbacks:

- ESP-Hosted over SPI;
- native P4 Ethernet when the board has a PHY.

`akvm_net` must present the same uplink state regardless of transport.

## ESP-Hosted integration plan

1. create a board-specific P4/C5 transport profile;
2. bring C5 firmware version under repository control or document the exact compatible release;
3. initialize the hosted netif before management services;
4. expose link/IP/DNS/RSSI metrics through the capability/status API;
5. add bounded reconnect with jitter;
6. provide a recovery action that resets only the C5 before rebooting the P4.

## WireGuard

WireGuard is the primary remote access backend.

Requirements:

- one WireGuard/lwIP stack in the image;
- private key stored only in protected device storage;
- tunnel lifecycle owned by a worker task;
- management listeners may bind to LAN according to local policy, but Internet-facing remote access is considered valid only when the VPN interface is ready;
- DNS and default-route behavior must be explicit: management-only tunnel vs full tunnel.

Recommended product default: **management-only tunnel**. The KVM does not need to route arbitrary client Internet traffic unless travel-router mode is explicitly added later.

## Tailscale-compatible backend

Optional, later. If integrated using a stack that embeds WireGuard/lwIP, it must share that implementation with classic WireGuard. Do not link two independent stacks with overlapping lwIP/WireGuard symbols.

## Service exposure

Default policy:

| Service | Local LAN | VPN | Public WAN |
|---|---|---|---|
| HTTPS UI | configurable | yes | no |
| video WS/stream | configurable | yes | no |
| KVM input WS | configurable | yes | no |
| provisioning | local only | no | no |
| diagnostics | local/admin | admin | no |

Listeners should bind by interface where possible rather than relying only on application-level source-IP checks.

## Wi-Fi provisioning

Do not hard-code credentials. Initial options, in order of preference:

1. USB serial provisioning;
2. temporary local provisioning AP with physical-presence trigger;
3. BLE is not assumed because C5 is used as hosted Wi-Fi and product complexity should stay low.

Provisioning mode must time out and must not expose ChatGPT/VPN secrets.

## Network failure behavior

- uplink loss: keep local KVM/HID alive;
- VPN loss: terminate remote KVM sessions or downgrade them to no-input according to policy;
- DNS failure: retain direct VPN peer IP if configured;
- C5 hang: reset C5 first, P4 only after repeated recovery failure;
- clock invalid: block TLS/Codex login until time is synchronized.
