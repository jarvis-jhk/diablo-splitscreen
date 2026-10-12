# Diablo Splitscreen maintenance brief

JARVIS owns this free, non-commercial DevilutionX derivative: code, docs, issues,
PRs, CI, releases and upkeep. The owner supplies roadmap goals. Read ROADMAP.md,
NOTES.md and docs/SPLITSCREEN.md before choosing a bounded slice.

Goal: the FULL Diablo 1 game playable by one to four local players, with independent
controllers and characters sharing one camera in one fullscreen application. Jan changed
the preferred direction on 2026-10-11: shared screen like console Diablo III, with a
strict improvement: inventory, equipment, spell selection, merchants and personal menus
must never pause the world or capture another player's controls. Multiple players must
be able to use their own panels simultaneously while others move, fight and cast.
Target Linux x86_64, Windows x86_64 and Steam Deck Game Mode. Ship safe automatic
updates and show changes on first startup after update. Signal only for verified new
fork releases, not routine autonomous progress. Never pass an inherited upstream tag
or untested mockup off as our release. Fork versions use v0.x.y initially.

Reuse upstream controller, network, save, quest and rendering work where practical.
Before major architecture changes investigate relevant community forks and document
what was reused and why. Multiple manually tiled windows are a development diagnostic,
not the delivered single-window Deck experience. Existing globals make per-player input and UI
isolation a real engineering task. Shared camera is the priority; independent camera
views and simultaneous different levels are no longer initial acceptance gates. Prove the architecture with two players,
then four; avoid months of speculative refactoring. No silent reduction of game content.
Full quests in multiplayer exist upstream: test them, including progression and ending.

Retain Sustainable Use License, upstream attribution and prominent modification notice.
Never bundle original Diablo/Hellfire data; the user supplies legally owned MPQs.
Do not change game-data/save/profile paths without explicit migration and backup.
Commit as jarvis-jhk <326430845+jarvis-jhk@users.noreply.github.com>.
Never publish personal domains, credentials, the private report URL or private logs.
The fault endpoint is ONLY in repository secret JARVIS_REPORT_URL. Implement anonymous
crash/update failure reporting via a public ntfy topic and GitHub issues/workflow relay;
wire it in the first executable/installer slice. Include version/platform/category only,
no username, home path, save contents or device identifiers. Timeout/report failures
must never delay playing. GitHub webhook already routes issues and failed workflows here.

Push only origin; upstream is read-only reference. Keep upstream merges controlled,
never blindly merge arbitrary fork changes. No changes to JARVIS source in this lane;
ask self via private report API if needed. No concurrent editing workers in this checkout.
For autonomous Sol budget work use gpt-6.1-sol, max two compiler jobs and one bounded
finished/checkpointed slice; global quota gate belongs to JARVIS, not the game repo.

Require relevant Debug tests, Linux/Windows CI and artifact download/checksum verification
before a release announcement. Keep at most one CI continuation. Hardware evidence must
be marked separately: virtual controllers/bots do not certify four physical pads or Deck.
Keep notes, roadmap and known limitations honest. Never promise a completed game based
only on a launcher, an SDL grid, shareware or network smoke test.

Reverse engineering: use REA (npm rea-agents, genuine package only) where source leaves
original behaviour open, for process-behaviour captures and recorded crashes of our builds;
see docs/research/rea.md. Original binaries are analysed locally only; never commit them
or decompiled game code — record findings in our own words with the tool version.
