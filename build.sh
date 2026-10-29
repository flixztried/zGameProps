#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

[ -n "${ANDROID_NDK_HOME:-}" ] || { echo "ANDROID_NDK_HOME not set"; exit 1; }
[ -x "$ANDROID_NDK_HOME/ndk-build" ] || { echo "ndk-build not found"; exit 1; }

[ -f "$ROOT/jni/zygisk.hpp" ] || curl -fsSL \
    "https://raw.githubusercontent.com/topjohnwu/zygisk-module-sample/master/module/jni/zygisk.hpp" \
    -o "$ROOT/jni/zygisk.hpp"

"$ANDROID_NDK_HOME/ndk-build" \
    NDK_PROJECT_PATH="$ROOT" \
    APP_BUILD_SCRIPT="$ROOT/jni/Android.mk" \
    NDK_APPLICATION_MK="$ROOT/jni/Application.mk" \
    NDK_OUT="$ROOT/build/obj" \
    NDK_LIBS_OUT="$ROOT/build/libs" \
    -j"$(nproc)"

DIST="$ROOT/dist"
STAGE="$DIST/stage"
OUT="$DIST/zGameProps.zip"

rm -rf "$DIST"
mkdir -p "$STAGE/zygisk"

cp "$ROOT/module/module.prop"  "$STAGE/"
cp "$ROOT/module/customize.sh" "$STAGE/"
cp "$ROOT/module/config.conf"  "$STAGE/"

for abi in arm64-v8a armeabi-v7a x86 x86_64; do
    src="$ROOT/build/libs/$abi/libzygisk.so"
    [ -f "$src" ] && cp "$src" "$STAGE/zygisk/$abi.so"
done

chmod +x "$STAGE/customize.sh"
( cd "$STAGE" && zip -qr "$OUT" . )
rm -rf "$STAGE"

echo "$OUT"
