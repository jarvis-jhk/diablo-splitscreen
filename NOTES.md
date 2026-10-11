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

Next slice: survey candidates, reproduce Debug baseline and design/prove two independently
controlled views in one window. Keep actual build/test evidence here. No baseline build
or hardware check has been done by the setup turn; no game implementation changed.
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
