#!/bin/bash
# Exercises Packaging/fork/linux/install.sh in a throwaway HOME.
#
#   test-linux-installer.sh                      fake tarballs only
#   test-linux-installer.sh <dist dir> <version> also installs the real release
#                                                tarball + SHA256SUMS from <dist dir>
#
# Covers: checksum refusal leaves the old install alone, atomic switch, previous version
# kept and older pruned, idempotent reinstall, menu entry, MPQ copied lower-case and never
# overwritten, saves untouched, Steam shortcut only with Steam closed (backup kept).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
linux="$here/../linux"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fails=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fails=$((fails + 1)); fi; }

# pgrep stand-in: "steam" runs while $work/steam-running exists.
mkdir -p "$work/bin"
cat > "$work/bin/pgrep" <<SH
#!/bin/sh
[ -e "$work/steam-running" ] && exit 0
exit 1
SH
chmod +x "$work/bin/pgrep"
export PATH="$work/bin:$PATH" DSS_YES=1 DEVILUTIONX_NO_REPORT=1 LANG=C
unset XDG_DATA_HOME LANGUAGE LC_ALL LC_MESSAGES
export HOME="$work/home"
mkdir -p "$HOME"

# A release directory holding install.sh (tag substituted), a tarball and SHA256SUMS.
fake_release() {
    local tag=$1 dir="$work/rel-$1" pkg="$work/pkg-$1/diablo-splitscreen"
    mkdir -p "$dir" "$pkg"
    printf '#!/bin/sh\necho %s\n' "$tag" > "$pkg/diablo-splitscreen.sh"
    printf '#!/bin/sh\necho appimage %s\n' "$tag" > "$pkg/devilutionx.AppImage"
    chmod +x "$pkg"/*
    cp "$linux/steam-shortcut.py" "$pkg/"
    printf 'png' > "$pkg/icon.png"
    tar -czf "$dir/diablo-splitscreen-$tag-linux-x86_64.tar.gz" -C "$work/pkg-$1" diablo-splitscreen
    (cd "$dir" && sha256sum ./*.tar.gz | sed 's#  \./#  #' > SHA256SUMS)
    sed "s/@TAG@/$tag/g" "$linux/install.sh" > "$dir/install.sh"
}
install_from() { DSS_RELEASE_URL="file://$work/rel-$1" bash "$work/rel-$1/install.sh" > "$work/log-$1" 2>&1; }

root="$HOME/.local/share/diablo-splitscreen"
pref="$HOME/.local/share/diasurgical/devilution"
mkdir -p "$pref" "$HOME/.steam/steam/userdata/42/config"
printf 'hero' > "$pref/single_0.sv"
printf 'MPQ-DATA' > "$work/DIABDAT.MPQ"
# An existing shortcut of another game must survive unchanged.
python3 - "$linux/steam-shortcut.py" "$HOME/.steam/steam/userdata/42/config/shortcuts.vdf" <<'PY'
import importlib.util, sys
spec = importlib.util.spec_from_file_location('s', sys.argv[1]); s = importlib.util.module_from_spec(spec); spec.loader.exec_module(s)
items = [[s.MAP, b'shortcuts', [s.new_entry(0, 'Other', '/opt/other', '/opt', '')]]]
open(sys.argv[2], 'wb').write(s.write_map(items))
PY
vdf="$HOME/.steam/steam/userdata/42/config/shortcuts.vdf"
original_vdf=$(sha256sum < "$vdf")
count_shortcuts() { python3 -c "
import importlib.util,sys
spec=importlib.util.spec_from_file_location('s','$linux/steam-shortcut.py');s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
items=s.load(open('$vdf','rb').read());print(len(items[0][2]), len(s.find(items,'$root/current/diablo-splitscreen/diablo-splitscreen.sh')))"; }

# 1. Steam running: install succeeds, shortcut skipped, shortcuts.vdf untouched.
fake_release v0.0.1
touch "$work/steam-running"
DSS_MPQ="$work/DIABDAT.MPQ" install_from v0.0.1
check "first install switches current" '[ "$(readlink "$root/current")" = versions/v0.0.1 ]'
check "launcher runs" '[ "$("$root/current/diablo-splitscreen/diablo-splitscreen.sh")" = v0.0.1 ]'
check "menu entry" 'grep -qx "Exec=\"$root/current/diablo-splitscreen/diablo-splitscreen.sh\"" "$HOME/.local/share/applications/diablo-splitscreen.desktop"'
check "MPQ copied lower-case" 'cmp -s "$work/DIABDAT.MPQ" "$pref/diabdat.mpq"'
check "no shortcut while Steam runs" '[ "$(sha256sum < "$vdf")" = "$original_vdf" ] && [ ! -e "$vdf.diablo-splitscreen-backup" ]'
rm "$work/steam-running"

# 2. Same version again, Steam closed: shortcut added once, backup is the original.
mkdir -p "$work/other"; printf 'OTHER' > "$work/other/DIABDAT.MPQ"
DSS_MPQ="$work/other/DIABDAT.MPQ" install_from v0.0.1
check "reinstall keeps current" '[ "$(readlink "$root/current")" = versions/v0.0.1 ] && [ ! -e "$root/versions/v0.0.1.old" ]'
check "existing MPQ never overwritten" 'cmp -s "$work/DIABDAT.MPQ" "$pref/diabdat.mpq"'
check "shortcut added, other kept" '[ "$(count_shortcuts)" = "2 1" ]'
check "backup is the original" '[ "$(sha256sum < "$vdf.diablo-splitscreen-backup")" = "$original_vdf" ]'
install_from v0.0.1
check "third install adds no duplicate" '[ "$(count_shortcuts)" = "2 1" ]'

# 3. A tarball that does not match SHA256SUMS changes nothing.
fake_release v0.0.2
printf 'tampered' >> "$work/rel-v0.0.2/diablo-splitscreen-v0.0.2-linux-x86_64.tar.gz"
if install_from v0.0.2; then bad=0; else bad=1; fi
check "checksum mismatch refused" '[ $bad = 1 ] && grep -q "(checksum)" "$work/log-v0.0.2"'
check "refused install left current" '[ "$(readlink "$root/current")" = versions/v0.0.1 ] && [ ! -e "$root/versions/v0.0.2" ]'

# 4. Updates: previous version kept, the one before removed.
fake_release v0.0.3; install_from v0.0.3
fake_release v0.0.4; install_from v0.0.4
check "update switches current" '[ "$("$root/current/diablo-splitscreen/diablo-splitscreen.sh")" = v0.0.4 ]'
check "previous kept, older pruned" '[ "$(ls "$root/versions" | tr "\n" " ")" = "v0.0.3 v0.0.4 " ]'
check "shortcut still one" '[ "$(count_shortcuts)" = "2 1" ]'
check "saves untouched" '[ "$(cat "$pref/single_0.sv")" = hero ] && [ "$(ls "$pref" | tr "\n" " ")" = "diabdat.mpq single_0.sv " ]'

# 5. The real release tarball, when given.
if [ $# -ge 2 ]; then
    dist=$(cd "$1" && pwd) tag="v$2"
    # The release's own installer when it is there (CI), else this checkout's.
    installer="$dist/install.sh"
    if [ ! -e "$installer" ]; then
        installer="$work/install-$tag.sh"
        sed "s/@TAG@/$tag/g" "$linux/install.sh" > "$installer"
    fi
    DSS_RELEASE_URL="file://$dist" bash "$installer" > "$work/log-real" 2>&1 || cat "$work/log-real"
    game="$root/current/diablo-splitscreen"
    check "real release installed" '[ "$(readlink "$root/current")" = "versions/$tag" ]'
    for f in diablo-splitscreen.sh devilutionx.AppImage steam-shortcut.py icon.png LOCAL-COOP.md LICENSE.md; do
        check "real package has $f" '[ -e "$game/$f" ]'
    done
    check "real AppImage runs" '"$game/devilutionx.AppImage" --appimage-extract-and-run --version | tee "$work/version" >/dev/null && [ -s "$work/version" ]'
    check "real launcher is the shortcut target" '[ "$(count_shortcuts)" = "2 1" ]'
    cat "$work/version" 2>/dev/null || true
fi

if [ $fails -ne 0 ]; then echo "$fails failure(s)"; for l in "$work"/log-*; do echo "== $l"; cat "$l"; done; exit 1; fi
echo "all installer checks passed"
