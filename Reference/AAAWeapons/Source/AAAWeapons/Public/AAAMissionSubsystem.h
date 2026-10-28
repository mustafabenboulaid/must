// AAA City Weapons - basic GAME LOOP, chapter 1 "السلاح من يد العدو" (Ghosts of November)
//
//   Boot -> Intro (title cards on black) -> Briefing (in-engine cinematic: camera shots on the men, dialogue)
//        -> Play (objectives: wipe out the French patrol, seize their rifles and the ammo crate, back to the rally point)
//        -> Outro (cinematic, chapter-end cards) -> Done (free roam)
//
// The HUD (AAAGhostsHud.cpp) draws everything this subsystem exposes: black / letterbox, the cards, the subtitles,
// the objectives, the banner, the world marker and the "E" prompt. Console: aaa.Mission 0 = no mission (free roam).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "AAAMissionSubsystem.generated.h"

class ACharacter;
class ACameraActor;
class AAAAPoliceOfficer;
class UAAAWeaponComponent;
class UAAAWeaponsSubsystem;

/** something the leader can take: an enemy rifle with its ammo (dropped where the man fell) or the ammo crate */
UCLASS(NotBlueprintable)
class AAAWEAPONS_API AAAAMissionPickup : public AActor
{
	GENERATED_BODY()
public:
	AAAAMissionPickup();
	virtual void Tick(float DeltaSeconds) override;
	/** 0 = rifle + ammo box, 1 = ammo crate */
	void Build(uint8 InKind);
	void Take();
	uint8 Kind = 0;
	bool bTaken = false;
private:
	UPROPERTY() TObjectPtr<USceneComponent> Spin;
	float T = 0.f;
};

enum class EAAAMissionPhase : uint8 { Boot, Intro, Briefing, Play, Outro, Done, Off };

UCLASS()
class AAAWEAPONS_API UAAAMissionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAAAMissionSubsystem, STATGROUP_Tickables); }
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------ read by the HUD
	EAAAMissionPhase Phase = EAAAMissionPhase::Boot;
	bool IsCinematic() const { return bDeathScene || Phase == EAAAMissionPhase::Boot || Phase == EAAAMissionPhase::Intro || Phase == EAAAMissionPhase::Briefing || Phase == EAAAMissionPhase::Outro; }
	/** the hero's death (in any phase): the slow fall, then a close shot while he raises his finger and says the shahada */
	bool bDeathScene = false;
	float Black = 1.f;          // full-screen black, 0..1
	float Letterbox = 0.f;      // cinema bars, 0..1
	// title card (centre of the screen): line 1 typed, line 2 fades in under it
	FString CardLine1, CardLine2;
	bool bCardTitle = false;    // big chapter title
	float CardT = -1.f;         // seconds since the card appeared (-1 = none)
	float CardAlpha = 0.f;
	// subtitle (dialogue)
	FString Speaker, Line;
	FLinearColor SpeakerColor = FLinearColor::White;
	float LineT = -1.f;         // seconds since the line started (-1 = none)
	// objectives (top right)
	struct FObjective { FString Text; int32 Have = 0; int32 Need = 0; bool bDone = false; float T = 0.f; };
	TArray<FObjective> Objectives;
	FString ChapterName;
	// a big line in the middle ("هدف جديد" ...)
	FString Banner, BannerSub;
	float BannerT = -1.f;
	// where to go (world) + its label
	bool bHasMarker = false;
	FVector Marker = FVector::ZeroVector;
	FString MarkerLabel;
	// "E" prompt near a pickup
	FString Prompt;
	// results
	int32 Kills = 0, RiflesTaken = 0, AmmoTaken = 0;
	float PlayTime = 0.f;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

private:
	struct FBeat
	{
		uint8 Type = 0;      // 0 card, 1 title card, 2 dialogue line
		FString A, B;        // card: two lines; line: text (A)
		int32 Who = -1;      // line: -1 leader, 0..2 the men, 3 all of them
		float Dur = 3.f;
	};
	TArray<FBeat> Beats;
	int32 BeatIdx = -1;
	float BeatT = 0.f, PhaseT = 0.f;
	void StartBeats(const TArray<FBeat>& InBeats);
	bool TickBeats(float Dt, APlayerController* PC);   // true while beats remain

	// cinematic camera
	TWeakObjectPtr<ACameraActor> Cam;
	int32 ShotWho = -99;
	float ShotT = 0.f;
	bool bInputLocked = false;
	void LockInput(APlayerController* PC, bool bLock);
	void TickCamera(float Dt, APlayerController* PC, bool bWide);
	void ViewCinematic(APlayerController* PC);
	void ViewGame(APlayerController* PC, float Blend);
	AActor* Actor(int32 Who) const;

	// the play
	TArray<TWeakObjectPtr<AAAAPoliceOfficer>> Enemies;
	TArray<TWeakObjectPtr<AAAAMissionPickup>> Pickups;
	TSet<TWeakObjectPtr<AAAAPoliceOfficer>> Counted;
	FVector Rally = FVector::ZeroVector, Camp = FVector::ZeroVector;
	bool bCrateTaken = false, bWasDead = false, bSavedInfinite = true;
	int32 TotalEnemies = 6;
	float SayT = 0.f;
	void BeginPlayPhase(ACharacter* L, UAAAWeaponComponent* Wp, UAAAWeaponsSubsystem* Ws);
	void TickPlay(float Dt, ACharacter* L, UAAAWeaponComponent* Wp, UAAAWeaponsSubsystem* Ws, APlayerController* PC);
	void EndMission(UAAAWeaponComponent* Wp, UAAAWeaponsSubsystem* Ws);
	void SetBanner(const FString& A, const FString& B) { Banner = A; BannerSub = B; BannerT = 0.f; }
	/** the floor under Around, looking from StartUp cm above it (low: never lands on a roof / awning over it) */
	bool Ground(const FVector& Around, FVector& Out, double StartUp = 150.0) const;
	bool bDeathShot = false;
	// ---- polish (2026-10-27): sounds, music, typing ticks, talking heads, camera depth of field / hand-held, the
	//      reinforcements, low-ammo hint, the rally-point checkpoint, debug screen messages hidden in the cinematics
	TWeakObjectPtr<class UAudioComponent> Music;
	int32 TypedCard = 0, TypedLine = 0;
	float TickGap = 0.f, HeartT = 0.f, CamNoiseT = 0.f;
	bool bReinforced = false, bLowAmmoSaid = false, bDebugMsgsSaved = false, bDebugMsgsWere = true;
	void Sfx(uint8 Snd, float Vol = 1.f, float Pitch = 1.f);
	void StartMusic(float Vol);
	void StopMusic(float FadeS);
	void TickPolish(float Dt);
	void SetTalk(int32 Who, float T);
	void CamLook(class ACameraActor* C, const FVector& Pos, const FVector& Look, float Dt, bool bCut, float NoiseDeg);
	void TickDeathScene(float Dt, APlayerController* PC, ACharacter* L, UAAAWeaponComponent* Wp);
	void TickDeathCamera(float Dt, ACharacter* L, float S);
	/** a standing spot (capsule centre) on the street level RefZ (+-2.2 m), not inside a building */
	bool StreetSpot(const FVector& Want, double RefZ, FVector& Out) const;
};
