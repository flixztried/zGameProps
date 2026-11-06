# zGameProps

Per-app Build field spoofing for Zygisk. 

## v2.0.0-hotfix
This is a hotfix release for v2.0.0. It removes the const-cast error in the memory scrubbing code, ensuring the module builds and unloads cleanly without native hooks.

## Architecture
The module operates exclusively at the Java layer. It modifies `Build.BRAND`, `Build.DEVICE`, `Build.MANUFACTURER`, and `Build.MODEL` via JNI before the app initializes, then immediately scrubs its memory and unloads.

- **No Native Hooks**: There are no GOT/PLT hooks or syscall intercepts.
- **No COW Engine**: There is no `mmap` over the property arena.
- **No Mount Hiding**: There are no `unshare` or `umount2` calls.

Because the game process is left 100% pristine of native tampering, it avoids anti-cheat memory scanners that flag modified memory maps or abnormal mount namespaces.

## Requirements
- Zygisk Next, ReZygisk, or NeoZygisk
- Magisk, KernelSU, or APatch
- **KernelSU + SUSFS** (or a resetprop script) to spoof the native properties (`ro.product.model`, etc.) globally, as this module only touches the Java layer.

## Configuration
Profiles are in `/data/adb/modules/zgameprops/config.conf`. 

## Credits
- UN1CA - original GamesPropsUtils concept and profiles
- topjohnwu - zygisk.hpp

## License
GPL-3.0
