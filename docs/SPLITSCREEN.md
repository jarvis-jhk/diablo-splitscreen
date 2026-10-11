# Split-screen architecture investigation

This is a design brief, not an implemented architecture. Preserve the complete Diablo
world and upstream network/gameplay behavior. One fullscreen window must expose up to
four independently controlled views; players may enter different dungeon levels.

The initial [community survey](COMMUNITY-SURVEY.md) found upstream PR #8354 at
`a010e2f0b8a951138eb628355bc4d3ab9cef6d37`. Its source uses a shared camera,
one panel/store owner and coordinated level transitions. It provides useful local
input/HUD references but does not satisfy independent views or different levels;
do not treat a wholesale merge as the architecture proof.

The inspected baseline has global MyPlayer/MyPlayerId, camera/render and UI state, plus
controller helpers querying any attached pad. Four viewports drawn in a loop around those
globals alone would leak input, inventory, visibility and selection across players.
A single-process context refactor must explicitly identify gameplay state versus each
local client's state, including RNG/events, level loading, audio and save ownership.

Compare two viable approaches with measured evidence:

1. In-process per-client contexts: integrate view/controller ownership into the engine,
   sharing only authoritative world state where upstream semantics permit it. Benefit:
   native single window and low IPC cost. Risk: broad mutable globals and simultaneous
   different dungeon levels; do not share state merely because it is currently global.
2. Isolated local engine clients communicating over loopback, with a single SDL host
   compositor and explicit per-client input/audio/frame transport. Benefit: reuse tested
   multiplayer and isolate globals/saves. Risk: Deck CPU/memory, IPC and Windows process
   handling. Clients must render without independent desktop windows or focus dependency;
   this is not a tiled-window wrapper or external Windows-only Nucleus setup.

Acceptance for the first spike: one window, two controllers/characters/cameras; one
player in inventory while the other moves/casts; distinct level travel; independent
profiles; clean quit and restart. Use real loopback protocol if clients are isolated,
not fake split panes of a single player. Measure and record frame time and memory.

Expand to four only once ownership/isolation is proven. Automated tests must cover
controller instance ids, independent simultaneous inputs, hotplug, stuck held inputs,
menus, inventory, targeting, character/save identity, death/rejoin and quest progression.
On Deck use legible custom per-view UI at 1280×800; shrinking the entire original HUD
four ways is not acceptance. Hardware tests and full-game playthrough remain separate
from virtual SDL devices and shareware test data.

Update/install and crash reporting design are required before the first playable preview:
versioned atomic installs, rollback, save/data protection, first-start changelog, independent
Windows updater, anonymous bounded fault relay; private queue secret stays server-side.
