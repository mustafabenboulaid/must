# أشباح نوفمبر — Ghosts of November

Unreal Engine 5.7 historical action-adventure (1945–1962). This repository holds the game's C++ core as a project plugin, the narrative data, and the build/test tooling. The engineering plan is [`Docs/Brief/GAME_BRIEF.md`](Docs/Brief/GAME_BRIEF.md) (Build Brief v3). Story and history come from the Arabic design document [`Docs/Brief/design/DESIGN_DOC_AR.md`](Docs/Brief/design/DESIGN_DOC_AR.md), which wins on story and history.

**Status:** M0 (skeleton, harness, gym) and M1 (narrative graph, consequences, checkpoints, timeline validator) are done. See [`CHANGELOG.md`](CHANGELOG.md).

## Layout

```
Plugins/GhostsOfNovember/            project plugin (13 runtime modules)
  GhostsOfNovember.uplugin
  Data/Narrative/                    campaign data, JSON (edit without recompiling)
    campaign.json characters.json places.json sources.json consequences.json
    missions/*.json
  Source/GoN<System>/
    Public/Logic, Private/Logic      pure C++ core (no Unreal headers), unit-tested
    Private/Tests/Logic              pure tests (standalone + Unreal automation)
    Private/Tests                    Unreal automation tests "GoN.*"
Tools/LogicTests/                    CMake build of the pure core + tests (no Unreal)
build.bat, run_tests.bat             full editor build / headless automation tests
Docs/Brief/                          brief v3, design doc, asset manifest, briefing.html
Docs/Concepts/                       costume concept sheets
Reference/AAAWeapons/                prototype source to port (not compiled)
```

Modules: `GoNCore` `GoNNarrative` `GoNStealth` `GoNSociety` `GoNSurvival` `GoNKnowledge` `GoNCharacter` `GoNCombat` `GoNAI` `GoNAir` `GoNUI` `GoNAudio` `GoNBlockout`. Each module has a `LogGoN<System>` log category, `gon.<system>.*` console commands and tests. Modules that belong to later milestones are skeletons for now.

## Install into the Unreal project

The repository root works as the project root. Put the contents next to `MyProject2.uproject`, so the plugin sits at `MyProject2/Plugins/GhostsOfNovember`. The plugin is enabled by default. Nothing here depends on the old `AAAWeapons` or `AAATraffic` plugins, so they can stay while the port is in progress. Their traffic, wanted and car systems will not be ported.

## Build and test

1. **Close the editor.** Header changes need a full rebuild, never Live Coding.
2. `build.bat`: runs UBT for `<Project>Editor Win64 Development`. Afterwards it prints the timestamps of the `UnrealEditor-GoN*.dll` files and of the newest source file. Set `UE_ROOT` if UE 5.7 is not installed in `C:\Program Files\Epic Games\UE_5.7`. Set `GON_PROJECT` if the `.uproject` is not next to the script.
3. `run_tests.bat`: runs `Automation RunTests GoN; Quit` headless. The report goes to `Saved\GoNTestReport`. Pass a filter to run fewer tests, for example `run_tests.bat GoN.Narrative`.
4. `Tools\run_logic_tests.bat` (or `.sh`): builds and runs the pure C++ tests without Unreal, in two forms:
   - a normal strict build, where shadowing is an error;
   - a unity build with Unreal and Windows macro names force-included.

   `--validate` prints every issue in the narrative data.

## Test gym (M0)

Press Play in any level, then run `gon.gym.open` in the console. This loads the empty map set in *Project Settings → Game → Ghosts of November → Gym Map Path* (default `/Engine/Maps/Entry`) with `AGoNGymGameMode`. The game mode generates the course in C++: cover at 100/200 cm, vaults at 60/90/120 cm, climbs at 150/200/250 cm, stairs (20 cm rise), ramps at 20°/35°/50°, and doorways at 90/120/200 cm. It also adds a sun, a sky and a player start.

You can also drop **GoN Gym Builder** into an empty level, set its metrics in Details, and save the level. To fly with your character instead of the default pawn, set *Gym Pawn Class*.

## Narrative (M1)

Missions are beat graphs in `Data/Narrative/missions/*.json`. The beat types are:

- `objective`, `trigger`, `dialogue`, `cinematic`, `scene`: wait for the event named in `completeOn` (default `<beatId>.done`);
- `wait`: waits for `seconds`;
- `branch`: picks a path from conditions on the consequence state;
- `checkpoint`: saves the game;
- `end`: finishes the mission with an outcome.

Effects (`add`, `set`, `setFlag`, `clearFlag`) change stats and flags declared in `consequences.json`. Keys starting with `history.` are reserved and refused, so gameplay cannot rewrite documented history. Each mission also carries the design-doc §12 checklist (visible goal, dramatic goal, obstacle, decision, consequence, transition) and a timeline entry (dates, place, characters present).

The **timeline validator** (`UGoNTimelineValidator`, `gon.timeline.validate`, test `GoN.Narrative.TimelineValidator`) rejects:

- a character in two regions at once;
- a historical figure after their death, or outside their documented region;
- a real person's line with no source and no `dramatized` tag;
- excluded figures (Lalla Fatma N'Soumer);
- broken mission graphs.

Disputed dates, such as Ben Boulaïd's death on 22 or 23 March 1956, produce warnings for the historical consultant to settle.

| Command | Purpose |
|---|---|
| `gon.narrative.list` / `start <id>` / `event <name>` / `skip` / `state` | run a mission by hand |
| `gon.narrative.autorun [id]` | skip through a mission to its end (default `gym_test_mission`) |
| `gon.narrative.flag <key> [0\|1]` | set a choice flag, e.g. `choice.group_power` |
| `gon.narrative.save` / `load` / `reload` | checkpoint slot, re-read JSON |
| `gon.narrative.debug 1` | on-screen mission/beat/consequence readout |
| `gon.timeline.validate [errors]` | validate data from disk (no Play needed) |
| `gon.core.modules` / `gon.core.buildinfo` | loaded modules / compile time of each |
| `gon.gym.open` / `rebuild` / `validate` | test gym |

Blueprints can use `UGoNNarrativeSubsystem` (game-instance subsystem): `StartMission`, `SendEvent`, `SetFlag`, `GetStat`, `SaveCheckpoint`, `LoadCheckpoint`, and the `OnBeatEntered`, `OnCheckpointReached` and `OnMissionEnded` events.

## Content rules enforced in data

- Ben Boulaïd's briefing lines from the prototype are kept, tagged `dramatized`.
- His squad is now fictional Aurès fighters (placeholder names marked `approved: false` until you approve them).
- Historical presence windows are marked `review: pending` for the consultant.
- No mission rewards harm to civilians or prisoners.
