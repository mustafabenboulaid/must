# أشباح نوفمبر — Ghosts of November · Build Brief v3 (2026-10-28)

> **For the model building the game.**
>
> - **The source of truth for story and design** is `design/Ashbah_November_Narrative_Gameplay_Design_AR.docx` (the full text is in `design/DESIGN_DOC_AR.md`). This brief turns it into an engineering plan.
> - **Where they disagree, the design doc wins** on story and history; this brief wins on code and process.
> - **Your job:** you write C++ (and HTML/JS for map screens). The user builds in Unreal 5.7, tests, makes the levels (blockouts), and sends you build errors, logs, test reports, screenshots and videos.
> - **If you run on the user's machine** (with access to the project folder and UE installed), build and run the automation tests yourself (§8).
>
> Read everything before writing code.

---

## 1. The game (from the design doc)

**Genre:** a linear historical action-adventure in third person, with stealth, investigation, survival and interactive scenes. Combat matters but is resource-limited; it never replaces infiltration, communication, survival and civilian work.

**Campaign:** a prologue + 7 chapters, from the secret meetings before 1 Nov 1954 to independence on 5 Jul 1962. There is no single super-hero: protagonists rotate between historical figures (within their documented roles) and fictional characters (who fill dramatic gaps).

| # | Chapter | Period / place | Core play (the mechanic carries the meaning) |
|---|---|---|---|
| 0 | Prologue — the scar of 1945 | Sétif / Guelma / Kherrata, 8 May 1945, then 1954 | Short exploration; protect a family member in the chaos. **No gunplay.** Fictional family. |
| 1 | شرارة الأوراس | Late Oct 1954 to early 1955, Aurès; **Mostefa Ben Boulaïd** | Planning, mountain crossing, resource management; terrain reading, safe routes, carrying supplies, fatigue / cold, short signals. Missions: «الليلة الأخيرة»، «الأول من نوفمبر» (intercut across several points), «الطريق إلى الجبل» (first loss). |
| 2 | الشمال القسنطيني | Jan–Aug 1955; **Zighoud Youcef** (Didouche Mourad died 18 Jan 1955, never alive after that) | Coordinating groups, timing, priorities, evacuation decisions. Success ≠ body count: objective, network kept, civilians protected. |
| 3 | العودة إلى الأوراس | Late 1955 to Mar 1956; Ben Boulaïd's last period | Social / organisational tension, rebuilding trust, saving a network. **Do not invent his last moments or words.** |
| 4 | الصومام | Spring–summer 1956, Ifri Ouzellaguen; **Abane Ramdane, Krim Belkacem** | Documents, evidence-based dialogue, linking names and facts. The big pace change: you can matter without firing a shot. |
| 5 | القصبة | Sep 1956 to Oct 1957; Hassiba Ben Bouali, Zohra Drif, Djamila Bouhired, Djamila Bouazza, Ali La Pointe (each distinct) | Urban stealth, blending into crowds, suspicion management, patrol behaviour, multiple escape routes. Ends 8 Oct 1957 as a human loss, not a reward. **No UI that teaches real bomb-making or targeting civilians.** |
| 6 | الجبال والحدود | Late 1957 to Sep 1960 | Navigation, sharing resources, protecting the wounded, maintaining the network; contradictory reports. |
| 7 | من التدويل إلى الاستقلال | Oct 1960 to Jul 1962 | Documentation / diplomacy, a civilian demonstration (non-combat goal), OAS threat after the ceasefire, a quiet interactive epilogue on 5 July. |

**Fixed dates** (the design doc's references):
- Evian Accords 18 Mar 1962; ceasefire 19 Mar; referendum 1 Jul; independence 5 Jul.
- Ben Boulaïd died in March 1956 (sources differ: 22 or 23 March; the historical consultant decides).

## 2. Historical integrity rules (enforced in data and code)

1. **One central timeline.** Every mission records start / end date, place and characters present. A tool validates that no historical character is in two places at once (§4, `UGoNTimelineValidator`).
2. **Historical characters are not moved between regions for the plot.**
3. **Every line attributed to a real person needs a source.** Otherwise it is tagged `bDramatized = true`. The subtitle and codex show the tag (e.g. a small «حوار درامي متخيّل» marker, configurable).
4. **Fictional characters never take the achievements of real ones.**
5. **No faction is a monolith:**
   - Harkis, settlers, French soldiers and civilians are written through motive, context and action.
   - Repression is never justified, and victims are never erased.
6. **Violence is never a visual reward:**
   - No points for harming civilians or prisoners.
   - Consequences are shown in scenes, reactions and evaluation.
   - No interactive torture.
7. **Disputed facts go to the in-game codex** with their sources and a certainty level, instead of a single silent version.
8. **Lalla Fatma N'Soumer does not appear** (she belongs to the 19th century): use a fictional woman inspired by women's roles.
9. **French antagonists are composite and realistic**, not cartoon villains.

## 3. Corrections to the existing prototype (must do)

| Prototype (`reference/AAAWeapons`) | Change |
|---|---|
| The player's squad with Ben Boulaïd is named بن مهيدي / ديدوش / زيغود. | They led other zones in 1954. Replace them with **fictional Aurès fighters** (names / backstories as data; the user approves). |
| Death scene: slow fall, lying pose, raised index finger, shahada subtitles, attributed to Ben Boulaïd. | Keep the system, but **never for a real figure's documented death**. Use it for the player's failure state with a fictional fighter, or make it a "mission failed" sequence. A real figure's death follows the sources only (ch. 3 rule). |
| Briefing lines spoken by Ben Boulaïd («يا الخاوة… يوم تقرير المصير»). | Keep them, tagged as dramatized. |
| The city / GTA systems (traffic, WANTED, Tesla). | Not part of this game. Don't port them. |
| The STG44 / MG42 / P08 / Kar98k weapons. | Replace with period-correct data assets: MAS-36, Lebel, MAT-49, hunting shotguns, captured MP40 / Thompson, pistols, grenades. Scarce. |

## 4. The Game Core — systems built once, used by every chapter

Everything tunable lives in **Data Assets / DataTables** (the user edits them in the editor without recompiling). Every system has:
- a log category (`LogGoN<System>`);
- `gon.<system>.*` console commands for live tuning / debug;
- an optional debug draw;
- automation tests.

| Module | System | Design doc reference | Notes |
|---|---|---|---|
| **GoNCore** | GameMode / State / Controller, Gameplay Tags, SaveGame, settings and **accessibility** (subtitle size / background, assist levels for stealth and navigation), developer settings | §2 accessibility | Save at checkpoints. Campaign state = consequences that carry over. |
| **GoNNarrative** | **Campaign → Chapter → Mission → Beat graph** (data). Each mission: visible goal, dramatic goal, obstacle, decision, consequence, transition (§12 checklist). Beats: objective, trigger, dialogue, cinematic, scene, wait, branch on state. | §5, §9, §12 | The mission graph is plain C++ logic (testable outside UE). |
| | **Consequence state:** suspicion, trust, network health, info availability, people saved / lost. Persists across missions; never changes documented history. | §6 loop "العواقب" | |
| | **Timeline + historical metadata:** date / place / characters per mission and line; `UGoNTimelineValidator` (editor + automation test). | §4 | |
| | **Dialogue:** speaker, language (ar-dz / fr / tamazight), line, VO, subtitle, `bDramatized`, source id. | §8, §10 | |
| | **Scene Director:** scripted ambient scenes (Casbah checkpoint search, ID checks, loudspeaker, patrol jeep, market life). | §10 | |
| | **Cinematic director:** plays the user's Level Sequences per beat; on-screen date / region / front status cards between chapters. | §10 | |
| **GoNStealth** | **Suspicion & exposure:** visible behavioural states (calm → noticing → suspicious → pursuit), per-observer meters driven by sight, sound, behaviour (running, weapon shown, wrong place / time), disguise / crowd blending. No magic "everyone sees you" alarm. | §6 table | MGS-like phases, readable from the NPC's body language and sound, with minimal UI. |
| **GoNSociety** | **Local trust:** civilians and districts remember the player's conduct and open or close routes, hiding places and help. Context-aware, not a black / white morality bar. | §6 | |
| | **Communication network:** messages are carried, delayed or lost; the player chooses what matters and confirms delivery; the network can break (arrests). | §6, §9 | The radio from the prototype becomes part of this. |
| **GoNSurvival** | **Fatigue & resources:** water, food, ammo, bandages, cold, carrying the wounded. Affects movement and choices. No tedious simulation, no crafting. | §5 ch. 1 / 6 | |
| **GoNKnowledge** | **Field notebook + codex:** documents, photos, newspapers, testimonies (respectfully reconstructed), each with source and certainty; separates documented fact from reconstruction. | §7 rewards | Progression is knowledge and network, not super skills. |
| | **Investigation:** document analysis, linking names / facts, evidence-based dialogue (the Soummam chapter). | ch. 4 | |
| **GoNCharacter** | Player / NPC characters, movement (crouch, prone, cover, vault / climb, carry a wounded man), camera per region / character, health. Native anim + IK. | — | Port from the prototype (§7). |
| **GoNCombat** | Scarce, period-correct weapons (data), bullets, recoil, loot, grenades, explosives; the mounted AA machine gun. | §1 "limited combat" | |
| **GoNAI** | Perception feeding GoNStealth; tactical cover / peek / flank; squad orders; archetypes: gendarmes, police, infantry, paratroopers, harkis (written with context), civilians; dogs; patrol routines and schedules. | §6 sound and environment | Port the tactical brain. |
| **GoNAir** | Aircraft (T-6, B-26, Piper spotter, H-34 helicopters): flight model, strafing / bombing runs, hit zones, physical crash with inertia. | ch. 5–6 | Lower priority (after the vertical slice). |
| **GoNUI** | Diegetic-leaning HUD (period look, few icons), subtitles with speaker / language / dramatized tag, notebook, codex, menus. **Tactical map** = `UGoNBriefingWidget` (`UWebBrowser` + bridge `gon`). | §6 "avoid icons alien to the era" | Arabic shaping (§8 rules). |
| **GoNAudio** | Music states (calm / tension / pursuit / combat / mourning), VO, environmental audio that drives decisions (steps, dogs, radios, patrols). | §6, §10 | |
| **GoNBlockout** | Placeable C++ blockout pieces with gameplay metrics and auto-generated collision, nav and cover points (douar house, Casbah alley / roof bridge, wall, stairs, rock, slope, cover), colour-coded.<br>Gameplay placeables: spawn, patrol path, observer post, crowd zone, trust zone, message drop, trigger, scene point, checkpoint. | — | The user builds every level with these. |

**Core loop per mission (design doc §6):** observe → prepare (route / partner / comm tool / resource) → execute (stealth / dialogue / survival / combat when necessary / civilian task) → consequences (suspicion / trust / group state / information) → reflection (letter, newspaper, witness, silent scene).

## 5. Vertical slice = Chapter 1 «شرارة الأوراس»

Build the Core only as far as Chapter 1 needs, polish that to a high standard, then prove the Core with a contrasting chapter (5 Casbah or 4 Soummam).

**M-A «الليلة الأخيرة»**
- Meetings, contacts, checking information. Trust and time matter more than firepower.
- Systems: dialogue, network messages, trust, observation.

**M-B «الأول من نوفمبر»**
- Opens with the **tactical briefing** (`ui/briefing/briefing.html`): the 5 zones of 1 Nov 1954, then the Aurès, then a fictional composite town with three groups (gendarmerie / power station / police).
- An intercut opening across several points (operations start in several zones, not in one place).
- The chosen group's mission. Limited combat; loot of scarce weapons.
- The other groups heard over the network.

**M-C «الطريق إلى الجبل»**
- Withdrawal, terrain reading, safe routes, supplies, fatigue / cold, the first loss (a fictional comrade).

**Ending:** mixed news from the other zones (map screen). "The revolution has started; how it will survive is unknown."

**Quality indicators** (design doc §13), checked with players who don't know Algerian history:
- motive clear before mid-mission;
- every mechanic explainable by the chapter;
- the region recognisable without its name;
- a timeline with no contradiction;
- peaks and quiet moments;
- history understood without a lecture.

## 6. Milestones (each ends with a 3–5 item check the user does)

| Milestone | Content | User checks |
|---|---|---|
| **M0** | Plugin / modules skeleton, log categories, `build.bat`, automation-test harness, **test gym map generated by C++** | Builds; the gym opens |
| **M1** | Narrative graph + consequence state + save / checkpoints + timeline validator (pure C++ core + tests) | A test mission runs start → end from data |
| **M2** | Character / movement / camera / anim port + GoNBlockout kit | Moves, crouches, vaults, carries; builds a blockout |
| **M3** | Suspicion / stealth + AI perception + patrol routines + dogs / sound | A patrol notices, suspects and pursues in the gym |
| **M4** | Dialogue / subtitles (dramatized tag) + network messages + trust + notebook / codex | M-A playable in the gym |
| **M5** | Scarce combat + period weapons + loot + survival resources / fatigue | M-B and M-C mechanics in the gym |
| **M6** | Tactical map bridge, cinematic director, Scene Director, music states, menus / accessibility | Chapter 1 flow end to end on blockouts |
| **M7** | Chapter 1 polish pass (the user's levels and assets) | Vertical slice |
| **M8** | Contrast chapter (Casbah or Soummam) | — |

## 7. Reuse from the prototype (`reference/AAAWeapons/`)

**Port, don't copy blindly:**

| File | Reuse |
|---|---|
| `AAARifleAnimInstance.*` | Native proxy anim: locomotion with measured play rates, hand IK + IK / FK guard, planted feet / turn in place, crouch ("same walk, only lower"), talking heads (`TalkT`), lying pose. |
| `AAAPoliceOfficer.*` | Tactical AI brain (contact memory, threat scoring, cover, peek, overwatch, flank, downed / revive, grenade evasion, active ragdoll). It becomes the combat layer of GoNAI; GoNStealth sits in front of it. |
| `AAAMissionSubsystem.*` + `AAAMissionAudio.h` | Phases, cinematic camera (DOF, hand-held, establishing shot), typed subtitles, death-scene mechanics (see §3), procedural sounds. |
| `AAAGhostsHud.cpp` | Correct Arabic shaping in Slate. |
| `AAAGrenade.*` | Grenade physics. |

**Not to port:** traffic, WANTED, car, city HUD pieces.

## 8. Engineering rules (from real incidents)

1. **Verifiable steps.** Each step lists the files changed and 3–5 checks for the user. Clear log prefixes (`GoN Stealth:` …). A debug command per system.
2. **Pure logic in plain C++ you compile and unit-test yourself:** mission graph, consequence state, suspicion maths, trust, network delivery, resources, timeline validation, ballistics, flight. UE classes stay thin wrappers.
3. **UE automation tests** for each system. On the user's machine:
   - Build: `"<UE>\Engine\Build\BatchFiles\Build.bat" MyProject2Editor Win64 Development "<path>\MyProject2.uproject" -WaitMutex`
   - Tests: `UnrealEditor-Cmd.exe "<uproject>" -ExecCmds="Automation RunTests GoN; Quit" -unattended -nullrhi -log`
4. **Header changes need a full rebuild (editor closed), never Live Coding.** Live Coding with a changed class layout crashed the editor and ran stale code. Design headers early; announce every `.h` change.
5. **Check the build time** (DLL vs sources) before diagnosing any screenshot or video.
6. **One writer per file.** Keep a `CHANGELOG.md` with file hashes. Use Git; commit each verified step.
7. **Shadowed names are compile errors in UE** (a local or lambda parameter hiding a member or outer local).
8. **Arabic text:** always shaped (`FSlateFontCache::ShapeBidirectionalText`, RTL, Amiri or a similar font); never plain `DrawText`. The user checks Darja spelling. French and Tamazight per context.
9. **Placement traces start ≤ 1.5 m above the point** (or things land on roofs / trees). Validate spawn spots: floor height near the reference, plus a free capsule.
10. **Anim maths in component space** with `FwdCS` / `UpCS` from the actor (the mesh is rotated).
11. **Assets:** you cannot open `.uasset`. Use `ASSET_MANIFEST.md` for existing paths. For anything missing (frisk, crawl, carry-wounded, salute…), ask the user to import it and give the expected path, or expose a `UPROPERTY` slot.
12. **Content safety (design doc):**
    - No instructions for real weapons or explosives.
    - No reward for civilian harm.
    - No interactive torture.
    - Real people's words are sourced or tagged dramatized.

## 9. What the user does in Unreal

- Blockout levels (GoNBlockout), then art.
- Places the gameplay actors.
- Imports animations, sounds and VO.
- Makes Sequencer cinematics.
- Tunes the data.
- Organises the historical review and playtests.

## 10. Package contents

```
GAME_BRIEF.md                         <- this brief (v3)
design/Ashbah_November_Narrative_Gameplay_Design_AR.docx   <- the design doc (source of truth)
design/DESIGN_DOC_AR.md               <- its full text (headings + tables) for easy reading
ASSET_MANIFEST.md                     <- assets that exist in the user's project (paths, skeleton bones)
ui/briefing/briefing.html             <- chapter 1 tactical briefing (HTML/SVG; JS bridge "gon")
reference/AAAWeapons/                 <- prototype plugin source to port
reference/docs/PROTOTYPE_README.md    <- overview of the prototype
```
