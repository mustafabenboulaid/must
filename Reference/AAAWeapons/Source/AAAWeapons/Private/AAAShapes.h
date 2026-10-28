// AAA City Weapons - engine basic shapes as building blocks (rifle model, targets, tracers, holes, casings)
#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"

namespace AAAShapes
{
	inline UStaticMesh* Cube() { static TWeakObjectPtr<UStaticMesh> M; if (!M.IsValid()) M = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")); return M.Get(); }
	inline UStaticMesh* Cylinder() { static TWeakObjectPtr<UStaticMesh> M; if (!M.IsValid()) M = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); return M.Get(); }
	inline UStaticMesh* Sphere() { static TWeakObjectPtr<UStaticMesh> M; if (!M.IsValid()) M = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")); return M.Get(); }
	inline UMaterialInterface* BaseMat() { static TWeakObjectPtr<UMaterialInterface> M; if (!M.IsValid()) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")); return M.Get(); }
	/** an unlit / emissive look for tracers and the muzzle flash (falls back to the basic material) */
	inline UMaterialInterface* GlowMat()
	{
		static TWeakObjectPtr<UMaterialInterface> M;
		static bool bTried = false;
		if (!M.IsValid() && !bTried)
		{
			bTried = true;
			M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		return M.IsValid() ? M.Get() : BaseMat();
	}

	inline UMaterialInstanceDynamic* Mat(UObject* Outer, const FLinearColor& Color, bool bGlow = false)
	{
		UMaterialInterface* Base = bGlow ? GlowMat() : BaseMat();
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, Outer);
		if (D) { D->SetVectorParameterValue(TEXT("Color"), Color); D->SetVectorParameterValue(TEXT("EmissiveColor"), Color); }
		return D;
	}

	/** a box / cylinder of SizeCm (cylinder: X/Y = diameter, Z = length along its axis) */
	inline UStaticMeshComponent* Make(UObject* Outer, bool bCylinder, const FVector& SizeCm, UMaterialInterface* M, bool bCollide = false, bool bShadow = true)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Outer);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(bCylinder ? Cylinder() : Cube());
		C->SetRelativeScale3D(SizeCm / 100.0);
		if (M) C->SetMaterial(0, M);
		if (bCollide) C->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		C->BodyInstance.bAutoWeld = false;
		C->SetCastShadow(bShadow);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		return C;
	}
}
