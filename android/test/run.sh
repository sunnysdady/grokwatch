#!/bin/sh
# Pixel conversion regression tests; JDK 17+ only, no Android SDK/device needed.
set -eu
cd "$(dirname "$0")"
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/grokwatch-pixels.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT
trap 'exit 1' HUP INT TERM
javac --release 17 -Xlint:all -Werror -d "$test_dir" ../app/src/main/java/io/grokwatch/app/WatchBgPixels.java ../app/src/main/java/io/grokwatch/app/WatchBgCrop.java WatchBgPixelsTest.java WatchBgCropTest.java
java -cp "$test_dir" io.grokwatch.app.WatchBgPixelsTest
java -cp "$test_dir" io.grokwatch.app.WatchBgCropTest
