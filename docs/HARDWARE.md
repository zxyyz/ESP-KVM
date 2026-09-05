# Hardware Plan

## Known platform

- Main MCU: ESP32-P4.
- Wireless coprocessor: ESP32-C5.
- Intended C5 role: Wi-Fi uplink through ESP-Hosted, preferably SDIO.
- Target function: HDMI-in IP-KVM with USB keyboard/mouse device output.

## Board facts still required

Before hardware drivers are bound, record these facts here:

- exact board vendor and model/revision;
- ESP32-P4 silicon revision (`v1.x` vs `v3.x` matters for video/H.264 behavior);
- flash size;
- PSRAM type and size;
- P4<->C5 transport (SDIO/SPI) and exact pins;
- C5 boot/enable/reset wiring;
- USB-OTG connector wiring to the target PC;
- whether USB-Serial-JTAG shares or conflicts with the intended device port;
- available MIPI-CSI lane count/pins;
- HDMI bridge part and oscillator/reference clock;
- microSD wiring;
- Ethernet PHY, if present;
- ATX power/reset/header interface, if present.

Until these are known, board-specific code remains disabled by Kconfig.

## HDMI input

ESP32-P4 does not accept HDMI electrically. An external HDMI receiver / bridge is required.

### Primary target: TC358743

Advantages:

- proven by existing ESP32-P4 KVM work;
- converts HDMI to CSI-2;
- existing register-sequence knowledge can be referenced upstream.

Risks:

- bridge initialization is hardware-sensitive;
- EDID and timing negotiation need testing across BIOS/UEFI, GPUs and servers;
- board layout and CSI timing matter.

### Planned alternative: LT6911 family

Keep the video component abstract enough that a different HDMI-to-CSI bridge can provide the same negotiated format interface.

### Minimum modes

Bring-up order:

1. 640x480/60 or 720p test pattern;
2. 1280x720/60;
3. 1920x1080/30;
4. 1920x1080/60 only if the full path supports it reliably.

The KVM product should prefer reliability and latency over nominal resolution.

## Video encode

Plan for two encoders:

- MJPEG: compatibility/debug fallback;
- H.264: primary WAN/VPN transport when stable on the installed P4 revision.

A runtime capability probe should report why H.264 is unavailable instead of silently falling back.

## USB device

The P4 presents itself to the controlled computer as a composite USB device.

Initial interfaces:

- boot-protocol keyboard;
- relative mouse;
- optional absolute mouse;
- optional consumer-control HID.

Later:

- read-only MSC virtual media.

USB HID must work independently from networking and AI.

## ATX control

If the final hardware exposes motherboard power/reset headers, use isolated/open-drain style outputs and explicit board definitions. Power/reset are destructive AI tools and always require a stronger policy gate.

## C5 transport

Preferred order:

1. 4-bit SDIO;
2. SPI if the board only exposes SPI.

Benchmark real P4<->C5 TCP throughput with video active. Do not use raw-link benchmark numbers as end-to-end KVM claims.

## Power budget

Budget for simultaneous peaks from:

- P4 CPU + CSI + encoder;
- C5 5 GHz transmit;
- HDMI bridge;
- PSRAM;
- SD card;
- USB device PHY;
- Ethernet PHY if fitted.

Measure 5 V rail droop during Wi-Fi TX + video encode before treating unexplained CSI/SD failures as software bugs.

## Hardware acceptance tests

A board profile is considered supported only after:

- 12 h HDMI capture soak;
- repeated HDMI unplug/replug;
- cold boot while target already outputs HDMI;
- target reboot through BIOS/UEFI/OS transitions;
- 10k keyboard and mouse report stress test;
- Wi-Fi reconnect while video active;
- VPN reconnect while video active;
- watchdog recovery from bridge/C5 failure;
- thermal measurements in enclosure.
