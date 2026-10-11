# Community survey — 2026-10-11

Scope: upstream's community index, its linked local-coop proposal and issue, known
Diablo gameplay mods, alternative engines, and Windows multi-instance tools. This
is a bounded survey, not proof that no other implementation exists. No candidate
code has been merged, and no candidate was executed in this slice.

## Directly relevant: Yuri Pourre's local co-op

The [upstream Community Mods index](https://github.com/diasurgical/DevilutionX/wiki/Community-Mods)
links [PR #8354](https://github.com/diasurgical/DevilutionX/pull/8354), backed by
[issue #1237](https://github.com/diasurgical/DevilutionX/issues/1237). The live API
on the survey date reports open, non-draft, eight commits, 58 changed files,
8,607 additions and 978 deletions. Last update: 2026-09-24. Audited head:
`a010e2f0b8a951138eb628355bc4d3ab9cef6d37`. Fetch the PR ref read-only; it is
not a tested part of this fork.

The commit descriptions mention splitting a viewport, but the actual source uses
a **shared camera**, not independently rendered local clients:

- `Source/controls/local_coop/local_coop.hpp`: one camera state and one
  `panelOwnerPlayerId`/`storeOwnerPlayerId` in `LocalCoopState`;
  `CalculateLocalCoopViewPosition` is documented as the players' average position.
- `local_coop.cpp::IsTilePositionOnScreen` rejects movement too far from other
  living players on player zero's level. `UpdateLocalCoopCamera` smooths one
  common camera. `LocalCoopPlayerContext` swaps only `MyPlayer`, `MyPlayerId`
  and `InspectPlayer`; it does not isolate level, visibility, render or UI state.
- `Source/engine/render/scrollrt.cpp` adds `GetLocalCoopCameraOffset` to the
  ordinary world draw and draws per-player HUDs around that world.
- `Source/levels/trigs.cpp::CheckTriggers` explicitly processes a trigger from
  any local player through `StartNewLvl(myPlayer, ...)`, bringing everyone along.

These observations are a source audit, not runtime confirmation. They disqualify
a wholesale merge as our independent-camera/different-level solution. Useful
ideas to inspect selectively later: device ownership, hero enumeration,
per-player HUD proportions, and player-id-aware loopback messages. Any reuse
needs attribution, baseline compatibility, and focused regressions first.

The author's PR reports experimental saves, targeting limitations and camera
jitter. September feedback reports skill assignment problems with three players.
Treat those as reports to reproduce rather than verified defects in our fork.

## Other indexed mods

The index labels [Infernity](https://github.com/qndel/Infernity) a Windows
Devilution mod and [Apocrypha](https://github.com/SoundChaser83/devilutionx-apocrypha)
a Windows balance patch based on 1.3.0. Their project descriptions support those
roles. [Middle Earth](https://github.com/DakkJaniels/DevilutionX-MiddleEarthMod)
recreates a gameplay conversion. [DiabloX](https://github.com/ikonomov/DiabloX)
is indexed for challenge/rebalancing. [Diablo Core](https://github.com/julealgon/DiabloCore)
explicitly describes a purely single-player focus. The index also lists automap,
experience-sharing, and controller/UI-related extension patches.

None of these indexed descriptions establishes independent local cameras or
simultaneous levels. They are optional later gameplay/UI references, not a reason
to change our vanilla complete-Diablo goal or combine old branches blindly.

## Alternative engines and multi-instance tools

[Freeablo](https://github.com/wheybags/freeablo) describes itself as an archived
Diablo engine reimplementation (the API archive flag differs from that title).
Its README does not establish complete controller-based couch co-op. Replacing
the engine would reopen content parity instead of reusing upstream progression.

[Nucleus Co-op](https://www.splitscreen.me/) describes a Windows tool that
launches multiple game instances. Its [handler documentation](https://www.splitscreen.me/docs/create-handlers/)
uses window positioning and XInput hooks. No reviewed source established a
DevilutionX handler meeting Linux/Deck single-window requirements. Its general
device-isolation idea is relevant to the isolated-client experiment; desktop
window tiling and Windows injection are not the delivered architecture.

## Result and next experiment

Preserve the upstream engine and multiplayer protocol. Compare a genuinely
isolated in-process client context against isolated engine workers with one
compositor. The PR is evidence that a small player-pointer swap is insufficient,
not evidence that full contexts are impossible. Neither architecture is selected
by this survey; baseline tests and a real two-client frame/input experiment come
first. Different-level travel and independent menus are early gates, not polish.
