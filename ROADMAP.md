# Diablo Splitscreen roadmap

JARVIS owns delivery; the owner edits goals. All work below is pending unless checked.
Initial priority: build a verified upstream baseline and prove independent two-player
input and nonblocking personal UI on a shared screen before expanding the release machinery or doing a large refactor.

## M0 — reproducible baseline and architecture proof

- [x] Public fork preserving upstream history, dedicated lane, webhook and maintenance brief.
- [x] Investigate upstream license, controllers, TCP multiplayer and full multiplayer quests.
- [x] Survey relevant community forks/mods and existing split-screen attempts; record evidence.
- [x] Reproduce Linux Debug/tests and Windows CI builds from the pinned baseline (two upstream pagination skips per platform; see NOTES).
- [ ] Back up user data and smoke upstream solo/controller and two-client loopback co-op.
- [ ] Audit PR8354 shared-camera reuse against the pinned baseline; implement the smallest
  viable TWO-player shared-screen proof with measured frame time and memory.
- [ ] Prove exclusive controller routing and personal UI/player state: one inventory open
  while the other moves/fights/casts; both inventories open with independent cursors.
- [ ] Personal menus, equipment, spell selection and merchants never globally pause or
  capture another controller; camera remains useful while a stationary player sorts items.
- [ ] Evaluate in-process ownership first; retain isolated clients/compositor as an unselected
  fallback if measured compatibility/performance evidence requires it.
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

## M3 — hardware and maintenance

- [ ] Physical Deck with four controllers: Game Mode, 1280×800 readability, suspend/resume,
  hotplug and dock/undock; benchmark frame pacing, battery and memory.
- [ ] Physical Windows and desktop Linux four-controller checks; record actual hardware.
- [ ] Full-game cooperative playthrough evidence; synthetic tests alone are insufficient.
- [ ] Regular upstream maintenance and review of useful forks, documented migrations.
- [ ] Optional Hellfire support only after the base Diablo goal; no distraction from M1.
