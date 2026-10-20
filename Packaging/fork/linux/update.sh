#!/bin/bash
# Automatic updates for installs made by install.sh (Linux, Steam Deck).
#
# Layout (install.sh): <root>/versions/<tag>/diablo-splitscreen/ and <root>/current, a
# relative symlink to versions/<tag>. The launcher calls this script:
#
#   update.sh check <game dir>   in the background while the game runs: asks GitHub for
#                                the newest fork release, downloads and checksum-verifies
#                                its tarball, unpacks it to versions/<tag> and writes
#                                <root>/pending. Never switches anything.
#   update.sh apply <game dir>   at launch, before the game starts: if a pending version
#                                passes its health check (its AppImage starts and reports
#                                a version), switches current to it with one atomic rename
#                                and prints its launcher. Prints nothing otherwise.
#   update.sh finish <game dir> <exit code> <seconds>
#                                after the first run of a just-applied version: a crash
#                                within the first minute rolls back to the previous version,
#                                marks the new one bad and prints the previous launcher.
#
# Only <root> is ever written. Game data (MPQs), saves and settings live under
# ~/.local/share/diasurgical and are never touched. Being offline is normal and silent;
# nothing here may delay or stop playing, so every failure ends in "carry on as before".
# Turned off by DSS_NO_UPDATE=1, a file <root>/no-auto-update, or a .warpzone-managed
# file in the game directory or its parent (Warpzone updates those installs itself).
set -u
DSS_REPO=jarvis-jhk/diablo-splitscreen
DSS_API="${DSS_API_URL:-https://api.github.com/repos/$DSS_REPO/releases?per_page=20}"
DSS_DOWNLOAD="${DSS_DOWNLOAD_URL:-https://github.com/$DSS_REPO/releases/download}"
DSS_FAULTS=https://ntfy.sh/diablo-splitscreen-faults-747d46c7c1c4de46
# A new version that crashes this soon after its first start is rolled back.
DSS_TRIAL_SECONDS=${DSS_TRIAL_SECONDS:-60}
DSS_CHECK_INTERVAL=${DSS_CHECK_INTERVAL:-21600}

cmd=${1:-} game=${2:-} tmp=""
[ -n "$game" ] && game=$(realpath -e -- "$game" 2>/dev/null) || exit 0
self_tag=$(basename "$(dirname "$game")")
root=$(dirname "$(dirname "$(dirname "$game")")")

valid_tag() { [[ "$1" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; }
# newer A B: A is a strictly higher version than B.
newer() { [ "$1" != "$2" ] && [ "$(printf '%s\n%s\n' "${1#v}" "${2#v}" | sort -V | tail -n1)" = "${1#v}" ]; }
is_bad() { [ -f "$root/bad" ] && grep -qx -- "$1" "$root/bad"; }
write_file() { printf '%s\n' "$2" > "$root/.$1.tmp" && mv -f "$root/.$1.tmp" "$root/$1"; }
read_file() { [ -f "$root/$1" ] && head -n1 "$root/$1"; }
fault() {
    # A fixed category leaves the device, nothing else: no paths, output or identifiers.
    [ "${DEVILUTIONX_NO_REPORT:-0}" = 1 ] && return 0
    (curl -fsS --max-time 8 -o /dev/null -d "Diablo Splitscreen $self_tag Linux update failed: $1" "$DSS_FAULTS" \
        >/dev/null 2>&1 &)
}
switch_to() {
    ln -sfn "versions/$1" "$root/current.new" && mv -T "$root/current.new" "$root/current"
}

# Only an installer install that is running as "current" updates itself.
managed() {
    valid_tag "$self_tag" || return 1
    [ "$(basename "$(dirname "$(dirname "$game")")")" = versions ] || return 1
    [ -L "$root/current" ] || return 1
    [ "$(realpath -e -- "$root/current/diablo-splitscreen" 2>/dev/null)" = "$game" ] || return 1
    [ "${DSS_NO_UPDATE:-0}" != 1 ] || return 1
    [ ! -e "$root/no-auto-update" ] || return 1
    [ ! -e "$game/.warpzone-managed" ] && [ ! -e "$(dirname "$game")/.warpzone-managed" ]
}

# The new version's own binary must start on this system (glibc, FUSE fallback).
healthy() {
    local g="$root/versions/$1/diablo-splitscreen" out
    [ -x "$g/diablo-splitscreen.sh" ] && [ -x "$g/devilutionx.AppImage" ] || return 1
    out=$(timeout 120 "$g/devilutionx.AppImage" --appimage-extract-and-run --version 2>&1) || return 1
    printf '%s\n' "$out" | grep -q '^DevilutionX '
}

do_check() {
    local now last tag tarball partial
    now=$(date +%s)
    last=$(read_file last-check) || last=0
    [[ "$last" =~ ^[0-9]+$ ]] || last=0
    [ $((now - last)) -ge "$DSS_CHECK_INTERVAL" ] || return 0
    # One checker at a time; a lock older than an hour is left over from a killed run.
    if ! mkdir "$root/.update-lock" 2>/dev/null; then
        [ -n "$(find "$root/.update-lock" -maxdepth 0 -mmin +60 2>/dev/null)" ] || return 0
        rm -rf "$root/.update-lock"; mkdir "$root/.update-lock" 2>/dev/null || return 0
    fi
    tmp=$(mktemp -d) || { rmdir "$root/.update-lock"; return 0; }
    trap 'rm -rf "$tmp" "$root/.update-lock"' EXIT
    write_file last-check "$now"
    curl -fsL --max-time 30 -H 'Accept: application/vnd.github+json' "$DSS_API" -o "$tmp/releases.json" || return 0
    # The highest fork version that is published and carries a Linux tarball.
    tag=$(python3 - "$tmp/releases.json" <<'PY'
import json, re, sys
try:
    releases = json.load(open(sys.argv[1]))
except Exception:
    sys.exit(0)
best = None
for r in releases if isinstance(releases, list) else []:
    tag = r.get('tag_name', '')
    m = re.fullmatch(r'v(\d+)\.(\d+)\.(\d+)', tag)
    names = {a.get('name') for a in r.get('assets', [])}
    if m and not r.get('draft') and f'diablo-splitscreen-{tag}-linux-x86_64.tar.gz' in names and 'SHA256SUMS' in names:
        key = tuple(map(int, m.groups()))
        if best is None or key > best[0]:
            best = (key, tag)
print(best[1] if best else '')
PY
) || return 0
    valid_tag "$tag" && newer "$tag" "$self_tag" && ! is_bad "$tag" || return 0
    if [ ! -x "$root/versions/$tag/diablo-splitscreen/diablo-splitscreen.sh" ]; then
        tarball="diablo-splitscreen-$tag-linux-x86_64.tar.gz"
        curl -fsL --max-time 900 "$DSS_DOWNLOAD/$tag/$tarball" -o "$tmp/$tarball" || return 0
        curl -fsL --max-time 60 "$DSS_DOWNLOAD/$tag/SHA256SUMS" -o "$tmp/SHA256SUMS" || return 0
        if ! grep -E "^[0-9a-f]{64}  $tarball\$" "$tmp/SHA256SUMS" > "$tmp/expected" \
            || ! (cd "$tmp" && sha256sum -c --quiet expected >/dev/null 2>&1); then
            fault checksum; return 0
        fi
        partial="$root/versions/.$tag.partial"
        rm -rf "$partial" "$root/versions/$tag"
        mkdir -p "$partial" && tar -xzf "$tmp/$tarball" -C "$partial" --no-same-owner 2>/dev/null \
            && [ -x "$partial/diablo-splitscreen/diablo-splitscreen.sh" ] && [ -x "$partial/diablo-splitscreen/devilutionx.AppImage" ] \
            && mv "$partial" "$root/versions/$tag" || { rm -rf "$partial"; fault unpack; return 0; }
    fi
    write_file pending "$tag"
}

do_apply() {
    local tag
    tag=$(read_file pending) || return 0
    rm -f "$root/pending"
    valid_tag "$tag" && newer "$tag" "$self_tag" && ! is_bad "$tag" || return 0
    if ! healthy "$tag"; then
        printf '%s\n' "$tag" >> "$root/bad"
        rm -rf "$root/versions/$tag"
        fault "health check $tag"
        return 0
    fi
    write_file previous "$self_tag"
    write_file trial "$tag"
    switch_to "$tag" || { rm -f "$root/trial" "$root/current.new"; return 0; }
    # Keep the new version and the one it replaces; everything else is downloadable again.
    local old name
    for old in "$root/versions"/* "$root/versions"/.*.partial; do
        [ -e "$old" ] || continue
        name=$(basename "$old")
        [ "$name" = "$tag" ] || [ "$name" = "$self_tag" ] || rm -rf "$old"
    done
    printf '%s\n' "$root/versions/$tag/diablo-splitscreen/diablo-splitscreen.sh"
}

do_finish() {
    local rc=${3:-0} seconds=${4:-0} previous
    [ "$(read_file trial)" = "$self_tag" ] || return 0
    rm -f "$root/trial"
    # Crash signals only (ILL, TRAP, ABRT, BUS, FPE, SEGV): Steam ending the game sends
    # TERM or KILL, a terminal INT, and those are no reason to go back.
    case "$rc" in 132|133|134|135|136|139) ;; *) return 0;; esac
    [ "$seconds" -lt "$DSS_TRIAL_SECONDS" ] || return 0
    previous=$(read_file previous) || return 0
    valid_tag "$previous" && [ -x "$root/versions/$previous/diablo-splitscreen/diablo-splitscreen.sh" ] || return 0
    printf '%s\n' "$self_tag" >> "$root/bad"
    switch_to "$previous" || return 0
    fault "crash on first start, rolled back"
    printf '%s\n' "$root/versions/$previous/diablo-splitscreen/diablo-splitscreen.sh"
}

managed || exit 0
case "$cmd" in
    check) do_check;;
    apply) do_apply;;
    finish) do_finish "$@";;
esac
exit 0
