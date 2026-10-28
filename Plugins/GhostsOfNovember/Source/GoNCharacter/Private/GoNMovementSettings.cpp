#include "GoNMovementSettings.h"

#include "Animation/AnimSequence.h"
#include "Misc/Paths.h"

namespace GoNMovementSettingsDetail
{
	// Project assets from ASSET_MANIFEST.md (UE5 Manny/Quinn, unarmed set).
	const TCHAR* const UnarmedFolder = TEXT("/Game/Characters/Mannequins/Anims/Unarmed");

	TSoftObjectPtr<UAnimSequence> Clip(const FString& PackagePath)
	{
		const FString AssetName = FPaths::GetBaseFilename(PackagePath);
		return TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(PackagePath + TEXT(".") + AssetName));
	}
}

FGoNAnimSettings::FGoNAnimSettings()
{
	using namespace GoNMovementSettingsDetail;
	const FString Folder(UnarmedFolder);
	Idle = Clip(Folder / TEXT("MM_Idle"));
	JumpStart = Clip(Folder / TEXT("Jump/MM_Jump"));
	FallLoop = Clip(Folder / TEXT("Jump/MM_Fall_Loop"));
	Land = Clip(Folder / TEXT("Jump/MM_Land"));
	WoundedPose = Clip(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01"));

	// Not in the project yet: import them at these paths (or assign others in a settings asset).
	const FString GoNFolder(TEXT("/Game/Characters/Mannequins/Anims/GoN"));
	ProneIdle = Clip(GoNFolder / TEXT("MM_GoN_Prone_Idle"));
	ProneCrawl = Clip(GoNFolder / TEXT("MM_GoN_Prone_Crawl_Fwd"));
	CarryIdle = Clip(GoNFolder / TEXT("MM_GoN_Carry_Idle"));
	CarryWalk = Clip(GoNFolder / TEXT("MM_GoN_Carry_Walk_Fwd"));
	VaultLow = Clip(GoNFolder / TEXT("MM_GoN_Vault_Low"));
	VaultMid = Clip(GoNFolder / TEXT("MM_GoN_Vault_Mid"));
	VaultHigh = Clip(GoNFolder / TEXT("MM_GoN_Vault_High"));
	Climb = Clip(GoNFolder / TEXT("MM_GoN_Climb"));
	for (int32 Index = 0; Index < GoN::Character::LocomotionDirections; ++Index)
	{
		const FString Suffix = ANSI_TO_TCHAR(GoN::Character::LocomotionDirectionSuffix(Index));
		Walk.Add(Clip(Folder / TEXT("Walk/MF_Unarmed_Walk_") + Suffix));
		Jog.Add(Clip(Folder / TEXT("Jog/MF_Unarmed_Jog_") + Suffix));
	}
}

GoN::Character::FMovementMetrics FGoNMovementMetrics::ToPure() const
{
	GoN::Character::FMovementMetrics Out;
	Out.CapsuleRadius = CapsuleRadius;
	Out.StandHalfHeight = StandHalfHeight;
	Out.CrouchHalfHeight = CrouchHalfHeight;
	Out.ProneHalfHeight = ProneHalfHeight;
	Out.WalkSpeed = WalkSpeed;
	Out.JogSpeed = JogSpeed;
	Out.SprintSpeed = SprintSpeed;
	Out.CrouchSpeed = CrouchSpeed;
	Out.ProneSpeed = ProneSpeed;
	Out.CarryWalkSpeed = CarryWalkSpeed;
	Out.CarryJogSpeed = CarryJogSpeed;
	Out.MaxStepHeight = MaxStepHeight;
	Out.WalkableSlopeDeg = WalkableSlopeDeg;
	Out.ReachDistance = ReachDistance;
	Out.MaxVaultHeight = MaxVaultHeight;
	Out.MaxVaultDepth = MaxVaultDepth;
	Out.MinMantleDepth = MinMantleDepth;
	Out.MaxClimbHeight = MaxClimbHeight;
	Out.LowVaultMax = LowVaultMax;
	Out.MidVaultMax = MidVaultMax;
	Out.VaultClearance = VaultClearance;
	Out.VaultBaseSeconds = VaultBaseSeconds;
	Out.VaultSecondsPerMetre = VaultSecondsPerMetre;
	Out.ClimbBaseSeconds = ClimbBaseSeconds;
	Out.ClimbSecondsPerMetre = ClimbSecondsPerMetre;
	return Out;
}

GoN::Character::FCameraProfile FGoNCameraProfile::ToPure() const
{
	GoN::Character::FCameraProfile Out;
	Out.ArmLength = ArmLength;
	Out.SprintArmLength = SprintArmLength;
	Out.CoverArmLength = CoverArmLength;
	Out.ProneArmLength = ProneArmLength;
	Out.ShoulderOffset = ShoulderOffset;
	Out.CoverShoulderOffset = CoverShoulderOffset;
	Out.PivotHeightStand = PivotHeightStand;
	Out.PivotHeightCrouch = PivotHeightCrouch;
	Out.PivotHeightProne = PivotHeightProne;
	Out.Fov = Fov;
	Out.SprintFov = SprintFov;
	Out.InterpSpeed = InterpSpeed;
	Out.ShoulderSwapSpeed = ShoulderSwapSpeed;
	return Out;
}
