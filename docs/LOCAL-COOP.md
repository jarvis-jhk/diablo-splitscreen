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
- Guests have no inventory, character sheet, spellbook, merchant or dialogue yet,
  and cannot spend stat points. Cursor spells (identify, repair, telekinesis,
  town portal, golem, ...) are pad 1 only.
- A guest does not use stairs or portals: the guest follows pad 1 to every level.
  A fallen guest gets back up when the party changes level.
- Guests exist only in offline games, never in network games.
- Tested with virtual SDL controllers only. Physical controllers and the Steam
  Deck still need a real test.
