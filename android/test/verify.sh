#!/bin/sh
# Full local Android verification; device tests explicitly target an emulator.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sh "$ROOT/test/run.sh"
cd "$ROOT"
./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:lintDebug --console=plain
if [ "${1:-}" = "--emulator" ]; then
    case "${ANDROID_SERIAL:-}" in emulator-*) ;; *) echo "Set ANDROID_SERIAL to the local emulator ID" >&2; exit 2 ;; esac
    ./gradlew :app:connectedDebugAndroidTest --console=plain
fi
