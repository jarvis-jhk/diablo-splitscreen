# Shared camera — 2026-10-16

Roadmap item diablo-shared-camera (branch `shared-camera`). With guests in play
`UpdateSharedCamera()` (called in DrawAndBlit before DrawView) centres the view on the
bounding-box middle of every living hero on the level, drawn positions incl. walking
offsets: ViewPosition = nearest tile, `SharedCameraOffset()` the pixel remainder,
used by CalcFirstTilePosition and the mouse (AlterMousePositionViaPlayer) in place of
MyPlayer's walking offset. While active the renderer starts one tile up/left and draws
5 more rows / 3 more columns, so a remainder of up to a tile never shows an edge.
`SharedCameraAllowsStep` in StartWalk (every local hero, pad 1 included) and in
WalkGuest refuses a step that grows the party box past the screen room (window less
2x64 px across, less 2x max(100, panel+16) px tall, /2 when zoomed); shrinking is
always allowed. A refused step stands the hero and clears its path. Off while
gbProcessPlayers is false (Diablo's death scene steers ViewPosition) and without guests
(upstream path untouched). Headless tests now set a 640x480 screen geometry.
Evidence: 3 new LocalCoopTest cases (20 total; the old 7-tile leash case replaced):
party centre on screen centre with pad 1 off centre; guest NE then pad 1 SW in town
until the screen (not a wall) stops them, both in view, then closing up works; no
guests = upstream camera. Local Clang23 Debug ASan/UBSan: all 20 pass. Negative
control (IsSharedCameraActive false): both camera cases fail, source restored.
Render 1280x800: JARVIS data/diablo/shared-camera-render/ (spread party, no edge gaps).
Native gate run 38949357469 on f6d756a1d: Linux 643 / Windows 617, 0 failures, the 2 known
pagination skips, all 20 LocalCoopTest cases on both; logged HEAD and artifact digests
verified (JARVIS data/diablo/camera-native/). Release notes docs/release-notes/v0.1.3.md.
Limits: guest inventory panels still cover the left of the view; no zoom-out when
the party spreads.

# Controller lobby — 2026-10-16

Roadmap item diablo-controller-lobby (branch `controller-lobby`, source aabaf327c).
Start on a free pad no longer joins at once: it claims the pad and opens a join panel
(`Joiner` in localcoop.cpp, drawn under the guest HUD lines) listing saved heroes not in
play (pad 1's gSaveNumber and other guests' saves left out; `pfile_read_hero_summary`
reads only the hero pack), then "New <class>" for every class the hero menu would allow
(spawn: Warrior only). Stick/D-pad choose (wraps), A joins, B/Back cancels and releases
the pad; panel buttons act on release so nothing is held when the pad changes hands.
A save picked by another pad meanwhile reopens the list. Start on a guest pad asks
"leave the game?" in its HUD line (A leaves, B/Start stays; hero stands meanwhile).
Guests carry a fixed `seat` (P2..P4): HUD label, panel position and new-hero name.
`JoinLocalGuest(device)` stays as a direct join with the first choice (tests/tools).
Evidence: 4 new LocalCoopTest cases (18 total), local Clang23 Debug ASan/UBSan all pass;
fixture writes a multiplayer save "Saved" in slot 1 (a single-player save is invisible
to an offline MP game: multi_N.sv vs single_N.sv). Render: JARVIS
data/diablo/lobby-render/ (join panel in town, leave question in the cathedral).
Native gate run 38921691125 on aabaf327c: Linux 641 / Windows 615, 0 failures, the 2 known
pagination skips, all 18 LocalCoopTest cases on both; logged HEAD and artifact digests
verified (JARVIS data/diablo/lobby-native/). Merged b1c7087f5 (merge adds Markdown only).
Prerelease v0.1.2 published from b1c7087f5 (fork-prerelease run 38924395571):
https://github.com/jarvis-jhk/diablo-splitscreen/releases/tag/v0.1.2 — assets downloaded,
GitHub digests and SHA256SUMS match, packaged LOCAL-COOP.md has the join panel, no game
data (JARVIS data/diablo/release-v0.1.2/). Next version > v0.1.2.
Release notes docs/release-notes/v0.1.2.md (also carries the Warpzone data change).
Open human test: real pads and the Deck. Not done: typed names for new heroes.

# Warpzone game data — 2026-10-16

The Warpzone launcher (>= 0.1.2) passes the DIABDAT/spawn source it found as
`WARPZONE_DATA_DIABDAT=<file or directory>` on every start. `GetMPQSearchPaths()` now adds
that directory (via `LauncherDataSearchPath`, unit-tested in assets_test) after
base/pref/config and before the system/GOG/PWD paths, so the found files are used in place,
no copy, no prompt. Descriptor `[[data]] diabdat` in jarvis-jhk/warpzone left unchanged for
now: files/present match, and `copy_to` (Linux pref path) is still what makes v0.1.0/v0.1.1
find the data. Once a release with this change is current, drop `copy_to`/`present` by PR
(on Windows `~/.local/share/...` is a pointless 500 MB copy). No `known` fingerprints: no
DIABDAT here to hash, and a list with only spawn.mpq would flag every real DIABDAT as
"unknown version"; the game's own loader validates.
Native gate run 38915851244 on 7e4903944: Linux 637 / Windows 611, 0 failures, the 2 known
pagination skips, both new LauncherDataSearchPathTest cases pass; logged HEAD checked (JARVIS
data/diablo/warpzone-data/). Ships with the next prerelease.

# Current state — 2026-10-16 (personal inventories)

**Personal guest inventories** (roadmap item diablo-personal-inventories; branch
`personal-inventories`, source e8ee04e29). Back on a guest pad opens that guest's own
panel on the left (seat k at x = k*320, pad 1's stays on the right); any number open
at once, nothing pauses. Guest state only: `Guest::inventoryOpen/slot`, items moved
directly on the guest's Player (AutoEquip/AutoPlaceItemInInventory/RemoveInvItem,
which send nothing for a non-MyPlayer hero); drop is a CMD_PUTITEM parsed as the
guest. A wears with swap (snapshot/restore if the pack has no room), takes off, or
uses potions/elixirs/books/aimable scrolls (scroll = cast with SpellType::Scroll and
spellFrom); Y pack<->belt; X drop; B/Back close. Upstream change: DrawInv split into
`DrawInvPanel(out, player, origin, highlight)`; InvRect exported for the test DLL.
Evidence: 3 new LocalCoopTest cases (14 total) with virtual pads — both inventories
open with independent cursors while a monster hits the guest; pad 1 walks while the
guest's stick steers its cursor; wear/swap, unequip, belt potion, belt<->pack, drop and
pick-up again, pad 1's hero untouched. Local Clang23 ASan/UBSan: localcoop 14, polling 8,
hotplug 6 pass. Negative control (guest Back toggle removed): the 3 new cases fail,
source restored byte-identical. Render (Xvfb 1280x800): JARVIS
data/diablo/personal-inv-render/ shows both panels side by side. Native gate run
38908816680 on e8ee04e29: Linux 635 / Windows 609 cases, 0 failures, the 2 known
pagination skips, all 14 LocalCoopTest cases on both; logged HEAD and artifact digests
verified (JARVIS data/diablo/personal-inv-native/). Merge adds only Markdown/release notes.
Limits: no gold split, no cursor scrolls/oils, pad 1's class art behind guest panels,
mouse ignores guest panels, no character sheet/spellbook/shop for guests yet.
Prerelease v0.1.1 published from merge 2ea8370f5 (fork-prerelease.yml):
https://github.com/jarvis-jhk/diablo-splitscreen/releases/tag/v0.1.1 — assets downloaded,
GitHub digests and SHA256SUMS match, packaged LOCAL-COOP.md has the inventory section, no
game data. Warpzone descriptor is pattern-based (PR6 still open), no change needed.
Open human test: real pads and the Deck (both inventories open on the couch).
Next: guest spell selection/character sheet/merchants (ROADMAP M0 personal menus).

# Current state — 2026-10-16

**Two-player proof merged** (PR2, merge 912ba8252; see docs/LOCAL-COOP.md). Native gate
on source e288ca847: run 38893868323, Linux 632 / Windows 606 cases, 0 failures, the 2
known pagination skips, all 11 LocalCoopTest cases on both, logged HEAD and artifact
digests verified (JARVIS data/diablo/localcoop-native/). The merge differs from the gated
commit only in the prerelease workflow, ROADMAP and release notes. Windows needed
DVL_API_FOR_TEST on every global the test touches (MSVC links tests against the DLL),
including ones reached through inline methods (MonstersData, setlvlnum, LevelMonsterTypes).
In an offline multiplayer game the first pad pressed is pad 1; Start on another pad
joins a guest hero (next saved hero, read only, else a fresh level-1 hero) beside pad 1.
`Source/localcoop.{hpp,cpp}`: the guest's device is claimed (controller.cpp: any-device
queries and stick motion skip it), its commands are queued and parsed with the guest as
sender (ParseCmd, never MyPlayer's loopback), and `IsLocalPlayer` replaces `== MyPlayer`
at the authority sites only: damage taken (monster melee, missiles), hits/kills on
monsters, experience split, light, death without remote item wipe, auto-pickup
(OnRequestAutoGetItem gives a local guest the item instead of deleting it). UI,
cursor, sounds and network sends stay MyPlayer's. Guests follow pad 1 on every level
load, fallen guests revive there, a 7-tile leash keeps them on pad 1's camera.

Evidence: test/localcoop_test.cpp, 11 cases, boots a real offline game on spawn.mpq and
drives two SDL2 virtual pads through FetchMessage -> GameEventHandler -> GameLogic:
join, pad-1 menu unchanged, each pad moves only its hero, leash, held guest buttons and
triggers never reach pad 1, guest kills a monster and gains XP while pad 1's inventory
stays open, monsters hurt only the guest, guest Firebolt uses guest mana, pickup into
the guest's own inventory, level follow, unplug removes only the guest. Local Clang23
Debug ASan/UBSan: all 11 pass; polling 8 and hotplug 6 still pass. Negative controls,
source restored byte-identical after each: claims disabled -> 3 isolation cases fail;
IsLocalPlayer reduced to MyPlayer -> XP, damage and pickup cases fail. The same suite
with LOCALCOOP_RENDER_DIR runs through the real renderer (Xvfb 1280x800) and saved
screenshots (JARVIS data/diablo/localcoop-render/): both heroes in the cathedral with pad
1's inventory open next to the guest's fight, guest HUD, Firebolt in flight.
Virtual pads only: physical controllers and the Deck remain Jan's test.

Found on the way: CreatePlayer loads and frees cursor sprites itself, so a fresh guest
hero mid-game must free/reload the cursor around it (IsCursorLoaded). Upstream Start
alone is pad 1's menu-navigator modifier; the game menu is Back+Start.

Known limits (docs/LOCAL-COOP.md): guests are not saved, have no panels/stats/shops,
no stairs/portals of their own, camera follows pad 1, offline games only.
Fault relay (brief: first executable slice): Source/fault_report.cpp posts
"Diablo Splitscreen <version> <Linux|Windows> fatal error" from app_fatal through a
detached curl to public ntfy topic diablo-splitscreen-faults-747d46c7c1c4de46; only in
builds configured with DEVILUTIONX_FAULT_REPORTS=ON (the prerelease workflow), opt-out
DEVILUTIONX_NO_REPORT=1. The Linux launcher reports crash signals after exit. Windows
SEH crashes are not reported yet. .github/workflows/fault-reports.yml turns posts into
fault-report issues and forwards new ones via JARVIS_REPORT_URL; *-test/dev builds are
skipped. Only app_fatal/ErrDlg/asserts report; missing MPQs, read-only folders and mod data errors
do not (player setup, not a fault). Verified locally: a report reached the topic, and
after narrowing, a missing-data start posts nothing. Guest pickups no
longer close pad 1's gold dialog.
Release path: .github/workflows/fork-prerelease.yml (exact revision, Linux tar.xz +
Windows zip, SHA256SUMS verified, GitHub prerelease with docs/release-notes/v<version>.md).
Prerelease v0.1.0 published from 912ba8252 (run 38895860946):
https://github.com/jarvis-jhk/diablo-splitscreen/releases/tag/v0.1.0 — every asset downloaded,
GitHub digests and SHA256SUMS match, contents checked, no game data. Linux needs glibc 2.38+.
Warpzone descriptor fixed for this release: jarvis-jhk/warpzone PR6. Open human test: real
pads and the Steam Deck (two controllers, guest join, inventory while the other fights).
Next roadmap item: Personal inventories.

# State — 2026-10-12

Authoritative direction: shared screen for up to four characters, with simultaneous
personal inventories/menus that never block other players or pause world simulation.
Jan's clarification replaces the initial independent-camera/different-level gates.

Active bounded slice: `fix/controller-hotplug-handles`, SDL handle cleanup for the
proof's disconnect/reconnect prerequisite. All six real SDL2 regression cases and
the complete local Debug ASan/UBSan suite pass (613 cases,611 passed,2 unchanged
upstream skips). Native Linux/Windows verification is pending; keep the branch in
a draft PR until its exact-head source logs and test artifacts are verified. The
PR and private `data/diablo/controller-hotplug.json` carry the source/run IDs.
No new CI continuation is scheduled by this quota tick; do not dispatch twice or
start another implementation while this gate is pending. Baseline results below
are historical validation and do not certify this new source.

The baseline gate is complete for sanitizer-config source
f53af5e5dd702cb5300d718947db6f7dded08d13. Native run38189532088 and both
Linux/Windows jobs completed successfully. Actual source outputs and downloaded
JUnit artifacts are verified: Linux584 (582 passed), Windows558 (556 passed),
zero failures and the same two upstream pagination skips per platform.

The local Clang23/libc++ mixed-instrumentation issue is diagnosed and resolved:
matching configured sanitizer flags on the four vendored GoogleTest/Mock targets
passes focused positive/negative controls and normal full ASan/UBSan CTest584
(582 passed,2 skips). Original failed43-exit evidence remains preserved.
No suppression or gameplay workaround. All original/portability native results
remain separate historical evidence; none was carried over to the current gate.

There is no pending baseline CI continuation. Next bounded experiment is specified
in docs/TWO-PLAYER-PROOF.md: audit selective PR8354 reuse, then exercise real
in-process two-character input and simultaneous inventories in one shared world.
This is a plan, not selected/proven architecture or implemented co-op. That
baseline checkpoint was completed before the hotplug prerequisite below.
No playable fork release, full-game, physical controller/Deck or performance proof.

## 2026-10-12 — monthly upstream maintenance (merged b0a3b7bab)

Fetched upstream read-only. Five commits since baseline 3755190275 (all Gleb
Mazovetskiy, FixedPoint series): c2080753e FixedPoint type, 92927ca91 player/item/
monster HP and mana to Fixed10_6/Fixed26_6, c361e764c integer promotion, 7fdf19da3
int comparison, b0a3b7bab AnimationInfo. Reviewed as a type-safety refactor: the
writehero/pack/net-pack tests keep identical raw values, so hero/save/network formats
are unchanged. Merge-tree was clean; no commits excluded. Merged as f3b99f0ce on
master (no-ff). It touches player.h/HP/mana, which the two-player proof will edit;
merging now keeps that later diff small.

Evidence on exact head f3b99f0ce: local Clang23/libc++ ASan/UBSan Debug607 cases
(605 passed,0 failures,2 upstream pagination skips; +23 new FixedPoint cases). Native
run38207744868: Linux607 and Windows581 cases, 0 failures, the same 2 skips; job logs
show git rev-parse f3b99f0ce; artifact zips match the API sha256 digests. Attempt1
Windows failed only because gtest discovery for crawl_test.exe timed out after 5s
at link time (an unrelated test, no compiler error); one rerun of that job passed. This is
a runner flake, not a source regression; if it recurs, raise DISCOVERY_TIMEOUT
in the fork workflow rather than retrying.

Community: PR8354 still open at a010e2f0b8 (last update 2026-09-24); no new split-screen
fork found by GitHub search since 2026-10-01; latest upstream release still 1.5.5.
No release; next work remains docs/TWO-PLAYER-PROOF.md.

## Historical setup and baseline observations (superseded where noted)

Project initialized; no fork release or working local shared-screen co-op exists yet.
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
docs/COMMUNITY-SURVEY.md and docs/BASELINE.md. Gameplay behavior is unchanged;
the two direct-include portability fixes below are the only game-source deltas.
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

Original native Debug run passed on both platforms; source SHA confirmed in logs
and both nonempty JUnit artifacts downloaded/verified. Linux584 cases,0 failures,
2 skipped; Windows558 cases,0 failures,2 skipped. The skips are upstream
VisualStoreTest.Pagination_NextAndPrevious and Pagination_ResetsHighlight on both
platforms; those cases are not claimed executed/passed. Run:
https://github.com/jarvis-jhk/diablo-splitscreen/actions/runs/38167962885
Workflow revision328738945; actual game source3755190275. These are different
identities because the workflow explicitly checks out the requested source commit.
The original source failed local Clang23/libc++ compilation: `textdat.h` declares
`std::vector<Speech>` without including `<vector>`. Direct include fixad72f2915
preserves CRLF and passes the affected `libdevilutionx_textdat` Debug compile.
The subsequent local compile also found missing `<cstdlib>` for `std::getenv` in
`Source/platform/locale.cpp`. Fixdf52384a2 passes that affected Debug object compile.
The full local game/all56 CTest build completed with BOTH includes. The vector-only
native run38169032303 is superseded; cancellation was accepted to avoid testing a
source that no longer contains the whole fix. Historical run:
https://github.com/jarvis-jhk/diablo-splitscreen/actions/runs/38169032303
That canceled run uses source/workflowad72f2915. Final-source native Debug CI is
already dispatched using source/workflowdf52384a2:
https://github.com/jarvis-jhk/diablo-splitscreen/actions/runs/38170302962
Do not carry over the original or vector-only checks as final-source evidence.

Clang23 also rejects upstream benchmark1.8.5's `__COUNTER__` under its independent
`-pedantic-errors` flags. Disabling its Werror option did not resolve that diagnostic.
The local build explicitly targets the game and all56 test executables, excluding
standalone performance benchmarks; no game warning flags or dependency source were
changed. Both native CI jobs still build all default targets. YAML parsing,
actionlint1.7.12 and the actual
agent TypeScript check passed before the workflow and include commits.

Local Clang23/libc++ Debug ASan/UBSan CTest finished584 cases:539 passed,43 failed
at process exit,2 skipped. All test assertions in those43 cases pass, but
LeakSanitizer reports one64-byte leak from OverridePaths.emplace_back(paths::PrefPath())
in Source/engine/assets.cpp746, called by LoadModArchives. The symbolized focused
Drlg_l1.CreateL5Dungeon_diablo_1_2588 case reproduces it. Standalone global
string/vector and minimal shared-library string/vector probes are clean. Root cause
is not established; no leak suppression or claim of a toolchain-only fault was made.
The game --help startup smoke passes; this is not a gameplay smoke test.

One replacement CI continuation is scheduled after the first tick was consumed;
private run/build/checkpoint evidence remains outside the public repository. Next:
verify final-source native Debug and investigate the bounded sanitizer concern,
then the two-player shared-screen/nonblocking inventory proof. Historical separate
offscreen render backend and input-poll ownership boundaries are in SPLITSCREEN.md.
Distribution, updates/changelog and central fault reporting are release requirements.
Private JARVIS_REPORT_URL repo secret is configured; do not embed it in artifacts.
The game/installer fault relay is pending the first executable slice.

Automation: regular Tue/Thu/Sat work sessions plus monthly upstream sweep are registered
in JARVIS. Shared Sol quota dispatch is owned by JARVIS/self and is now active; this game
lane does not implement or bypass its budget/reset/slot gates.

Sources reviewed:
- https://github.com/diasurgical/DevilutionX (README, platform support, MPQ requirements)
- https://github.com/diasurgical/DevilutionX/blob/master/LICENSE.md
- https://github.com/diasurgical/DevilutionX/wiki/Multiplayer (TCP/loopback, separate config)
- https://github.com/diasurgical/DevilutionX/wiki/Community-Mods (candidate survey entry point)
- Source/options.cpp multiplayerFullQuests; Source/controls/controller.cpp;
  Source/controls/devices/game_controller.cpp; Source/engine/render/scrollrt.cpp; Source/player.h.
Initial web search did not establish an existing ready-to-use four-player Deck splitscreen
implementation. Complete the fork/mod survey before claiming none exists.


## 2026-10-11 — shared-screen direction and nonblocking inventories

Jan clarified the preferred product: shared screen like console Diablo III. He also
explicitly rejects blocking everyone while one player sorts items. This supersedes
the original independent-camera/different-level M0 requirements. CLAUDE, README,
ROADMAP and SPLITSCREEN now require personal input/UI ownership, simultaneous panels
and continuing world simulation/other-player movement, combat and spells. PR8354 is
more relevant for camera/input reuse, but its single panel/store owner does not meet
this requirement. No gameplay implementation or performance proof is claimed.

Baseline work remains on source df52384a2; native run38170302962 is already dispatched
and still running at20:04UTC. Original native evidence and the unresolved local43
LeakSanitizer exits are preserved. No rebuild/re-dispatch for this documentation change.
Next: finish existing baseline validation/triage, then bounded two-player shared-screen
and nonblocking inventory proof. Exactly one continuation retains its existing time;
its prompt now carries the authoritative changed product direction.

## 2026-10-12 — native artifact verification and bounded sanitizer diagnosis

Final portability source df52384a2b7835fc16cee3362f6dbbca08a8b2b6 passed native
run38170302962. Linux artifact11714118499:584 cases,0 failures,2 upstream skips;
Windows artifact11713808038:558 cases,0 failures,2 upstream skips. Downloaded
ZIP hashes: Linux96d259c6466ff4bb8548fa67f6287eb34fdb9ff0cd3afaaf420132ea77b383f9,
Windows92241b68df66e1f8f8899c20c132647c852626a49edc959b98f0a288ef373af9.
The verifier now accepts GitHub's whole-job log packaging and checks the actual
`git rev-parse HEAD` output on each platform; checkout inputs alone are insufficient.

The original local584-case failing run remains historical failure evidence. A
load/unload/reload probe with the real backend/test main/unsanitized GoogleTest
reproduces poisoned live vector/string-pointer memory and a64-byte LeakSanitizer
exit failure. Rebuilding only GoogleTest with ASan/UBSan makes those same addresses
unpoisoned and the probe exit0. The unchanged original dungeon case also passes
with the instrumented library. A fresh negative-control relink against unsanitized
GoogleTest repeats the poison and64-byte report. Destructor tracing confirms both
vector and string destruction; debugger-only leak-disabled runs are diagnostics,
not sanitizer passes. See docs/BASELINE.md for the supported configuration.

The fix propagates configured sanitizer flags to vendored gtest/gtest_main/gmock/
gmock_main only. All four normal CMake compile commands carry ASan/UBSan in the
local Debug cache. Imported system/vcpkg libraries and gameplay are unchanged.
No suppressions or archive-clear workaround; no architecture/release/hardware claim.

Normal CMake game/all56-test rebuild (max2 jobs) completed successfully. Full
Clang23/libc++ ASan/UBSan CTest584 now passes:582 passed,0 failures,2 unchanged
upstream pagination skips,173.89s. Game --help exits0. The precommit tested
configuration SHA256 is428ccefa2c7101ada1b7e678196a4d1507c1b26f627b89e12c491a9ac729aa6e;
all game source remains unchanged. Agent TypeScript check passed before commit.
Fresh native final-source CI remains the gate before selecting/implementing the
two-player shared-screen/nonblocking inventory proof. No playable fork release.

Sanitizer-config source f53af5e5dd702cb5300d718947db6f7dded08d13 is committed
and pushed. Fresh native Linux/Windows Debug run38189532088 was accepted once,
both jobs in progress at00:50UTC:
https://github.com/jarvis-jhk/diablo-splitscreen/actions/runs/38189532088
This new run is the current gate; portability run38170302962 is verified history.
One continuation is scheduled; local probes/full suite need no redundant rebuild.
No Signal progress, no playable release or architecture implementation.

## 2026-10-12 — sanitizer-config native baseline gate completed

Native run38189532088 completed successfully, including both platform jobs:
https://github.com/jarvis-jhk/diablo-splitscreen/actions/runs/38189532088
Actual git rev-parse HEAD outputs confirm source f53af5e5dd702cb5300d718947db6f7dded08d13
on both platforms; workflow head is the same. Verification completed01:21:50UTC.
Linux584 cases:582 passed,0 failures,2 skips; Windows558:556 passed,0 failures,2 skips.
Both skips are VisualStoreTest.Pagination_NextAndPrevious and Pagination_ResetsHighlight.

Downloaded Linux JUnit ZIP artifact11721367434:60561 bytes, SHA256
`a948842d6aa1e096cc4d7d7eb6b9be55da7da26aaf0a4813590082a7fb4065ba`.
Windows artifact11721126885:58478 bytes, SHA256
`becf9d99dcc229780a35772c1341692118f76043815dcc68d241b6fb9dd8a97e`.
Both XML reports contain the expected nonempty case counts and no failures/errors.
These are test artifacts, not playable packages. The local full584 sanitizer suite
and focused controls already passed; no additional build was needed here.

Baseline source portability and supported vendored-test sanitizer configuration are
resolved within these evidence boundaries. Original/portability runs and original
local43 LSan failures remain history. Two upstream skipped cases remain unverified;
standalone local Clang23 benchmarks remain excluded for the documented upstream
pedantic issue. CI's native default-target builds completed on both platforms.
The consumed b2e78fba continuation is closed under the schedule lock, with no new
baseline continuation. Recurring work/upstream schedules remain registered.

Chosen next bounded proof is docs/TWO-PLAYER-PROOF.md, linked from the roadmap and
architecture brief. It evaluates explicit in-process input/UI ownership and selective
PR8354 reuse with real game/input/render paths, two simultaneously operated inventories,
continuing world/combat/spells, held-input cleanup and measured runtime costs. The
plan does not select the production architecture or establish runtime behavior.
No source changes, playable release or Signal progress message in this checkpoint.

## 2026-10-12 — release SDL handles on controller/joystick removal

The two-player proof's hotplug audit found both GameController::Remove and
Joystick::Remove erase raw-pointer wrappers without closing the SDL reference
opened by Add. SDL keeps disconnected handles alive until that reference is
released. A gamepad also gets a separate raw-joystick registration, so each
registry must close only its own reference. PR8354 at a010e2f0b8 contains the same
missing cleanup; no community code was imported for this independent fix.

Both removal paths now close their matching handle before erasing the entry.
Unknown/repeated removal remains harmless, and the other registry, other devices
and separately opened external SDL references retain their own lifetimes. SDL3
uses its corresponding close APIs; SDL1 behavior is unchanged. Executed device
coverage is SDL2 only; the virtual-device target requires SDL2 >=2.0.14.

Six production-backend regression cases exercise controller-only/raw-joystick
cleanup, both orders of dual registration removal, independently opened external
references, repeated/unknown removal and twelve real SDL virtual-device hotplug
cycles while a second device remains connected. All six fail with the unchanged
pre-fix game source at 78c50d146, and all six pass with the fix. The tests use the
real SDL event queue and HandleControllerAddedOrRemovedEvent for hotplug cycles;
no mocked device handles or copied removal functions.

Local Clang23/libc++ Debug ASan/UBSan game and all58 test executables built with
max2 jobs. Full CTest613 completed in190.15s:611 passed,0 failures,2 unchanged
VisualStore pagination skips. No sanitizer suppression or disabled leak checking.
CLI --help exits0; this is only startup evidence. Test/source/backend hashes and
positive/negative JUnit/logs are retained privately in data/diablo/hotplug-*.
The four tested source file hashes remain unchanged before commit. Formatting of
new code/test passes; Clang23's existing joystick.cpp line135 replacement is
identical before/after the patch. Actual agent TypeScript compiler also passed.

Native Linux and Windows Debug must still verify this pushed head before merging:
require actual git rev-parse outputs, six executed ControllerHotplugTest cases on
both platforms, no failures and only the two known pagination skips. Expected
full totals are Linux613/Windows587. A single dispatch is requested after push;
its live ID/link belong to the draft PR/private slice checkpoint. No schedule is
added, no release or Signal announcement. This quota tick ends after this slice.

This fixes SDL resource lifetime only. Global stick axes, padmapper/suppression
state, assigned-player routing, simultaneous inventory/cursor/item ownership and
held-action cleanup remain unproven and are still part of TWO-PLAYER-PROOF.md.
The branch does not implement local co-op, select the production architecture or
certify physical controllers/Deck, full-game progression, rendering or timing.

## 2026-10-12: REA adopted (docs/research/rea.md)

Jan asked the game lanes to use REA (rea-agents 6.4.0, toolchain shared with the other
lanes, outside the repo). First real question, from TWO-PLAYER-PROOF's disposable-profile
requirement: two `rea capture-process` runs of the local Debug ASan build of dc3374e8b with
shareware data to the main menu. With `--config-dir`/`--save-dir` the only effect was
`diablo.ini` in the given config directory and HOME stayed untouched; the control run
without them wrote `HOME/.local/share/diasurgical/devilution/diablo.ini`. Startup only — no
character or save was created, so save isolation remains to be captured in the proof.
No original Diablo.exe is available here, so Ghidra analysis of original behaviour waits
for a user-supplied copy analysed locally. Scenarios/captures: JARVIS data/diablo/rea/.

## 2026-10-13 — hotplug handle fix verified natively and merged

Dispatch run 38212199406 on dc3374e8b passed both jobs. Logged `git rev-parse HEAD`
printed dc3374e8b on Linux and Windows; downloaded JUnit artifacts (sha256 400e3f13…
Linux, aa94e97e… Windows) hold Linux613/Windows587 cases, all six ControllerHotplugTest
cases run on both, zero failures and only the two known VisualStore pagination skips.
Merged into master; the only conflict was this file (both entries kept, chronological).
Game/test source in the merge is byte-identical to the tested head. Still SDL2 virtual
devices only — no SDL3 execution, physical pads, Deck or player/UI ownership claimed.

## 2026-10-13 — bounded two-player ownership audit

Continued t_mv61a3gt2opq4 after the usage pause. Existing hotplug merge401ad33aa
was already committed; no previous source work was repeated. Live GitHub review
found no open issues/PRs. Native baseline38212199406 remains successful on
source dc3374e8b. Revalidated retained downloaded JUnit ZIP hashes and all six
executed hotplug tests: Linux613/Windows587, zero failures, two known pagination
skips each. Entire merge diff against that source contains Markdown only; game,
tests and build configuration are identical. No fresh native merge-head run
is claimed. The latest green38336348164 is inherited Xbox One CI, not baseline.

Two inherited Xbox workflows were still active despite the original disable
pass. Disabled xbox_nxdk/xbox_one through GitHub and verified the dedicated
manual Fork Debug baseline is now the only active workflow. Source definitions
are preserved. Private live snapshot: data/diablo/ownership-review-github.json.

Completed docs/OWNERSHIP-AUDIT.md: finite device/input, navigation/cursor/items,
command authority and rendering boundaries, restoration contract and selective
PR8354 reuse decision. Concrete source-derived hazards include cross-device
combos, shared release/suppression state, sender-blind same-tick command
throttling, one panel rectangle/cursor and PR local-vs-remote item classification.
The PR's first-match device exclusion can also hide a later eligible device.
These are inspected scenarios, not executed failure tests. No PR code imported.

The audit stops before a broad context refactor. Next bounded prerequisite is
explicit-device button/combo polling with two real SDL virtual devices, both
registration orders, raw-joystick fallback, split modifiers/removal, solo
compatibility and an executed negative control; then native Debug Linux/Windows.
Added Device polling to the combined roadmap. Two-player proof remains started
and incomplete; its implementation and Personal inventories are not marked done.
No world/UI architecture selected, runtime/screenshot/performance/hardware proof,
playable release, new CI dispatch/continuation or Signal progress message.

## 2026-10-15 — explicit-device button/combo polling (prerequisite only)

Live GitHub check: no open issues/PRs; latest native baseline 38212199406 green.
Added `IsControllerButtonPressedOnDevice`, `IsControllerButtonComboPressedOnDevice`
and `ControllerEventDevice` (controller.h). A device is one SDL joystick instance;
its gamepad and raw-joystick registrations are both consulted, as the any-device
query does. A combo needs both buttons on the same device. The keyboard controller
is not a device and stays on the solo path. Existing any-device queries and every
caller are unchanged, so solo behavior is untouched; nothing uses the new API yet.

New ControllerDevicePollingTest (8 cases, two to three real SDL2 virtual devices):
solo compatibility, both registration orders, split modifier across pads (the old
combo query still combines — asserted as the hazard), raw-joystick registration,
raw-registration removal, unassigned/unknown devices, removal while held, and
event-to-device resolution. Local Clang23 Debug ASan/UBSan: full suite 621 cases,
619 pass, the 2 known VisualStore pagination skips, no failures. Negative control
(per-device query returns the any-device answer): 4 cases fail with 12 failed
assertions; source restored byte-identical before the positive run. On desktop
builds no raw-joystick button mapping is defined, so the raw path is proven only to
mirror that device's own state, not to map any button.

Still open before the two-player proof: stick axes and padmapper/held-action state
are global, and no gameplay/UI path routes by seat yet. No runtime, hardware, Deck
or release claim. Evidence: JARVIS data/diablo/device-polling/.
Native dispatch 38671325916 on exact head 1192a6e2f passed both jobs; logged
`git rev-parse HEAD` printed it on Linux and Windows. Downloaded JUnit artifacts
(sha256 576f5f0a… Linux, 53d66090… Windows): Linux 621 / Windows 595 cases, all 8
ControllerDevicePollingTest and 6 hotplug cases executed, zero failures, only the
two known pagination skips. Fast-forwarded into master; later commits are docs only.
