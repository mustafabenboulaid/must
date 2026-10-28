# Changelog

Newest first. Each step lists changed files, header changes (need a full rebuild, editor closed) and the checks to run in Unreal. File hashes come from `python Tools/hash_sources.py`; `--check` shows what changed since the newest entry.

## Fixes + M2 — character, movement, native animation, blockout kit, markers (2026-10-28)

**Rebuild:** full rebuild with the editor closed (`build.bat`). Headers changed in GoNCore, GoNNarrative and GoNBlockout, plus new headers in GoNCharacter and GoNBlockout (listed below). `GhostsOfNovember.uplugin` now depends on the Enhanced Input plugin.

### Small fixes (from your M0/M1 report)
- **`gon.narrative.debug 1` showed nothing.** The narrative subsystem now ticks as an `FTickableGameObject` of the game world, so pause and time dilation apply. The overlay is drawn through `UDebugDrawService` ("Game"), the same path the prototype HUD used, which worked on your machine. Before, it relied on `FTSTicker` + `AddOnScreenDebugMessage`; I couldn't run UE here to tell which of the two failed, so both were replaced. The line now ends with `ticks N`, which shows the subsystem is ticking (wait beats depend on it; `autorun` skips them).
- **`run_tests.bat`:**
  - default filter is now `StartsWith:GoN.`. This is the UE 5.2+ prefix match. A plain `GoN.` still matches case-insensitively inside names such as "Polygon.".
  - `UE_ROOT` defaults to `J:\Unreal Multi\Editor\UE_5.7` in both `.bat` files.
- **Ben Boulaïd presence:**
  - Aurès 1954-10-01..10-21, **Algiers (algerois) 1954-10-23..10-24 (meeting of the six)**, Aurès 1954-10-26..1955-01-31;
  - the travel days are left open for the consultant;
  - `algiers` added to `places.json`;
  - new validator rule `history.presence_overlap`.
- **M-B: Ben Boulaïd only gives the briefing.** Missions can now split their timeline into **segments** (dates, place, characters, POV), and each beat names its segment.
  - M-B: `briefing` (mountains, Ben Boulaïd + group), then `action` (composite town, fictional group only, POV `fic_aures_fighter_01`), then `rally`.
  - The validator checks per segment: speakers present where they speak, the POV is present, segments sit inside the mission dates, and no one is in two regions at once.
  - A data test fails if Ben Boulaïd is present in any M-B beat outside the briefing, or if an action beat has a historical POV.
- **Fighter names:** still placeholders (`approved: false`). Your message kept the template text "[approved / replace with …]"; I asked in chat.

### M2 — GoNCharacter
- **`AGoNCharacter`** (+ `UGoNCharacterMovementComponent`, `AGoNPlayerController`):
  - walk / jog / sprint;
  - crouch: the same walk, only lower;
  - prone (UE clamps the capsule half height to the radius: 84 cm);
  - vault and climb as a custom movement mode along a tested path;
  - carry a wounded comrade (slower, no sprint, no vault/climb, no prone).
- **Camera:** shoulder swap, crouch/prone pivot heights, cover arm/offset (low cover in front while crouched, high cover while standing), sprint arm/FOV. Everything is in data.
- **`UGoNMovementSettings`** data asset: movement metrics (match the gym), camera profile, animation clips and tuning, carry bone/offset.
- **Input:** Enhanced Input built in C++ (`UGoNInputSet::CreateRuntimeDefault`); the README lists the keys. The controller guarantees `UEnhancedPlayerInput`, and the character creates an `UEnhancedInputComponent`.
- **`UGoNAnimInstance`:** native proxy ported from `AAARifleAnimInstance`:
  - 8-way walk/jog on one phase, with play rates measured from each clip's planted foot;
  - crouch hip drop with legs re-solved onto the clip's feet;
  - planted feet + turn in place (the body turns when the camera passes 95° while idle);
  - foot/pelvis IK on stairs and ramps (the prototype's traces);
  - hand IK through an IK/FK reach guard (ledge hands, carry hands);
  - head look-at toward the camera.
  The decisions behind all of these are pure C++ in `Logic/GoNAnimLogic`, with tests.
- **`AGoNWoundedComrade`:** lies in the end pose of `MM_Death_Front_01`; E picks him up and puts him down. The put-down trace starts 1.5 m above the point (brief rule 9).
- **Debug:**
  - `gon.char.debug 1`: on-screen state, speed, capsule and probe decision, plus capsule, face, top and landing traces in the world;
  - `gon.char.metrics`: every gym vault, ledge, step, ramp and door against the movement settings (registered in GoNBlockout, which depends on GoNCharacter).

### M2 — GoNBlockout kit
- **Pieces:** Douar House (1–2 rooms, flat roof, parapet, exterior stairs), Alley (stepped Casbah alley, doorways, covered passages), Roof Bridge, Wall, Stairs, Rock (deterministic seed), Slope (walkable / too steep), Cover (low/high).
  - Instanced cubes with BlockAll collision, contributing to navigation, colour-coded by role.
  - Labels show the metrics.
  - Cover points are generated in pure C++ (low/high, never inside geometry or door gaps) and stored on the actor as `CoverPoints` for the M3 AI.
- **Markers:** Spawn Point, Patrol Path (spline), Observer Post (view cone), Crowd Zone, Trust Zone, Message Drop, Narrative Trigger, Checkpoint Marker, Scene Point. Each has an editor sprite, a label and a shape drawn in the editor (and in play with `gon.markers.debug 1`).
  - They only store data until M3–M6.
  - The **Narrative Trigger** calls `UGoNNarrativeSubsystem::SendEvent` on player overlap. It disarms only after it completes a beat.
- **Commands:** `gon.blockout.list`, `gon.blockout.covers <0|1>`, `gon.blockout.rebuild`, `gon.markers.debug`.
- **Gym:** spawns the GoN character + controller (unless *Gym Pawn Class* is set), a wounded comrade 3 m ahead of the start, and a narrative trigger `gym.reached_cover` around the first low cover. It starts `gym_test_mission` (*Gym Auto Start Mission*).

### Animation clips to import (procedural pose until then)
Retarget each to `SK_Mannequin`, **in place** (no root motion: the capsule moves the character), and save it at the path below. No data asset is needed for these paths. To use other paths, assign them in a GoN Movement Settings asset.

| Slot | Expected asset | What it should be |
|---|---|---|
| ProneIdle | `/Game/Characters/Mannequins/Anims/GoN/MM_GoN_Prone_Idle` | lying on the belly, looped |
| ProneCrawl | `/Game/Characters/Mannequins/Anims/GoN/MM_GoN_Prone_Crawl_Fwd` | belly crawl forward, looped, in place |
| CarryIdle | `/Game/Characters/Mannequins/Anims/GoN/MM_GoN_Carry_Idle` | standing with a man over the shoulders (fireman carry) |
| CarryWalk | `/Game/Characters/Mannequins/Anims/GoN/MM_GoN_Carry_Walk_Fwd` | walking with that load, looped, in place |
| VaultLow / Mid / High | `/Game/Characters/Mannequins/Anims/GoN/MM_GoN_Vault_Low` / `_Mid` / `_High` | vault over 60 / 90 / 120 cm, in place (stretched to the vault time) |
| Climb | `/Game/Characters/Mannequins/Anims/GoN/MM_GoN_Climb` | climb onto a 150–200 cm ledge, in place |

**New headers:**
- GoNCharacter: `GoNAnimInstance.h`, `GoNCharacter.h`, `GoNCharacterMovementComponent.h`, `GoNInputSet.h`, `GoNMovementSettings.h`, `GoNPlayerController.h`, `GoNWoundedComrade.h`, `Logic/GoNAnimLogic.h`, `Logic/GoNMovementLogic.h`
- GoNBlockout: `GoNBlockoutPiece.h`, `GoNMarkers.h`, `Logic/GoNBlockoutKit.h`, `Logic/GoNMovementGymCheck.h`

**Changed headers:**
- GoNCore: `GoNDeveloperSettings.h` (`GymAutoStartMission`; *Gym Pawn Class* now defaults to the GoN character)
- GoNNarrative: `GoNNarrativeSubsystem.h` (tickable + debug draw), `Logic/GoNNarrativeTypes.h` (segments, POV, beat segment)
- GoNBlockout: `GoNGymBuilder.h` (`GetPureMetrics`, `GetLowCoverBounds`), `GoNGymGameMode.h` (constructor, `StartPlay`)

**Build.cs:**
- GoNCharacter adds InputCore, EnhancedInput, AnimationCore, Projects.
- GoNBlockout adds GoNNarrative, GoNCharacter.

**Verified here (no Unreal):** all pure logic, **68 tests / 4792 checks**, passes in both builds:
- the strict build with `-Wshadow -Werror -fno-exceptions -fno-rtti`;
- the per-module unity build with Unreal/Windows macro names.

The suites cover movement and traversal rules, anim maths, camera rig, gym-metrics comparison, blockout kit and cover points, narrative segments, and the shipped data. `--validate` reports 0 errors in the data.

**Not verified:** the Unreal code (character, anim proxy, input, pieces, markers, gym) has not been compiled. The procedural poses and the default carry offset are first guesses to tune by eye.

**Your checks**
1. `build.bat` and `run_tests.bat` pass. The new tests are `GoN.Character.*`, `GoN.Blockout.Logic.Kit.*` and `MetricsCheck.*`; the old ones still pass.
2. In the gym (`gon.gym.open`, with `gon.char.debug 1` to see each probe):
   - walk (hold Alt), sprint (hold Shift), crouch (C), prone (Z);
   - vault 60/90/120 and climb 150/200 (Space facing them); the 250 ledge is refused;
   - walk up the 35° ramp and slide off the 50° ramp;
   - take the stairs with the feet planted on the steps.
   `gon.char.metrics` should report 0 failed.
3. Carry the wounded comrade 3 m ahead of the start (E). Sprint, vault and prone are refused while carrying; E puts him down.
4. Drop each GoN Blockout piece in an empty level with a Nav Mesh Bounds Volume. Walk on and against it, and press P: the nav mesh covers floors, roofs, stairs and the walkable slope. Green cones show the cover points.
5. In the gym, walk to the first low cover: the log shows `trigger ... sent 'gym.reached_cover' (completed a beat)`, and `gon.narrative.debug 1` shows the mission on `radio` (then `ticks` counting).

**Files and hashes**

| File | SHA-256 (12) |
|---|---|
| `Plugins/GhostsOfNovember/Data/Narrative/campaign.json` | `5b761e80632b` |
| `Plugins/GhostsOfNovember/Data/Narrative/characters.json` | `427200c1a5bc` |
| `Plugins/GhostsOfNovember/Data/Narrative/consequences.json` | `9b214780f3e9` |
| `Plugins/GhostsOfNovember/Data/Narrative/missions/ch1_ma_last_night.json` | `eef5ac7f0335` |
| `Plugins/GhostsOfNovember/Data/Narrative/missions/ch1_mb_first_november.json` | `a3ecc889b2a8` |
| `Plugins/GhostsOfNovember/Data/Narrative/missions/ch1_mc_road_to_mountain.json` | `bbaf86e53963` |
| `Plugins/GhostsOfNovember/Data/Narrative/missions/gym_test_mission.json` | `81762d5e9378` |
| `Plugins/GhostsOfNovember/Data/Narrative/places.json` | `d144b42f4311` |
| `Plugins/GhostsOfNovember/Data/Narrative/sources.json` | `b2a0f0fcf14b` |
| `Plugins/GhostsOfNovember/GhostsOfNovember.uplugin` | `bf6f9e955217` |
| `Plugins/GhostsOfNovember/Source/GoNAI/GoNAI.Build.cs` | `fe48ad35e1f2` |
| `Plugins/GhostsOfNovember/Source/GoNAI/Private/GoNAIModule.cpp` | `f4cc110551cc` |
| `Plugins/GhostsOfNovember/Source/GoNAI/Public/GoNAILog.h` | `be77be9ad5b3` |
| `Plugins/GhostsOfNovember/Source/GoNAir/GoNAir.Build.cs` | `6ba744189f84` |
| `Plugins/GhostsOfNovember/Source/GoNAir/Private/GoNAirModule.cpp` | `fa863b9bc4e3` |
| `Plugins/GhostsOfNovember/Source/GoNAir/Public/GoNAirLog.h` | `8f8bbd0776a6` |
| `Plugins/GhostsOfNovember/Source/GoNAudio/GoNAudio.Build.cs` | `4ae17c7cdf20` |
| `Plugins/GhostsOfNovember/Source/GoNAudio/Private/GoNAudioModule.cpp` | `ece4b8c24a3e` |
| `Plugins/GhostsOfNovember/Source/GoNAudio/Public/GoNAudioLog.h` | `2f4121404c66` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/GoNBlockout.Build.cs` | `4d2b05f29a37` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/GoNBlockoutKitCommands.cpp` | `c263dddf9352` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/GoNBlockoutModule.cpp` | `3f392718c8a7` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/GoNBlockoutPiece.cpp` | `c817f9234187` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/GoNGymBuilder.cpp` | `96d283f6b3e1` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/GoNGymGameMode.cpp` | `5b273f989539` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/GoNMarkers.cpp` | `bbb1b73fb8e4` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Logic/GoNBlockoutKit.cpp` | `19f52da3a254` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Logic/GoNGymLayout.cpp` | `9a423f28ef6b` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Logic/GoNMovementGymCheck.cpp` | `c1af18fbfe2c` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Tests/GoNBlockoutAutomationTests.cpp` | `759a53604e01` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Tests/Logic/GoNBlockoutKitTests.cpp` | `02a824d9a5bb` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Tests/Logic/GoNBlockoutLogicTests.cpp` | `9e0e2e2a6535` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Tests/Logic/GoNMovementGymCheckTests.cpp` | `4c3f9067c5f2` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/GoNBlockoutLog.h` | `512eb734d494` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/GoNBlockoutPiece.h` | `8f5ef4cc002e` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/GoNGymBuilder.h` | `72b3b681a632` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/GoNGymGameMode.h` | `95d9938465b1` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/GoNMarkers.h` | `f05eb297aa20` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/Logic/GoNBlockoutKit.h` | `e63fc3b711bd` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/Logic/GoNGymLayout.h` | `d3cd05879084` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/Logic/GoNMovementGymCheck.h` | `6aa6c29baae1` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/GoNCharacter.Build.cs` | `effa1b9d230e` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/GoNAnimInstance.cpp` | `d59e609280c5` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/GoNCharacter.cpp` | `3d00757b13fd` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/GoNCharacterModule.cpp` | `c7b2091e1e3c` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/GoNCharacterMovementComponent.cpp` | `d69672d6861d` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/GoNInputSet.cpp` | `646adf4dec10` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/GoNMovementSettings.cpp` | `29c4fc80d2d9` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/GoNPlayerController.cpp` | `f643b4d9217f` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/GoNWoundedComrade.cpp` | `8744e8b1cb58` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/Logic/GoNAnimLogic.cpp` | `1ab6c99ab8c2` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/Logic/GoNMovementLogic.cpp` | `a4a2cc6d868e` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/Tests/GoNCharacterAutomationTests.cpp` | `4b014b498b22` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/Tests/Logic/GoNCharacterLogicTests.cpp` | `b505cb0f619c` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/GoNAnimInstance.h` | `dc42f9080a29` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/GoNCharacter.h` | `5ee3096b713e` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/GoNCharacterLog.h` | `ec27eb1b70ca` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/GoNCharacterMovementComponent.h` | `d76f83d08dd7` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/GoNInputSet.h` | `e04915b2d290` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/GoNMovementSettings.h` | `7f8554d1e6ed` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/GoNPlayerController.h` | `96b706996d7f` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/GoNWoundedComrade.h` | `4e48a5040537` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/Logic/GoNAnimLogic.h` | `3d4925551dc1` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/Logic/GoNMovementLogic.h` | `c4e0daea4787` |
| `Plugins/GhostsOfNovember/Source/GoNCombat/GoNCombat.Build.cs` | `e705f8e14adb` |
| `Plugins/GhostsOfNovember/Source/GoNCombat/Private/GoNCombatModule.cpp` | `e55742184163` |
| `Plugins/GhostsOfNovember/Source/GoNCombat/Public/GoNCombatLog.h` | `ccb41fd30721` |
| `Plugins/GhostsOfNovember/Source/GoNCore/GoNCore.Build.cs` | `45e6af2e629a` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNBuildInfo.cpp` | `2b7da2429796` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNCoreModule.cpp` | `d3e2ffc5e189` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNDeveloperSettings.cpp` | `190dd452cf8a` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNGameMode.cpp` | `276dc846b881` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNSaveGame.cpp` | `d04c7b957e81` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/Tests/GoNCoreAutomationTests.cpp` | `4e659756dfdf` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/Tests/Logic/GoNCoreLogicTests.cpp` | `a8e2498b6a1a` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNBuildInfo.h` | `7f926f0c535b` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNCoreLog.h` | `c2f4c71292da` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNDeveloperSettings.h` | `20e34be48c4b` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNGameMode.h` | `fdb4297d3553` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNLogicAutomation.h` | `50b27e45e283` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNSaveGame.h` | `9f1f71aabc78` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNStringConv.h` | `f6ba671fc543` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNUnrealFileSystem.h` | `57bf3a74ca5e` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/Logic/GoNDate.h` | `d8dad89ad44e` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/Logic/GoNFileSystem.h` | `de6383bed46e` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/Logic/GoNJson.h` | `c9c7a837d0b5` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/Logic/GoNLogicTest.h` | `ce41ae7aade7` |
| `Plugins/GhostsOfNovember/Source/GoNKnowledge/GoNKnowledge.Build.cs` | `ded836aad4cd` |
| `Plugins/GhostsOfNovember/Source/GoNKnowledge/Private/GoNKnowledgeModule.cpp` | `2e4c3e93269f` |
| `Plugins/GhostsOfNovember/Source/GoNKnowledge/Public/GoNKnowledgeLog.h` | `9ea3d10289aa` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/GoNNarrative.Build.cs` | `2aa83b9c8693` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/GoNNarrativeCommands.cpp` | `9704968894d2` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/GoNNarrativeModule.cpp` | `097d8a899fdf` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/GoNNarrativeSubsystem.cpp` | `22a3ef01eb38` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/GoNTimelineValidator.cpp` | `2a4fde399cfc` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNCampaignState.cpp` | `8a80cf47cc24` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNConsequences.cpp` | `7f37c91b6eae` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNMissionRunner.cpp` | `1f652b839aa1` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNNarrativeLoader.cpp` | `7082a64e9020` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNTimeline.cpp` | `9ba0b0adf730` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Tests/GoNNarrativeAutomationTests.cpp` | `d73e1a1917fe` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Tests/Logic/GoNNarrativeLogicTests.cpp` | `4ef3dfe1f382` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/GoNNarrativeLog.h` | `bc948d7f5fde` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/GoNNarrativeSubsystem.h` | `5a498a9455c2` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/GoNTimelineValidator.h` | `f0092b2f69f3` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNCampaignState.h` | `b7161eead79c` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNConsequences.h` | `f8e82669df15` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNMissionRunner.h` | `a0911b3618e4` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNNarrativeLoader.h` | `43dc01d3bb24` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNNarrativeTypes.h` | `c0df64a63717` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNTimeline.h` | `2caf4826cdd6` |
| `Plugins/GhostsOfNovember/Source/GoNSociety/GoNSociety.Build.cs` | `0f390f495255` |
| `Plugins/GhostsOfNovember/Source/GoNSociety/Private/GoNSocietyModule.cpp` | `c147c84bc8f6` |
| `Plugins/GhostsOfNovember/Source/GoNSociety/Public/GoNSocietyLog.h` | `a79e0f1e7e28` |
| `Plugins/GhostsOfNovember/Source/GoNStealth/GoNStealth.Build.cs` | `3b07ff9c1970` |
| `Plugins/GhostsOfNovember/Source/GoNStealth/Private/GoNStealthModule.cpp` | `b9d2a91d50bb` |
| `Plugins/GhostsOfNovember/Source/GoNStealth/Public/GoNStealthLog.h` | `5e762b453cbf` |
| `Plugins/GhostsOfNovember/Source/GoNSurvival/GoNSurvival.Build.cs` | `02a6b6c3fba8` |
| `Plugins/GhostsOfNovember/Source/GoNSurvival/Private/GoNSurvivalModule.cpp` | `b50b21ac974e` |
| `Plugins/GhostsOfNovember/Source/GoNSurvival/Public/GoNSurvivalLog.h` | `fca2892827d3` |
| `Plugins/GhostsOfNovember/Source/GoNUI/GoNUI.Build.cs` | `bfea2828740f` |
| `Plugins/GhostsOfNovember/Source/GoNUI/Private/GoNUIModule.cpp` | `41ab83776b9c` |
| `Plugins/GhostsOfNovember/Source/GoNUI/Public/GoNUILog.h` | `2e8b78f0db1c` |
| `Tools/LogicTests/CMakeLists.txt` | `a0f33f39415c` |
| `Tools/LogicTests/main.cpp` | `528fff576e06` |
| `Tools/LogicTests/ue_macro_shim.h` | `7be579a54406` |
| `Tools/hash_sources.py` | `4453bc67a3a8` |
| `Tools/run_logic_tests.bat` | `0996d709480c` |
| `Tools/run_logic_tests.sh` | `fa8dc3055cf2` |
| `build.bat` | `2ed61a50cf01` |
| `run_tests.bat` | `e93473325cdb` |

## M0 + M1 — plugin skeleton, test harness, C++ gym, narrative core (2026-10-28)

**Rebuild:** full rebuild with the editor closed (`build.bat`). Every header is new.

**What's in it**
- `GhostsOfNovember` plugin with 13 runtime modules. Each has a `LogGoN<System>` category, a compile-time stamp (`gon.core.buildinfo`) and `gon.<system>.*` commands.
- **GoNCore:**
  - `AGoNGameMode` and `UGoNSaveGame`;
  - `UGoNDeveloperSettings` (*Project Settings → Game → Ghosts of November*);
  - header-only pure utilities: JSON, dates, the file-system seam and the logic-test registry;
  - Unreal automation adapters (`GON_IMPLEMENT_LOGIC_AUTOMATION_TESTS`).
- **GoNNarrative** (pure C++ + thin wrappers):
  - beat graph runner, consequence state, campaign state/checkpoint payload;
  - JSON loader with typo detection;
  - mission-graph validation, timeline/history validation (`UGoNTimelineValidator`);
  - `UGoNNarrativeSubsystem` and the `gon.narrative.*` / `gon.timeline.validate` commands.
- **GoNBlockout:** pure gym layout and metrics validation, `AGoNGymBuilder` (instanced cubes and labels), `AGoNGymGameMode`, `gon.gym.*`.
- **Data:**
  - Chapter 1 missions M-A / M-B / M-C as skeletons: design checklist, timeline, beats, branches, checkpoints; placeholders where your sequences go;
  - `gym_test_mission`;
  - historical figures with presence windows (`review: pending`) and fictional Aurès fighters (`approved: false`);
  - sources from design doc §14.
- **Prototype corrections in data:**
  - Ben Boulaïd's 10 briefing lines are kept, tagged dramatized;
  - Ben M'Hidi / Didouche / Zighoud are out of his squad, and a test proves the old setup fails validation;
  - no traffic, wanted or car code.
- **Tooling:** `build.bat` (refuses to run while the editor is open; prints DLL vs. source times), `run_tests.bat`, `Tools/LogicTests` (CMake), `Tools/run_logic_tests.{bat,sh}`, `Tools/hash_sources.py`.

**Header changes (new):**
- GoNCore: `GoNBuildInfo.h`, `GoNCoreLog.h`, `GoNDeveloperSettings.h`, `GoNGameMode.h`, `GoNLogicAutomation.h`, `GoNSaveGame.h`, `GoNStringConv.h`, `GoNUnrealFileSystem.h`, `Logic/GoNDate.h`, `Logic/GoNFileSystem.h`, `Logic/GoNJson.h`, `Logic/GoNLogicTest.h`
- GoNNarrative: `GoNNarrativeLog.h`, `GoNNarrativeSubsystem.h`, `GoNTimelineValidator.h`, `Logic/GoNCampaignState.h`, `Logic/GoNConsequences.h`, `Logic/GoNMissionRunner.h`, `Logic/GoNNarrativeLoader.h`, `Logic/GoNNarrativeTypes.h`, `Logic/GoNTimeline.h`
- GoNBlockout: `GoNBlockoutLog.h`, `GoNGymBuilder.h`, `GoNGymGameMode.h`, `Logic/GoNGymLayout.h`
- Other modules: `GoN<System>Log.h`

**Verified here (no Unreal available):**
- The pure core plus all 30 logic tests (221 checks) build and pass with GCC 13, C++20, `-Wall -Wextra -Wshadow -Werror -fno-exceptions -fno-rtti`.
- They also pass as a per-module unity build with Unreal/Windows macro names force-included.
- `--validate` reports 0 errors in the shipped data. Its 13 warnings are unapproved placeholder names and the two cinematic/scene beats that wait for your sequences.

**Not verified (needs your machine):**
- The Unreal wrappers (`UCLASS`es, console commands, gym actor and game mode) were written against the UE 5.7 API but not compiled here.

**Your checks**
1. `build.bat` builds without errors. `gon.core.modules` lists 13/13 loaded, and `gon.core.buildinfo` shows today's compile time.
2. `run_tests.bat` passes every `GoN.*` test: `GoN.Core.*`, `GoN.Narrative.Logic.*`, `GoN.Narrative.TimelineValidator`, `GoN.Blockout.Logic.*`.
3. Play, then `gon.gym.open`: the gym appears with labelled cover, vault, climb, stairs, ramps and doorways, lit, and you spawn facing the courses.
4. In Play: `gon.narrative.debug 1`, then `gon.narrative.autorun`. The log walks `gym_test_mission` from `reach_cover` to `end_with_help` and saves at `cp_after_radio`. `gon.narrative.load` resumes at the checkpoint.
5. `gon.timeline.validate` (editor, no Play) reports 0 errors. In a copy of `ch1_mb_first_november.json`, add `"ben_mhidi"` to `timeline.characters`; the validator now reports `history.region`.

**Files and hashes**

| File | SHA-256 (12) |
|---|---|
| `Plugins/GhostsOfNovember/Data/Narrative/campaign.json` | `5b761e80632b` |
| `Plugins/GhostsOfNovember/Data/Narrative/characters.json` | `94c2f4a0185d` |
| `Plugins/GhostsOfNovember/Data/Narrative/consequences.json` | `9b214780f3e9` |
| `Plugins/GhostsOfNovember/Data/Narrative/missions/ch1_ma_last_night.json` | `eef5ac7f0335` |
| `Plugins/GhostsOfNovember/Data/Narrative/missions/ch1_mb_first_november.json` | `288a678dd722` |
| `Plugins/GhostsOfNovember/Data/Narrative/missions/ch1_mc_road_to_mountain.json` | `bbaf86e53963` |
| `Plugins/GhostsOfNovember/Data/Narrative/missions/gym_test_mission.json` | `81762d5e9378` |
| `Plugins/GhostsOfNovember/Data/Narrative/places.json` | `ccb29d290bc2` |
| `Plugins/GhostsOfNovember/Data/Narrative/sources.json` | `b2a0f0fcf14b` |
| `Plugins/GhostsOfNovember/GhostsOfNovember.uplugin` | `6528954b3411` |
| `Plugins/GhostsOfNovember/Source/GoNAI/GoNAI.Build.cs` | `fe48ad35e1f2` |
| `Plugins/GhostsOfNovember/Source/GoNAI/Private/GoNAIModule.cpp` | `f4cc110551cc` |
| `Plugins/GhostsOfNovember/Source/GoNAI/Public/GoNAILog.h` | `be77be9ad5b3` |
| `Plugins/GhostsOfNovember/Source/GoNAir/GoNAir.Build.cs` | `6ba744189f84` |
| `Plugins/GhostsOfNovember/Source/GoNAir/Private/GoNAirModule.cpp` | `fa863b9bc4e3` |
| `Plugins/GhostsOfNovember/Source/GoNAir/Public/GoNAirLog.h` | `8f8bbd0776a6` |
| `Plugins/GhostsOfNovember/Source/GoNAudio/GoNAudio.Build.cs` | `4ae17c7cdf20` |
| `Plugins/GhostsOfNovember/Source/GoNAudio/Private/GoNAudioModule.cpp` | `ece4b8c24a3e` |
| `Plugins/GhostsOfNovember/Source/GoNAudio/Public/GoNAudioLog.h` | `2f4121404c66` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/GoNBlockout.Build.cs` | `c18dbcd1b517` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/GoNBlockoutModule.cpp` | `2d6fd774bc61` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/GoNGymBuilder.cpp` | `aab94fc37ee7` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/GoNGymGameMode.cpp` | `c19402c0dd51` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Logic/GoNGymLayout.cpp` | `9a423f28ef6b` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Tests/GoNBlockoutAutomationTests.cpp` | `759a53604e01` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Private/Tests/Logic/GoNBlockoutLogicTests.cpp` | `9e0e2e2a6535` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/GoNBlockoutLog.h` | `512eb734d494` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/GoNGymBuilder.h` | `a1b86ff063e6` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/GoNGymGameMode.h` | `b3eeb615fda1` |
| `Plugins/GhostsOfNovember/Source/GoNBlockout/Public/Logic/GoNGymLayout.h` | `d3cd05879084` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/GoNCharacter.Build.cs` | `4ff5fbb6c7a7` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Private/GoNCharacterModule.cpp` | `f1635e65deb9` |
| `Plugins/GhostsOfNovember/Source/GoNCharacter/Public/GoNCharacterLog.h` | `ec27eb1b70ca` |
| `Plugins/GhostsOfNovember/Source/GoNCombat/GoNCombat.Build.cs` | `e705f8e14adb` |
| `Plugins/GhostsOfNovember/Source/GoNCombat/Private/GoNCombatModule.cpp` | `e55742184163` |
| `Plugins/GhostsOfNovember/Source/GoNCombat/Public/GoNCombatLog.h` | `ccb41fd30721` |
| `Plugins/GhostsOfNovember/Source/GoNCore/GoNCore.Build.cs` | `45e6af2e629a` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNBuildInfo.cpp` | `2b7da2429796` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNCoreModule.cpp` | `d3e2ffc5e189` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNDeveloperSettings.cpp` | `190dd452cf8a` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNGameMode.cpp` | `276dc846b881` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/GoNSaveGame.cpp` | `d04c7b957e81` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/Tests/GoNCoreAutomationTests.cpp` | `4e659756dfdf` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Private/Tests/Logic/GoNCoreLogicTests.cpp` | `a8e2498b6a1a` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNBuildInfo.h` | `7f926f0c535b` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNCoreLog.h` | `c2f4c71292da` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNDeveloperSettings.h` | `9946f35760a4` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNGameMode.h` | `fdb4297d3553` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNLogicAutomation.h` | `50b27e45e283` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNSaveGame.h` | `9f1f71aabc78` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNStringConv.h` | `f6ba671fc543` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/GoNUnrealFileSystem.h` | `57bf3a74ca5e` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/Logic/GoNDate.h` | `d8dad89ad44e` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/Logic/GoNFileSystem.h` | `de6383bed46e` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/Logic/GoNJson.h` | `c9c7a837d0b5` |
| `Plugins/GhostsOfNovember/Source/GoNCore/Public/Logic/GoNLogicTest.h` | `ce41ae7aade7` |
| `Plugins/GhostsOfNovember/Source/GoNKnowledge/GoNKnowledge.Build.cs` | `ded836aad4cd` |
| `Plugins/GhostsOfNovember/Source/GoNKnowledge/Private/GoNKnowledgeModule.cpp` | `2e4c3e93269f` |
| `Plugins/GhostsOfNovember/Source/GoNKnowledge/Public/GoNKnowledgeLog.h` | `9ea3d10289aa` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/GoNNarrative.Build.cs` | `2aa83b9c8693` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/GoNNarrativeCommands.cpp` | `9704968894d2` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/GoNNarrativeModule.cpp` | `097d8a899fdf` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/GoNNarrativeSubsystem.cpp` | `e29b3a59ed42` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/GoNTimelineValidator.cpp` | `2a4fde399cfc` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNCampaignState.cpp` | `8a80cf47cc24` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNConsequences.cpp` | `7f37c91b6eae` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNMissionRunner.cpp` | `1f652b839aa1` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNNarrativeLoader.cpp` | `28c4e6a3c915` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Logic/GoNTimeline.cpp` | `a2bee4b74af9` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Tests/GoNNarrativeAutomationTests.cpp` | `d73e1a1917fe` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Private/Tests/Logic/GoNNarrativeLogicTests.cpp` | `bb25e755d834` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/GoNNarrativeLog.h` | `bc948d7f5fde` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/GoNNarrativeSubsystem.h` | `a6a1257ea5e4` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/GoNTimelineValidator.h` | `f0092b2f69f3` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNCampaignState.h` | `b7161eead79c` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNConsequences.h` | `f8e82669df15` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNMissionRunner.h` | `a0911b3618e4` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNNarrativeLoader.h` | `43dc01d3bb24` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNNarrativeTypes.h` | `9207e07f3af0` |
| `Plugins/GhostsOfNovember/Source/GoNNarrative/Public/Logic/GoNTimeline.h` | `2caf4826cdd6` |
| `Plugins/GhostsOfNovember/Source/GoNSociety/GoNSociety.Build.cs` | `0f390f495255` |
| `Plugins/GhostsOfNovember/Source/GoNSociety/Private/GoNSocietyModule.cpp` | `c147c84bc8f6` |
| `Plugins/GhostsOfNovember/Source/GoNSociety/Public/GoNSocietyLog.h` | `a79e0f1e7e28` |
| `Plugins/GhostsOfNovember/Source/GoNStealth/GoNStealth.Build.cs` | `3b07ff9c1970` |
| `Plugins/GhostsOfNovember/Source/GoNStealth/Private/GoNStealthModule.cpp` | `b9d2a91d50bb` |
| `Plugins/GhostsOfNovember/Source/GoNStealth/Public/GoNStealthLog.h` | `5e762b453cbf` |
| `Plugins/GhostsOfNovember/Source/GoNSurvival/GoNSurvival.Build.cs` | `02a6b6c3fba8` |
| `Plugins/GhostsOfNovember/Source/GoNSurvival/Private/GoNSurvivalModule.cpp` | `b50b21ac974e` |
| `Plugins/GhostsOfNovember/Source/GoNSurvival/Public/GoNSurvivalLog.h` | `fca2892827d3` |
| `Plugins/GhostsOfNovember/Source/GoNUI/GoNUI.Build.cs` | `bfea2828740f` |
| `Plugins/GhostsOfNovember/Source/GoNUI/Private/GoNUIModule.cpp` | `41ab83776b9c` |
| `Plugins/GhostsOfNovember/Source/GoNUI/Public/GoNUILog.h` | `2e8b78f0db1c` |
| `Tools/LogicTests/CMakeLists.txt` | `1a98430cb803` |
| `Tools/LogicTests/main.cpp` | `528fff576e06` |
| `Tools/LogicTests/ue_macro_shim.h` | `7be579a54406` |
| `Tools/hash_sources.py` | `4453bc67a3a8` |
| `Tools/run_logic_tests.bat` | `0996d709480c` |
| `Tools/run_logic_tests.sh` | `fa8dc3055cf2` |
| `build.bat` | `26779123534e` |
| `run_tests.bat` | `44f34bd58284` |
