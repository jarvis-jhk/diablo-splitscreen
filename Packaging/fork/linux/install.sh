#!/bin/bash
# Installs Diablo Splitscreen for this user on Linux and Steam Deck (desktop mode).
#
# The release tarball of the SAME tag is downloaded, checked against the release's
# SHA256SUMS, unpacked into versions/<tag> and switched to with one atomic rename of
# the "current" symlink, so a failed or interrupted install leaves the previous
# version playing. The previous version is kept for rollback, older ones are removed.
# Game data (MPQs), saves and settings live elsewhere and are never overwritten.
#
# Non-interactive use (tests, scripting): DSS_YES=1 answers every question with yes
# and prints instead of showing dialogs; DSS_MPQ=<file> is the MPQ to offer.
# DEVILUTIONX_NO_REPORT=1 turns off the anonymous fault report.
set -euo pipefail
DSS_TAG="@TAG@"
DSS_RELEASE="${DSS_RELEASE_URL:-https://github.com/jarvis-jhk/diablo-splitscreen/releases/download/$DSS_TAG}"
DSS_DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
DSS_ROOT="$DSS_DATA/diablo-splitscreen"
# Where the game looks for diabdat.mpq (SDL pref path, unchanged from upstream).
DSS_PREF="$DSS_DATA/diasurgical/devilution"
DSS_NAME="Diablo Splitscreen"

dss_german() {
    local v
    for v in "$(printf %s "${LANGUAGE:-}" | cut -d: -f1)" "${LC_ALL:-}" "${LC_MESSAGES:-}" "${LANG:-}"; do
        case "$v" in de|de_*|de.*|de@*) return 0;; ""|C|C.*|POSIX) ;; *) return 1;; esac
    done
    return 1
}
if dss_german; then DSS_DE=1; else DSS_DE=0; fi
t() { if [ "$DSS_DE" = 1 ]; then printf '%s' "$2"; else printf '%s' "$1"; fi; }

dialog() {
    local kind=$1 text=$2
    if [ "${DSS_YES:-0}" = 1 ]; then
        printf '[%s] %s\n' "$kind" "$text"
        return 0
    fi
    if command -v zenity >/dev/null; then
        if [ "$kind" = question ]; then
            zenity --question --no-wrap --text="$text" --ok-label="$(t Yes Ja)" --cancel-label="$(t No Nein)"
        else zenity --"$kind" --no-wrap --text="$text"; fi
    elif command -v kdialog >/dev/null; then
        case "$kind" in question) kdialog --yesno "$text";; error) kdialog --error "$text";; *) kdialog --msgbox "$text";; esac
    elif [ "$kind" = question ]; then
        local answer
        read -rp "$text $(t '[y/N]' '[j/N]') " answer && [[ "$answer" == [yYjJ]* ]]
    else printf '%s\n' "$text"; fi
}

pick_file() {
    if [ "${DSS_YES:-0}" = 1 ]; then
        [ -n "${DSS_MPQ:-}" ] && printf '%s\n' "$DSS_MPQ"
        return 0
    fi
    if command -v zenity >/dev/null; then
        zenity --file-selection --title="$1" || true
    elif command -v kdialog >/dev/null; then
        kdialog --getopenfilename "$HOME" '*.mpq *.MPQ' --title "$1" || true
    else
        local answer
        read -rp "$1: " answer || true
        printf '%s\n' "$answer"
    fi
}

fault() {
    # Only a fixed stage name leaves the device: no paths, no output, no identifiers.
    if [ "${DEVILUTIONX_NO_REPORT:-0}" != 1 ]; then
        (curl -fsS --max-time 8 -o /dev/null -d "Diablo Splitscreen $DSS_TAG Linux installer failed: $1" \
            https://ntfy.sh/diablo-splitscreen-faults-747d46c7c1c4de46 >/dev/null 2>&1 &)
    fi
    dialog error "$(t "Installation failed ($1). The previous installation, saves and game data are unchanged." \
        "Die Installation ist fehlgeschlagen ($1). Vorherige Installation, Spielstände und Spieldaten sind unverändert.")" || true
    exit 1
}

for tool in curl tar sha256sum python3; do
    command -v "$tool" >/dev/null || { dialog error "$(t "Please install $tool first." "Bitte installiere zuerst $tool.")"; exit 1; }
done
dialog question "$(t "Install $DSS_NAME $DSS_TAG? Diablo for up to four controllers on one screen. You need your own DIABDAT.MPQ (Diablo CD or GOG)." \
    "$DSS_NAME $DSS_TAG installieren? Diablo für bis zu vier Controller auf einem Bildschirm. Du brauchst deine eigene DIABDAT.MPQ (Diablo-CD oder GOG).")" || exit 0

DSS_TMP=$(mktemp -d)
trap 'rm -rf "$DSS_TMP"' EXIT
tarball="diablo-splitscreen-$DSS_TAG-linux-x86_64.tar.gz"
curl -fsSL --max-time 600 "$DSS_RELEASE/$tarball" -o "$DSS_TMP/$tarball" || fault download
curl -fsSL --max-time 60 "$DSS_RELEASE/SHA256SUMS" -o "$DSS_TMP/SHA256SUMS" || fault download
grep -E "^[0-9a-f]{64}  $tarball\$" "$DSS_TMP/SHA256SUMS" > "$DSS_TMP/expected" || fault checksum
(cd "$DSS_TMP" && sha256sum -c --quiet expected) || fault checksum

mkdir -p "$DSS_ROOT/versions" || fault unpack
partial="$DSS_ROOT/versions/.$DSS_TAG.partial"
rm -rf "$partial"
mkdir "$partial" || fault unpack
tar -xzf "$DSS_TMP/$tarball" -C "$partial" --no-same-owner || fault unpack
for f in diablo-splitscreen.sh devilutionx.AppImage; do
    [ -x "$partial/diablo-splitscreen/$f" ] || fault unpack
done
# Reinstalling the same tag: put the old copy aside first so "current" never points at
# a half-removed directory.
target="$DSS_ROOT/versions/$DSS_TAG"
if [ -e "$target" ]; then
    rm -rf "$target.old"
    mv "$target" "$target.old" || fault switch
fi
mv "$partial" "$target" || fault switch
previous=""
if [ -L "$DSS_ROOT/current" ]; then
    previous=$(basename "$(readlink "$DSS_ROOT/current")")
fi
ln -sfn "versions/$DSS_TAG" "$DSS_ROOT/current.new" || fault switch
mv -T "$DSS_ROOT/current.new" "$DSS_ROOT/current" || fault switch
# Keep this version and the one before it; everything else is downloadable again.
for old in "$DSS_ROOT/versions"/* "$DSS_ROOT/versions"/.*.partial; do
    [ -e "$old" ] || continue
    name=$(basename "$old")
    if [ "$name" != "$DSS_TAG" ] && [ "$name" != "$previous" ]; then rm -rf "$old"; fi
done

game="$DSS_ROOT/current/diablo-splitscreen"
icon="$game/icon.png"
mkdir -p "$DSS_DATA/applications"
cat > "$DSS_DATA/applications/diablo-splitscreen.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=$DSS_NAME
Comment=Diablo for up to four controllers on one screen
Comment[de]=Diablo für bis zu vier Controller auf einem Bildschirm
Exec="$game/diablo-splitscreen.sh"
Icon=$icon
Terminal=false
Categories=Game;RolePlaying;
DESKTOP

# Game data: offer to copy the player's own MPQ to where the game looks for it, under
# the lower-case name it expects. An existing file is never replaced.
if [ ! -e "$DSS_PREF/diabdat.mpq" ] && [ ! -e "$DSS_PREF/spawn.mpq" ]; then
    if dialog question "$(t "The game needs DIABDAT.MPQ from your Diablo CD or GOG folder (SPAWN.MPQ for the shareware). Choose it now? It is copied to $DSS_PREF." \
        "Das Spiel braucht DIABDAT.MPQ von deiner Diablo-CD oder aus dem GOG-Ordner (SPAWN.MPQ für die Shareware). Jetzt auswählen? Sie wird nach $DSS_PREF kopiert.")"; then
        source_mpq=$(pick_file "$(t 'Choose DIABDAT.MPQ' 'DIABDAT.MPQ auswählen')")
        if [ -n "$source_mpq" ]; then
            case "$(basename "$source_mpq" | tr '[:upper:]' '[:lower:]')" in
                diabdat.mpq) mpq=diabdat.mpq;; spawn.mpq) mpq=spawn.mpq;; *) mpq="";;
            esac
            if [ -z "$mpq" ] || [ ! -f "$source_mpq" ]; then
                dialog error "$(t 'That is not DIABDAT.MPQ or SPAWN.MPQ. Copy it by hand later.' 'Das ist weder DIABDAT.MPQ noch SPAWN.MPQ. Kopiere sie später von Hand.')" || true
            else
                mkdir -p "$DSS_PREF"
                cp -- "$source_mpq" "$DSS_PREF/.$mpq.part" && mv -n -- "$DSS_PREF/.$mpq.part" "$DSS_PREF/$mpq" \
                    || dialog error "$(t 'Copying the MPQ failed. Copy it by hand later.' 'Das Kopieren der MPQ ist fehlgeschlagen. Kopiere sie später von Hand.')" || true
                rm -f -- "$DSS_PREF/.$mpq.part"
            fi
        fi
    fi
fi

# Steam (Deck Game Mode): only when there is a Steam user, and only with Steam closed.
steam_tool="$game/steam-shortcut.py"
steam_state=$(python3 "$steam_tool" status "$game/diablo-splitscreen.sh" 2>/dev/null) || steam_state=error
steam_done=0
case "$steam_state" in
    present) steam_done=1;;
    missing|error)
        if pgrep -x steam >/dev/null; then
            dialog question "$(t 'Installed. To add it to Steam, fully exit Steam now, then choose Yes. No skips it.' \
                'Installiert. Für die Steam-Verknüpfung beende Steam jetzt vollständig und wähle dann Ja. Nein überspringt das.')" || steam_state=skip
        fi
        if [ "$steam_state" != skip ] && python3 "$steam_tool" add "$DSS_NAME" "$game/diablo-splitscreen.sh" "$game" "$icon"; then
            steam_done=1
        else
            dialog info "$(t 'The Steam shortcut was not added (Steam still running?). Run this installer again with Steam closed, or add the game to Steam by hand.' \
                'Die Steam-Verknüpfung wurde nicht angelegt (läuft Steam noch?). Starte den Installer bei geschlossenem Steam erneut oder füge das Spiel von Hand zu Steam hinzu.')" || true
        fi;;
esac

if [ -e "$DSS_PREF/diabdat.mpq" ] || [ -e "$DSS_PREF/spawn.mpq" ]; then data_note=""; else
    data_note=$(t " Before playing, put your DIABDAT.MPQ (lower case: diabdat.mpq) into $DSS_PREF." \
        " Lege vor dem Spielen deine DIABDAT.MPQ (kleingeschrieben: diabdat.mpq) nach $DSS_PREF.")
fi
if [ "$steam_done" = 1 ]; then steam_note=$(t ' It is in Steam too; restart Steam to see it.' ' Es ist auch in Steam; starte Steam neu, um es zu sehen.'); else steam_note=""; fi
dialog info "$(t "$DSS_NAME $DSS_TAG is installed in your applications menu." "$DSS_NAME $DSS_TAG ist im Anwendungsmenü installiert.")$steam_note$data_note" || true
# END OF DIABLO SPLITSCREEN INSTALLER
