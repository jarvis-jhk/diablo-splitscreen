# Local co-op (two-player proof)

This is the first playable slice of couch co-op: a second controller plays its
own hero in the same game, on the same screen, while pad 1 keeps every normal
control. It is a proof, not the finished couch game; the limits below are real.

## How to play

1. Start **Multi Player → Offline**, create a game and pick pad 1's hero as usual.
2. In the game, press any button on pad 1 first. The first pad pressed is pad 1.
3. Press **Start on another controller**. A join panel opens on the shared screen
   with that controller's hero choices: every saved hero that is not already
   playing (pad 1's hero and other guests' heroes are left out), then a new
   level-1 hero of each class this game data allows. Choose with the stick or
   D-pad, **A** joins, **B** cancels and gives the controller back. Nothing
   pauses while a pad chooses. Up to three extra controllers can join this way.
4. To leave, press **Start on the guest's controller**: the guest's line at the
   top asks "leave the game?". **A** leaves, **B** (or Start again) stays. The
   hero becomes free to pick again and the controller is released.

A new hero is called "Player 2" (3, 4 by seat). Each guest keeps its seat
(P2, P3, P4) and its panel position for as long as it plays, even when another
guest leaves.

Guest controls:

| Button | Action |
|---|---|
| Left stick / D-pad | walk |
| A (hold) | attack the nearest monster, otherwise pick up a nearby item |
| X | cast the selected spell at the nearest monster (or straight ahead) |
| Y | select the next spell the hero knows |
| B | drink a potion from the belt (life first if hurt, then mana) |
| Back | open or close the guest's own inventory |
| Start | ask to leave the game (A leaves, B stays) |

## The guest's inventory

Back on a guest's controller opens that guest's own inventory panel on the
left of the screen (a second guest's panel opens beside it). Pad 1's inventory
keeps opening on the right as before, and any number of panels can be open
at the same time. Nothing pauses: monsters keep moving and fighting, and every
other player keeps walking, fighting and casting while one sorts their pack.

| Button | In the guest's inventory |
|---|---|
| Left stick / D-pad | move the guest's own slot cursor (hold to repeat) |
| A | wear the selected item (swapping out what it replaces), take off a worn item, or use a potion, elixir, book or scroll |
| Y | move the item between the pack and the belt |
| X | drop the item on the ground in front of the hero |
| B or Back | close the panel |

The panel shows the guest's own equipment, pack and belt (the row under the
panel), the name of the selected item and these buttons. While the panel is
open the guest's hero stands still; the stick steers the cursor instead.

The guest's life, mana and selected spell are shown at the top of the screen.
Unplugging the guest's controller removes the guest; pad 1 keeps playing.

## Shared camera

With a guest in play the camera centres on the middle of every living hero on the
level, not on pad 1. Any hero, pad 1 included, may walk as far as the screen allows;
a step that would push the party wider or taller than the screen is refused like a
step into a wall (the hero stands, a mouse path is dropped). Walking back towards the
others is always allowed. The room is the window less a sprite's width at the sides,
a sprite's height at the top and the control panel at the bottom; at 1280x800 that is
about 18 tiles across. Fallen heroes do not count. Without guests the camera is
upstream's, unchanged.

## What is isolated

- The guest's pad is claimed: none of its buttons, sticks or triggers reach pad 1
  (no menu, no inventory toggle, no held attack, no camera movement).
- Pad 1's open inventory, character sheet or spellbook never stops the guest:
  the world keeps running and the guest keeps walking, fighting and casting.
- The guest's commands are parsed with the guest as sender, through the same
  command handlers as any multiplayer player. Damage taken, experience, kills,
  spell mana and item pickups go to the guest's own hero.

## Known limits of this slice

- **Guests are not saved.** A guest plays a copy of a saved hero; nothing it
  gains is written back. Your save files are never touched by a guest.
- Guests have their own inventory, but no character sheet, spellbook, merchant
  or dialogue yet, and cannot spend stat points. Cursor spells and scrolls
  (identify, repair, telekinesis, town portal, golem, ...) and oils are pad 1 only.
- A guest moves items with buttons, not by picking them up onto a cursor: items
  go to the first free place in the pack, and gold cannot be split.
- The guest's panel uses pad 1's class artwork for its background, and the
  mouse does not interact with a guest's panel.
- A guest does not use stairs or portals: the guest follows pad 1 to every level.
  A fallen guest gets back up when the party changes level.
- Guests exist only in offline games, never in network games.
- Tested with virtual SDL controllers only. Physical controllers and the Steam
  Deck still need a real test.
