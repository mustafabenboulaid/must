# Changelog

Newest first. Each step lists changed files, header changes (need a full rebuild, editor closed) and the checks to run in Unreal. File hashes come from `python Tools/hash_sources.py`; `--check` shows what changed since the newest entry.

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
