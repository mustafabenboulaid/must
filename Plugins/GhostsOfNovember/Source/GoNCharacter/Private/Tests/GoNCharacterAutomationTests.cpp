#include "GoNLogicAutomation.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GoNMovementSettings.h"

GON_IMPLEMENT_LOGIC_AUTOMATION_TESTS(FGoNCharacterLogicTests, "GoN.Character.Logic", "Character.")

// The editable USTRUCT defaults must match the pure defaults the logic tests were written for.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoNCharacterSettingsDefaultsTest, "GoN.Character.SettingsMatchPureDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FGoNCharacterSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const GoN::Character::FMovementMetrics FromAsset = FGoNMovementMetrics().ToPure();
	const GoN::Character::FMovementMetrics Pure;
	TestEqual(TEXT("radius"), FromAsset.CapsuleRadius, Pure.CapsuleRadius);
	TestEqual(TEXT("stand half height"), FromAsset.StandHalfHeight, Pure.StandHalfHeight);
	TestEqual(TEXT("crouch half height"), FromAsset.CrouchHalfHeight, Pure.CrouchHalfHeight);
	TestEqual(TEXT("prone half height"), FromAsset.ProneHalfHeight, Pure.ProneHalfHeight);
	TestEqual(TEXT("sprint"), FromAsset.SprintSpeed, Pure.SprintSpeed);
	TestEqual(TEXT("max vault"), FromAsset.MaxVaultHeight, Pure.MaxVaultHeight);
	TestEqual(TEXT("max climb"), FromAsset.MaxClimbHeight, Pure.MaxClimbHeight);
	TestEqual(TEXT("walkable slope"), static_cast<float>(FromAsset.WalkableSlopeDeg), static_cast<float>(Pure.WalkableSlopeDeg));
	const FGoNAnimSettings Anim;
	TestEqual(TEXT("8 walk clips"), Anim.Walk.Num(), GoN::Character::LocomotionDirections);
	TestEqual(TEXT("8 jog clips"), Anim.Jog.Num(), GoN::Character::LocomotionDirections);
	TestTrue(TEXT("idle path set"), !Anim.Idle.IsNull());
	return true;
}

#endif
