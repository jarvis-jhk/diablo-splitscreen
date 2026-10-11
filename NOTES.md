# Current state — 2026-10-11

Project initialized; no fork release or working local splitscreen exists yet.
Upstream baseline: 37551902752724327948a44db3e917e089598b37 (master at setup).
Upstream latest published release observed: 1.5.5. Fork versions start at v0.x.y.
Full upstream master history was preserved using a GitHub fork. origin is the fork;
upstream is fetch-only. No original game assets have been added.

Already available upstream: controller navigation/play, TCP co-op, Linux/Windows ports,
portable mode and the Full quests in Multiplayer option (default false). That option
is a starting point, not proof that our eventual couch game preserves every quest.
Upstream source uses MyPlayer/MyPlayerId and global render/UI/control state; the
controller code can query input pressed on any controller. Independent local players
therefore need deliberate isolation. See docs/SPLITSCREEN.md.

Current Sustainable Use License allows free non-commercial derivatives and requires
notices; this is source-available, not a claim of an OSI-approved open-source license.
Retain LICENSE.md and upstream notices. Original Diablo data remains user-provided.

Current slice: community survey complete; Debug baseline validation underway. See
docs/COMMUNITY-SURVEY.md and docs/BASELINE.md. No game implementation changed.
PR #8354 at a010e2f0b8a951138eb628355bc4d3ab9cef6d37 is a real experimental
local-coop reference, but its source has a shared camera, exclusive panel/store owner
and coordinated level travel. It is not the independent-view/different-level solution.

Dedicated dispatch-only Fork Debug baseline CI builds the pinned3755190275 game
source on Linux/GCC14 and native Windows/MSVC, tests enabled and max2 jobs. All30
inherited fork workflows were disabled through Actions settings; definitions retained.
No release workflow or playable package has been introduced. Test fixture spawn.mpq
is ignored, downloaded from upstream assets and SHA256-checked; no full-game MPQs.

Local configuration: Debian GCC12 is too old for the C++23 library requirements.
Clang23 + matching libc++ configured Debug successfully. Vendored GoogleTest1.15.2
also needs its matching googlemock include path for standalone tests; mixing the
older system gmock headers caused a compile failure before gameplay code. Reconfigured
with that include path, build running max2. Native CI remains required regardless.
No runtime, full-data progression, physical controller or performance proof yet.
Distribution, updates/changelog and central fault reporting are release requirements.
Private JARVIS_REPORT_URL repo secret is configured; do not embed it in artifacts.
The game/installer fault relay is pending the first executable slice.

Automation: regular Tue/Thu/Sat work sessions plus monthly upstream sweep are registered
in JARVIS. Shared Sol quota dispatch was requested from self; it is not implemented by
this bootstrap. Existing in-flight Siedler CI/claims remain intact.

Sources reviewed:
- https://github.com/diasurgical/DevilutionX (README, platform support, MPQ requirements)
- https://github.com/diasurgical/DevilutionX/blob/master/LICENSE.md
- https://github.com/diasurgical/DevilutionX/wiki/Multiplayer (TCP/loopback, separate config)
- https://github.com/diasurgical/DevilutionX/wiki/Community-Mods (candidate survey entry point)
- Source/options.cpp multiplayerFullQuests; Source/controls/controller.cpp;
  Source/controls/devices/game_controller.cpp; Source/engine/render/scrollrt.cpp; Source/player.h.
Initial web search did not establish an existing ready-to-use four-player Deck splitscreen
implementation. Complete the fork/mod survey before claiming none exists.
