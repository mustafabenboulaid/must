#include "GoNInputSet.h"

#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace GoNInputSetDetail
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, FName(Name), RF_Transient);
		Action->ValueType = ValueType;
		return Action;
	}

	void AddSwizzle(FEnhancedActionKeyMapping& Mapping, UObject* Outer)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Outer, NAME_None, RF_Transient);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}

	void AddNegate(FEnhancedActionKeyMapping& Mapping, UObject* Outer, bool bNegateX, bool bNegateY)
	{
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Outer, NAME_None, RF_Transient);
		Negate->bX = bNegateX;
		Negate->bY = bNegateY;
		Negate->bZ = false;
		Mapping.Modifiers.Add(Negate);
	}

	void AddDeadZone(FEnhancedActionKeyMapping& Mapping, UObject* Outer)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(Outer, NAME_None, RF_Transient));
	}
}

UGoNInputSet* UGoNInputSet::CreateRuntimeDefault(UObject* Outer)
{
	using namespace GoNInputSetDetail;
	UGoNInputSet* Set = NewObject<UGoNInputSet>(Outer, TEXT("GoNInputSet_RuntimeDefault"), RF_Transient);
	UInputMappingContext* Context = NewObject<UInputMappingContext>(Set, TEXT("IMC_GoN_Default"), RF_Transient);
	Set->MappingContext = Context;

	Set->Move = MakeAction(Set, TEXT("IA_GoN_Move"), EInputActionValueType::Axis2D);
	Set->Look = MakeAction(Set, TEXT("IA_GoN_Look"), EInputActionValueType::Axis2D);
	Set->Jump = MakeAction(Set, TEXT("IA_GoN_Jump"), EInputActionValueType::Boolean);
	Set->Sprint = MakeAction(Set, TEXT("IA_GoN_Sprint"), EInputActionValueType::Boolean);
	Set->Walk = MakeAction(Set, TEXT("IA_GoN_Walk"), EInputActionValueType::Boolean);
	Set->Crouch = MakeAction(Set, TEXT("IA_GoN_Crouch"), EInputActionValueType::Boolean);
	Set->Prone = MakeAction(Set, TEXT("IA_GoN_Prone"), EInputActionValueType::Boolean);
	Set->Interact = MakeAction(Set, TEXT("IA_GoN_Interact"), EInputActionValueType::Boolean);
	Set->ShoulderSwap = MakeAction(Set, TEXT("IA_GoN_ShoulderSwap"), EInputActionValueType::Boolean);

	// Move: X = right, Y = forward.
	AddSwizzle(Context->MapKey(Set->Move, EKeys::W), Context);
	{
		FEnhancedActionKeyMapping& Back = Context->MapKey(Set->Move, EKeys::S);
		AddSwizzle(Back, Context);
		AddNegate(Back, Context, true, true);
	}
	Context->MapKey(Set->Move, EKeys::D);
	AddNegate(Context->MapKey(Set->Move, EKeys::A), Context, true, true);
	AddDeadZone(Context->MapKey(Set->Move, EKeys::Gamepad_Left2D), Context);

	// Look: X = yaw, Y = pitch. Mouse Y is negated as in the UE5 third-person template.
	AddNegate(Context->MapKey(Set->Look, EKeys::Mouse2D), Context, false, true);
	{
		FEnhancedActionKeyMapping& Stick = Context->MapKey(Set->Look, EKeys::Gamepad_Right2D);
		AddDeadZone(Stick, Context);
		AddNegate(Stick, Context, false, true);
	}

	Context->MapKey(Set->Jump, EKeys::SpaceBar);
	Context->MapKey(Set->Jump, EKeys::Gamepad_FaceButton_Bottom);
	Context->MapKey(Set->Sprint, EKeys::LeftShift);
	Context->MapKey(Set->Sprint, EKeys::Gamepad_LeftThumbstick);
	Context->MapKey(Set->Walk, EKeys::LeftAlt);
	Context->MapKey(Set->Crouch, EKeys::C);
	Context->MapKey(Set->Crouch, EKeys::Gamepad_FaceButton_Right);
	Context->MapKey(Set->Prone, EKeys::Z);
	Context->MapKey(Set->Prone, EKeys::Gamepad_DPad_Down);
	Context->MapKey(Set->Interact, EKeys::E);
	Context->MapKey(Set->Interact, EKeys::Gamepad_FaceButton_Left);
	Context->MapKey(Set->ShoulderSwap, EKeys::X);
	Context->MapKey(Set->ShoulderSwap, EKeys::Gamepad_RightThumbstick);
	return Set;
}
