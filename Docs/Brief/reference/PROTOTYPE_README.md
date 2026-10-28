# أشباح نوفمبر — Ghosts of November · Handoff (2026-10-28)

A third-person action game in Unreal Engine **5.7** about the Algerian resistance (1 November 1954).
The player is the leader **مصطفى بن بولعيد** with a fire team of three (**بن مهيدي، ديدوش، زيغود**) fighting French army patrols in a procedural city.
Chapter 1, "السلاح من يد العدو": kill the patrol, seize their rifles and ammo crate, return to the rally point.

> **Note for the model taking over:** read this whole file before touching code. The rules in §7 come from real crashes and lost work.

---

## 1. What is in the two zips

**`GhostsOfNovember_Handoff_Core.zip`** (about 60 MB) — everything you edit:

```
GhostsOfNovember_Handoff/
  README.md                     <- this file
  Project/                      <- the Unreal project (MyProject2), without Binaries/Intermediate/Saved/DDC
    MyProject2.uproject         (UE 5.7)
    Config/                     (GameDefaultMap = /Game/ThirdPerson/Lvl_ThirdPerson)
    Source/MyProject2/          (the template game module, barely used)
    Plugins/
      AAAWeapons/               <- THE GAME (C++ only, about 1.2 MB of source)
      AAATraffic/               <- cars, traffic lights, police chases, WANTED stars (AAAWeapons depends on it)
      AAARocketCar/             <- the drivable Tesla (E to get in / out)
    Content/AAACity/
      UI/GhostsNovember/        <- HUD art (PNG, loaded at runtime from disk) + Fonts (Amiri-Bold, CourierPrime-Bold)
      HUD/  Sounds/  Weapons/  PlayerCar/  traffic.json
  CityBuild/                    <- how the city level is generated
    aaa_city_setup.py           <- ONE-CLICK: imports city.glb, places tiles / collision / instanced props / traffic / car
    aaa_optimize.py             <- Nanite on leaves, HISM cull distances, shadows (also run by setup)
    aaa_city_live.py            <- live link from the web editor (optional)
    unreal_manifest.json  city.json  traffic.json  player_car.glb  sfx/  hud/  ...
```

**`GhostsOfNovember_Handoff_LargeAssets.zip`** (about 520 MB, unzip over the same folder):
- `CityBuild/city.glb` (318 MB): the whole city.
- `CityBuild/vehicles.glb` (73 MB).
- `Project/Content/Characters/Mannequins/`: UE5 Manny / Quinn mesh + **Rifle / Pistol / Unarmed anim sets**. The code loads them by path.

Not included, because they can be rebuilt: `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, the imported city assets (`Content/AAACity/city`, `Content/AAATraffic`), and the level actors. `aaa_city_setup.py` regenerates all of them.

## 2. Rebuild from zero

1. Unzip both zips into one folder. Rename `Project/` to `MyProject2/` if you like.
2. Open `MyProject2.uproject` with UE 5.7. Answer **Yes** to "rebuild modules" (Visual Studio 2022 + C++ workload required).
3. Enable the **Python Editor Script Plugin** (Edit → Plugins) and restart.
4. Open an empty level (or `Lvl_ThirdPerson`), then run **Tools → Execute Python Script → `CityBuild/aaa_city_setup.py`**.
   - The first run may copy or refresh plugins and ask for a restart; run it again after the restart.
5. Press Play.
   - The chapter 1 intro starts automatically.
   - Console `aaa.Mission 0` = free roam with no mission.

## 3. Architecture (plugin `AAAWeapons`, everything is C++, no Blueprints needed)

| File | Role |
|---|---|
| `AAAWeaponsSubsystem` (TickableWorldSubsystem) | Gives the player's character `UAAAWeaponComponent`. Spawns and manages the **fire team** (3 men, 2 s after start), the **army patrols**, and the police on foot (from AAATraffic dismounts). Owns "the radio" (shared contacts, callouts, cover-fire requests, medic claims). |
| `AAAWeaponComponent` | The hero: weapons (STG44 rifle, MG42 LMG, P08 pistol, Kar98k sniper; procedural meshes), simulated bullets, recoil and camera shake, reload stages, grenades, binoculars, health / regen, **death (shahada scene) + respawn**, crouch & smart cover, climbing / vaulting (with a baked Mixamo clip), the radial wheel, squad orders input, an old canvas HUD. |
| `AAARifleAnimInstance` (native proxy, **no anim graph**) | All character animation for the hero AND every soldier: 8-way locomotion with measured play rates, jump states, hit reacts, death clips, aim pitch, two-bone IK for both hands on the weapon (left hand on the forend, IK + FK guard), Kar98k bolt / reload choreography, grenade throw, planted feet / turn in place, the crouch ("same walk, only lower"), climbing clip retarget, talking heads, the shahada pose. |
| `AAAPoliceOfficer` | One soldier class for every side (`Team`: Player / Police / Enemy). Tactical brain (`ThinkTactical`): contact memory, threat score, cover search, peeking, overwatch, flanking, trades (rifleman / marksman / support), downed + revive, grenade evasion, active ragdoll. |
| `AAAMissionSubsystem` (+ `AAAMissionAudio.h`) | **The game loop / chapter 1**: Boot → Intro (title cards) → Briefing (in-engine cinematic camera + dialogue) → Play (objectives, loot pickups, weapon lock, reinforcements, checkpoint) → Outro → Done (free roam). It also runs the hero's **death scene**. Sounds and music are synthesised in code. |
| `AAAGhostsHud` (+ `AAASmartHudWidget`) | The Slate HUD "أشباح نوفمبر": dossier, compass, detection eye, health / stamina, weapon panel, radial wheel, squad cards, radio callouts, world markers (rooster for enemies), mission overlay (cinema bars, cards, subtitles, objectives, banner, marker, E prompt). Arabic is shaped with `ShapeBidirectionalText`, RTL. |
| `AAAGrenade` | Tumbling throw, spoon, fuse, 44 fragment rays, warnings so soldiers evade. |
| `AAAClip*.h` | Mixamo clips baked into C++ arrays: braced hang, crouch walk (unused now), throw, sniper fire (only timing used). |
| `AAAFireTeamTypes.h` | Teams, orders, formations, squad names and colours. |
| `AAASquadManagerComponent`, `AAASquadMemberCharacter`, `AAASquadTypes.h` | **Inactive** older system from a parallel session. Kept but not attached; the user chose the subsystem / officer version. |

**Teams:** player vs army always; player vs police only while WANTED; police and army ignore each other.

## 4. Controls

| Key | Action |
|---|---|
| WASD / mouse | Move / look |
| LMB / RMB | Fire / aim |
| R | Reload |
| Space | Jump / climb / vault |
| C | Crouch (auto cover near low walls) |
| Shift | Sprint |
| F1 / F2 / F3 / F4 | Rifle / LMG / pistol / Kar98k (in chapter 1 locked until looted) |
| TAB / Q (hold) | Radial wheel (weapons, equipment, orders) |
| G | Cook / throw grenade |
| MMB | Context order (assault the enemy under the aim, or move there) |
| V / Y / U | Follow / hold / suppress |
| 1–3 / 4 | Select a man / select all |
| N | Formation |
| K | Hold fire |
| F (hold) | First aid |
| E | Take loot / enter a car |
| Enter / Space / Backspace | Next line / skip a cinematic |

## 5. Chapter 1 flow (`AAAMissionSubsystem.cpp`)

1. **Intro:** black cards (typed) → "الجزائر — ليلة الفاتح من نوفمبر 1954" … → title "أشباح نوفمبر / الفصل الأول — السلاح من يد العدو". Music: synth drone + bendir pulse.
2. **Briefing:** 5 s establishing aerial shot, then close shots on each speaker (depth of field, hand-held drift, the speaker nods via anim input `TalkT`), ending with "تحيا الجزائر!".
3. **Play:**
   - **Start:** pistol + 16 rounds (`bInfiniteReserve` off). Camp 55–85 m away at street level (`StreetSpot` keeps everything off roofs). 6 soldiers (3 guards + 3 on a beat) and an ammo crate.
   - **Loot:** each kill drops a rifle + ammo pickup. **E**:
     - Rifle: +30 rounds; the first rifle is equipped.
     - Crate: +120 rounds, resupplies the squad, unlocks the LMG.
   - **Reinforcements:** 2 more soldiers at 4 kills.
   - **Low-ammo hint.**
   - **Objectives** top right; **marker** with distance (kept inside a screen ellipse); return to the rally point.
4. **Outro:** 2 dialogue shots → "تمّت المهمة" with stats → "نهاية الفصل الأول — يتبع…" → free roam.
5. **Death (any phase):**
   - Slow-motion fall (time dilation 0.35).
   - Procedural **lying-on-the-back** pose (the death clips stop mid-fall).
   - The right index finger is raised to the sky (fist from the idle grip + straight index, arm IK).
   - Subtitles "أشهدُ أن لا إله إلا الله" / "وأشهدُ أنّ محمدًا رسولُ الله"; heartbeat slowing.
   - Card "استُشهد البطل — والثورة لا تموت.", then black.
   - Respawn at the rally point.

## 6. Status (2026-10-28)

**Works (confirmed in the user's screenshots):** HUD, chapter 1 objectives / loot / markers, squad, enemies, the shahada subtitles and camera.

**Written but not yet confirmed in game** (the last full build was before some of it):
- The lying pose with the raised finger.
- Polish pass 1 (sounds, music, establishing shot, depth of field, talking heads, reinforcements, checkpoint).
- `aaa_optimize.py`.

**Measured:** GPU about 8.6 ms per frame (fine). The CPU side (`stat unit`, `stat game`) has not been measured; officers / squad use `OnlyTickMontagesWhenNotRendered` + URO.

**Ideas not done:** recorded voices (dialogue / shahada), French enemy callouts, location caption, more chapters, a proper save / checkpoint system, packaged-build staging (add `Content/AAACity/UI/GhostsNovember` to "Additional Non-Asset Directories to Package").

## 7. Rules that matter (learned the hard way)

1. **A change to any `.h` (a new member, field or UCLASS) needs the editor CLOSED and a full build.** Live Coding with a changed class layout crashed the editor (`~FAAARifleProxy` access violation) and silently ran stale code. Safe way: close Unreal, delete `Plugins/AAAWeapons/Binaries` and `Intermediate`, reopen, rebuild. Live Coding (Ctrl+Alt+F11) is only for `.cpp`-only edits.
2. **Before diagnosing a screenshot or video, compare the DLL time** (`Plugins/AAAWeapons/Binaries/Win64/UnrealEditor-AAAWeapons.dll` and `patch_N` files) with the source times. Many "bugs" were old builds.
3. **One editor of the plugin at a time.** Two sessions editing `AAARifleAnimInstance.cpp` in parallel overwrote each other. Check md5s before writing.
4. Unreal treats **shadowed names as errors** (a local or lambda parameter that hides a class member or an outer local, e.g. a local `Fall` vs the proxy's `Fall` sequence). Pick unique names.
5. **Arabic text:** always shape with `FontCache->ShapeBidirectionalText(..., RightToLeft)` using the Amiri composite font. Plain `DrawText` breaks the letters.
6. **Ground traces must start low** (≤ 1.5 m above the point) or they land on roofs, awnings and tree crowns (that bug put loot on a roof).
7. The F keys also trigger UE debug view modes in PIE; the component resets `viewmode lit` after any F key.
8. The anim instance has **no graph**: everything is in `FAAARifleProxy::Update` / `Evaluate`. Inputs come in through `FAAARifleInputs` (set each tick by the component / officer / mission). The order of the stages in `Evaluate` matters: locomotion → clips → planted feet → ground IK → crouch → aim / recoil → arms IK → reload / bolt.
9. UE mannequin axes: in component space use `FwdCS` / `UpCS` / `Axis` (computed from the actor), never raw X / Y.

## 8. Where the detailed history is

The design notes for every system (numbers, timelines, why things are the way they are) are in the claude.ai Project **"smat city"**:
- `chapter1-game-loop`, `polish-pass-1`, `hero-death-shahada`, `fire-team-ai`, `grenade-frag`
- `kar98-clips-decision`, `crouch-rebuild`, `feet-planted-turn-in-place`, `crouch-cover`
- `climbing`, `weapons-2026-10-23`, `weapons-shot-reference`, `hud-ghosts-november`
- `optimization`, `traffic-system`, `traffic-ai`, `handoff-2026-10-27`

Ask the user to export or paste them if you cannot reach the project.
