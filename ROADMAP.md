# Diablo Splitscreen roadmap

JARVIS owns delivery; the owner edits goals. All work below is pending unless checked.
Initial priority: build a verified upstream baseline and prove independent two-player
input/rendering before expanding the release machinery or doing a large refactor.

## M0 — reproducible baseline and architecture proof

- [x] Public fork preserving upstream history, dedicated lane, webhook and maintenance brief.
- [x] Investigate upstream license, controllers, TCP multiplayer and full multiplayer quests.
- [ ] Survey relevant community forks/mods and existing split-screen attempts; record evidence.
- [ ] Reproduce Linux Debug/tests and Windows CI builds from the pinned baseline.
- [ ] Back up user data and smoke upstream solo/controller and two-client loopback co-op.
- [ ] Compare in-process contexts with isolated client workers + one compositor; measure
  Deck memory/CPU and implement the smallest viable TWO-player single-window proof.
- [ ] Prove exclusive controller routing and independent camera/UI/player state, with
  inventory open for one player while the other moves and casts; different dungeon levels.
- [ ] Wire real crash and launch failures through anonymous fault relay into GitHub issues.

## M1 — complete 1–4-player couch game

- [ ] Controller-only lobby, character creation/selection, join/leave and stable pad ownership.
- [ ] Adaptive one/two/three/four view layouts; independent HUD, automap, menus and inventory.
- [ ] Independent targeting, spells, belt, merchants, dialogue, death/revival and level travel.
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
