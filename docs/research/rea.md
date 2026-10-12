# REA in this fork

REA ("Reverse Engineer Anything", MIT) is a CLI/MCP layer that returns evidence JSON
for a question about software we cannot simply read: native binaries through Ghidra
headless, process behaviour under a PTY, recorded Linux crashes. Genuine package only:
npm `rea-agents` (maintainer morluto, github.com/morluto/rea). Lookalike repositories
offering a "Download rea" are probably malware.

It does not replace reading DevilutionX source. Source is authoritative for what our
build does; REA is for what source leaves open.

## Where it fits here

- **Original behaviour.** When DevilutionX leaves original Diablo behaviour open
  (multiplayer sync, panel/store ownership, what the 1.09 game kept per player), check
  the original binary in Ghidra with `rea function <exe> <address>`. There is no
  original `Diablo.exe` in this environment; it is Blizzard's and comes only from a
  user's own install, analysed locally. Never commit it, MPQs or decompiled game code;
  record addresses and behaviour in our own words, with the REA/Ghidra version.
- **Process behaviour.** `rea capture-process <scenario.json>` runs our build in a PTY
  with a chosen environment and records output, exit and before/after filesystem
  snapshots of chosen directories. Use it for profile/save isolation and startup
  regressions of splitscreen builds.
- **Crashes.** `rea inspect-recorded-crash <core>` for a Linux amd64 core of our build,
  for example from a fault report that can be reproduced locally.

Ghidra on our own Debug ASan binary (174 MB) is the wrong tool: import alone takes far
longer than reading the source. The diablo2 lane measured about 20 minutes for a 10 MB
ELF; PIE addresses are rebased by 0x100000, so always check `procedure.name`.

## Setup

Outside the repository, in JARVIS's toolchain directory (shared with the other game
lanes): rea-agents 6.4.0, Ghidra 12.1.4, Temurin JDK 21.0.12.1+1. Set `JAVA_HOME`,
`PATH` and `GHIDRA_INSTALL_DIR`; `REA_GHIDRA_STARTUP_TIMEOUT_MS=1800000` for large
binaries. `rea doctor` calls the Debian 12 host unsupported, but the Ghidra and
process providers work. Scenario files and captures live outside the repository too.

## First question: do `--config-dir`/`--save-dir` keep a test run off the real profile?

docs/TWO-PLAYER-PROOF.md requires disposable test profiles and an untouched user
profile. That rests on DevilutionX writing nothing outside the directories it is given.

Two captures (rea-agents 6.4.0, process provider 3, 2026-10-12) of the local Clang 23
Debug ASan/UBSan build of `dc3374e8b` (binary sha256 `e1aae980…9a9d`), shareware
`spawn.mpq` (sha256 `64427cd7…6e38`), SDL dummy video/audio, `-n --spawn`, a fresh
empty `HOME`, observing `HOME`, the config directory and the save directory. Each ran
20 s to the main menu, then SIGTERM; both exited 0 within about 0.3 s.

- With `--config-dir C --save-dir S`: the only filesystem effect was `C/diablo.ini`
  created (4312 bytes). `HOME` and `S` were unchanged.
- Control, same run without the two options: `HOME/.local/share/diasurgical/devilution/diablo.ini`
  created (the same 4312 bytes, identical hash), with the directories above it.

So the options do redirect the startup config write, and the control shows the capture
would have seen a write into `HOME`. Limits: only startup to the main menu was
exercised — no character was created, so no save or hero file was written; filesystem
evidence is before/after snapshots, not a syscall trace, and network activity is not
observed. The two-player proof must still capture a run that creates and saves
characters before relying on this for saves.
