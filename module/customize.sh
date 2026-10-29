#!/system/bin/sh

SKIPUNZIP=0

if [ -d /data/adb/modules/rezygisk ]; then
    RUNTIME=rezygisk
elif [ -d /data/adb/modules/zygisksu ]; then
    RUNTIME=zygisksu
else
    abort "! Zygisk Next, ReZygisk, or NeoZygisk required"
fi

ZYGISK="✅zygisk"

COW="❌cow"
[ -f /dev/__properties__/properties_serial ] && COW="✅cow"

MOUNT="❌mount"
# Test if unshare actually works in this environment
if unshare -m true >/dev/null 2>&1; then
    MOUNT="✅mount"
fi

DESC="[$ZYGISK, $COW, $MOUNT] Minimal per-app Build spoof"

if [ -f "$MODPATH/module.prop" ]; then
    sed -i "s|^description=.*|description=$DESC|" "$MODPATH/module.prop"
fi

ui_print "- Runtime: $RUNTIME"
ui_print "- Hiding: $DESC"
ui_print "- Do not enable Magisk Enforce DenyList."
ui_print "- Target packages:"
grep '^packages=' "$MODPATH/config.conf" | sed 's/^packages=//' | tr ',' '\n' | \
    while read -r pkg; do
        [ -n "$pkg" ] && ui_print "    * $pkg"
    done
ui_print "- Credits: UN1CA (GamesPropsUtils), Device Faker (COW)"
