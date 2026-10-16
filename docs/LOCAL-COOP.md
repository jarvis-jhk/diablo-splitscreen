# Local co-op (two-player proof)

This is the first playable slice of couch co-op: a second controller plays its
own hero in the same game, on the same screen, while pad 1 keeps every normal
control. It is a proof, not the finished couch game; the limits below are real.

## How to play

1. Start **Multi Player → Offline**, create a game and pick pad 1's hero as usual.
2. In the game, press any button on pad 1 first. The first pad pressed is pad 1.
3. Press **Start on another controller**. A second hero joins next to pad 1.
   Up to three extra controllers can join this way.

The guest hero is the next saved hero in your list that is not pad 1's. If you
have only one hero, the guest is a fresh level-1 character ("Player 2").

Guest controls:

| Button | Action |
|---|---|
| Left stick / D-pad | walk |
| A (hold) | attack the nearest monster, otherwise pick up a nearby item |
| X | cast the selected spell at the nearest monster (or straight ahead) |
| Y | select the next spell the hero knows |
| B | drink a potion from the belt (life first if hurt, then mana) |
| Back | open or close the guest's own inventory |

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
- The camera follows pad 1; a guest cannot walk more than 7 tiles away from pad 1.
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
