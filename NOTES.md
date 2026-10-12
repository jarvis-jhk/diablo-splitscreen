# Current state — 2026-10-12

Authoritative direction: shared screen for up to four characters, with simultaneous
personal inventories/menus that never block other players or pause world simulation.
Jan's clarification replaces the initial independent-camera/different-level gates.

Original and final two-include portability Linux/Windows Debug baselines are
verified. Run38170302962 built source df52384a2 on both platforms: Linux584 and
Windows558 cases, zero failures, the same two upstream pagination skips. Source
SHA outputs and downloaded JUnit artifacts are verified.

The local Clang sanitizer failure is now reproduced as mixed instrumentation in
vendored GoogleTest/libc++ container templates: after archive reload, live vector
entries remained poisoned. Matching sanitizer flags make the focused archive and
original dungeon probes pass. The bounded vendored-test-library CMake fix passes the normal complete game/
all56-test build and all584 local Debug cases (582 passed,2 upstream skips).
A fresh native run is required for this new source.
No architecture has been selected or implemented.

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
