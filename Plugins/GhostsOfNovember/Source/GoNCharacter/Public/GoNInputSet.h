#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "GoNInputSet.generated.h"

class UInputAction;
class UInputMappingContext;

// Enhanced Input actions + mapping context of the GoN character.
// Leave AGoNCharacter::InputSet empty to use CreateRuntimeDefault() (built in C++, no assets
// needed). To rebind keys in the editor, create a GoN Input Set data asset with your own
// IA_/IMC_ assets (value types listed below) and assign it to the character.
UCLASS(BlueprintType)
class GONCHARACTER_API UGoNInputSet : public UDataAsset
{
	GENERATED_BODY()

public:
	// Builds the default bindings as transient objects:
	//   Move (Axis2D)  WASD, left stick          Look (Axis2D)      mouse, right stick
	//   Jump (Bool)    Space, A: jump/vault/climb Sprint (Bool, hold) Left Shift, left stick click
	//   Walk (Bool, hold) Left Alt               Crouch (Bool)      C, B
	//   Prone (Bool)   Z, D-pad down             Interact (Bool)    E, X: carry / put down
	//   ShoulderSwap (Bool) X key, right stick click
	static UGoNInputSet* CreateRuntimeDefault(UObject* Outer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Look;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Sprint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Walk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Crouch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Prone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Interact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ShoulderSwap;
};
