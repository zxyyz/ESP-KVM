# Storage Architecture

## New hardware assumption

The product will add an additional **256 MB flash device** for bulk data.

This is intentionally treated as **secondary/bulk storage**, not as the ESP32-P4 boot flash. ESP32-P4's normal boot/main-flash path is documented for up to 64 MB, while secondary flash can be accessed through the `esp_flash_*` APIs on another SPI controller/chip-select.

The exact 256 MB device is still unknown and must be recorded before a driver is selected:

- manufacturer + part number;
- NOR vs NAND;
- SPI/QSPI interface;
- voltage;
- maximum clock;
- erase block / sector geometry;
- 3-byte/4-byte addressing behavior;
- dedicated SPI bus and GPIOs;
- whether the device shares a bus with SD/HDMI/other peripherals.

## Two storage domains

### Domain A: main boot/security flash

Use the normal ESP32-P4 main flash only for data that depends on bootloader/cache/encryption support:

- bootloader;
- partition table;
- OTA application slots;
- NVS configuration;
- encrypted credentials/secrets;
- signed OTA metadata;
- recovery metadata;
- minimal immutable web/recovery assets.

Long-lived secrets stay here:

- ChatGPT/Codex refresh credentials;
- WireGuard private key;
- Wi-Fi credentials;
- device TLS private key;
- administrative pairing/session root secrets.

### Domain B: 256 MB bulk flash

Use the additional flash for large non-executable objects:

- read-only virtual-media images;
- cached/uploaded ISO/IMG payloads;
- optional web static assets;
- update staging files that are later verified/copied through the normal OTA path;
- bounded diagnostics and performance captures;
- optional AI/KVM audit records after redaction;
- future screen snapshots only if explicitly enabled.

Do **not** assume this flash can:

- execute firmware;
- participate in the normal OTA boot selection;
- be `mmap`-ed like main flash;
- use ESP32 hardware Flash Encryption;
- safely hold OAuth/VPN/device private keys in plaintext.

If sensitive bulk content is required, use application-layer authenticated encryption with a key rooted in protected main-flash/eFuse state, or do not persist the content.

## Proposed logical layout

The 256 MB device should not use the ESP-IDF boot partition table. It gets its own storage metadata.

Initial logical budget:

| Region | Suggested size | Purpose |
|---|---:|---|
| superblock A/B | 2 x 64 KiB | redundant format/version/generation metadata |
| object index / journal | 4 MiB | bounded metadata, wear-aware updates |
| virtual media | 192 MiB | ISO/IMG-style objects |
| update staging | 32 MiB | verified temporary download/staging |
| diagnostics/audit | 16 MiB | bounded ring; redacted only |
| reserve | remainder | bad-block/alignment/future use |

These are product budgets, not fixed offsets yet. Final geometry depends on NOR/NAND sector/block size.

## Filesystem choice

Do not choose a filesystem until the flash type is known.

### If SPI NOR

Candidates:

- LittleFS for metadata/small objects;
- FATFS if host tooling/interoperability matters;
- custom append/object store for large immutable virtual-media payloads.

A hybrid design is attractive: small redundant metadata + large extent-based immutable objects, avoiding frequent filesystem rewrites for ISO-sized files.

### If SPI NAND

Use a NAND-aware layer/filesystem with bad-block management and ECC expectations. Do not pretend a NAND device is byte-compatible with NOR `esp_flash` semantics.

## Virtual-media design

The 256 MB flash makes virtual media a first-class feature.

Recommended behavior:

1. media is uploaded by an authenticated administrator;
2. upload writes into a temporary extent;
3. hash/signature metadata is finalized atomically;
4. only finalized objects become mountable;
5. USB MSC exports a selected object read-only;
6. AI may select only from already-approved media IDs;
7. AI may never provide an arbitrary remote URL for automatic download+mount.

A power loss during upload must leave the previous object index valid.

## Update staging

The bulk flash may cache a firmware package, but the final OTA image still goes through ESP-IDF's normal signed OTA/application partition path on main flash.

Required sequence:

```text
network download -> bulk flash staging -> hash/signature verify
                 -> OTA write to main flash -> boot verification/rollback
```

Do not execute or boot directly from the secondary 256 MB device.

## Performance isolation

Bulk-flash writes can stall a shared SPI bus and may interact badly with video/network timing. Therefore:

- prefer a dedicated SPI controller/bus;
- chunk long erase/write operations;
- never perform erase/write in HID critical paths;
- throttle upload/diagnostic writes while video latency is high;
- collect p99 HID/video latency during flash erase/program stress.

## Storage API plan

Add a hardware-independent `akvm_storage` service with two explicit backends:

```text
AKVM_STORE_SECURE   -> main flash / protected NVS
AKVM_STORE_BULK     -> external 256 MB device
```

The API must make it difficult to accidentally route secrets into bulk storage.

## Acceptance tests

- detect and report exact JEDEC/device geometry;
- full-device read/write/erase test on development hardware;
- power-cut recovery during metadata update;
- 1000 upload/delete cycles on a test region;
- virtual media read soak while video + VPN + HID are active;
- erase/program stress with HID latency measurements;
- verify protected secrets never appear in bulk-flash dumps.
