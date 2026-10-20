#!/bin/bash
# Exercises the Linux automatic update (Packaging/fork/linux/update.sh and the
# launcher diablo-splitscreen.sh) in a throwaway HOME, with fake releases served
# from file:// URLs in place of GitHub.
#
#   test-linux-update.sh                       fake releases only
#   test-linux-update.sh <dist dir> <version>  also updates a fake older install to the
#                                              real release tarball in <dist dir>, health
#                                              check with the real AppImage included
#
# Covers: download only in the background and only a newer, checksum-correct release;
# switch at the next launch; previous version kept; failed health check and crash on
# first start both roll back and are never retried; Warpzone/opt-out/foreign installs,
# throttling and being offline change nothing; MPQ, saves and settings untouched.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
linux="$here/../linux"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fails=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fails=$((fails + 1)); fi; }

mkdir -p "$work/bin"
printf '#!/bin/sh\nexit 1\n' > "$work/bin/pgrep"   # Steam is never running here
chmod +x "$work/bin/pgrep"
export PATH="$work/bin:$PATH" DSS_YES=1 DEVILUTIONX_NO_REPORT=1 LANG=C DSS_CHECK_INTERVAL=0
unset XDG_DATA_HOME LANGUAGE LC_ALL LC_MESSAGES DSS_NO_UPDATE DSS_UPDATE_APPLIED
export HOME="$work/home"
export DSS_API_URL="file://$work/api.json" DSS_DOWNLOAD_URL="file://$work/dl"
mkdir -p "$HOME" "$work/dl"
root="$HOME/.local/share/diablo-splitscreen"
pref="$HOME/.local/share/diasurgical/devilution"

# A release: real launcher + updater, a fake AppImage. mode ok | broken (fails its
# --version health check) | crash (segfaults when played).
fake_release() {
    local tag=$1 mode=${2:-ok} pkg="$work/pkg-$1/diablo-splitscreen" dir="$work/dl/$1"
    mkdir -p "$pkg" "$dir"
    sed "s/@VERSION@/$tag/" "$linux/diablo-splitscreen.sh" > "$pkg/diablo-splitscreen.sh"
    cp "$linux/update.sh" "$linux/steam-shortcut.py" "$pkg/"
    printf 'png' > "$pkg/icon.png"
    cat > "$pkg/devilutionx.AppImage" <<SH
#!/bin/bash
[ "\${1:-}" = --appimage-extract-and-run ] && shift
if [ "\${1:-}" = --version ]; then
    [ $mode = broken ] && exit 1
    echo "DevilutionX $tag" >&2; exit 0
fi
echo $tag >> "$work/ran"
[ $mode = crash ] && kill -SEGV \$\$
exit 0
SH
    chmod +x "$pkg"/*.sh "$pkg/devilutionx.AppImage"
    tar -czf "$dir/diablo-splitscreen-$1-linux-x86_64.tar.gz" -C "$work/pkg-$1" diablo-splitscreen
    (cd "$dir" && sha256sum ./*.tar.gz | sed 's#  \./#  #' > SHA256SUMS)
    sed "s/@TAG@/$1/g" "$linux/install.sh" > "$dir/install.sh"
}
# The GitHub API answer listing the given tags (newest first, like GitHub).
publish() {
    python3 - "$work/api.json" "$@" <<'PY'
import json, sys
out, tags = sys.argv[1], sys.argv[2:]
rel = [{'tag_name': t, 'draft': False, 'prerelease': True, 'assets': [
    {'name': f'diablo-splitscreen-{t}-linux-x86_64.tar.gz'}, {'name': 'SHA256SUMS'}]} for t in reversed(tags)]
# A draft and a non-fork tag must never be chosen.
rel.insert(0, {'tag_name': 'v9.9.9', 'draft': True, 'assets': rel[0]['assets'] if rel else []})
rel.insert(0, {'tag_name': '1.6.0', 'draft': False, 'assets': []})
json.dump(rel, open(out, 'w'))
PY
}
install_from() { DSS_RELEASE_URL="file://$work/dl/$1" bash "$work/dl/$1/install.sh" > "$work/log-install" 2>&1; }
launch() { : > "$work/ran"; "$root/current/diablo-splitscreen/diablo-splitscreen.sh" > "$work/log-launch" 2>&1 || true; }
ran() { tr '\n' ' ' < "$work/ran" | sed 's/ $//'; }
current() { basename "$(readlink "$root/current")"; }
wait_for() { for _ in $(seq 100); do [ -e "$1" ] && [ ! -e "$root/.update-lock" ] && return 0; sleep 0.1; done; return 1; }
updater() { "$root/current/diablo-splitscreen/update.sh" "$@"; }

mkdir -p "$pref"
printf 'hero' > "$pref/single_0.sv"; printf 'MPQ' > "$pref/diabdat.mpq"; printf '[Game]\n' > "$pref/diablo.ini"
userdata=$(cd "$pref" && sha256sum ./* | sha256sum)

# 1. Installed v0.0.1, v0.0.2 published: playing downloads it in the background only.
fake_release v0.0.1
install_from v0.0.1
fake_release v0.0.2; publish v0.0.1 v0.0.2
launch
check "first launch plays the installed version" '[ "$(ran)" = v0.0.1 ]'
check "background check stages the newer release" 'wait_for "$root/pending" && [ "$(cat "$root/pending")" = v0.0.2 ] && [ -x "$root/versions/v0.0.2/diablo-splitscreen/diablo-splitscreen.sh" ]'
check "nothing switched while playing" '[ "$(current)" = v0.0.1 ]'

# 2. Next launch switches and plays the new version; the old one is kept.
launch
check "next launch plays the update" '[ "$(ran)" = v0.0.2 ] && [ "$(current)" = v0.0.2 ]'
check "previous version kept" '[ "$(ls "$root/versions" | tr "\n" " ")" = "v0.0.1 v0.0.2 " ] && [ "$(cat "$root/previous")" = v0.0.1 ]'
check "clean first run ends the trial" '[ ! -e "$root/trial" ] && [ ! -e "$root/pending" ] && [ ! -e "$root/bad" ]'
check "the menu/Steam path follows the update" 'grep -q "DSS_VERSION=\"v0.0.2\"" "$root/current/diablo-splitscreen/diablo-splitscreen.sh"'

# 3. A tarball not matching SHA256SUMS is never staged.
fake_release v0.0.3; publish v0.0.1 v0.0.2 v0.0.3
# A valid tarball that is not the one SHA256SUMS names.
printf 'tampered' > "$work/pkg-v0.0.3/diablo-splitscreen/extra"
tar -czf "$work/dl/v0.0.3/diablo-splitscreen-v0.0.3-linux-x86_64.tar.gz" -C "$work/pkg-v0.0.3" diablo-splitscreen
updater check "$root/current/diablo-splitscreen"
check "checksum mismatch refused" '[ ! -e "$root/pending" ] && [ ! -e "$root/versions/v0.0.3" ]'

# 4. A version whose binary does not start stays out and is not retried.
fake_release v0.0.4 broken; publish v0.0.2 v0.0.4
updater check "$root/current/diablo-splitscreen"
check "broken release staged" '[ "$(cat "$root/pending")" = v0.0.4 ]'
launch
check "failed health check keeps playing the old version" '[ "$(ran)" = v0.0.2 ] && [ "$(current)" = v0.0.2 ] && grep -qx v0.0.4 "$root/bad"'
updater check "$root/current/diablo-splitscreen"
check "bad version not fetched again" '[ ! -e "$root/pending" ] && [ ! -e "$root/versions/v0.0.4" ]'

# 5. A version that crashes on its first start is rolled back and the old one starts.
fake_release v0.0.5 crash; publish v0.0.2 v0.0.5
updater check "$root/current/diablo-splitscreen"
launch
check "crash on first start rolls back and plays on" '[ "$(ran)" = "v0.0.5 v0.0.2" ] && [ "$(current)" = v0.0.2 ] && grep -qx v0.0.5 "$root/bad"'
updater check "$root/current/diablo-splitscreen"
check "rolled-back version not offered again" '[ ! -e "$root/pending" ]'

# 5b. Steam or a terminal ending the new version's first run is not a crash.
fake_release v0.0.6 ok; publish v0.0.2 v0.0.6
updater check "$root/current/diablo-splitscreen"
DSS_UPDATE_APPLIED= "$root/current/diablo-splitscreen/update.sh" apply "$root/current/diablo-splitscreen" > /dev/null
updater finish "$root/current/diablo-splitscreen" 143 3
check "TERM on first start keeps the update" '[ "$(current)" = v0.0.6 ] && ! grep -qx v0.0.6 "$root/bad"'

# 6. A newer good release after a bad one is taken.
fake_release v0.0.7; publish v0.0.2 v0.0.5 v0.0.6 v0.0.7
updater check "$root/current/diablo-splitscreen"; launch
check "next good release applied" '[ "$(ran)" = v0.0.7 ] && [ "$(current)" = v0.0.7 ] && [ "$(ls "$root/versions" | tr "\n" " ")" = "v0.0.6 v0.0.7 " ]'

# 7. Nothing happens when updates are off, under Warpzone, throttled, offline or
#    outside an installer install.
fake_release v0.0.8; publish v0.0.7 v0.0.8
game="$root/current/diablo-splitscreen"
touch "$root/no-auto-update"; updater check "$game"; rm "$root/no-auto-update"
check "opt-out file" '[ ! -e "$root/pending" ]'
DSS_NO_UPDATE=1 updater check "$game"
check "DSS_NO_UPDATE" '[ ! -e "$root/pending" ]'
touch "$game/.warpzone-managed"; updater check "$game"; rm "$game/.warpzone-managed"
check "Warpzone-managed install" '[ ! -e "$root/pending" ]'
DSS_API_URL="file://$work/nowhere.json" updater check "$game"
check "offline is silent" '[ ! -e "$root/pending" ] && [ "$(current)" = v0.0.7 ]'
DSS_CHECK_INTERVAL=3600 updater check "$game"
check "checked once per interval" '[ ! -e "$root/pending" ]'
mkdir -p "$work/loose"; tar -xzf "$work/dl/v0.0.1/diablo-splitscreen-v0.0.1-linux-x86_64.tar.gz" -C "$work/loose"
: > "$work/ran"; "$work/loose/diablo-splitscreen/diablo-splitscreen.sh" >/dev/null 2>&1; sleep 1
check "unpacked tarball never updates" '[ "$(ran)" = v0.0.1 ] && [ ! -e "$root/pending" ] && [ ! -e "$work/loose/pending" ]'
updater check "$game"
check "then the check works" '[ "$(cat "$root/pending")" = v0.0.8 ]'

check "MPQ, saves and settings untouched" '[ "$(cd "$pref" && sha256sum ./* | sha256sum)" = "$userdata" ]'

# 8. The real release: a fake older install updates to it, health check with the real binary.
if [ $# -ge 2 ]; then
    dist=$(cd "$1" && pwd) tag="v$2"
    rm -rf "$HOME/.local/share/diablo-splitscreen"
    install_from v0.0.1
    mkdir -p "$work/dl/$tag"
    ln -s "$dist/diablo-splitscreen-$tag-linux-x86_64.tar.gz" "$dist/SHA256SUMS" "$work/dl/$tag/"
    publish v0.0.1 "$tag"
    updater check "$root/current/diablo-splitscreen"
    real="$root/versions/$tag/diablo-splitscreen"
    check "real release staged" '[ "$(cat "$root/pending" 2>/dev/null)" = "$tag" ]'
    check "real package has updater and versioned launcher" '[ -x "$real/update.sh" ] && grep -q "^DSS_VERSION=\"$tag\"" "$real/diablo-splitscreen.sh"'
    next=$(updater apply "$root/current/diablo-splitscreen")
    check "real binary passes the health check and is switched to" '[ "$next" = "$real/diablo-splitscreen.sh" ] && [ "$(current)" = "$tag" ] && [ ! -e "$root/bad" ]'
fi

if [ $fails -ne 0 ]; then echo "$fails failure(s)"; for l in "$work"/log-*; do echo "== $l"; cat "$l"; done; exit 1; fi
echo "all update checks passed"
