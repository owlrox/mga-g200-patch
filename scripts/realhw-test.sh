#!/bin/sh
# Real-hardware test script for the mga G200 EXA fixes.
#
# Run as root on the machine with the G200 card, from a text console
# (X must not be running on the mga device). Captures everything into
# one text file for feedback.
#
# Usage: sh mga-realhw-test.sh [XSERVER_SECONDS]
# Default X run: 60 seconds. Pass 0 to skip the Xorg smoke test.

set -u

SECS=${1:-60}
OUT=mga-hw-results-$(date +%Y%m%d-%H%M%S).txt
REPO=$(cd "$(dirname "$0")/.." && pwd)
LOGDIR=$(mktemp -d)

say() { echo "=== $*" | tee -a "$OUT"; }
run() { say "$ $*"; "$@" 2>&1 | tee -a "$OUT"; echo >> "$OUT"; }

: > "$OUT"
say "mga G200 EXA fix -- hardware test $(date -u)"
say "repo: $REPO"
say ""

say "hardware"
run lspci -nn | grep -i -e matrox -e vga
run dmesg | tail -40
say ""

say "unit tests (make check)"
( cd "$REPO" && make -C . check 2>&1 || make check 2>&1 ) >> "$OUT" 2>&1
say "make check exit: $?"

if [ "$SECS" -gt 0 ]; then
    say ""
    say "Xorg smoke test: ${SECS}s on :99"

    # Use the freshly built driver: point the module path at the build
    # tree so we do not overwrite the installed driver.
    MODPATH="$REPO/src/.libs"
    [ -f "$REPO/src/.libs/mga_drv.so" ] || MODPATH="$REPO/src"

    cat > "$LOGDIR/mga-test.conf" <<EOF
Section "ServerFlags"
    Option "AutoAddGPU" "off"
EndSection
Section "Device"
    Identifier "G200"
    Driver "mga"
EndSection
Section "Monitor"
    Identifier "Monitor0"
EndSection
Section "Screen"
    Identifier "Screen0"
    Device "G200"
    Monitor "Monitor0"
EndSection
Section "ServerLayout"
    Identifier "Layout"
    Screen "Screen0"
EndSection
EOF

    run Xorg :99 -config "$LOGDIR/mga-test.conf" \\
        -modulepath "$MODPATH" -logfile "$LOGDIR/xorg.log" \\
        -noreset -audit 0 &
    XPID=$!
    sleep "$SECS"
    kill $XPID 2>/dev/null
    sleep 2
    kill -9 $XPID 2>/dev/null

    say "Xorg log:"
    cat "$LOGDIR/xorg.log" >> "$OUT" 2>&1
    say ""
    say "dmesg after X run"
    run dmesg | tail -60
fi

say ""
say "done -- send this file back: $OUT"
echo "results in: $OUT"
