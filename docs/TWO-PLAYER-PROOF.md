# Next bounded proof: two players, one world, two inventories

Status: ownership audit complete; runtime proof planned, not implemented. The baseline gate is complete for source
`f53af5e5dd702cb5300d718947db6f7dded08d13`; see BASELINE.md and NOTES.md.
This experiment evaluates in-process ownership before selecting a production
architecture. End after its evidence/decision; do not expand it into four players,
merchants, a launcher or a release in the same slice.

## Starting point and reuse boundary

Audit the pinned PR8354 reference `a010e2f0b8a951138eb628355bc4d3ab9cef6d37`
against our verified source. Select only the minimum two-character initialization,
assigned-device input and shared-camera pieces needed to exercise the real game.
Record reused files/commits and attribution; preserve the baseline and ordinary
solo path. Its `LocalCoopPlayerContext` changes player pointers, while its single
`panelOwnerPlayerId`/`storeOwnerPlayerId` serializes panels. Neither is sufficient
inventory isolation. Do not import the entire PR or infer compatibility from its
source audit.

Map reads/writes before changing ownership, including these known boundaries:

- `Source/controls/controller.cpp` converts events from their actual device, but
  continuous button/combo queries still poll any gamepad/joystick. Route both
  events and polling through the assigned slot, including axes and modifiers.
- `Source/diablo.cpp::InventoryKeyPressed` changes global `invflag` and closes
  other UI; inventory/global-menu input checks can intercept gameplay before
  `GameLogic` advances players, monsters and missiles. A panel must capture only
  its owner's input without interrupting the shared simulation.
- `Source/inv.cpp`/`inv.h`, `Source/cursor.cpp`/`cursor.h` and their controller
  navigation/render callers retain global selection, panel placement and cursor
  state. Audit carried-item state, targeting, text/tooltip buffers, control mode
  and command sender identity as well as `MyPlayer`/`MyPlayerId`/`InspectPlayer`.
  Use each real Player's inventory/equipment; do not create shadow inventories.

The audit must produce a bounded field/caller list and identify restoration at
input/render/world boundaries. If reliable isolation requires a broad unrelated
refactor, stop with the concrete obstacle and a smaller next slice. An ownership
model alone is not the completed gameplay proof.

Source audit and bounded prerequisite: [OWNERSHIP-AUDIT.md](OWNERSHIP-AUDIT.md).
It identifies device polling, padmapper lifecycle, shared navigation/cursor state,
sender-blind command throttling and panel layout as concrete isolation boundaries.
Next implement explicit-device button/combo polling before the full scenario.

## Executed acceptance scenario

Use one rendered window at 1280×800, two stable SDL device assignments and two
distinct characters on the same level. Back up profiles first and run against
separate disposable test profiles; never overwrite the user's saves or MPQs.
(`--config-dir`/`--save-dir` keep startup's `diablo.ini` out of HOME — REA capture in
docs/research/rea.md; capture a character-creating run before relying on it for saves.)
Shareware fixtures establish only the scenario they can execute; full Diablo
progression requires user-provided data and separate evidence.

1. In ordinary solo mode, verify inventory open/move/equip/close and gameplay.
   In two-player mode, verify each device moves only its character; held buttons,
   axes and cross-device modifier combinations must not leak across assignments.
2. A opens inventory and moves a real item while B walks, attacks a real target
   and casts an available spell. Observe world ticks and resulting player/target/
   missile state, not just submitted input. Reverse roles. Record both owned
   overlays and the still-visible world through the real render path.
3. Both open inventory, move separate cursors and equip/move distinct identified
   items through production handlers. Assert each resulting inventory/equipment
   and carried item belongs to its character, with no loss/duplication. Close and
   reopen A's panel while B keeps navigating; B's state must remain intact.
4. Disconnect one assigned device while an action is held, including an open
   panel/carried item. Preserve or safely cancel that player's item operation,
   release held input before reconnect/slot reuse, and verify the other player
   remains responsive. Death/level transition with open UI must safely preserve
   item ownership or remain an explicit blocker to extending the prototype.

Automate repeatable production-path input/state checks with two virtual SDL pads,
including a negative control that deliberately removes slot/UI isolation and
fails executed assertions. Capture rendered output separately; a headless state
test does not prove overlays. `HeadlessMode` skips graphics and is unsuitable for
this rendered proof. Synthetic devices are not physical controller/Deck evidence.

Measure the same scenario in solo, two-player/no-panel, one-panel and two-panel
modes: hardware/build identity, sampling duration/frame count, frame-time median/
p95/p99, peak RSS and observed input-to-action latency. State missing measurements
explicitly; VPS/dummy-driver timing is not Deck feasibility. Build with max two
jobs, run affected Debug tests and fresh Linux/native Windows CI for any code
change, and retain source/artifact evidence before claiming completion.

## Stop and record

Publish the ownership audit, selected reuse, exact tested source, executed checks,
screenshots/measurements and any failure. Decide whether the two-player approach
supports simultaneous inventory ownership before expanding to four. Merchants,
spell/personal menus, party travel, full quests/saves and physical Deck remain
separate gates. Isolated clients remain an unselected fallback, and anonymous
fault relay is required before the first playable release. No release is implied
by this proof plan.
