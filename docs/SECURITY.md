# Security Model

## Threat model

This device may simultaneously hold:

- keyboard/mouse control over another computer;
- optional ATX power control;
- video from sensitive screens;
- Wi-Fi credentials;
- VPN private keys;
- ChatGPT/Codex refresh/access tokens.

Compromise therefore has a much larger impact than compromise of a normal IoT sensor.

## Mandatory production controls

### ESP platform security

Production images should enable and validate:

- Secure Boot;
- Flash Encryption;
- protected/encrypted NVS for long-lived secrets;
- signed OTA with rollback protection;
- disabled or physically controlled debug/JTAG access for production units.

Development builds may relax these only when clearly marked.

### Network

- no public WAN listener by default;
- VPN preferred for remote access;
- TLS even on LAN/VPN for browser credential/session protection;
- rate limiting on login/pairing endpoints;
- no unauthenticated WebSocket control endpoint.

### KVM authorization

Separate four concepts:

1. web/session authentication;
2. device/operator authorization;
3. AI policy mode;
4. per-action approval.

A valid ChatGPT login does **not** grant KVM input permission.

## AI policy

### Observe

Allowed: status and screen observations.

Denied: all input and destructive actions.

### Assist

Mutating KVM actions need interactive approval. The UI may approve a short action batch, but the scope and expiry must be explicit.

### Autonomous

A user may authorize a bounded workflow (for example, "install this OS until the first reboot"). Normal HID actions may execute within that scope. Destructive actions remain separately approved.

## Secret classes

| Secret | Storage | Export |
|---|---|---|
| Wi-Fi PSK | encrypted NVS | never via normal UI |
| WireGuard private key | encrypted secret store | admin-only rotation, not display |
| ChatGPT refresh token | encrypted secret store | never |
| access token | RAM + optional protected cache | never |
| web session key | protected storage/RAM | never |
| TLS device private key | protected storage | certificate workflow only |

## Logging

Never log:

- Authorization headers;
- OAuth codes/tokens beyond the user-facing short device code;
- VPN private keys;
- passwords typed through KVM;
- full screen text by default.

Support a redacted diagnostic mode.

## Physical presence

Require physical presence for at least:

- first administrative pairing;
- factory reset;
- enabling an insecure provisioning AP;
- exporting diagnostics containing identifiers;
- disabling Secure Boot/Flash Encryption on production hardware (prefer impossible).

## Safe failure

On auth/policy ambiguity, deny input. On AI failure, preserve human KVM. On VPN loss, revoke remote-input sessions. On watchdog reset, send USB `release all` as early as possible to avoid stuck modifiers/buttons.
