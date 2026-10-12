# Shared-screen couch co-op architecture investigation

This is a design brief, not implemented gameplay. Jan's 2026-10-11 clarification
supersedes the original independent-view direction: prefer one shared screen like
console Diablo III, with up to four independently controlled characters. The full
Diablo game, quests, saves and platform/release requirements remain unchanged.
Separate cameras and simultaneous different dungeon levels are no longer M0 gates.

## Nonblocking personal UI contract

Opening inventory, equipment, spell selection, character details, merchant or another
personal menu captures only that player's assigned controller. World simulation and
other players' movement, attacks and spells continue. Closing a panel releases only
its owner's UI state and must not forward a held menu button into a gameplay action.
Two or more players can use personal panels simultaneously, with separate cursors,
selection, carried items, transactions and player/save identity. A single global
panelOwner/storeOwner or a global pause flag cannot satisfy this contract.

Use compact, clearly owned overlays around the shared world; do not reduce the full
original screen four ways. Keep the action readable at 1280×800. Shared-camera tracking
must account for a player standing still in a panel: another player needs useful room
to move and fight. Camera bounds, separation recovery and party-level travel need real
playtesting; the plan does not promise unlimited separation or silently teleport heroes.
Personal UI must survive or close safely on death, disconnect and level transition,
without lost/duplicated items or abandoned pending merchant transactions.

First executed proof: one window, two characters/controllers, shared camera; player A
sorts items while B walks, attacks and casts; reverse roles; both open inventories and
move different cursors; close/reopen one without altering the other. Verify continuous
world ticks, exact inventory ownership, disconnect-held-input cleanup, and normal solo
behavior. Exercise real gameplay/input paths rather than testing just a UI state model.
Then extend the same contract to four and measure frame pacing, memory and input latency.
Physical Deck/four-pad and full-game progression evidence remain separate from bots,
virtual controllers and shareware fixtures.

## Reuse decision and bounded implementation

The community survey inspected upstream PR8354 at
`a010e2f0b8a951138eb628355bc4d3ab9cef6d37`: shared camera, coordinated level travel,
and one panel/store owner. Shared-camera and per-player input/HUD work are now relevant
reuse candidates. The single-owner UI is specifically insufficient for Jan's second
clarification; do not merge the whole branch and call the requested behavior delivered.
Audit attribution, pinned-baseline compatibility and focused regressions for each reused
slice. No architecture or playable fork is selected/proven at this checkpoint.

Prioritize an in-process shared-world camera and explicit per-player input/UI context.
Inventory/render/selection globals and helpers polling every controller need audited
ownership at both event and continuous-poll boundaries. Keep upstream world/network,
quest and save semantics. Prototype two players before broad refactoring. Isolated
clients with one compositor remain an unselected fallback if actual compatibility or
performance evidence requires them; a process comparison is no longer a prerequisite
merely to obtain independent cameras. Record measurements before a major architecture
commitment. Do not substitute manually tiled windows for the product.

The next bounded audit and executed acceptance scenario are specified in
[TWO-PLAYER-PROOF.md](TWO-PLAYER-PROOF.md). The baseline gate is complete; that
proof is planned and must be executed before choosing the production architecture.

## Historical independent-view investigation

The following source boundaries were recorded before the shared-screen clarification.
They remain useful fallback evidence, not requirements to implement worker processes,
independent cameras or different levels in the first shared-screen slice.

## Baseline boundaries for the first experiment

`Source/utils/display.h` owns global window, renderer, texture, palette and
`PalSurface`. `Source/engine/dx.cpp::RenderPresent` obtains the output surface,
uploads its pixels to the SDL texture and presents the window. This is a concrete
frame-export boundary for the worker experiment, but export must include a stable
format, pitch, dimensions and ownership lifetime. Capture after palette conversion;
an 8-bit palette buffer alone is not a complete RGB frame.

Upstream `HeadlessMode` is unsuitable as a windowless rendering switch: it skips
world drawing in `scrollrt.cpp`, skips graphics loads in player/items/monster
paths and returns from `RenderPresent`. A worker needs a separate offscreen
render backend that retains real graphics/UI initialization and game timing.
A dummy SDL window can diagnose rendering, but is not evidence of an implemented
production worker backend or a single host window on Windows/Deck.

`GameController::Get(event)` resolves SDL instance ids, whereas
`IsPressedOnAnyController` polls the whole device collection. The host must own
physical devices and forward only one assigned slot's input to a worker;
forwarding events while leaving global physical polling enabled leaks ownership.
Disconnect must release held state before slot reuse. Frame transport must not
block simulation when the host stalls; use bounded latest-frame delivery with
explicit buffer acknowledgements rather than an unbounded queue.

For comparison, an in-process context must cover graphics/UI state above and
level/visibility/simulation ownership, not only player pointers. Start from two
normal network client lifecycles, retain separate profiles and save locks, and
make only the intentionally shared session/controller-layout state host-owned.
Measure two/four clients' aggregate memory, CPU, frame-time distribution and input
latency on the same scenario. No performance measurements exist at this checkpoint;
an IPC bandwidth estimate is not measured Deck feasibility.

Update/install and crash reporting design are required before the first playable preview:
versioned atomic installs, rollback, save/data protection, first-start changelog, independent
Windows updater, anonymous bounded fault relay; private queue secret stays server-side.
