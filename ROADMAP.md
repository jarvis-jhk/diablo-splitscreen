# Diablo Splitscreen roadmap

JARVIS owns delivery; the owner edits goals. All work below is pending unless checked.
Initial priority: build a verified upstream baseline and prove independent two-player
input and nonblocking personal UI on a shared screen before expanding the release machinery or doing a large refactor.

## M0 — reproducible baseline and architecture proof

- [x] Public fork preserving upstream history, dedicated lane, webhook and maintenance brief.
- [x] Investigate upstream license, controllers, TCP multiplayer and full multiplayer quests.
- [x] Survey relevant community forks/mods and existing split-screen attempts; record evidence.
- [x] Reproduce Linux Debug/tests and Windows CI builds from the pinned baseline (two upstream pagination skips per platform; see NOTES).
- [x] Diagnose/fix vendored-test sanitizer consistency and verify fresh native Linux/Windows
  Debug on that source, including actual source output and JUnit artifact hashes.
- [x] Specify the next bounded two-player ownership/reuse experiment and acceptance scenario
  (docs/TWO-PLAYER-PROOF.md; planned behavior, no implementation proof yet).
- [x] Bound the input/inventory/command/render ownership audit and PR8354 reuse decision
  (docs/OWNERSHIP-AUDIT.md; source evidence only).
- [x] Prove explicit-device button/combo polling with two virtual SDL pads, including
  both registration orders, split modifiers and executed negative control.
- [ ] Back up user data and smoke upstream solo/controller and two-client loopback co-op.
- [x] Implement the smallest viable TWO-player shared-screen proof (docs/LOCAL-COOP.md):
  guest joins with Start, own hero, walk/attack/cast/damage/XP/pickup, pad 1 unchanged.
  Frame time/memory not yet measured.
- [x] Prove exclusive controller routing and personal UI/player state: one inventory open
  while the other moves/fights/casts; both inventories open with independent cursors
  (personal guest inventories, virtual pads; physical pads still Jan's test).
- [ ] Personal menus, equipment, spell selection and merchants never globally pause or
  capture another controller; camera remains useful while a stationary player sorts items.
  Inventory/equipment/belt done for guests; spell selection panel, character sheet and
  merchants still to do.
- [x] Evaluate in-process ownership first: selected (guest commands parsed as the guest,
  IsLocalPlayer at authority sites); isolated clients stay an unused fallback.
- [ ] Wire real crash and launch failures through anonymous fault relay into GitHub issues.

## M1 — complete 1–4-player couch game

- [x] Controller-only lobby, character creation/selection, join/leave and stable pad ownership
  (2026-10-16: in-game join panel picks a saved hero not in play or a new hero of any
  playable class; Start on a guest pad leaves; seats P2-P4 stay fixed. New heroes get
  seat names, no typed name; physical pads still Jan's test).
- [x] Shared camera for one to four characters; readable personal HUD and simultaneous
  inventory/menu overlays at Deck resolution, with adequate visible gameplay area.
  (2026-10-16: camera centres on all living heroes; no hero, pad 1 included, can step
  off the screen; replaces the 7-tile guest leash. Guest panels still cover the left
  side of the view while open; physical pads/Deck still Jan's test.)
- [ ] Independent targeting, spells, belt, merchants, dialogue and death/revival; explicit
  party level-travel behavior that cannot silently discard a personal transaction.
- [ ] Stable offline local co-op using upstream protocol/world behavior; no internet dependency.
- [ ] Full Diablo quest/progression coverage and ending, difficulty progression, loot and saves.
- [ ] Controller disconnect/reconnect, held-input release, replacement and slot reuse.
  (2026-10-16: an unplugged guest stays and stands; Start on any free pad takes the seat
  back with nothing held. Pad 1 disconnect is upstream behaviour; physical pads untested.)
- [ ] Save/reload and migrations across game/update versions; protect original saves.
  (2026-10-17: guest heroes are saved to their own slots with pad 1's saves and on leave;
  new guest heroes get a free slot; slots holding any file are never reused.)
- [ ] One-player compatibility and keyboard accessibility; no regression of normal gameplay.

## M2 — usable distribution

- [ ] Fork-branded Linux portable build and Windows ZIP/installer with checksummed manifests.
- [ ] Steam Deck installer and Steam shortcut: idempotent, no edits while Steam is running.
  (2026-10-17: install.sh + double-click .desktop release assets; checksum-verified,
  versioned atomic install, previous kept, menu entry, MPQ copy offer, Steam shortcut only
  with Steam closed and a backup; scripted test against the real artifacts in CI. Physical
  Deck run still Jan's test.)
- [ ] Automatic background update check, safe versioned install at next launch, rollback on
  failed health check; never overwrite MPQs, saves or configuration. Windows parity required.
  (2026-10-20, Linux/Deck: launcher + update.sh check GitHub in the background at most
  every 6 h, stage a checksum-verified newer release, switch at next launch after an
  AppImage --version health check, roll back a crash in the first minute, skip bad
  versions; off under Warpzone/opt-out/unpacked tarballs. Windows parity open with the
  Windows installer.)
- [ ] Startup changelog once per installed version, controller-readable and dismissible.
- [ ] Nonblocking anonymous crash/failed-update reporting and local redacted diagnostics.
- [ ] Verify downloads and both platform artifacts before publishing/announcing a release.
- [ ] Warpzone (jarvis-jhk/warpzone, games/diablo-splitscreen.toml): tier 0 descriptor
  listing release assets, launch path and the DIABDAT.MPQ/spawn.mpq data entry; keep it
  in step with each release. A future own updater stays silent in a `.warpzone-managed`
  install. No chat or free-text player content.
- [ ] Report faults through Warpzone feedback (request t_mvc433sf2lckk, 2026-10-17): app_fatal/
  ErrDlg via wz_report or tier-0 POST /api/v1/feedback (game diablo-splitscreen), Linux/Windows
  crash file sent on next start; stable titles, no personal data, never blocks. Blocked until
  libwarpzone 0.1.3 is released and the server answers the endpoint (404 on 2026-10-17). Keep
  the ntfy relay until the Warpzone path is verified.

## M3 — hardware and maintenance

- [ ] Physical Deck with four controllers: Game Mode, 1280×800 readability, suspend/resume,
  hotplug and dock/undock; benchmark frame pacing, battery and memory.
- [ ] Physical Windows and desktop Linux four-controller checks; record actual hardware.
- [ ] Full-game cooperative playthrough evidence; synthetic tests alone are insufficient.
- [ ] Regular upstream maintenance and review of useful forks, documented migrations.
- [ ] Online co-op through Warpzone (DevilutionX TCP co-op over its peer channel), after
  the couch milestones (M1) are playable.
- [ ] Optional Hellfire support only after the base Diablo goal; no distraction from M1.

## M4 — macOS support

Owner goal (2026-10-19, combined roadmap item diablo-macos-support). Comes after the
milestones above; it does not reorder them.

- [ ] macOS build (Apple Silicon and Intel, or one universal .app) built in CI on a GitHub
  macOS runner, starting from upstream's macOS_arm64/macOS_x86_64 workflows (disabled in
  this fork) folded into the fork's own gates, with the Debug tests running there.
- [ ] Smoke test on the runner: the packaged .app starts, finds no MPQ and exits cleanly
  with the expected message; LocalCoopTest virtual-pad cases pass on macOS.
- [ ] Attach the macOS artifact (zipped .app or .dmg, checksummed) to fork prereleases and
  include it in download/checksum verification before an announcement.
- [ ] Player docs: macOS install/start, where to put DIABDAT.MPQ, save location. Unsigned
  is acceptable; document the Gatekeeper workaround (right-click → Open, or
  `xattr -dr com.apple.quarantine`).
- Known risks: SDL2 renders through Metal/OpenGL on macOS and the Retina/HiDPI scaling of
  the shared camera and guest panels needs checking; controllers go through SDL's
  GameController/IOKit/GCController backends, so pad ids, hotplug and the claimed-device
  routing must be re-proven there; data/save paths are under ~/Library/Application Support
  and must not move (no migration without backup); the Linux install.sh/Steam shortcut
  does not apply; the fault relay and any updater must cope with the quarantine attribute
  and an unsigned bundle; Warpzone needs a macOS launcher/libwarpzone build and a
  descriptor asset before the Warpzone milestones can include macOS. Physical Mac and
  controller checks are hardware evidence, separate from CI.
