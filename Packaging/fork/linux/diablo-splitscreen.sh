#!/bin/bash
# Starts Diablo Splitscreen (Linux, Steam Deck).
# In an install made by install.sh it first switches to an update downloaded during an
# earlier session, checks for the next one in the background while the game runs, and
# rolls back a new version that crashes within its first minute (update.sh).
# Reports an anonymous crash (version, platform, signal) afterwards.
# DEVILUTIONX_NO_REPORT=1 turns reporting off, DSS_NO_UPDATE=1 turns updates off.
DSS_VERSION="@VERSION@"
BASEDIR="$(dirname "$(realpath "$0")")"
updater="$BASEDIR/update.sh"

if [ -z "${DSS_UPDATE_APPLIED:-}" ] && [ -x "$updater" ]; then
    next=$("$updater" apply "$BASEDIR" 2>/dev/null)
    [ -n "$next" ] && [ -x "$next" ] && DSS_UPDATE_APPLIED=1 exec "$next" "$@"
fi
if [ -x "$updater" ]; then
    (nice -n 10 "$updater" check "$BASEDIR" >/dev/null 2>&1 &)
fi

started=$(date +%s)
if [ -e /dev/fuse ]; then
    "$BASEDIR/devilutionx.AppImage" "$@"
else
    "$BASEDIR/devilutionx.AppImage" --appimage-extract-and-run "$@"
fi
rc=$?
if [ "$rc" -gt 128 ] && [ "${DEVILUTIONX_NO_REPORT:-}" != "1" ]; then
    (curl -fsS --max-time 8 -o /dev/null -d "Diablo Splitscreen $DSS_VERSION Linux crash signal $((rc - 128))" \
        https://ntfy.sh/diablo-splitscreen-faults-747d46c7c1c4de46 >/dev/null 2>&1 &)
fi
if [ -x "$updater" ]; then
    back=$("$updater" finish "$BASEDIR" "$rc" "$(($(date +%s) - started))" 2>/dev/null)
    # The previous version, after the new one crashed at once: play on with that.
    [ -n "$back" ] && [ -x "$back" ] && DSS_UPDATE_APPLIED=1 exec "$back" "$@"
fi
exit "$rc"
