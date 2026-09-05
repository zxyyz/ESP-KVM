# Hardware Plan

## Known platform

- Main MCU: ESP32-P4.
- Wireless coprocessor: ESP32-C5.
- Intended C5 role: Wi-Fi uplink through ESP-Hosted, preferably SDIO.
- Target function: HDMI-in IP-KVM with USB keyboard/mouse device output.
- Additional storage: one **256 MB flash device** dedicated to bulk data.

The 256 MB device is currently planned as **secondary storage**, not as the P4 boot flash. ESP32-P4's documented main-flash path is limited to 64 MB, so firmware/OTA/security storage stays on the normal main flash while the large device is treated as a separate bulk backend.

## Board facts still required

Before hardware drivers are bound, record these facts here:

- exact board vendor and model/revision;
- ESP32-P4 silicon revision (`v1.x` vs `v3.x` matters for video/H.264 behavior);
- main boot flash size and part number;
- PSRAM type and size;
- **256 MB bulk-flash manufacturer/part number**;
- bulk flash type: NOR vs NAND;
- bulk flash interface/bus, chip-select and exact pins;
- bulk flash erase/program geometry and address mode;
- P4<->C5 transport (SDIO/SPI) and exact pins;
- C5 boot/enable/reset wiring;
- USB-OTG connector wiring to the target PC;
- whether USB-Serial-JTAG shares or conflicts with the intended device port;
- available MIPI-CSI lane count/pins;
- HDMI bridge part and oscillator/reference clock;
- microSD wiring, if still present after adding bulk flash;
- Ethernet PHY, if present;
- ATX power/reset/header interface, if present.

Until these are known, board-specific code remains disabled by Kconfig.

## Flash/storage topology

Recommended topology:

```text
ESP32-P4
  |
  +-- main flash (<= 64 MB documented boot path)
  |     bootloader / partition table
  |     OTA A/B
  |     encrypted NVS / secrets
  |     recovery metadata
  |
  +-- secondary 256 MB flash
        virtual media
        update staging
        bounded diagnostics/audit
        optional cached static assets
```

The secondary flash is not assumed to support execution, normal boot partitions, mmap, or ESP32 hardware Flash Encryption. OAuth/VPN/device private keys must not be placed there in plaintext. See `docs/STORAGE.md`.

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

- read-only MSC virtual media backed by approved objects on the 256 MB bulk flash.

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
- main flash;
- 256 MB bulk flash during erase/program;
- SD card if retained;
- USB device PHY;
- Ethernet PHY if fitted.

Measure 5 V rail droop during Wi-Fi TX + video encode + bulk-flash erase/program before treating unexplained CSI/storage failures as software bugs.

## Hardware acceptance tests

A board profile is considered supported only after:

- exact main/bulk flash parts and geometry are detected and recorded;
- full-device development read/write/erase test for the 256 MB flash;
- power-cut recovery during bulk-storage metadata update;
- 12 h HDMI capture soak;
- repeated HDMI unplug/replug;
- cold boot while target already outputs HDMI;
- target reboot through BIOS/UEFI/OS transitions;
- 10k keyboard and mouse report stress test;
- Wi-Fi reconnect while video active;
- VPN reconnect while video active;
- bulk-flash erase/program while measuring HID/video p99 latency;
- watchdog recovery from bridge/C5 failure;
- thermal measurements in enclosure.
