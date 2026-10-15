# Two-player ownership audit

Source inspected: fork `401ad33aa3f933a0e988b3d4c5abb9ca0833746b`.
Community reference: PR8354 `a010e2f0b8a951138eb628355bc4d3ab9cef6d37`,
compared to its recorded base `a3e5fd54fad4e8993e82aa59c07bdaafcbc6ae47`.
This is source evidence, not an executed gameplay or performance proof. No
community code is imported and production architecture remains unselected.

## Reuse decision

The PR adds about 5,352 lines in `controls/local_coop/local_coop.cpp` and 854 in
its header alone. Importing that subsystem would couple character selection,
input, camera, merchants, saving and UI before simultaneous inventories work.
Use its `UpdateLocalCoopCamera`, character initialization and explicit-device
input as references for separately tested slices, retaining upstream attribution
for any code actually copied. Do not cherry-pick the whole PR.

`LocalCoopPlayerContext` saves/restores only `MyPlayer`, `MyPlayerId` and
`InspectPlayer`. This cannot isolate the fields below. `TryClaimPanelOwnership`
returns false when another player's panels are open; `panelOwnerPlayerId` and
`storeOwnerPlayerId` therefore encode a different product contract.

Two further source hazards prohibit unreviewed reuse:

- PR `IsControllerButtonPressed` asks `IsPressedOnAnyController` for its first
  pressed device, then excludes secondary players. If the first pressed device
  belongs to a secondary player, a later eligible primary device is never
  searched. Selection must precede polling, including the raw-joystick path.
- PR `msg.cpp::OnRequestGetItem`/`OnRequestAutoGetItem` use active-player/id
  checks to classify local recipients. Active remote players also satisfy those
  checks. Local authority needs an explicit local-seat membership predicate;
  preserve remote-client item synchronization and sender identity.

These are source-derived failure scenarios, not executed regression results.
The PR also predates the current FixedPoint HP/mana refactor. Use the PR-base
comparison for attribution, then adapt individual changes to the current fork;
a diff between current master and PR head includes unrelated upstream reversals.

## Required state and caller boundaries

### Device and held input

`controls/controller.cpp::ToControllerButtonEvents` resolves a real SDL device,
but its returned button event does not retain device identity. Resolve the
assigned seat from the original SDL event before handing off the button.
`IsControllerButtonPressed` polls all controllers, keyboard-controller state
and raw joysticks; `IsControllerButtonComboPressed` performs two independent
queries. A modifier on A and a button on B can therefore form a combination.

`controls/controller_motion.h` exports raw/scaled left/right stick coordinates,
scaling flags and `SimulatingMouseWithPadmapper`. Both device backends write
these globals. Merely filtering button events leaves continuous axes shared.
Trigger/hat edge state is already stored in each backend device wrapper; retain
that device ownership rather than duplicating SDL handles into player state.

`controls/padmapper.cpp` stores `ButtonToReleaseAction` by button alone;
`PadmapperPress`, `PadmapperRelease`, `PadmapperIsActionActive` and
`PadmapperReleaseAllActiveButtons` share it. `SuppressedButton`,
`PadMenuNavigatorActive`, `PadHotspellMenuActive`, `ControllerActionHeld` and
`StandToggle` also have no seat owner. A release/disconnect for one pad must
never run another pad's release callback or clear its held attack.

Small next prerequisite: explicit device selection for event and continuous
button/combo polling, with ordinary solo behavior retained. Test two real SDL
virtual devices, both device registration orders, split modifiers, raw joystick
fallback, unassigned devices and removal. Require an executed negative control
and fresh native Linux/Windows Debug CI. This proves polling only; axes,
padmapper lifecycle and gameplay/UI routing remain subsequent gates.
Done 2026-10-15: `IsControllerButtonPressedOnDevice`,
`IsControllerButtonComboPressedOnDevice` and `ControllerEventDevice`, native CI
38671325916 (see NOTES.md). No caller uses them yet.

### Inventory, cursor and tooltip

The canonical item state already belongs to `Player`: `InvBody`, `InvList`,
`InvGrid`, `SpdList`, `HoldItem`, inventory count and gold. Keep it there; never
create shadow inventories or swap entire player/world objects for rendering.
`inv.cpp::CheckInvCut` and `CheckInvPaste` accept `Player &` and cursor position,
but hit-testing uses shared panel geometry and their cursor/equipment effects
still check `MyPlayer`. `CheckInvItem` dispatches with global `MyPlayer` and
`MousePosition`; `DrawInv`, `DrawInvBelt` and `CheckInvHLight` use `InspectPlayer`.

The first owned inventory context must include `invflag`, `MousePosition`,
cursor image/size (`pcurs` and dimensions), `pcursinvitem`, `pcursstashitem`,
world targets (`pcursmonst`, `pcursitem`, `ObjectUnderCursor`,
`PlayerUnderCursor`, `cursPosition`, temporary cursor selection), controller
navigation `Slot` and `PreviousInventoryColumn`, plus owned navigation repeat
state. `CheckInventoryMove` and `HandleRightStickMotion` have function-static
repeater/accumulator state; pointer switching does not restore it. Audit
`ControlMode`/`ControlDevice`, `PointAndClickState`, `rightStickLastMove`,
`LastPlayerAction`, `sgbMouseDown`, `pcurstrig`, `pcursmissile`, `pcursquest`,
`InfoString`, `FloatingInfoString` and `InfoColor` at their writers too.

`InventoryKeyPressed` toggles global `invflag`, changes the cursor position,
clears `SpellbookFlag` and closes gold withdrawal/stash/visual store globally.
A seat-specific inventory entry must not invoke those cross-seat closures.
Stash/merchant/gold state is out of the two-inventory prototype: use an explicit
unsupported transition rather than letting that shared state operate on the
wrong character. Broader personal-menu support remains required afterwards.

### Simulation and command authority

`diablo.cpp::ProcessInput` returns false on global `PauseMode == 2` and on an
active solo game menu. `GameLogic` then skips simulation. Inventory itself does
not currently stop world ticks. The blocker is that `Movement` checks the global
UI handler and `plrctrls_after_game_logic` moves only `*MyPlayer`. A second local
seat needs its own UI-vs-gameplay dispatch without introducing an early return
from the shared tick. Run world simulation once; do not run `GameLogic` once per
player.

`msg.cpp::WasPlayerCmdAlreadyRequested` uses one `lastSentPlayerCmd`, comparing
command/position/parameters without sender identity; `ClearLastSentPlayerCmd`
clears it once per tick. If two seats submit the same movement/attack/spell
command in that tick, the second can be throttled. Add sender-aware tests through
production send/receive before relying on repeated pointer-context calls.
Do not remove the upstream desync workaround: retain same-seat throttling while
allowing the second local sender. `multi.cpp::NetSendHiPri`/`NetSendLoPri` accept
player ids but append into shared priority buffers. Packet construction,
loopback delivery, item handlers and save ownership still need explicit audit;
pointer restoration alone is not a network/save authority model.

### Drawing and layout

`control/control_panel.cpp` keeps single `LeftPanel`/`RightPanel` rectangles.
Inventory hit-testing, `DrawInv` and controller slot coordinates must all use
the same owner-specific rectangle. Drawing twice at `GetRightPanel()` would
cover the first player's overlay and leave input ambiguous.

`engine/render/scrollrt.cpp` draws one inventory or spellbook and one cursor.
Cursor background save/restore and viewport shifting also assume one panel
set. Render the world once with shared camera state, then each owned overlay
and software cursor; allocate separate cursor restore storage or draw overlays
in a final compositing pass. Hardware cursor mutation in `NewCursor` cannot
represent two pad cursors. Do not let a personal render context change shared
camera/light/world state or presentation resources.

## Restoration contract for the experiment

An input scope loads one seat's device, UI/cursor, navigation and held state;
production handlers mutate that seat's real `Player`, then store its changed
UI/input state and restore the previous context on every exit. No retained
pointer to transient context storage. Device removal clears only that seat's
held state before reassignment; carrying an item must preserve the real
`Player::HoldItem` until a tested safe disposition exists.

The shared tick starts with stable session/authority identity and runs once.
Any seat-derived command carries its sender to enqueue, throttling and parse.
A rendering scope selects only the overlay owner/geometry/tooltip state and
restores it before another overlay or presentation. Rendering must not create
transactions, change item ownership or overwrite saves. Explicit APIs are
preferable where the audit exposes hidden function-static state; a blanket
swap of globals is not established as safe.

## Stop decision and remaining evidence

A three-pointer context plus single panel owner is insufficient. Simultaneous
inventories require coordinated changes across input, navigation, command
throttling and rendering; these cannot honestly be presented as a small reuse
patch. Stop this audit slice here and implement the bounded explicit-device
polling prerequisite next. Then prove held/axis state and production command
identity before the two-character inventory experiment in TWO-PLAYER-PROOF.md.

No two-character runtime, item transfer, screenshots, frame/RSS/latency samples,
character-save isolation, full Diablo content or physical controller/Deck proof
was executed in this slice. No playable release or architecture commitment.
The existing Debug/sanitizer and native hotplug results remain baseline evidence,
not evidence for the newly identified ownership boundaries.
