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
- [ ] Prove exclusive controller routing and personal UI/player state: one inventory open
  while the other moves/fights/casts (done, virtual pads); both inventories open with
  independent cursors (next: Personal inventories).
- [ ] Personal menus, equipment, spell selection and merchants never globally pause or
  capture another controller; camera remains useful while a stationary player sorts items.
- [x] Evaluate in-process ownership first: selected (guest commands parsed as the guest,
  IsLocalPlayer at authority sites); isolated clients stay an unused fallback.
- [ ] Wire real crash and launch failures through anonymous fault relay into GitHub issues.

## M1 — complete 1–4-player couch game

- [ ] Controller-only lobby, character creation/selection, join/leave and stable pad ownership.
- [ ] Shared camera for one to four characters; readable personal HUD and simultaneous
  inventory/menu overlays at Deck resolution, with adequate visible gameplay area.
- [ ] Independent targeting, spells, belt, merchants, dialogue and death/revival; explicit
  party level-travel behavior that cannot silently discard a personal transaction.
- [ ] Stable offline local co-op using upstream protocol/world behavior; no internet dependency.
- [ ] Full Diablo quest/progression coverage and ending, difficulty progression, loot and saves.
- [ ] Controller disconnect/reconnect, held-input release, replacement and slot reuse.
- [ ] Save/reload and migrations across game/update versions; protect original saves.
- [ ] One-player compatibility and keyboard accessibility; no regression of normal gameplay.

## M2 — usable distribution

- [ ] Fork-branded Linux portable build and Windows ZIP/installer with checksummed manifests.
- [ ] Steam Deck installer and Steam shortcut: idempotent, no edits while Steam is running.
- [ ] Automatic background update check, safe versioned install at next launch, rollback on
  failed health check; never overwrite MPQs, saves or configuration. Windows parity required.
- [ ] Startup changelog once per installed version, controller-readable and dismissible.
- [ ] Nonblocking anonymous crash/failed-update reporting and local redacted diagnostics.
- [ ] Verify downloads and both platform artifacts before publishing/announcing a release.
- [ ] Warpzone (jarvis-jhk/warpzone, games/diablo-splitscreen.toml): tier 0 descriptor
  listing release assets, launch path and the DIABDAT.MPQ/spawn.mpq data entry; keep it
  in step with each release. A future own updater stays silent in a `.warpzone-managed`
  install. No chat or free-text player content.

## M3 — hardware and maintenance

- [ ] Physical Deck with four controllers: Game Mode, 1280×800 readability, suspend/resume,
  hotplug and dock/undock; benchmark frame pacing, battery and memory.
- [ ] Physical Windows and desktop Linux four-controller checks; record actual hardware.
- [ ] Full-game cooperative playthrough evidence; synthetic tests alone are insufficient.
- [ ] Regular upstream maintenance and review of useful forks, documented migrations.
- [ ] Online co-op through Warpzone (DevilutionX TCP co-op over its peer channel), after
  the couch milestones (M1) are playable.
- [ ] Optional Hellfire support only after the base Diablo goal; no distraction from M1.
