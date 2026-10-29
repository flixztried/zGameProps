# zGameProps

Per-app Build field spoofing for Zygisk. Minimal by design: only 4 fields
(`BRAND`, `DEVICE`, `MANUFACTURER`, `MODEL`) and their matching `ro.product.*`
properties.

## Requirements

- Zygisk Next, ReZygisk, or NeoZygisk
- Magisk, KernelSU, or APatch

Magisk built-in Zygisk is not supported.

## Stealth layers

| Layer | Mechanism |
|---|---|
| Linker hiding | `set_size(0)` -> `remove_link_map()` -> `DLCLOSE_MODULE_LIBRARY`. No address gap, no `soinfo` trace. |
| COW properties | `mmap(MAP_PRIVATE \| MAP_FIXED)` over `/dev/__properties__`, overwrite `ro.product.*` in place. Covers native reads. No GOT/PLT hooks. |
| Mount isolation | `unshare(CLONE_NEWNS)`, `MS_PRIVATE` recursion, unmount `/data/adb` traces. |
| Env scrub | Unset Magisk, KSU, APatch environment variables. |

All layers run per-process. The module unloads immediately after spoofing.
The installer dynamically checks the device and updates the module description
with `[✅soinfo, ✅cow, ✅mount]` to show which layers are active.

## Note

Do not enable Magisk's "Enforce DenyList". It blocks Zygisk modules from
loading into denylisted apps. This module does its own hiding.

## Config

Profiles in `/data/adb/modules/zgameprops/config.conf`.

## Build

    export ANDROID_NDK_HOME=/path/to/ndk
    ./build.sh

Output: `dist/zGameProps.zip`.

## Credits

- UN1CA — original GamesPropsUtils concept and profiles
- Device Faker (Seyud) — COW property engine reference
- ReZygisk / NeoZygisk — linker hiding reference
- topjohnwu — zygisk.hpp

## License

GPL-3.0
