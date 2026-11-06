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
JAVA="✅java"
SERVER="✅sys_server"

DESC="[$ZYGISK, $JAVA, $SERVER] COPG-style pure Java spoof"

if [ -f "$MODPATH/module.prop" ]; then
    sed -i "s|^description=.*|description=$DESC|" "$MODPATH/module.prop"
fi

ui_print "- Runtime: $RUNTIME"
ui_print "- Architecture: $DESC"
ui_print "- NOTE: This version uses NO native hooks or COW mapping."
ui_print "- You MUST use KernelSU + SUSFS (or Resetprop) to spoof native properties."
ui_print "- Target packages:"
grep '^packages=' "$MODPATH/config.conf" | sed 's/^packages=//' | tr ',' '\n' | \
    while read -r pkg; do
        [ -n "$pkg" ] && ui_print "    * $pkg"
    done
