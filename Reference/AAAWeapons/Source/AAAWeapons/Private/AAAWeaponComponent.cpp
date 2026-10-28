#include "AAAWeaponComponent.h"
#include "AAASmartHudWidget.h"
#include "AAAGrenade.h"
#include "AAARifleAnimInstance.h"
#include "AAAClipBracedHang.h"
#include "Engine/SkeletalMesh.h"
#include "AAATrainingTarget.h"
#include "AAAShapes.h"
#include "TrafficAIManager.h"
#include "AAAWeaponsSubsystem.h"
#include "AAAPoliceOfficer.h"
#include "DrawDebugHelpers.h"

#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/StrongObjectPtr.h"
#include "StaticMeshDescription.h"
#include "StaticMeshAttributes.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 MAX_TRACERS = 48, MAX_HOLES = 90, MAX_PUFFS = 24, MAX_CASINGS = 16;

	FCollisionObjectQueryParams BulletObjects()
	{
		FCollisionObjectQueryParams Q;
		Q.AddObjectTypesToQuery(ECC_WorldStatic);
		Q.AddObjectTypesToQuery(ECC_WorldDynamic);
		Q.AddObjectTypesToQuery(ECC_PhysicsBody);
		Q.AddObjectTypesToQuery(ECC_Vehicle);
		Q.AddObjectTypesToQuery(ECC_Pawn);
		Q.AddObjectTypesToQuery(ECC_Destructible);
		return Q;
	}

	USoundBase* LoadSfx(const TCHAR* Name)
	{
		const FString P = FString::Printf(TEXT("/Game/AAACity/Sounds/%s.%s"), Name, Name);
		return LoadObject<USoundBase>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	// ---- AAA 3D Spatial Audio Attenuation Profiles (Binaural Panning + Distance LPF Air Absorption + Real-time Wall Occlusion)
	USoundAttenuation* Get3DCombatAttenuation()
	{
		static USoundAttenuation* CombatAtten = nullptr;
		if (!CombatAtten)
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("AAA_3DCombatAttenuation"));
			A->AddToRoot();
			FSoundAttenuationSettings& S = A->Attenuation;
			S.bAttenuate = true;
			S.bSpatialize = true;
			S.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
			S.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
			S.AttenuationShape = EAttenuationShape::Sphere;
			S.AttenuationShapeExtents = FVector(165.f, 0.f, 0.f);
			S.FalloffDistance = 12000.f; // 120m outdoor urban carry
			S.dBAttenuationAtMax = -56.f;
			S.StereoSpread = 55.f;
			// Distance Low-Pass Filter (Air Absorption: distant gunshots sound deep & atmospheric)
			S.bAttenuateWithLPF = true;
			S.LPFRadiusMin = 350.f;
			S.LPFRadiusMax = 9000.f;
			S.LPFFrequencyAtMin = 20000.f;
			S.LPFFrequencyAtMax = 1650.f;
			// Directional Listener Focus
			S.bEnableListenerFocus = true;
			S.FocusAzimuth = 35.f;
			S.NonFocusAzimuth = 115.f;
			S.FocusVolumeAttenuation = 1.12f;
			S.NonFocusVolumeAttenuation = 0.82f;
			// Real-time 3D Geometry Occlusion (muffles shots behind buildings/vehicles)
			S.bEnableOcclusion = true;
			S.OcclusionTraceChannel = ECC_Visibility;
			S.OcclusionLowPassFilterFrequency = 1150.f;
			S.OcclusionVolumeAttenuation = 0.48f;
			S.OcclusionInterpolationTime = 0.12f;
			CombatAtten = A;
		}
		return CombatAtten;
	}

	USoundAttenuation* Get3DFoleyAttenuation()
	{
		static USoundAttenuation* FoleyAtten = nullptr;
		if (!FoleyAtten)
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("AAA_3DFoleyAttenuation"));
			A->AddToRoot();
			FSoundAttenuationSettings& S = A->Attenuation;
			S.bAttenuate = true;
			S.bSpatialize = true;
			S.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
			S.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
			S.AttenuationShape = EAttenuationShape::Sphere;
			S.AttenuationShapeExtents = FVector(60.f, 0.f, 0.f); // Tight 60cm inner radius for pinpoint 3D localization
			S.FalloffDistance = 3800.f; // 38m range for impacts, casings, footsteps, bullet snaps
			S.dBAttenuationAtMax = -60.f;
			S.StereoSpread = 32.f;
			// Air Absorption LPF
			S.bAttenuateWithLPF = true;
			S.LPFRadiusMin = 120.f;
			S.LPFRadiusMax = 3200.f;
			S.LPFFrequencyAtMin = 20000.f;
			S.LPFFrequencyAtMax = 2800.f;
			// Real-time 3D Occlusion
			S.bEnableOcclusion = true;
			S.OcclusionTraceChannel = ECC_Visibility;
			S.OcclusionLowPassFilterFrequency = 1400.f;
			S.OcclusionVolumeAttenuation = 0.42f;
			S.OcclusionInterpolationTime = 0.10f;
			FoleyAtten = A;
		}
		return FoleyAtten;
	}

	struct FDelayed3DEcho
	{
		TWeakObjectPtr<USoundBase> Sound;
		FVector Loc = FVector::ZeroVector;
		float Delay = 0.f;
		float Vol = 0.25f;
		float Pitch = 0.75f;
	};

	struct FAAA3DAudioRuntime
	{
		TArray<FDelayed3DEcho> Echoes;
		FVector LastFootPos = FVector::ZeroVector;
		float StepDistAccum = 0.f;
		bool bLeftStep = true;
		bool bWasFalling = false;
		bool bWasMantling = false;
	};
	// Constant-initialised pointer (no dynamic initializer): a global with a TArray crashed in the Live Coding patch DLL's dllmain.
	static FAAA3DAudioRuntime* G3DAudioPtr = nullptr;
	static FAAA3DAudioRuntime& Get3DAudio()
	{
		if (!G3DAudioPtr) G3DAudioPtr = new FAAA3DAudioRuntime();
		return *G3DAudioPtr;
	}

	// ---- Runtime 3D M4A1 Vltor MUR Mesh + PBR Material Builder (loads Lower_Receiver_low.fbx + Upper_Receiver_low.fbx + 3D carbine parts)
	static UStaticMesh* GM4BodyMesh = nullptr;
	static UStaticMesh* GM4MagMesh = nullptr;
	static UMaterialInterface* GM4SectionMats[4] = { nullptr, nullptr, nullptr, nullptr };

	// ---- Runtime 3D Colt 1911 Mesh + PBR Materials (Body + Slide + Hammer + Separate Magazine)
	static UStaticMesh* GColtBodyMesh = nullptr;
	static UStaticMesh* GColtSlideMesh = nullptr;
	static UStaticMesh* GColtMagMesh = nullptr;
	static UStaticMesh* GColtHammerMesh = nullptr;
	static UMaterialInterface* GColtMat = nullptr;
	static TWeakObjectPtr<UStaticMeshComponent> GPistolSlideComp = nullptr;
	static TWeakObjectPtr<UStaticMeshComponent> GPistolHammerComp = nullptr;
	static float GPistolSlideAnim = 0.0f;
	static float GPistolHammerRot = 1.0f;

	// Intelligent Grip Anchor & Socket System:
	// Automatically adds or queries a "Grip" socket positioned in the physical center of the weapon's handle.
	// Snaps the weapon so the Grip socket aligns with the right palm center (Rifle origin) with zero manual guessing.
	static void EnsureWeaponMeshSockets(UStaticMesh* Mesh, const FVector& GripLoc, const FRotator& GripRot, const FVector& MuzzleLoc, const FVector& EjectLoc)
	{
		if (!Mesh) return;
		auto AddSock = [&](FName Name, const FVector& Loc, const FRotator& Rot)
		{
			if (!Mesh->FindSocket(Name))
			{
				UStaticMeshSocket* S = NewObject<UStaticMeshSocket>(Mesh);
				S->SocketName = Name;
				S->RelativeLocation = Loc;
				S->RelativeRotation = Rot;
				S->RelativeScale = FVector::OneVector;
				Mesh->AddSocket(S);
			}
		};
		AddSock(TEXT("Grip"), GripLoc, GripRot);
		AddSock(TEXT("Muzzle"), MuzzleLoc, FRotator::ZeroRotator);
		AddSock(TEXT("Ejection"), EjectLoc, FRotator::ZeroRotator);
	}

	static void AlignWeaponComponentByGripSocket(UStaticMeshComponent* Comp, UStaticMesh* Mesh, float UniformScale = 1.0f)
	{
		if (!Comp || !Mesh) return;
		const UStaticMeshSocket* Socket = Mesh->FindSocket(TEXT("Grip"));
		if (!Socket) return;

		const FVector SLoc = Socket->RelativeLocation;
		const FRotator SRot = Socket->RelativeRotation;

		// The component's rotation is the inverse of the socket's rotation:
		const FQuat CompRot = SRot.Quaternion().Inverse();

		// The component's scale is directly UniformScale:
		const FVector CompScale = FVector(UniformScale);

		// The component's location is offset so the socket lands exactly at (0, 0, 0):
		const FVector CompLoc = - CompRot.RotateVector(SLoc * UniformScale);

		Comp->SetRelativeLocationAndRotation(CompLoc, CompRot);
		Comp->SetRelativeScale3D(CompScale);
	}

	static UStaticMesh* BuildStaticMeshFromBinFile(const FString& BinPath, const TArray<UMaterialInterface*>& SectionMats)
	{
		TArray<uint8> Raw;
		if (!FFileHelper::LoadFileToArray(Raw, *BinPath) || Raw.Num() < 8)
		{
			return nullptr;
		}
		const uint8* Ptr = Raw.GetData();
		const uint8* End = Ptr + Raw.Num();
		uint32 NumSections = *reinterpret_cast<const uint32*>(Ptr);
		Ptr += 4;
		if (NumSections == 0 || NumSections > 16) return nullptr;

		UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Public | RF_Standalone);
		Mesh->AddToRoot();
		// No ray-tracing geometry for these runtime meshes: BuildFromMeshDescriptions + InitResources below registered
		// it twice in the ray tracing geometry manager, and the editor asserted on exit
		// ("Unexpected mismatch of FRayTracingGeometry in FRayTracingGeometryGroup", RayTracingGeometryManager.cpp:361).
		Mesh->bSupportRayTracing = false;
		UStaticMeshDescription* Desc = UStaticMesh::CreateStaticMeshDescription(GetTransientPackage());
		FMeshDescription& MD = Desc->GetMeshDescription();
		FStaticMeshAttributes Attrs(MD);

		TVertexAttributesRef<FVector3f> Positions = Attrs.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attrs.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attrs.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> BinormSigns = Attrs.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attrs.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attrs.GetVertexInstanceUVs();
		TPolygonGroupAttributesRef<FName> SlotNames = Attrs.GetPolygonGroupMaterialSlotNames();
		if (UVs.GetNumChannels() < 1) UVs.SetNumChannels(1);

		for (uint32 Sec = 0; Sec < NumSections; ++Sec)
		{
			if (Ptr + 4 > End) break;
			uint32 NumTris = *reinterpret_cast<const uint32*>(Ptr);
			Ptr += 4;
			if (Ptr + (size_t)NumTris * 96 > End) break;

			const FName SlotName(*FString::Printf(TEXT("M4Slot_%u"), Sec));
			UMaterialInterface* Mat = SectionMats.IsValidIndex((int32)Sec) ? SectionMats[Sec] : nullptr;
			Mesh->GetStaticMaterials().Add(FStaticMaterial(Mat, SlotName));
			const FPolygonGroupID PG = MD.CreatePolygonGroup();
			SlotNames[PG] = SlotName;

			for (uint32 T = 0; T < NumTris; ++T)
			{
				FVertexInstanceID VInst[3];
				for (int32 C = 0; C < 3; ++C)
				{
					const float* F = reinterpret_cast<const float*>(Ptr);
					Ptr += 32;
					const FVertexID VId = MD.CreateVertex();
					Positions[VId] = FVector3f(F[0], F[1], F[2]);
					const FVertexInstanceID Vi = MD.CreateVertexInstance(VId);
					FVector3f N(F[3], F[4], F[5]);
					if (!N.Normalize()) N = FVector3f(0.f, 0.f, 1.f);
					Normals[Vi] = N;
					FVector3f Tan = FVector3f::CrossProduct(N, FMath::Abs(N.Z) < 0.9f ? FVector3f(0.f, 0.f, 1.f) : FVector3f(1.f, 0.f, 0.f)).GetSafeNormal();
					Tangents[Vi] = Tan;
					BinormSigns[Vi] = 1.0f;
					Colors[Vi] = FVector4f(1.f, 1.f, 1.f, 1.f);
					UVs.Set(Vi, 0, FVector2f(F[6], F[7]));
					VInst[C] = Vi;
				}
				MD.CreateTriangle(PG, MakeArrayView(VInst, 3));
			}
		}

		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bFastBuild = true;
		Params.bCommitMeshDescription = false;
		Params.bBuildSimpleCollision = false;
		Params.bAllowCpuAccess = false;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		Mesh->InitResources();
		Mesh->CalculateExtendedBounds();
		UE_LOG(LogTemp, Log, TEXT("AAA Weapons: Successfully built runtime static mesh from %s (Sections=%u)"), *BinPath, NumSections);
		return Mesh;
	}

	static void EnsureM4A1MeshesBuilt(UObject* Context)
	{
		if (GM4BodyMesh && GM4MagMesh) return;
		const FString Dir = FPaths::ProjectContentDir() / TEXT("AAACity/Weapons/M4A1");

		UMaterialInterface* GltfBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/InterchangeAssets/gltf/MaterialInstances/MI_Default_Opaque.MI_Default_Opaque"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!GltfBase)
		{
			GltfBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AAACity/city/Materials/M_metal_0.M_metal_0"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		auto LoadPngTex = [&](const FString& File, bool bSRGB, TextureCompressionSettings Comp = TC_Default) -> UTexture2D*
		{
			UTexture2D* T = FImageUtils::ImportFileAsTexture2D(Dir / File);
			if (T)
			{
				T->AddToRoot();
				T->SRGB = bSRGB;
				T->CompressionSettings = Comp;
				T->UpdateResource();
			}
			return T;
		};

		auto MakeTexPbrMat = [&](const FString& Prefix, const FLinearColor& FallbackColor) -> UMaterialInterface*
		{
			if (GltfBase)
			{
				UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(GltfBase, GetTransientPackage());
				if (D)
				{
					D->AddToRoot();
					if (UTexture2D* Bc = LoadPngTex(Prefix + TEXT("_basecolor.png"), true))
						D->SetTextureParameterValue(TEXT("BaseColorTexture"), Bc);
					if (UTexture2D* Nm = LoadPngTex(Prefix + TEXT("_normal.png"), false, TC_Normalmap))
						D->SetTextureParameterValue(TEXT("NormalTexture"), Nm);
					if (UTexture2D* Mr = LoadPngTex(Prefix + TEXT("_mr.png"), false))
						D->SetTextureParameterValue(TEXT("MetallicRoughnessTexture"), Mr);
					D->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor::White);
					D->SetScalarParameterValue(TEXT("MetallicFactor"), 1.0f);
					D->SetScalarParameterValue(TEXT("RoughnessFactor"), 1.0f);
					return D;
				}
			}
			UMaterialInstanceDynamic* Fallback = AAAShapes::Mat(Context, FallbackColor);
			if (Fallback) Fallback->AddToRoot();
			return Fallback;
		};

		auto MakeSolidPbrMat = [&](const FLinearColor& Color, float Metallic, float Roughness) -> UMaterialInterface*
		{
			if (GltfBase)
			{
				UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(GltfBase, GetTransientPackage());
				if (D)
				{
					D->AddToRoot();
					D->SetVectorParameterValue(TEXT("BaseColorFactor"), Color);
					D->SetScalarParameterValue(TEXT("MetallicFactor"), Metallic);
					D->SetScalarParameterValue(TEXT("RoughnessFactor"), Roughness);
					return D;
				}
			}
			UMaterialInstanceDynamic* Fallback = AAAShapes::Mat(Context, Color);
			if (Fallback) Fallback->AddToRoot();
			return Fallback;
		};

		GM4SectionMats[0] = MakeTexPbrMat(TEXT("lower"), FLinearColor(0.045f, 0.048f, 0.054f));
		GM4SectionMats[1] = MakeTexPbrMat(TEXT("upper"), FLinearColor(0.055f, 0.060f, 0.068f));
		GM4SectionMats[2] = MakeSolidPbrMat(FLinearColor(0.038f, 0.041f, 0.045f), 0.85f, 0.38f);
		GM4SectionMats[3] = MakeSolidPbrMat(FLinearColor(0.28f, 0.22f, 0.15f), 0.08f, 0.65f);

		TArray<UMaterialInterface*> BodyMats = { GM4SectionMats[0], GM4SectionMats[1], GM4SectionMats[2], GM4SectionMats[3] };
		TArray<UMaterialInterface*> MagMats = { GM4SectionMats[2] };

		GM4BodyMesh = BuildStaticMeshFromBinFile(Dir / TEXT("m4a1_body.bin"), BodyMats);
		GM4MagMesh  = BuildStaticMeshFromBinFile(Dir / TEXT("m4a1_mag.bin"), MagMats);
	}

	static void EnsureColt1911MeshesBuilt(UObject* Context, bool bForceRebuild = false)
	{
		if (!bForceRebuild && GColtBodyMesh && GColtSlideMesh && GColtMagMesh && GColtHammerMesh && GColtMat) return;
		const FString Dir = FPaths::ProjectContentDir() / TEXT("AAACity/Weapons/Colt1911");

		// 1. High-fidelity PBR Material from M_Colt1911_PBR
		UMaterialInterface* PbrParent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AAACity/Weapons/Colt1911/M_Colt1911_PBR.M_Colt1911_PBR"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!PbrParent)
		{
			PbrParent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AAACity/city/Materials/M_metal_0.M_metal_0"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		if (PbrParent)
		{
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(PbrParent, GetTransientPackage());
			if (D)
			{
				D->AddToRoot();
				UTexture2D* Bc = LoadObject<UTexture2D>(nullptr, TEXT("/Game/AAACity/Weapons/Colt1911/colt_1911/Textures/colt_1911_texture_0.colt_1911_texture_0"));
				if (Bc) D->SetTextureParameterValue(TEXT("BaseColorTexture"), Bc);
				UTexture2D* Mr = LoadObject<UTexture2D>(nullptr, TEXT("/Game/AAACity/Weapons/Colt1911/colt_1911/Textures/colt_1911_texture_1.colt_1911_texture_1"));
				if (Mr) D->SetTextureParameterValue(TEXT("MetallicRoughnessTexture"), Mr);
				UTexture2D* Nm = LoadObject<UTexture2D>(nullptr, TEXT("/Game/AAACity/Weapons/Colt1911/colt_1911/Textures/colt_1911_texture_2.colt_1911_texture_2"));
				if (Nm) D->SetTextureParameterValue(TEXT("NormalTexture"), Nm);
				D->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor::White);
				D->SetScalarParameterValue(TEXT("MetallicFactor"), 1.0f);
				D->SetScalarParameterValue(TEXT("RoughnessFactor"), 1.0f);
				GColtMat = D;
			}
			else
			{
				GColtMat = PbrParent;
			}
		}

		if (!GColtMat)
		{
			GColtMat = AAAShapes::Mat(Context, FLinearColor(0.08f, 0.085f, 0.09f));
			if (GColtMat) GColtMat->AddToRoot();
		}

		TArray<UMaterialInterface*> ColtMats = { GColtMat };
		GColtBodyMesh   = BuildStaticMeshFromBinFile(Dir / TEXT("colt1911_body.bin"), ColtMats);
		GColtSlideMesh  = BuildStaticMeshFromBinFile(Dir / TEXT("colt1911_slide.bin"), ColtMats);
		GColtMagMesh    = BuildStaticMeshFromBinFile(Dir / TEXT("colt1911_mag.bin"), ColtMats);
		GColtHammerMesh = BuildStaticMeshFromBinFile(Dir / TEXT("colt1911_hammer.bin"), ColtMats);
	}
}
#define G3DAudio (Get3DAudio())

UAAAWeaponComponent::UAAAWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

ACharacter* UAAAWeaponComponent::Char() const { return Cast<ACharacter>(GetOwner()); }

APlayerController* UAAAWeaponComponent::ControllingPC() const
{
	const ACharacter* C = Char();
	APlayerController* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr;
	return PC && PC->IsLocalController() ? PC : nullptr;
}

void UAAAWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	Ammo = MagazineSize;
	if (ACharacter* C = Char())
	{
		if (USkeletalMeshComponent* M = C->GetMesh())
		{
			DefaultMeshLoc = M->GetRelativeLocation();
			DefaultMeshRot = M->GetRelativeRotation();
			M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		if (C->GetCapsuleComponent()) C->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	if (AActor* O = GetOwner()) O->OnTakeAnyDamage.AddDynamic(this, &UAAAWeaponComponent::OnOwnerDamaged);
	if (!HudHandle.IsValid()) HudHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &UAAAWeaponComponent::DrawHud));
	TracerMat = AAAShapes::Mat(this, FLinearColor(1.f, 0.78f, 0.35f) * 6.f, true);
	PoliceTracerMat = AAAShapes::Mat(this, FLinearColor(1.f, 0.55f, 0.3f) * 5.f, true);
	HoleMat = AAAShapes::Mat(this, FLinearColor(0.012f, 0.011f, 0.01f));
	PuffMat = AAAShapes::Mat(this, FLinearColor(0.42f, 0.4f, 0.37f));
	BrassMat = AAAShapes::Mat(this, FLinearColor(0.62f, 0.45f, 0.14f));
	LoadSounds();
	SetupAaaPostProcess();
	CreateSmartHud();
	if (AvailableWeapons.Num() > 0 && !CurrentWeaponData)
	{
		EquipWeaponByIndex(0);
	}
}

void UAAAWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (APlayerController* PC = ControllingPC()) PC->ClearAudioListenerOverride();
	if (bDead && GetWorld()) UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);   // (left in the death slow motion)
	G3DAudio.Echoes.Reset();
	if (SmartHudWidget) { SmartHudWidget->RemoveFromParent(); SmartHudWidget = nullptr; }
	if (HudHandle.IsValid()) { UDebugDrawService::Unregister(HudHandle); HudHandle.Reset(); }
	if (bArmed) SetArmed(false);
	for (const TWeakObjectPtr<AActor>& T : SpawnedTargets) if (T.IsValid()) T->Destroy();
	SpawnedTargets.Reset();
	Super::EndPlay(Reason);
}

void UAAAWeaponComponent::LoadSounds()
{
	bSoundsLoaded = true;

	// Real M4A1 gunshot sound from user upload (m4_shot)
	M4ShotSound = LoadSfx(TEXT("m4_shot"));
	if (!M4ShotSound) M4ShotSound = LoadSfx(TEXT("rifle_shot"));
	ShotSound = M4ShotSound;

	// Real Beretta M9 pistol gunshot sound from user upload (pistol_shot)
	PistolShotSound = LoadSfx(TEXT("pistol_shot"));
	if (!PistolShotSound) PistolShotSound = M4ShotSound;

	// Real weapon reload & handling mechanical foley (m4_reload, m4_equip)
	ReloadSound = LoadSfx(TEXT("m4_reload"));
	if (!ReloadSound) ReloadSound = LoadSfx(TEXT("rifle_reload"));
	EquipSound = LoadSfx(TEXT("m4_equip"));
	if (!EquipSound) EquipSound = LoadSfx(TEXT("rifle_equip"));

	DrySound = LoadSfx(TEXT("rifle_dry"));
	ImpactSound = LoadSfx(TEXT("bullet_impact"));
	HurtSound = LoadSfx(TEXT("player_hurt"));
	// Authentic weapon reload foley (mag out, mag in, bolt rack sliced directly from recorded audio)
	MagOutSound = LoadSfx(TEXT("m4_mag_out"));
	if (!MagOutSound) MagOutSound = LoadSfx(TEXT("rifle_mag_out"));
	MagInSound = LoadSfx(TEXT("m4_mag_in"));
	if (!MagInSound) MagInSound = LoadSfx(TEXT("rifle_mag_in"));
	BoltSound = LoadSfx(TEXT("m4_bolt"));
	if (!BoltSound) BoltSound = LoadSfx(TEXT("rifle_bolt"));

	// Load dynamic footstep sound variations (footstep_01 to footstep_06)
	FootstepSounds.Reset();
	for (int32 I = 1; I <= 6; ++I)
	{
		const FString Name = FString::Printf(TEXT("footstep_%02d"), I);
		if (USoundBase* S = LoadSfx(*Name))
		{
			S->AttenuationSettings = Get3DFoleyAttenuation();
			FootstepSounds.Add(S);
		}
	}

	if (M4ShotSound) M4ShotSound->AttenuationSettings = Get3DCombatAttenuation();
	if (PistolShotSound) PistolShotSound->AttenuationSettings = Get3DCombatAttenuation();
	if (ReloadSound) ReloadSound->AttenuationSettings = Get3DFoleyAttenuation();
	if (EquipSound) EquipSound->AttenuationSettings = Get3DFoleyAttenuation();
	if (MagOutSound) MagOutSound->AttenuationSettings = Get3DFoleyAttenuation();
	if (MagInSound) MagInSound->AttenuationSettings = Get3DFoleyAttenuation();
	if (BoltSound) BoltSound->AttenuationSettings = Get3DFoleyAttenuation();
}

void UAAAWeaponComponent::PlaySound(USoundBase* S, const FVector& At, float Vol, float Pitch) const
{
	if (!S || Vol < 0.01f) return;
	USoundAttenuation* Atten = (S == M4ShotSound || S == PistolShotSound || S == ShotSound) ? Get3DCombatAttenuation() : Get3DFoleyAttenuation();
	const float MaxRange = (S == M4ShotSound || S == PistolShotSound || S == ShotSound) ? 12000.f : 4200.f;
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager && FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), At) > FMath::Square(MaxRange))
		{
			return;
		}
	}
	UGameplayStatics::PlaySoundAtLocation(this, S, At, FRotator::ZeroRotator, Vol, Pitch, 0.f, Atten);
}

ATrafficAIManager* UAAAWeaponComponent::Traffic()
{
	if (!TrafficMgr.IsValid() && TrafficSearchT <= 0.f && GetWorld())
	{
		TrafficSearchT = 2.f;
		for (TActorIterator<ATrafficAIManager> It(GetWorld()); It; ++It) { TrafficMgr = *It; break; }
	}
	return TrafficMgr.Get();
}

/* ============================================================================================ rifle model */

UStaticMeshComponent* UAAAWeaponComponent::Pooled(TArray<TObjectPtr<UStaticMeshComponent>>& Pool, int32& Next, int32 Max, bool bCylinder, UMaterialInstanceDynamic* M, bool bShadow, int32& OutIdx)
{
	if (Pool.Num() < Max)
	{
		UStaticMeshComponent* C = AAAShapes::Make(GetOwner(), bCylinder, FVector(1.0), M, false, bShadow);
		C->SetMobility(EComponentMobility::Movable);
		C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
		C->RegisterComponent();
		OutIdx = Pool.Add(C);
		return C;
	}
	OutIdx = Next;
	Next = (Next + 1) % Max;
	return Pool[OutIdx];
}

void UAAAWeaponComponent::RebuildWeaponModel()
{
	ACharacter* C = Char();
	if (!C) return;
	for (TObjectPtr<UStaticMeshComponent>& Part : RifleParts)
	{
		if (Part) Part->DestroyComponent();
	}
	RifleParts.Reset();
	if (USceneComponent* Kb = KarBolt.Get()) Kb->DestroyComponent();
	KarBolt = nullptr;
	SniperFireT = -1.f;
	Magazine = nullptr;
	GPistolSlideComp = nullptr;
	GPistolHammerComp = nullptr;
	bBuilt = false;
	const float Dist = (Anim && Anim->bCalibrated) ? Anim->LeftDist : (CurrentWeaponType == EAAAWeaponType::Pistol ? 8.f : 32.f);
	BuildRifle(Dist);
	AttachRifle();
}

void UAAAWeaponComponent::BuildRifle(float LeftDist)
{
	ACharacter* C = Char();
	if (!C || bBuilt) return;
	bBuilt = true;
	if (!Rifle)
	{
		Rifle = NewObject<USceneComponent>(C, TEXT("AAARifle"));
		Rifle->SetMobility(EComponentMobility::Movable);
		Rifle->SetupAttachment(C->GetMesh());
		Rifle->RegisterComponent();
	}
	UMaterialInstanceDynamic* Metal = AAAShapes::Mat(this, MetalColor);
	UMaterialInstanceDynamic* Poly = AAAShapes::Mat(this, PolymerColor);
	UMaterialInstanceDynamic* Dark = AAAShapes::Mat(this, FLinearColor(0.015f, 0.015f, 0.016f));

	auto Add = [&](bool bCyl, const FVector& Loc, const FRotator& Rot, const FVector& Size, UMaterialInstanceDynamic* M) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* P = AAAShapes::Make(C, bCyl, Size, M, false, true);
		P->SetMobility(EComponentMobility::Movable);
		P->SetupAttachment(Rifle);
		P->SetRelativeLocationAndRotation(Loc, Rot);
		P->SetVisibility(true);
		P->RegisterComponent();
		RifleParts.Add(P);
		return P;
	};
	const FRotator Along(90.0, 0.0, 0.0);   // cylinder axis (Z) -> X

	if (CurrentWeaponData)
	{
		// Data-Driven Blueprint Weapon Mesh Construction
		if (CurrentWeaponData->BodyMesh)
		{
			UStaticMeshComponent* BodyComp = NewObject<UStaticMeshComponent>(C);
			BodyComp->SetMobility(EComponentMobility::Movable);
			BodyComp->SetupAttachment(Rifle);
			BodyComp->SetStaticMesh(CurrentWeaponData->BodyMesh);
			if (CurrentWeaponData->MaterialOverride) BodyComp->SetMaterial(0, CurrentWeaponData->MaterialOverride);
			BodyComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BodyComp->SetCastShadow(true);
			BodyComp->SetVisibility(true);
			BodyComp->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			BodyComp->RegisterComponent();
			RifleParts.Add(BodyComp);
		}
		if (CurrentWeaponData->SlideMesh)
		{
			UStaticMeshComponent* SlideComp = NewObject<UStaticMeshComponent>(C);
			SlideComp->SetMobility(EComponentMobility::Movable);
			SlideComp->SetupAttachment(Rifle);
			SlideComp->SetStaticMesh(CurrentWeaponData->SlideMesh);
			if (CurrentWeaponData->MaterialOverride) SlideComp->SetMaterial(0, CurrentWeaponData->MaterialOverride);
			SlideComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SlideComp->SetCastShadow(true);
			SlideComp->SetVisibility(true);
			SlideComp->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			SlideComp->RegisterComponent();
			RifleParts.Add(SlideComp);
			GPistolSlideComp = SlideComp;
		}
		if (CurrentWeaponData->HammerMesh)
		{
			UStaticMeshComponent* HammerComp = NewObject<UStaticMeshComponent>(C);
			HammerComp->SetMobility(EComponentMobility::Movable);
			HammerComp->SetupAttachment(Rifle);
			HammerComp->SetStaticMesh(CurrentWeaponData->HammerMesh);
			if (CurrentWeaponData->MaterialOverride) HammerComp->SetMaterial(0, CurrentWeaponData->MaterialOverride);
			HammerComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			HammerComp->SetCastShadow(true);
			HammerComp->SetVisibility(true);
			HammerComp->SetRelativeLocationAndRotation(CurrentWeaponData->HammerPivot, FRotator::ZeroRotator);
			HammerComp->RegisterComponent();
			RifleParts.Add(HammerComp);
			GPistolHammerComp = HammerComp;
		}
		if (CurrentWeaponData->MagazineMesh)
		{
			UStaticMeshComponent* MagComp = NewObject<UStaticMeshComponent>(C);
			MagComp->SetMobility(EComponentMobility::Movable);
			MagComp->SetupAttachment(Rifle);
			MagComp->SetStaticMesh(CurrentWeaponData->MagazineMesh);
			if (CurrentWeaponData->MaterialOverride) MagComp->SetMaterial(0, CurrentWeaponData->MaterialOverride);
			MagComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MagComp->SetCastShadow(true);
			MagComp->SetVisibility(true);
			MagComp->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			MagComp->RegisterComponent();
			RifleParts.Add(MagComp);
			Magazine = MagComp;
			MagRel = Magazine->GetRelativeTransform();
		}

		MuzzleLocal = CurrentWeaponData->MuzzleLocation;
		EjectLocal = CurrentWeaponData->EjectionLocation;
	}
	else if (CurrentWeaponType == EAAAWeaponType::Kar98)
	{
		// Kar98k bolt-action sniper, built from primitives (rifle frame: origin = right palm, X forward, Z up, bore axis at Z 8.6)
		UMaterialInstanceDynamic* Walnut = AAAShapes::Mat(this, FLinearColor(0.16f, 0.085f, 0.04f));
		UMaterialInstanceDynamic* Blued  = AAAShapes::Mat(this, FLinearColor(0.028f, 0.032f, 0.042f));
		UMaterialInstanceDynamic* Steel  = AAAShapes::Mat(this, FLinearColor(0.11f, 0.115f, 0.125f));
		UMaterialInstanceDynamic* Brass  = AAAShapes::Mat(this, FLinearColor(0.62f, 0.45f, 0.14f));
		UMaterialInstanceDynamic* Glass  = AAAShapes::Mat(this, FLinearColor(0.02f, 0.12f, 0.28f) * 2.0f, true);
		// stock: butt, comb / wrist, forend
		Add(false, FVector(-22.5, 0.0, 2.6), FRotator(-5.0, 0.0, 0.0), FVector(20.0, 3.7, 10.5), Walnut);   // butt: real length of pull (~33 cm), it ends in the shoulder pocket
		Add(false, FVector(-11.0, 0.0, 4.2), FRotator::ZeroRotator, FVector(14.0, 3.9, 6.8), Walnut);
		Add(false, FVector(-1.5, 0.0, 1.0), FRotator(14.0, 0.0, 0.0), FVector(3.8, 3.2, 9.0), Walnut);   // pistol grip
		Add(false, FVector(30.0, 0.0, 5.6), FRotator::ZeroRotator, FVector(38.0, 3.6, 5.2), Walnut);
		Add(false, FVector(-33.0, 0.0, 2.2), FRotator(-5.0, 0.0, 0.0), FVector(1.2, 3.9, 10.8), Blued);   // butt plate
		// receiver, barrel, front sight, rear sight
		Add(false, FVector(3.5, 0.0, 8.2), FRotator::ZeroRotator, FVector(19.0, 3.4, 4.0), Blued);
		Add(true, FVector(37.0, 0.0, 8.6), Along, FVector(1.9, 1.9, 62.0), Blued);
		Add(true, FVector(65.0, 0.0, 8.6), Along, FVector(2.7, 2.7, 3.0), Blued);                        // front sight hood ring
		Add(false, FVector(65.0, 0.0, 10.6), FRotator::ZeroRotator, FVector(1.2, 0.5, 1.6), Steel);
		Add(true, FVector(50.0, 0.0, 8.6), Along, FVector(2.6, 2.6, 1.2), Steel);                        // barrel band
		// trigger guard + trigger + floor plate
		Add(false, FVector(2.5, 0.0, -1.6), FRotator::ZeroRotator, FVector(10.0, 2.2, 0.9), Blued);
		Add(false, FVector(1.0, 0.0, 1.2), FRotator(12.0, 0.0, 0.0), FVector(0.7, 0.7, 2.2), Steel);
		// 8x scope: tube, objective bell, eyepiece, lenses, two rings
		Add(true, FVector(5.0, 0.0, 12.9), Along, FVector(3.0, 3.0, 28.0), Blued);
		Add(true, FVector(18.5, 0.0, 12.9), Along, FVector(4.5, 4.5, 8.0), Blued);
		Add(true, FVector(-9.5, 0.0, 12.9), Along, FVector(3.8, 3.8, 5.0), Blued);
		Add(true, FVector(22.6, 0.0, 12.9), Along, FVector(4.0, 4.0, 0.4), Glass);
		Add(true, FVector(-12.2, 0.0, 12.9), Along, FVector(3.2, 3.2, 0.4), Glass);
		Add(false, FVector(0.0, 0.0, 10.6), FRotator::ZeroRotator, FVector(2.2, 2.6, 3.4), Steel);
		Add(false, FVector(11.5, 0.0, 10.6), FRotator::ZeroRotator, FVector(2.2, 2.6, 3.4), Steel);
		// 5-round stripper clip in the well: it is the "magazine" the reload animation takes out and puts back
		Magazine = Add(false, FVector(5.0, 0.0, 4.8), FRotator::ZeroRotator, FVector(7.0, 1.6, 2.2), Brass);
		MagRel = Magazine->GetRelativeTransform();

		// the bolt: body + sleeve + a bent handle on the right side (swept down) with a knob; it rotates up, slides back,
		// forward and down with the sniper clip (SniperFireT)
		USceneComponent* Bolt = NewObject<USceneComponent>(C, TEXT("AAAKarBolt"));
		Bolt->SetMobility(EComponentMobility::Movable);
		Bolt->SetupAttachment(Rifle);
		Bolt->SetRelativeLocation(FVector(1.0, 0.0, 8.6));
		Bolt->RegisterComponent();
		KarBolt = Bolt;
		auto AddBolt = [&](bool bCyl, const FVector& Loc, const FRotator& Rot, const FVector& Size, UMaterialInstanceDynamic* M)
		{
			UStaticMeshComponent* P = Add(bCyl, Loc, Rot, Size, M);
			P->AttachToComponent(Bolt, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
			P->SetRelativeLocationAndRotation(Loc, Rot);
		};
		AddBolt(true, FVector(-4.0, 0.0, 0.0), Along, FVector(1.8, 1.8, 13.0), Steel);              // bolt body
		AddBolt(true, FVector(-9.5, 0.0, 0.0), Along, FVector(2.3, 2.3, 4.0), Blued);               // bolt sleeve / shroud
		AddBolt(false, FVector(-2.0, 2.6, -0.9), FRotator::ZeroRotator, FVector(1.1, 4.4, 1.0), Steel);   // handle stem (out to the right, down)
		AddBolt(false, FVector(-2.0, 5.4, -1.8), FRotator::ZeroRotator, FVector(2.0, 2.0, 2.0), Steel);   // knob

		MuzzleLocal = FVector(67.5, 0.0, 8.6);
		EjectLocal = FVector(3.0, 2.6, 9.4);
	}
	else if (CurrentWeaponType == EAAAWeaponType::Pistol)
	{
		// Real 3D Colt 1911 (.45 ACP) with PBR Materials, Animated Recoil Slide, Hammer & Separate Magazine
		EnsureColt1911MeshesBuilt(this);

		if (GColtBodyMesh && GColtSlideMesh && GColtMagMesh && GColtHammerMesh)
		{
			// 1. Colt 1911 Frame / Lower Receiver & Barrel
			UStaticMeshComponent* ColtBodyComp = NewObject<UStaticMeshComponent>(C);
			ColtBodyComp->SetMobility(EComponentMobility::Movable);
			ColtBodyComp->SetupAttachment(Rifle);
			ColtBodyComp->SetStaticMesh(GColtBodyMesh);
			if (GColtMat) ColtBodyComp->SetMaterial(0, GColtMat);
			ColtBodyComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ColtBodyComp->SetCastShadow(true);
			ColtBodyComp->SetVisibility(true);
			ColtBodyComp->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			ColtBodyComp->RegisterComponent();
			RifleParts.Add(ColtBodyComp);

			// 2. Colt 1911 Animated Recoil Slide (Blowback slider)
			UStaticMeshComponent* ColtSlideComp = NewObject<UStaticMeshComponent>(C);
			ColtSlideComp->SetMobility(EComponentMobility::Movable);
			ColtSlideComp->SetupAttachment(Rifle);
			ColtSlideComp->SetStaticMesh(GColtSlideMesh);
			if (GColtMat) ColtSlideComp->SetMaterial(0, GColtMat);
			ColtSlideComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ColtSlideComp->SetCastShadow(true);
			ColtSlideComp->SetVisibility(true);
			ColtSlideComp->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			ColtSlideComp->RegisterComponent();
			RifleParts.Add(ColtSlideComp);
			GPistolSlideComp = ColtSlideComp;

			// 3. Colt 1911 Animated Cocking / Striking Hammer
			UStaticMeshComponent* ColtHammerComp = NewObject<UStaticMeshComponent>(C);
			ColtHammerComp->SetMobility(EComponentMobility::Movable);
			ColtHammerComp->SetupAttachment(Rifle);
			ColtHammerComp->SetStaticMesh(GColtHammerMesh);
			if (GColtMat) ColtHammerComp->SetMaterial(0, GColtMat);
			ColtHammerComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ColtHammerComp->SetCastShadow(true);
			ColtHammerComp->SetVisibility(true);
			ColtHammerComp->SetRelativeLocationAndRotation(FVector(-2.6f, 0.0f, 6.0f), FRotator::ZeroRotator);
			ColtHammerComp->RegisterComponent();
			RifleParts.Add(ColtHammerComp);
			GPistolHammerComp = ColtHammerComp;

			// 4. Colt 1911 Separate Magazine (physically drops and inserts on reload)
			UStaticMeshComponent* ColtMagComp = NewObject<UStaticMeshComponent>(C);
			ColtMagComp->SetMobility(EComponentMobility::Movable);
			ColtMagComp->SetupAttachment(Rifle);
			ColtMagComp->SetStaticMesh(GColtMagMesh);
			if (GColtMat) ColtMagComp->SetMaterial(0, GColtMat);
			ColtMagComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ColtMagComp->SetCastShadow(true);
			ColtMagComp->SetVisibility(true);
			ColtMagComp->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			ColtMagComp->RegisterComponent();
			RifleParts.Add(ColtMagComp);
			Magazine = ColtMagComp;
			MagRel = Magazine->GetRelativeTransform();

			// Authentic .45 ACP Muzzle Location
			MuzzleLocal = FVector(17.43, 0.0, 8.2);
			// Authentic Ejection Port Location (right side of slide)
			EjectLocal = FVector(7.5, 1.1, 7.8);
		}
		else
		{
			// Fallback procedural pistol if binary mesh files are missing
			Add(false, FVector(-1.0, 0.0, -3.2), FRotator(18.0, 0.0, 0.0), FVector(3.4, 2.4, 9.6), Dark);
			Add(false, FVector(4.0, 0.0, 3.7), FRotator::ZeroRotator, FVector(17.2, 2.7, 3.0), Metal);
			Magazine = Add(false, FVector(-0.2, 0.0, -4.8), FRotator(18.0, 0.0, 0.0), FVector(2.6, 2.0, 9.4), Metal);
			MagRel = Magazine->GetRelativeTransform();
			MuzzleLocal = FVector(13.6, 0.0, 3.7);
			EjectLocal = FVector(2.8, 1.6, 4.2);
		}
	}
	else
	{
		// Real 3D M4A1 Vltor MUR Mesh (Lower_Receiver_low.fbx + Upper_Receiver_low.fbx + 4K PBR Textures)
		EnsureM4A1MeshesBuilt(this);
		if (GM4BodyMesh && GM4MagMesh)
		{
			UStaticMeshComponent* M4BodyComp = NewObject<UStaticMeshComponent>(C);
			M4BodyComp->SetMobility(EComponentMobility::Movable);
			M4BodyComp->SetupAttachment(Rifle);
			M4BodyComp->SetStaticMesh(GM4BodyMesh);
			for (int32 SecIdx = 0; SecIdx < 4; ++SecIdx)
			{
				if (GM4SectionMats[SecIdx])
				{
					M4BodyComp->SetMaterial(SecIdx, GM4SectionMats[SecIdx]);
				}
			}
			M4BodyComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M4BodyComp->SetCastShadow(true);
			M4BodyComp->SetVisibility(true);
			M4BodyComp->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			M4BodyComp->RegisterComponent();
			RifleParts.Add(M4BodyComp);

			if (CurrentWeaponType == EAAAWeaponType::LMG)
			{
				UMaterialInstanceDynamic* FdeAccent = AAAShapes::Mat(this, FLinearColor(0.32f, 0.26f, 0.18f));
				Magazine = Add(false, FVector(11.2, 0.0, -5.5), FRotator(-5.0, 0.0, 0.0), FVector(14.0, 8.5, 12.0), FdeAccent);
			}
			else
			{
				UStaticMeshComponent* M4MagComp = NewObject<UStaticMeshComponent>(C);
				M4MagComp->SetMobility(EComponentMobility::Movable);
				M4MagComp->SetupAttachment(Rifle);
				M4MagComp->SetStaticMesh(GM4MagMesh);
				if (GM4SectionMats[2]) M4MagComp->SetMaterial(0, GM4SectionMats[2]);
				M4MagComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				M4MagComp->SetCastShadow(true);
				M4MagComp->SetVisibility(true);
				M4MagComp->SetRelativeLocationAndRotation(FVector(11.2, 0.0, -3.5), FRotator::ZeroRotator);
				M4MagComp->RegisterComponent();
				RifleParts.Add(M4MagComp);
				Magazine = M4MagComp;
			}
			MagRel = Magazine->GetRelativeTransform();
			MuzzleLocal = FVector(58.0, 0.0, 8.42);
			EjectLocal = FVector(9.2, 2.4, 8.42);
		}
		else
		{
			// Fallback Procedural M4A1 if binary mesh files are missing
			const double D = FMath::Clamp((double)LeftDist, 22.0, 48.0);
			const double HgA = 17.5, HgB = D + 12.5;
			UMaterialInstanceDynamic* AnodizedUpper = AAAShapes::Mat(this, FLinearColor(0.042f, 0.045f, 0.050f));
			UMaterialInstanceDynamic* ForgedLower   = AAAShapes::Mat(this, FLinearColor(0.025f, 0.026f, 0.029f));
			UMaterialInstanceDynamic* FdeAccent     = AAAShapes::Mat(this, FLinearColor(0.32f, 0.26f, 0.18f));
			Add(false, FVector(-1.0, 0.0, -3.0), FRotator(16.0, 0.0, 0.0), FVector(3.4, 2.7, 10.6), Dark);
			Add(false, FVector(5.5, 0.0, 4.8), FRotator::ZeroRotator, FVector(19.5, 4.0, 4.6), ForgedLower);
			Add(false, FVector(6.2, 0.0, 8.8), FRotator::ZeroRotator, FVector(20.5, 4.2, 3.8), AnodizedUpper);
			Add(true, FVector((HgA + HgB) * 0.5, 0.0, 8.6), Along, FVector(4.2, 4.2, HgB - HgA), Dark);
			Add(true, FVector(HgB + 7.5, 0.0, 8.6), Along, FVector(1.8, 1.8, 15.0), ForgedLower);
			Magazine = Add(false, FVector(12.5, 0.0, -4.4), FRotator(-11.0, 0.0, 0.0), FVector(6.2, 2.4, 15.2), AnodizedUpper);
			MagRel = Magazine->GetRelativeTransform();
			MuzzleLocal = FVector(HgB + 18.8, 0.0, 8.6);
			EjectLocal = FVector(9.2, 2.8, 8.6);
		}
	}

	if (!Flash)
	{
		Flash = AAAShapes::Make(C, true, FVector(3.5, 3.5, 12.0), AAAShapes::Mat(this, FLinearColor(1.0f, 0.52f, 0.12f) * 3.5f, true), false, false);
		Flash->SetMobility(EComponentMobility::Movable);
		Flash->SetupAttachment(Rifle);
		Flash->RegisterComponent();
		Flash->SetVisibility(false);
	}
	Flash->SetStaticMesh(AAAShapes::Cylinder());
	Flash->SetMaterial(0, AAAShapes::Mat(this, FLinearColor(1.0f, 0.52f, 0.12f) * 3.5f, true));
	Flash->SetRelativeLocation(MuzzleLocal + FVector(CurrentWeaponType == EAAAWeaponType::Pistol ? 4.0 : 6.5, 0.0, 0.0));
	Flash->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));

	if (CurrentWeaponType == EAAAWeaponType::Pistol)
	{
		Flash->SetRelativeScale3D(FVector(0.025, 0.025, 0.07));
	}
	else if (CurrentWeaponType == EAAAWeaponType::LMG)
	{
		Flash->SetRelativeScale3D(FVector(0.045, 0.045, 0.16));
	}
	else if (CurrentWeaponType == EAAAWeaponType::Kar98)
	{
		Flash->SetRelativeScale3D(FVector(0.05, 0.05, 0.2));
	}
	else
	{
		Flash->SetRelativeScale3D(FVector(0.035, 0.035, 0.12));
	}

	if (!FlashLight)
	{
		FlashLight = NewObject<UPointLightComponent>(C);
		FlashLight->SetMobility(EComponentMobility::Movable);
		FlashLight->SetupAttachment(Rifle);
		FlashLight->SetIntensity(0.f);
		FlashLight->SetAttenuationRadius(700.f);
		FlashLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.42f));
		FlashLight->SetCastShadows(false);
		FlashLight->RegisterComponent();
	}
	FlashLight->SetRelativeLocation(MuzzleLocal + FVector(10.0, 0.0, 0.0));
	Rifle->SetVisibility(bArmed, true);
}

void UAAAWeaponComponent::AttachRifle()
{
	ACharacter* C = Char();
	if (!C || !Rifle) return;
	const FTransform Grip = (Anim && Anim->bCalibrated) ? Anim->GripRel : FTransform::Identity;
	Rifle->AttachToComponent(C->GetMesh(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, false), TEXT("hand_r"));
	const FVector ExtraOffset = (CurrentWeaponType == EAAAWeaponType::Pistol) ? FVector::ZeroVector : GripOffset;
	const FRotator ExtraRot = (CurrentWeaponType == EAAAWeaponType::Pistol) ? FRotator::ZeroRotator : GripRotation;
	Rifle->SetRelativeTransform(FTransform(ExtraRot, ExtraOffset) * Grip);
	Rifle->SetVisibility(true, true);
	if (Flash) Flash->SetVisibility(false);
	bRifleAttached = true;
}

void UAAAWeaponComponent::BuildBinoculars()
{
	ACharacter* C = Char();
	if (!C || BinocularsComp.IsValid()) return;

	USceneComponent* Root = NewObject<USceneComponent>(C, TEXT("AAABinoculars"));
	Root->SetMobility(EComponentMobility::Movable);
	Root->SetupAttachment(C->GetMesh(), TEXT("head"));
	Root->RegisterComponent();
	BinocularsComp = Root;

	UMaterialInstanceDynamic* BodyMat = AAAShapes::Mat(this, FLinearColor(0.02f, 0.02f, 0.025f)); // Tactical dark polymer
	UMaterialInstanceDynamic* RubberMat = AAAShapes::Mat(this, FLinearColor(0.04f, 0.04f, 0.04f)); // Matte rubber
	UMaterialInstanceDynamic* LensMat = AAAShapes::Mat(this, FLinearColor(0.02f, 0.25f, 0.45f) * 2.5f, true); // Anti-reflective coated optic glass
	UMaterialInstanceDynamic* DialMat = AAAShapes::Mat(this, FLinearColor(0.85f, 0.65f, 0.15f)); // Brass focus ring

	auto AddPart = [&](bool bCyl, const FVector& Loc, const FRotator& Rot, const FVector& Size, UMaterialInstanceDynamic* M)
	{
		UStaticMeshComponent* P = AAAShapes::Make(C, bCyl, Size, M, false, true);
		P->SetMobility(EComponentMobility::Movable);
		P->SetupAttachment(Root);
		P->SetRelativeLocationAndRotation(Loc, Rot);
		P->RegisterComponent();
		BinocularsParts.Add(P);
		return P;
	};

	// Right barrel & optics
	AddPart(true, FVector(11.0, 3.8, 2.0), FRotator(90.0, 0.0, 0.0), FVector(3.2, 3.2, 13.0), BodyMat);
	AddPart(true, FVector(8.5, 3.8, 2.0), FRotator(90.0, 0.0, 0.0), FVector(3.4, 3.4, 6.5), RubberMat);
	AddPart(true, FVector(17.5, 3.8, 2.0), FRotator(90.0, 0.0, 0.0), FVector(3.0, 3.0, 0.4), LensMat);
	AddPart(true, FVector(4.2, 3.8, 2.0), FRotator(90.0, 0.0, 0.0), FVector(2.2, 2.2, 2.2), RubberMat);

	// Left barrel & optics
	AddPart(true, FVector(11.0, -3.8, 2.0), FRotator(90.0, 0.0, 0.0), FVector(3.2, 3.2, 13.0), BodyMat);
	AddPart(true, FVector(8.5, -3.8, 2.0), FRotator(90.0, 0.0, 0.0), FVector(3.4, 3.4, 6.5), RubberMat);
	AddPart(true, FVector(17.5, -3.8, 2.0), FRotator(90.0, 0.0, 0.0), FVector(3.0, 3.0, 0.4), LensMat);
	AddPart(true, FVector(4.2, -3.8, 2.0), FRotator(90.0, 0.0, 0.0), FVector(2.2, 2.2, 2.2), RubberMat);

	// Central bridge & focus dial
	AddPart(false, FVector(10.5, 0.0, 2.0), FRotator::ZeroRotator, FVector(6.0, 3.6, 1.8), BodyMat);
	AddPart(true, FVector(11.5, 0.0, 3.1), FRotator(0.0, 0.0, 90.0), FVector(1.8, 1.8, 2.4), DialMat);

	Root->SetVisibility(false, true);
}

bool UAAAWeaponComponent::AimTrace(FVector& OutAt, ACharacter*& OutEnemy) const
{
	// where the leader points: the camera's centre ray (soldiers count from a little off the ray)
	OutEnemy = nullptr;
	APlayerController* PC = ControllingPC();
	ACharacter* C = Char();
	if (!PC || !PC->PlayerCameraManager || !C) { OutAt = C ? C->GetActorLocation() : FVector::ZeroVector; return false; }
	const FVector From = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Dir = PC->PlayerCameraManager->GetCameraRotation().Vector();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAALeaderAim), false, C);
	FHitResult H;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(H, From, From + Dir * 15000.0, ECC_Visibility, Q);
	OutAt = bHit ? H.ImpactPoint : From + Dir * 15000.0;
	UAAAWeaponsSubsystem* Sq = GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>();
	float Best = 1e9f;
	for (TActorIterator<AAAAPoliceOfficer> It(GetWorld()); It; ++It)
	{
		AAAAPoliceOfficer* O = *It;
		if (!O || O->IsOut() || O->Team == AAATeam::Player) continue;
		if (!AAAAPoliceOfficer::Hostile(AAATeam::Player, O->Team, Sq ? FMath::Max(1, Sq->WantedStars()) : 1)) continue;
		const FVector P = O->GetActorLocation() + FVector(0.0, 0.0, 30.0);
		const double Along = FVector::DotProduct(P - From, Dir);
		if (Along < 100.0 || Along > (bHit ? H.Distance + 400.0 : 15000.0)) continue;
		const float Off = (float)FMath::PointDistToLine(P, Dir, From);
		const float Allow = FMath::Max(120.f, (float)Along * 0.035f);
		if (Off < Allow && Off / Allow < Best) { Best = Off / Allow; OutEnemy = O; }
	}
	if (OutEnemy) OutAt = OutEnemy->GetActorLocation();
	// on the ground below the point (an order to a wall face means its foot)
	FHitResult G;
	if (GetWorld()->LineTraceSingleByChannel(G, OutAt + FVector(0.0, 0.0, 80.0) - Dir * 60.0, OutAt - FVector(0.0, 0.0, 800.0), ECC_Visibility, Q)) OutAt = G.ImpactPoint;
	return bHit || OutEnemy != nullptr;
}

AAAAPoliceOfficer* UAAAWeaponComponent::GetRevivePatient() const { return RevivePatient.Get(); }

void UAAAWeaponComponent::UpdateSquadInput(APlayerController* PC, ACharacter* C, float Dt, bool bInputOk)
{
	UAAAWeaponsSubsystem* Sq = GetWorld() ? GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>() : nullptr;
	if (!Sq || !PC || !C || bDead) { ReviveHoldT = 0.f; return; }
	if (!bInputOk || bRadialMenuOpen) { ReviveHoldT = 0.f; return; }
	// 1..3: one man, 4: the whole team (the weapons moved to F1..F3)
	if (PC->WasInputKeyJustPressed(EKeys::One) && RadialClosedT > 0.1f) { Sq->SquadSelect(0); RadialAction(FString::Printf(TEXT("أوامر إلى: %s"), FAAASquadInfo::Name(0))); }
	if (PC->WasInputKeyJustPressed(EKeys::Two) && RadialClosedT > 0.1f) { Sq->SquadSelect(1); RadialAction(FString::Printf(TEXT("أوامر إلى: %s"), FAAASquadInfo::Name(1))); }
	if (PC->WasInputKeyJustPressed(EKeys::Three) && RadialClosedT > 0.1f) { Sq->SquadSelect(2); RadialAction(FString::Printf(TEXT("أوامر إلى: %s"), FAAASquadInfo::Name(2))); }
	if (PC->WasInputKeyJustPressed(EKeys::Four) && RadialClosedT > 0.1f) { Sq->SquadSelect(-1); RadialAction(TEXT("أوامر إلى: الفريق كله")); }
	// middle mouse: the context order (an enemy under the aim = attack him, else go there)
	if (PC->WasInputKeyJustPressed(EKeys::MiddleMouseButton))
	{
		FVector At; ACharacter* Tgt = nullptr;
		AimTrace(At, Tgt);
		Sq->SquadOrder(Tgt ? EAAASquadOrder::Assault : EAAASquadOrder::Move, At, Tgt);
	}
	if (PC->WasInputKeyJustPressed(EKeys::V)) Sq->SquadOrder(EAAASquadOrder::Follow, C->GetActorLocation(), nullptr);
	if (PC->WasInputKeyJustPressed(EKeys::Y)) Sq->SquadOrder(EAAASquadOrder::Hold, C->GetActorLocation(), nullptr);
	if (PC->WasInputKeyJustPressed(EKeys::U)) { FVector At; ACharacter* Tgt = nullptr; AimTrace(At, Tgt); Sq->SquadOrder(EAAASquadOrder::Suppress, At, nullptr); }
	if (PC->WasInputKeyJustPressed(EKeys::N)) Sq->SquadCycleFormation();
	if (PC->WasInputKeyJustPressed(EKeys::K)) Sq->SquadToggleFire();
	// first aid: hold F next to a downed man
	AAAAPoliceOfficer* Pat = Sq->DownedNear(C->GetActorLocation(), 190.f);
	RevivePatient = Pat;
	if (Pat && PC->IsInputKeyDown(EKeys::F) && C->GetVelocity().Size2D() < 60.f)
	{
		ReviveHoldT += Dt;
		Pat->AddRevive(Dt / 2.6f);
	}
	else ReviveHoldT = 0.f;
}

// The two sub wheels of the HUD (icons = Content/AAACity/UI/GhostsNovember/<Icon>.png)
const FAAARadialSubItem* UAAAWeaponComponent::GetRadialSubItems(int32 Kind, int32& OutNum)
{
	static const FAAARadialSubItem WeaponsRing[] = {
		{ TEXT("s17"), TEXT("STG 44"), 0 }, { TEXT("s20"), TEXT("MG 42"), 1 }, { TEXT("s24"), TEXT("P08"), 2 }, { TEXT("s6"), TEXT("إخفاء السلاح"), 3 }, { TEXT("kar98"), TEXT("Kar98k"), 4 } };
	static const FAAARadialSubItem GearRing[] = {
		{ TEXT("s3"), TEXT("قنبلة يدوية"), 10 }, { TEXT("s28"), TEXT("منظار"), 11 }, { TEXT("s2"), TEXT("حقيبة ذخيرة"), 12 }, { TEXT("s23"), TEXT("قمقم ماء"), 13 } };
	static const FAAARadialSubItem OrdersRing[] = {
		{ TEXT("o0"), TEXT("اتبعوني"), 20 }, { TEXT("o1"), TEXT("تقدّموا"), 21 }, { TEXT("o2"), TEXT("اثبتوا"), 22 }, { TEXT("o3"), TEXT("اهجموا"), 23 },
		{ TEXT("o4"), TEXT("احتموا"), 24 }, { TEXT("o5"), TEXT("نيران تغطية"), 25 }, { TEXT("o6"), TEXT("انسحبوا"), 26 }, { TEXT("o7"), TEXT("ضبط النار"), 27 } };
	if (Kind == 3) { OutNum = UE_ARRAY_COUNT(OrdersRing); return OrdersRing; }
	if (Kind == 1) { OutNum = UE_ARRAY_COUNT(WeaponsRing); return WeaponsRing; }
	if (Kind == 2) { OutNum = UE_ARRAY_COUNT(GearRing); return GearRing; }
	OutNum = 0;
	return nullptr;
}

void UAAAWeaponComponent::ExecuteRadialSub(int32 Kind, int32 Index, ACharacter* C)
{
	int32 Num = 0;
	const FAAARadialSubItem* Items = GetRadialSubItems(Kind, Num);
	if (!Items || Index < 0 || Index >= Num) return;
	const FAAARadialSubItem& It = Items[Index];
	switch (It.Action)
	{
	case 0: case 1: case 2: case 4:
	{
		if (bBinocularsActive) SetBinoculars(false);
		const EAAAWeaponType Want = It.Action == 0 ? EAAAWeaponType::Rifle : (It.Action == 1 ? EAAAWeaponType::LMG : (It.Action == 4 ? EAAAWeaponType::Kar98 : EAAAWeaponType::Pistol));
		if (CurrentWeaponType != Want) SwitchWeapon(Want);
		if (!bArmed) SetArmed(true);
		break;   // the HUD shows "تم تجهيز ..." by itself
	}
	case 3:
		if (bArmed) SetArmed(false);
		RadialAction(It.Label);
		break;
	case 10:
		StartCookingGrenade();
		RadialAction(It.Label);
		break;
	case 11:
		ToggleBinoculars();
		RadialAction(bBinocularsActive ? TEXT("المراقبة: المنظار") : TEXT("إنزال المنظار"));
		break;
	case 12:
		Ammo = MagazineSize;
		ReserveAmmo += MagazineSize * 2;
		if (C && ReloadSound) PlaySound(ReloadSound.Get(), C->GetActorLocation(), 0.7f, 1.1f);
		OnAmmoChanged.Broadcast(Ammo, ReserveAmmo);
		{
			// the men within 10 m fill their pouches too
			const int32 Fed = (C && GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>()) ? GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>()->ResupplySquad(C->GetActorLocation()) : 0;
			RadialAction(Fed > 0 ? FString::Printf(TEXT("حقيبة ذخيرة: +2 مخازن (وزّعت على %d)"), Fed) : FString(TEXT("حقيبة ذخيرة: +2 مخازن")));
		}
		break;
	case 20: case 21: case 22: case 23: case 24: case 25: case 26:
	{
		// the leader's orders: on the point he aims at (an enemy there = the target)
		if (UAAAWeaponsSubsystem* Sq = GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>())
		{
			FVector At; ACharacter* Tgt = nullptr;
			AimTrace(At, Tgt);
			const EAAASquadOrder O = (EAAASquadOrder)(It.Action - 20);
			Sq->SquadOrder(O, (O == EAAASquadOrder::Follow || O == EAAASquadOrder::FallBack || O == EAAASquadOrder::Hold || O == EAAASquadOrder::TakeCover) ? (C ? C->GetActorLocation() : At) : At, O == EAAASquadOrder::Assault ? Tgt : nullptr);
		}
		break;
	}
	case 27:
		if (UAAAWeaponsSubsystem* Sq = GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>()) Sq->SquadToggleFire();
		break;
	case 13:
		Stamina = 100.f;
		Health = FMath::Min(MaxHealth, Health + 10.f);
		RadialAction(TEXT("قمقم ماء: الطاقة ممتلئة"));
		break;
	default: break;
	}
}

void UAAAWeaponComponent::ToggleBinoculars()
{
	SetBinoculars(!bBinocularsActive);
}

void UAAAWeaponComponent::SetBinoculars(bool bActive)
{
	ACharacter* C = Char();
	if (!C || bDead) return;
	bBinocularsActive = bActive;
	if (bBinocularsActive)
	{
		if (!bArmed) SetArmed(true);
		BuildBinoculars();
		if (BinocularsComp.IsValid()) BinocularsComp->SetVisibility(false, true);
		if (Rifle) Rifle->SetVisibility(false, true);
		if (C->GetMesh()) C->GetMesh()->SetOwnerNoSee(true);
		PlaySound(EquipSound ? EquipSound.Get() : nullptr, C->GetActorLocation(), 0.7f, 1.45f);
	}
	else
	{
		if (BinocularsComp.IsValid()) BinocularsComp->SetVisibility(false, true);
		if (bArmed && Rifle) Rifle->SetVisibility(true, true);
		if (C->GetMesh()) C->GetMesh()->SetOwnerNoSee(false);
		PlaySound(EquipSound ? EquipSound.Get() : nullptr, C->GetActorLocation(), 0.6f, 0.95f);
	}
}

FTransform UAAAWeaponComponent::MuzzleTransform() const
{
	if (Rifle && bRifleAttached) return FTransform(MuzzleLocal) * Rifle->GetComponentTransform();
	const ACharacter* C = Char();
	return C ? FTransform(C->GetActorRotation(), C->GetActorLocation() + C->GetActorForwardVector() * 60.0 + FVector(0.0, 0.0, 40.0)) : FTransform::Identity;
}

void UAAAWeaponComponent::TriggerExplosionExposure(float Strength)
{
	ExposureFlashT = FMath::Clamp(Strength, 0.f, 1.f);
	HurtFlash = FMath::Max(HurtFlash, Strength * 0.75f);
	CamTrauma = FMath::Min(1.f, CamTrauma + FMath::Clamp(Strength, 0.f, 1.f) * 0.8f * CameraShakeScale);   // blast rattles the view
}

/* ============================================================================================ arm / holster */

void UAAAWeaponComponent::SetArmed(bool bNewArmed)
{
	ACharacter* C = Char();
	if (!C || bNewArmed == bArmed) return;
	USkeletalMeshComponent* M = C->GetMesh();
	UCharacterMovementComponent* CM = C->GetCharacterMovement();
	if (bNewArmed)
	{
		if (bDead || !M) return;
		bArmed = true; ArmT = 0.f; ReloadT = -1.f;
		if (!SavedAnimClass && M->GetAnimClass() != UAAARifleAnimInstance::StaticClass())
		{
			SavedAnimClass = M->GetAnimClass();
		}
		if (M->GetAnimClass() != UAAARifleAnimInstance::StaticClass())
		{
			M->SetAnimInstanceClass(UAAARifleAnimInstance::StaticClass());
		}
		Anim = Cast<UAAARifleAnimInstance>(M->GetAnimInstance());
		if (Anim && !Anim->bAssetsOk) { M->SetAnimInstanceClass(SavedAnimClass); Anim = nullptr; }   // rifle animations missing: keep the old ones
		if (CM)
		{
			bSavedOrient = CM->bOrientRotationToMovement; bSavedDesired = CM->bUseControllerDesiredRotation; SavedWalk = CM->MaxWalkSpeed;
			CM->bOrientRotationToMovement = false; CM->bUseControllerDesiredRotation = true;
		}
		bSavedYaw = C->bUseControllerRotationYaw;
		C->bUseControllerRotationYaw = false;
		// a calibrated model fits the new instance only after it evaluated once: re-attach then
		bRifleAttached = false;
		if (Rifle) Rifle->SetVisibility(false, true);
		PlaySound(EquipSound ? EquipSound.Get() : ReloadSound.Get(), C->GetActorLocation(), EquipSound ? 0.8f : 0.35f, EquipSound ? 1.f : 1.6f);
	}
	else
	{
		if (bBinocularsActive) SetBinoculars(false);
		bArmed = false; ReloadT = -1.f; bAiming = false;
		if (ReloadStage == 2 && Magazine && Rifle) { Magazine->AttachToComponent(Rifle, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false)); Magazine->SetRelativeTransform(MagRel); }
		ReloadStage = 0;
		if (Rifle) Rifle->SetVisibility(false, true);
		// Keep UAAARifleAnimInstance active in Unarmed mode so Turn-In-Place, Binoculars IK, and Unarmed Hand-Touch IK work seamlessly
		if (M && M->GetAnimClass() == UAAARifleAnimInstance::StaticClass())
		{
			Anim = Cast<UAAARifleAnimInstance>(M->GetAnimInstance());
		}
		if (CM) { CM->bOrientRotationToMovement = bSavedOrient; CM->bUseControllerDesiredRotation = bSavedDesired; CM->MaxWalkSpeed = SavedWalk; }
		C->bUseControllerRotationYaw = bSavedYaw;
	}
}

/* ============================================================================================ frame */

void UAAAWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* C = Char();
	if (!C || !GetWorld()) return;
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.05f);
	TrafficSearchT -= Dt;
	APlayerController* PC = ControllingPC();
	const bool bControlled = PC && !C->IsHidden();
	bHudOn = bControlled || bDead;

	StepBullets(Dt);
	UpdateFx(Dt);
	HitMarkT -= Dt; PointsT -= Dt; HelpT -= Dt;
	HurtFlash = FMath::Max(0.f, HurtFlash - Dt * 1.2f);
	ExposureFlashT = FMath::Max(0.f, ExposureFlashT - Dt * 2.2f);
	LastHitFromT += Dt;
	if (PlayerHitT >= 0.f) { PlayerHitT += Dt; if (PlayerHitT > 1.2f) PlayerHitT = -1.f; }

	if (bDead)
	{
		DeadT += Dt;
		// the death clip first (direction of the fatal hit), then the ragdoll takes over from its last pose
		if (!bRagdolled)
		{
			if (Anim)
			{
				Anim->Inputs.DeathTime = DeadT;
				Anim->Inputs.DeathIndex = DeathIdx + 100;   // (+100: the player - the anim lays him on his back for the shahada)
				Anim->Inputs.LocalVel = FVector2D::ZeroVector;
				Anim->Inputs.bFalling = false;
				// the martyr's last moments: lying where the clip left him (no ragdoll), he raises his index finger
				// and says the shahada (AAAMissionSubsystem shows the scene); the pose is in the anim instance
				Anim->Inputs.ShahadaT = DeadT >= DeathAnimLen ? DeadT - DeathAnimLen : -1.f;
			}
			if (!Anim) StartRagdoll();
		}
		// the fall in slow motion (x0.35), back to normal speed as he reaches the ground
		const float FallLen = (DeathIdx != INDEX_NONE && !bRagdolled) ? DeathAnimLen : 1.0f;
		if (UWorld* Wd = GetWorld())
			UGameplayStatics::SetGlobalTimeDilation(Wd, DeadT < FallLen ? FMath::Lerp(0.35f, 1.f, FMath::SmoothStep(FallLen * 0.7f, FallLen, DeadT)) : 1.f);
		UpdateCamera(Dt, false);
		if (DeadT > FallLen + 10.6f) Respawn();   // fall + testimony (raise 1.4, hold to 7.4, lower to 9) + fade
		return;
	}
	SinceHurt += Dt;
	if (SinceHurt > RegenDelay && Health < MaxHealth) Health = FMath::Min(MaxHealth, Health + RegenPerSecond * Dt);

	if (!bControlled)
	{
		if (bArmed) SetArmed(false);   // got into a car / lost control
		UpdateCamera(Dt, false);
		return;
	}

	const bool bInputOk = !PC->IsMoveInputIgnored();   // ignored while BUSTED

	// Auto-refresh weapon mesh on initial setup / Live Coding reload
	static UAAAWeaponComponent* GLastRefreshedCompV9 = nullptr;
	if (GLastRefreshedCompV9 != this)
	{
		GLastRefreshedCompV9 = this;
		EnsureColt1911MeshesBuilt(this, true);
		RebuildWeaponModel();
		if (!bArmed) SetArmed(true);
	}

	// Radial Weapon Wheel Input (Hold TAB or Q)
	const float RealDt = FMath::Clamp((C && C->CustomTimeDilation > 0.01f) ? (Dt / C->CustomTimeDilation) : Dt, 0.001f, 0.05f);
	const bool bRadialKeyHeld = bInputOk && (PC->IsInputKeyDown(EKeys::Tab) || PC->IsInputKeyDown(EKeys::Q));
	if (!bRadialKeyHeld) bRadialNeedRelease = false;
	RadialClosedT += RealDt;
	if (bRadialKeyHeld && !bRadialMenuOpen && !bRadialNeedRelease)
	{
		bRadialMenuOpen = true;
		RadialCursorOffset = FVector2D::ZeroVector;
		RadialKind = 0;
		RadialKindT = 9.f;
		PC->SetIgnoreLookInput(true);
		if (C) C->CustomTimeDilation = 0.25f; // Smooth tactical slow-motion
		if (C && EquipSound) PlaySound(EquipSound.Get(), C->GetActorLocation(), 0.45f, 1.35f);
	}
	else if (!bRadialKeyHeld && bRadialMenuOpen)
	{
		bRadialMenuOpen = false;
		PC->SetIgnoreLookInput(false);
		if (C) C->CustomTimeDilation = 1.0f; // Return to normal speed
		RadialClosedT = 0.f;

		// Execute action for selected radial sector (matching the 8 illustrated wheel panels)
		const int32 ExecKind = RadialKind;
		RadialKind = 0;
		if (ExecKind != 0) ExecuteRadialSub(ExecKind, RadialSubIndex, C);
		else switch (RadialSelectedIndex)
		{
		case 0: // [01] المراقبة (Surveillance / Recon Binoculars / Jumelle)
			ToggleBinoculars();
			RadialAction(bBinocularsActive ? TEXT("المراقبة: المنظار") : TEXT("إنزال المنظار"));
			break;
		case 1: // [02] الأسلحة (Weapons - Cycles / Equips Rifle -> LMG -> Pistol)
			if (!bArmed)
			{
				SetArmed(true);
			}
			else if (CurrentWeaponType == EAAAWeaponType::Rifle)
			{
				SwitchWeapon(EAAAWeaponType::LMG);
			}
			else if (CurrentWeaponType == EAAAWeaponType::LMG)
			{
				SwitchWeapon(EAAAWeaponType::Pistol);
			}
			else
			{
				SwitchWeapon(EAAAWeaponType::Rifle);
			}
			break;
		case 2: // [03] المعدات (Equipment - Squad LMG M249)
			if (CurrentWeaponType != EAAAWeaponType::LMG) SwitchWeapon(EAAAWeaponType::LMG);
			if (!bArmed) SetArmed(true);
			break;
		case 3: // [04] الأوامر (Orders - Deploy Training Targets)
			SpawnTargets(C);
			RadialAction(TEXT("الأوامر"));
			break;
		case 4: // [05] التسلل (Stealth - Holster Weapon)
			if (bArmed) SetArmed(false);
			RadialAction(TEXT("وضع التسلل"));
			break;
		case 5: // [06] العلاج (Medical - +50 HP Heal)
			Health = FMath::Min(MaxHealth, Health + 50.f);
			RadialAction(TEXT("تم العلاج +50"));
			if (C) PlaySound(EquipSound.Get(), C->GetActorLocation(), 0.8f, 1.2f);
			break;
		case 6: // [07] المهام (Missions - Frag Grenade)
			StartCookingGrenade();
			RadialAction(TEXT("المهام"));
			break;
		case 7: // [08] الخريطة (Map - Tactical Pistol GLOCK-19X)
			if (CurrentWeaponType != EAAAWeaponType::Pistol) SwitchWeapon(EAAAWeaponType::Pistol);
			if (!bArmed) SetArmed(true);
			break;
		default:
			break;
		}
	}

	if (bRadialMenuOpen)
	{
		float MouseX = 0.f, MouseY = 0.f;
		PC->GetInputMouseDelta(MouseX, MouseY);

		// Natural flick gesture: mouse delta creates directional impulse without being tied to compass angles
		RadialCursorOffset.X += MouseX * 1.8f;
		RadialCursorOffset.Y -= MouseY * 1.8f;
		RadialCursorOffset = RadialCursorOffset.GetClampedToMaxSize(45.f);

		RadialKindT += RealDt;
		int32 SubNum = 0;
		if (RadialKind != 0) GetRadialSubItems(RadialKind, SubNum);
		// open a sub ring (الأسلحة / المعدات) or go back to the main ring
		const bool bOpenKey = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton) || PC->WasInputKeyJustPressed(EKeys::Enter);
		const bool bBackKey = PC->WasInputKeyJustPressed(EKeys::RightMouseButton) || PC->WasInputKeyJustPressed(EKeys::BackSpace);
		if (RadialKind == 0 && bOpenKey && (RadialSelectedIndex == 1 || RadialSelectedIndex == 2 || RadialSelectedIndex == 3))
		{
			RadialKind = RadialSelectedIndex;
			RadialKindT = 0.f;
			RadialSubIndex = 0;
			if (RadialKind == 1)
				RadialSubIndex = !bArmed ? 3 : (CurrentWeaponType == EAAAWeaponType::Kar98 ? 4 : (CurrentWeaponType == EAAAWeaponType::LMG ? 1 : (CurrentWeaponType == EAAAWeaponType::Pistol ? 2 : 0)));
			RadialCursorOffset = FVector2D::ZeroVector;
			if (C && EquipSound) PlaySound(EquipSound.Get(), C->GetActorLocation(), 0.4f, 1.2f);
		}
		else if (RadialKind != 0 && bBackKey)
		{
			RadialKind = 0;
			RadialKindT = 0.f;
			RadialCursorOffset = FVector2D::ZeroVector;
			if (C && EquipSound) PlaySound(EquipSound.Get(), C->GetActorLocation(), 0.35f, 1.0f);
		}
		else if (RadialKind != 0 && SubNum > 0)
		{
			const bool bInstant = bOpenKey && RadialKindT > 0.25f;   // click an item = pick it and close
			if (bInstant)
			{
				bRadialMenuOpen = false;
				PC->SetIgnoreLookInput(false);
				if (C) C->CustomTimeDilation = 1.0f;
				bRadialNeedRelease = true;
				RadialClosedT = 0.f;
				const int32 K = RadialKind;
				RadialKind = 0;
				ExecuteRadialSub(K, RadialSubIndex, C);
			}
		}
		const int32 RingNum = RadialKind != 0 ? FMath::Max(SubNum, 1) : 8;
		int32& RingSel = RadialKind != 0 ? RadialSubIndex : RadialSelectedIndex;

		int32 NewIdx = RingSel;

		// Number key shortcuts [1 - 8]
		if (PC->WasInputKeyJustPressed(EKeys::One)) NewIdx = 0;
		else if (PC->WasInputKeyJustPressed(EKeys::Two)) NewIdx = 1;
		else if (PC->WasInputKeyJustPressed(EKeys::Three)) NewIdx = 2;
		else if (PC->WasInputKeyJustPressed(EKeys::Four)) NewIdx = 3;
		else if (PC->WasInputKeyJustPressed(EKeys::Five)) NewIdx = 4;
		else if (PC->WasInputKeyJustPressed(EKeys::Six)) NewIdx = 5;
		else if (PC->WasInputKeyJustPressed(EKeys::Seven)) NewIdx = 6;
		else if (PC->WasInputKeyJustPressed(EKeys::Eight)) NewIdx = 7;
		else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown) || PC->WasInputKeyJustPressed(EKeys::Right))
		{
			NewIdx = (RingSel + 1) % RingNum;
		}
		else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp) || PC->WasInputKeyJustPressed(EKeys::Left))
		{
			NewIdx = (RingSel + RingNum - 1) % RingNum;
		}
		else if (RadialCursorOffset.SizeSquared() > 100.f) // > 10px deadzone
		{
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(-RadialCursorOffset.Y, RadialCursorOffset.X));
			float Norm = AngleDeg - (-90.f - 22.5f);
			while (Norm < 0.f) Norm += 360.f;
			while (Norm >= 360.f) Norm -= 360.f;
			const int32 Flick = FMath::Clamp(FMath::FloorToInt(Norm / 45.f), 0, 7);
			if (Flick < RingNum) NewIdx = Flick;
		}

		// Decay virtual offset back to center so player can immediately flick in another direction
		RadialCursorOffset = FMath::Vector2DInterpTo(RadialCursorOffset, FVector2D::ZeroVector, RealDt, 10.f);

		if (NewIdx >= RingNum) NewIdx = RingSel;   // number keys past the last item of a sub ring
		if (bRadialMenuOpen && NewIdx != RingSel)
		{
			RingSel = NewIdx;
			if (C && DrySound) PlaySound(DrySound.Get(), C->GetActorLocation(), 0.35f, 1.45f);
		}
	}
	UpdateSquadInput(PC, C, RealDt, bInputOk);

#if !UE_BUILD_SHIPPING
	// The editor / development build binds F1..F12 to debug view modes (wireframe, unlit, detail lighting, shader /
	// light complexity: the green / brown screen with the "Good .. Bad" bar). The game uses the F keys for weapons and
	// the squad, so after any F key the normal lit view is restored (also once at start, in case it was left on).
	{
		static const FKey FKeys[12] = { EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12 };
		static bool bViewModeFixed = false;   // (function-local: no header change, Live Coding is enough)
		bool bAnyF = !bViewModeFixed;
		for (const FKey& K : FKeys) bAnyF = bAnyF || PC->WasInputKeyJustPressed(K);
		if (bAnyF)
		{
			bViewModeFixed = true;
			PC->ConsoleCommand(TEXT("viewmode lit"), false);
		}
	}
#endif

	const float TargetAlpha = bRadialMenuOpen ? 1.0f : 0.0f;
	RadialMenuAlpha = FMath::FInterpTo(RadialMenuAlpha, TargetAlpha, RealDt, 18.f);

	if (PC->WasInputKeyJustPressed(EKeys::F1) && bInputOk && !bRadialMenuOpen && RadialClosedT > 0.1f)   // weapons on F1..F3 (1..4 pick the men)
	{
		if (bBinocularsActive) SetBinoculars(false);
		if (CurrentWeaponType != EAAAWeaponType::Rifle)
		{
			SwitchWeapon(EAAAWeaponType::Rifle);
			if (!bArmed) SetArmed(true);
		}
		else
		{
			SetArmed(!bArmed);
		}
	}
	if (PC->WasInputKeyJustPressed(EKeys::F2) && bInputOk && !bRadialMenuOpen && RadialClosedT > 0.1f)   // weapons on F1..F3 (1..4 pick the men)
	{
		if (bBinocularsActive) SetBinoculars(false);
		if (CurrentWeaponType != EAAAWeaponType::LMG) SwitchWeapon(EAAAWeaponType::LMG);
		if (!bArmed) SetArmed(true);
	}
	if (PC->WasInputKeyJustPressed(EKeys::F3) && bInputOk && !bRadialMenuOpen && RadialClosedT > 0.1f)   // weapons on F1..F3 (1..4 pick the men)
	{
		if (bBinocularsActive) SetBinoculars(false);
		if (CurrentWeaponType != EAAAWeaponType::Pistol) SwitchWeapon(EAAAWeaponType::Pistol);
		if (!bArmed) SetArmed(true);
	}
	if (PC->WasInputKeyJustPressed(EKeys::F4) && bInputOk && !bRadialMenuOpen && RadialClosedT > 0.1f)   // Kar98k sniper
	{
		if (bBinocularsActive) SetBinoculars(false);
		if (CurrentWeaponType != EAAAWeaponType::Kar98) SwitchWeapon(EAAAWeaponType::Kar98);
		if (!bArmed) SetArmed(true);
	}
	if (PC->WasInputKeyJustPressed(EKeys::T)) SpawnTargets(C);

	// Binoculars (Jumelle) input: B key toggles
	if (PC->WasInputKeyJustPressed(EKeys::B) && bInputOk && !bRadialMenuOpen)
	{
		ToggleBinoculars();
	}

	// Dynamic zoom controls when binoculars active
	if (bBinocularsActive && bInputOk)
	{
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			// Step up magnification: 2x -> 4x -> 8x -> 16x -> 24x
			if (TargetBinocularsZoom < 3.5f) TargetBinocularsZoom = 4.0f;
			else if (TargetBinocularsZoom < 7.0f) TargetBinocularsZoom = 8.0f;
			else if (TargetBinocularsZoom < 14.0f) TargetBinocularsZoom = 16.0f;
			else TargetBinocularsZoom = 24.0f;
			if (C && DrySound) PlaySound(DrySound.Get(), C->GetActorLocation(), 0.35f, 1.6f);
		}
		else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			// Step down magnification
			if (TargetBinocularsZoom > 18.0f) TargetBinocularsZoom = 16.0f;
			else if (TargetBinocularsZoom > 10.0f) TargetBinocularsZoom = 8.0f;
			else if (TargetBinocularsZoom > 5.0f) TargetBinocularsZoom = 4.0f;
			else TargetBinocularsZoom = 2.0f;
			if (C && DrySound) PlaySound(DrySound.Get(), C->GetActorLocation(), 0.35f, 1.2f);
		}
		else if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
		{
			// Quick cycle zoom
			if (TargetBinocularsZoom < 3.5f) TargetBinocularsZoom = 4.0f;
			else if (TargetBinocularsZoom < 7.0f) TargetBinocularsZoom = 8.0f;
			else if (TargetBinocularsZoom < 14.0f) TargetBinocularsZoom = 16.0f;
			else if (TargetBinocularsZoom < 20.0f) TargetBinocularsZoom = 24.0f;
			else TargetBinocularsZoom = 2.0f;
			if (C && DrySound) PlaySound(DrySound.Get(), C->GetActorLocation(), 0.4f, 1.45f);
		}
	}

	bAiming = !bBinocularsActive && bArmed && bInputOk && !bRadialMenuOpen && PC->IsInputKeyDown(EKeys::RightMouseButton);
	if (!SmartHudWidget) CreateSmartHud();

	// Grenade regeneration
	if (Grenades < MaxGrenades)
	{
		GrenadeRegenTimer += Dt;
		if (GrenadeRegenTimer >= GrenadeRegenDelay)
		{
			GrenadeRegenTimer = 0.f;
			Grenades = FMath::Min(MaxGrenades, Grenades + 1);
		}
	}

	// Grenade input (G key: hold to cook & aim with trajectory arc, release to throw)
	if (PC->WasInputKeyJustPressed(EKeys::G) && bInputOk)
	{
		StartCookingGrenade();
	}
	if (bCookingGrenade)
	{
		if (!PC->IsInputKeyDown(EKeys::G))
		{
			ReleaseGrenade();
		}
		else
		{
			CookTimer += Dt;
			UpdateTrajectory(C, PC);
			if (CookTimer >= 3.2f)
			{
				ReleaseGrenade();
			}
		}
	}
	if (bCookingGrenade && TrajectoryPoints.Num() > 1 && GetWorld())
	{
		for (int32 i = 0; i < TrajectoryPoints.Num() - 1; ++i)
		{
			DrawDebugLine(GetWorld(), TrajectoryPoints[i], TrajectoryPoints[i + 1], FColor(255, 185, 40), false, -1.f, 0, 2.5f);
		}
		if (!PredictedLandingLoc.IsZero())
		{
			DrawDebugCircle(GetWorld(), PredictedLandingLoc + FVector(0.0, 0.0, 3.0), 45.f, 20, FColor(255, 60, 40), false, -1.f, 0, 3.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);
		}
	}
	if (ThrowT >= 0.f)
	{
		ThrowT += Dt;
		if (ThrowT >= 0.80f && !bGrenadeSpawned)   // release point of the throw animation (wind-up, whip)
		{
			SpawnThrownGrenade();
		}
		if (ThrowT >= 1.60f)
		{
			ThrowT = -1.f;
			bGrenadeSpawned = false;
		}
	}

	// Ensure UAAARifleAnimInstance is active on the mesh even when starting unarmed
	if (!Anim && C->GetMesh())
	{
		USkeletalMeshComponent* M = C->GetMesh();
		if (!SavedAnimClass && M->GetAnimClass() != UAAARifleAnimInstance::StaticClass())
		{
			SavedAnimClass = M->GetAnimClass();
		}
		if (M->GetAnimClass() != UAAARifleAnimInstance::StaticClass())
		{
			M->SetAnimInstanceClass(UAAARifleAnimInstance::StaticClass());
		}
		Anim = Cast<UAAARifleAnimInstance>(M->GetAnimInstance());
		if (Anim && !Anim->bAssetsOk)
		{
			M->SetAnimInstanceClass(SavedAnimClass);
			Anim = nullptr;
		}
	}

	// crouch & smart cover (before the speeds: a crouched character moves at MaxWalkSpeedCrouched)
	if (APlayerController* CPC = ControllingPC()) UpdateCrouchCover(C, CPC, Dt, !CPC->IsMoveInputIgnored());

	// Dynamic Walk and Run Speed System (Active both Armed and Unarmed)
	if (UCharacterMovementComponent* CM = C->GetCharacterMovement())
	{
		CM->MaxWalkSpeedCrouched = (bArmed && bAiming) ? 140.f : 175.f;   // crouched: careful steps, slower aimed
		float DesiredSpeed = 230.f; // Default Walk (Unarmed & Armed)
		if (bArmed && bAiming)
		{
			DesiredSpeed = AimWalkSpeed; // ~190.f
		}
		else
		{
			const bool bShiftHeld = bInputOk && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
			const bool bAltHeld = bInputOk && (PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::LeftControl));

			// Caps Lock toggles default mode (Walk <-> Run)
			if (bInputOk && PC->WasInputKeyJustPressed(EKeys::CapsLock))
			{
				bRunByDefault = !bRunByDefault;
			}

			if (bShiftHeld)
			{
				DesiredSpeed = 520.f; // Sprint / Fast Run
			}
			else if (bAltHeld)
			{
				DesiredSpeed = 190.f; // Slow / Stealth Walk
			}
			else if (bRunByDefault)
			{
				DesiredSpeed = 440.f; // Jog / Run
			}
			else
			{
				DesiredSpeed = 230.f; // Realistic Walk
			}
		}

		CM->MaxWalkSpeed = FMath::FInterpTo(CM->MaxWalkSpeed, DesiredSpeed, Dt, 10.f);
	}

	if (bArmed)
	{
		ArmT += Dt;
		if (!bRifleAttached)
		{
			if (Anim && Anim->bCalibrated) { BuildRifle(Anim->LeftDist); AttachRifle(); }
			else if (!Anim && ArmT > 0.1f) { BuildRifle(32.f); AttachRifle(); }
			else if (ArmT > 1.0f) { BuildRifle(32.f); AttachRifle(); }
		}
		else if (Rifle && Anim && Anim->bCalibrated)
		{
			const FTransform Grip = Anim->GripRel;
			const FVector ExtraOffset = (CurrentWeaponType == EAAAWeaponType::Pistol) ? FVector::ZeroVector : GripOffset;
			const FRotator ExtraRot = (CurrentWeaponType == EAAAWeaponType::Pistol) ? FRotator::ZeroRotator : GripRotation;
			// pivot = the grip (rifle frame origin = right palm)
			const FTransform InHand(FRotator(FMath::Clamp(InHandPitch, -4.f, 6.f), FMath::Clamp(InHandYaw, -3.f, 3.f), 0.0), FVector::ZeroVector);
			Rifle->SetRelativeTransform(InHand * FTransform(ExtraRot, ExtraOffset) * Grip);
		}

		// reload, timed by the animation itself: the magazine leaves with the left hand when the hand pulls it,
		// goes back in when the hand returns to the magazine well, then the bolt (time limits as a fallback)
		const float RL = Anim ? Anim->GetReloadLength() : 2.2f;
		if (ReloadT >= 0.f && CurrentWeaponType == EAAAWeaponType::Kar98)
		{
			// Kar98k: the rounds go in one by one (each counts the moment the thumb presses it in); it goes on while
			// walking / running / crouching / jumping (the anim layers it on the movement), and waits while climbing
			if (!ClimbState.bMantling && ClimbState.ClimbWeight < 0.3f) ReloadT += Dt;
			const FVector At = C->GetActorLocation();
			const float LenK = AAAKarReload::Length(KarReloadN);
			if (!bKarBoltOpenSnd && ReloadT >= 0.26f) { bKarBoltOpenSnd = true; PlaySound(BoltSound, At, 0.8f, 0.95f); }
			if (KarLoaded < KarReloadN && ReloadT >= AAAKarReload::PushTime(KarLoaded))
			{
				++KarLoaded;
				if (Ammo < MagazineSize) { ++Ammo; if (!bInfiniteReserve) ReserveAmmo = FMath::Max(0, ReserveAmmo - 1); }
				PlaySound(MagInSound ? MagInSound.Get() : ImpactSound.Get(), At, 0.55f, 1.5f + 0.06f * KarLoaded);
				OnAmmoChanged.Broadcast(Ammo, ReserveAmmo);
			}
			if (!bKarBoltCloseSnd && ReloadT >= AAAKarReload::CloseStart(KarReloadN) + 0.18f) { bKarBoltCloseSnd = true; PlaySound(BoltSound, At, 0.85f, 1.2f); }
			// the round between the fingers while it is carried from the pouch to the action
			if (!KarRound && C->GetMesh() && BrassMat)
			{
				KarRound = AAAShapes::Make(C, true, FVector(0.9, 0.9, 7.6), BrassMat, false, false);
				KarRound->SetupAttachment(C->GetMesh(), TEXT("middle_01_r"));
				KarRound->SetRelativeLocationAndRotation(FVector(0.0, 0.0, 0.0), FRotator(0.0, 0.0, 90.0));
				KarRound->RegisterComponent();
				KarRound->SetVisibility(false);
			}
			bool bShowRound = false;
			if (KarLoaded < KarReloadN)
			{
				const float S = ReloadT - (AAAKarReload::Open + KarLoaded * AAAKarReload::Per);
				bShowRound = S >= 0.12f && S < 0.34f;
			}
			if (KarRound && KarRound->IsVisible() != bShowRound) KarRound->SetVisibility(bShowRound);
			if (ReloadT >= LenK)
			{
				ReloadT = -1.f; ReloadStage = 0;
				if (KarRound) KarRound->SetVisibility(false);
				OnReloadStage.Broadcast(5, FName(TEXT("Complete")));
				ReceiveOnReloadStage(5, FName(TEXT("Complete")));
				OnAmmoChanged.Broadcast(Ammo, ReserveAmmo);
			}
		}
		else if (ReloadT >= 0.f)
		{
			ReloadT += Dt;
			const float F = ReloadT / RL;
			const FVector At = C->GetActorLocation();
			float Hd = 1e9f;
			if (Magazine && Rifle && C->GetMesh()) Hd = (float)FVector::Dist(C->GetMesh()->GetSocketLocation(CurrentWeaponType == EAAAWeaponType::Kar98 ? TEXT("hand_r") : TEXT("hand_l")), (FTransform(MagRel) * Rifle->GetComponentTransform()).GetLocation());
			if (ReloadStage == 0 && ((Hd < 18.f && F > 0.05f) || F > 0.3f))
			{
				ReloadStage = 1;
				OnReloadStage.Broadcast(1, FName(TEXT("HandOnMag")));
				ReceiveOnReloadStage(1, FName(TEXT("HandOnMag")));
			}
			if (ReloadStage == 1 && ((Hd > 26.f && F > 0.12f) || F > 0.42f))
			{
				ReloadStage = 2;
				if (Magazine && C->GetMesh()) Magazine->AttachToComponent(C->GetMesh(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, false), CurrentWeaponType == EAAAWeaponType::Kar98 ? TEXT("hand_r") : TEXT("hand_l"));
				PlaySound(MagOutSound, At, 1.0f, 1.0f);
				OnReloadStage.Broadcast(2, FName(TEXT("MagExtracted")));
				ReceiveOnReloadStage(2, FName(TEXT("MagExtracted")));
			}
			if (ReloadStage == 2 && ((Hd < 16.f && F > 0.4f) || F > 0.74f))
			{
				ReloadStage = 3;
				if (Magazine && Rifle) { Magazine->AttachToComponent(Rifle, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false)); Magazine->SetRelativeTransform(MagRel); }
				PlaySound(MagInSound, At, 1.0f, 1.0f);
				OnReloadStage.Broadcast(3, FName(TEXT("MagInserted")));
				ReceiveOnReloadStage(3, FName(TEXT("MagInserted")));
			}
			if (ReloadStage == 3 && F > 0.84f)
			{
				ReloadStage = 4;
				PlaySound(BoltSound, At, 1.0f, 1.0f);
				OnReloadStage.Broadcast(4, FName(TEXT("BoltChambered")));
				ReceiveOnReloadStage(4, FName(TEXT("BoltChambered")));
			}
			if (ReloadT >= RL)
			{
				ReloadT = -1.f;
				if (ReloadStage == 2 && Magazine && Rifle) { Magazine->AttachToComponent(Rifle, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false)); Magazine->SetRelativeTransform(MagRel); }
				ReloadStage = 0;
				const int32 Need = MagazineSize - Ammo;
				const int32 Take = bInfiniteReserve ? Need : FMath::Min(Need, ReserveAmmo);
				Ammo += Take;
				if (!bInfiniteReserve) ReserveAmmo -= Take;
				OnReloadStage.Broadcast(5, FName(TEXT("Complete")));
				ReceiveOnReloadStage(5, FName(TEXT("Complete")));
				OnAmmoChanged.Broadcast(Ammo, ReserveAmmo);
			}
		}
		if (KarRound && ReloadT < 0.f && KarRound->IsVisible()) KarRound->SetVisibility(false);   // (reload interrupted)
		const bool bCanReload = Ammo < MagazineSize && (bInfiniteReserve || ReserveAmmo > 0) && bRifleAttached && SniperFireT < 0.f;
		if (ReloadT < 0.f && bCanReload && bInputOk && (PC->WasInputKeyJustPressed(EKeys::R) || (Ammo == 0 && SinceShot > 0.3f)))
		{
			ReloadT = 0.f; ReloadStage = 0;
			if (CurrentWeaponType == EAAAWeaponType::Kar98)
			{
				KarReloadN = FMath::Clamp(MagazineSize - Ammo, 0, bInfiniteReserve ? 5 : FMath::Min(5, ReserveAmmo));
				KarLoaded = 0; bKarBoltOpenSnd = false; bKarBoltCloseSnd = false;
			}
			else if (!MagOutSound) PlaySound(ReloadSound, C->GetActorLocation(), 0.8f, 2.2f / FMath::Max(RL, 0.5f));   // the old one-piece sound
		}

		// fire (full auto or semi-auto for pistol/data asset) - not while the weapon is still coming up
		const float EL = Anim ? Anim->GetEquipLength() : 0.f;
		const bool bAuto = CurrentWeaponData ? CurrentWeaponData->bFullAuto : (CurrentWeaponType != EAAAWeaponType::Pistol && CurrentWeaponType != EAAAWeaponType::Kar98);
		const bool bTrigger = !bRadialMenuOpen && RadialClosedT > 0.2f && !bBinocularsActive && bInputOk && bRifleAttached && ArmT > FMath::Max(0.2f, EL * 0.8f) && (bAuto ? PC->IsInputKeyDown(EKeys::LeftMouseButton) : PC->WasInputKeyJustPressed(EKeys::LeftMouseButton));
		FireCD -= Dt;
		if (bTrigger && ReloadT < 0.f)
		{
			if (Ammo > 0 && FireCD <= 0.f && !(CurrentWeaponType == EAAAWeaponType::Kar98 && SniperFireT >= 0.f)) { FireShot(PC); FireCD += 1.f / FMath::Max(FireRate, 1.f); }
			else if (Ammo == 0 && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)) PlaySound(DrySound, C->GetActorLocation(), 0.7f, 1.f);
		}
		if (!bTrigger || FireCD < -0.05f) FireCD = FMath::Max(FireCD, 0.f);
	}

	// recoil: weapon spring, camera recovery, spread bloom
	SinceShot += Dt;
	KickV += (-Kick * 260.f - KickV * 26.f) * Dt;
	Kick = FMath::Max(0.f, Kick + KickV * Dt);

	// Kar98k: the fire + bolt-cycle clip (2.00 s) and the bolt / casing / sound events synced to it
	if (SniperFireT >= 0.f)
	{
		SniperFireT += Dt;
		if (!bBoltSoundPlayed && SniperFireT >= 0.55f)   // the hand pulls the bolt back
		{
			bBoltSoundPlayed = true;
			PlaySound(BoltSound, C->GetActorLocation(), 0.9f, 0.85f);
		}
		if (!bBoltCasingOut && SniperFireT >= 0.78f)     // the bolt is fully back: the spent brass flies out
		{
			bBoltCasingOut = true;
			SpawnCasing(C);
		}
		if (SniperFireT >= 1.45f) SniperFireT = -1.f;   // bolt closed, hand back on the grip
	}
	if (USceneComponent* Kb = KarBolt.Get())
	{
		// handle up (0.37-0.57 s), bolt back 11 cm (0.57-0.80), forward (0.80-1.02), handle down (1.02-1.15)
		const float T = SniperFireT;
		float Roll = 0.f, Back = 0.f;
		if (T >= 0.f)
		{
			const float Up = FMath::SmoothStep(0.37f, 0.57f, T) * (1.f - FMath::SmoothStep(1.02f, 1.15f, T));
			Roll = -70.f * Up;
			Back = -11.f * FMath::SmoothStep(0.57f, 0.80f, T) * (1.f - FMath::SmoothStep(0.80f, 1.02f, T));
		}
		else if (ReloadT >= 0.f && CurrentWeaponType == EAAAWeaponType::Kar98)
		{
			// reloading: the bolt stands open (up and back) while the rounds go in from the top, then closes
			float UpK = 0.f, BackK = 0.f;
			AAAKarReload::Bolt(ReloadT, KarReloadN, UpK, BackK);   // (the right hand works it on the same timeline)
			Roll = -70.f * UpK;
			Back = -11.f * BackK;
		}
		Kb->SetRelativeLocationAndRotation(FVector(1.0 + Back, 0.0, 8.6), FRotator(0.0, 0.0, Roll));
	}

	// Colt 1911 high-speed mechanical blowback, slide lock & hammer animation
	if (CurrentWeaponType == EAAAWeaponType::Pistol)
	{
		if (Ammo == 0 && ReloadT < 0.f)
		{
			// Slide locked back completely on empty chamber
			GPistolSlideAnim = FMath::FInterpTo(GPistolSlideAnim, 1.0f, Dt, 25.f);
			GPistolHammerRot = FMath::FInterpTo(GPistolHammerRot, 1.0f, Dt, 25.f);
		}
		else
		{
			// Smooth spring return from blowback to battery (0.06s)
			GPistolSlideAnim = FMath::FInterpTo(GPistolSlideAnim, 0.0f, Dt, 24.f);
			// Hammer stays cocked ready to fire
			GPistolHammerRot = FMath::FInterpTo(GPistolHammerRot, 1.0f, Dt, 35.f);
		}

		if (GPistolSlideComp.IsValid())
		{
			// Realistic slide travel (data-driven or default 3.8 cm)
			const float Travel = CurrentWeaponData ? CurrentWeaponData->SlideTravelDistance : 3.8f;
			const float SlideX = -GPistolSlideAnim * Travel;
			GPistolSlideComp->SetRelativeLocation(FVector(SlideX, 0.0f, 0.0f));
		}

		if (GPistolHammerComp.IsValid())
		{
			// Hammer pivot and rotation (data-driven or default [-2.6, 0, 6.0], 32 deg)
			const FVector Pivot = CurrentWeaponData ? CurrentWeaponData->HammerPivot : FVector(-2.6f, 0.0f, 6.0f);
			const float MaxAngle = CurrentWeaponData ? CurrentWeaponData->HammerCockedAngle : 32.0f;
			const float PitchRot = (1.0f - GPistolHammerRot) * MaxAngle;
			GPistolHammerComp->SetRelativeLocationAndRotation(Pivot, FRotator(PitchRot, 0.0f, 0.0f));
		}
	}
	for (int32 InHandStep = 0; InHandStep < 2; ++InHandStep)   // in-hand give: stiff (38 rad/s), damped (zeta 0.6) - over in ~0.15 s
	{
		const float InHandDt = Dt * 0.5f;
		InHandPitchV += (-1444.f * InHandPitch - 45.6f * InHandPitchV) * InHandDt; InHandPitch += InHandPitchV * InHandDt;
		InHandYawV   += (-1444.f * InHandYaw - 45.6f * InHandYawV) * InHandDt;     InHandYaw   += InHandYawV * InHandDt;
	}
	if (RecoilAccum > 0.f)
	{
		// between bursts: full recovery; during sustained fire the shooter keeps pulling down (40%), so the
		// view climbs ~1-2 deg and holds there instead of rising shot after shot
		const float PullRate = SinceShot > 0.12f ? 7.f : 2.8f;
		const float Back = FMath::Min(RecoilAccum, FMath::Max(RecoilAccum * PullRate * Dt, (SinceShot > 0.12f ? 1.5f : 0.3f) * Dt));
		RecoilAccum -= Back;
		FRotator R = PC->GetControlRotation();
		R.Pitch -= Back;
		PC->SetControlRotation(R.GetNormalized());
	}
	Spread = FMath::Max(0.f, Spread - Dt * (Spread * 3.5f + 0.6f));
	FlashT -= Dt;
	const bool bFlash = bArmed && FlashT > 0.f;
	if (Flash && Flash->IsVisible() != bFlash) Flash->SetVisibility(bFlash);
	if (FlashLight && (FlashLight->Intensity > 0.f) != bFlash) FlashLight->SetIntensity(bFlash ? 6000.f : 0.f);

	// inputs for the rifle / pistol / unarmed animation
	if (Anim)
	{
		FAAARifleInputs& I = Anim->Inputs;
		const FVector V = C->GetVelocity();
		const float Speed2D = (float)V.Size2D();
		const bool bFallingNow = C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling();

		I.LocalVel = FVector2D(FVector::DotProduct(V, C->GetActorForwardVector()), FVector::DotProduct(V, C->GetActorRightVector()));
		I.AimPitch = FRotator::NormalizeAxis(PC->GetControlRotation().Pitch);
		I.Kick = Kick;
		I.KickSide = KickSide;
		I.ReloadTime = ReloadT;
		I.EquipTime = (bArmed && ArmT < Anim->GetEquipLength()) ? ArmT : -1.f;
		I.ThrowTime = ThrowT;
		I.SniperFireTime = (bArmed && CurrentWeaponType == EAAAWeaponType::Kar98) ? SniperFireT : -1.f;
		I.bKarRifle = (CurrentWeaponType == EAAAWeaponType::Kar98);
		I.ReloadStage = ReloadStage;
		I.KarReloadN = KarReloadN;
		I.MagRelLoc = (Magazine && CurrentWeaponType != EAAAWeaponType::Pistol) ? MagRel.GetLocation() : FVector::ZeroVector;
		I.CookWeight = bCookingGrenade ? 1.f : 0.f;
		I.CrouchWeight = CrouchOut;
		I.LeanAmount = LeanOut;
		I.bFalling = bFallingNow;
		I.bJumpUp = V.Z > 50.0;
		I.VelZ = (float)V.Z;
		I.HitTime = PlayerHitT;
		I.HitIndex = PlayerHitIdx;
		I.HitDir = PlayerHitDir;
		I.HitStrength = PlayerHitStr;
		I.DeathTime = -1.f;
		I.bAiming = bArmed && bAiming;
		I.bLeftHandIK = bArmed;
		I.bIsPistol = (CurrentWeaponType == EAAAWeaponType::Pistol);
		I.bUnarmed = !bArmed;
		I.bBinoculars = bBinocularsActive;
		I.BinocularsWeight = BinocularsBlend;

		// ---- Procedural Ledge / Vehicle Mantling System (Clean Vault onto Cars, Hoods, Walls & Ledges)
		FClimbControllerState& CS = ClimbState;   // per component (a function-local static map crashed after Live Coding)
		CS.Cooldown = FMath::Max(0.f, CS.Cooldown - Dt);
		CS.SinceEnd = CS.bMantling ? 0.f : CS.SinceEnd + Dt;

		UCharacterMovementComponent* CM = C->GetCharacterMovement();
		const float HalfHeight = C->GetSimpleCollisionHalfHeight();
		const FVector ActorLoc = C->GetActorLocation();
		const FVector Fwd = C->GetActorForwardVector();
		const FVector Right = C->GetActorRightVector();
		const FVector Up = FVector::UpVector;
		const float FeetZ = (float)(ActorLoc.Z - HalfHeight);

		FCollisionQueryParams ClimbParams(TEXT("IKClimbSystem"), false, C);
		FCollisionObjectQueryParams AllClimbObjs(FCollisionObjectQueryParams::AllObjects);

		// ---- baked Mixamo clip "Braced Hang To Crouch": its hips / toes mapped onto the real ledge
		namespace Clip = AAAClipBracedHang;
		const float ClipFace = 45.f, ClipTop = 165.f;          // the clip's wall face (forward) and ledge top (up), cm
		const float ClipEnd = (float)(Clip::NumFrames - 1) / Clip::FPS;   // clip time
		const float ClipWallTime = ClipEnd / 0.85f;            // played a little slower: a heavier pull-up
		auto ClipLocal = [&](int32 P, float Time) -> FVector
		{
			const float Ft = FMath::Clamp(Time * Clip::FPS, 0.f, (float)(Clip::NumFrames - 1));
			const int32 F0 = FMath::Min((int32)Ft, Clip::NumFrames - 1), F1 = FMath::Min(F0 + 1, Clip::NumFrames - 1);
			const float Fa = Ft - (float)F0;
			const float* Pa = Clip::Point[F0][P];
			const float* Pb = Clip::Point[F1][P];
			return FVector(FMath::Lerp(Pa[0], Pb[0], Fa), FMath::Lerp(Pa[1], Pb[1], Fa), FMath::Lerp(Pa[2], Pb[2], Fa));
		};
		auto ClipWorld = [&](int32 P, float Time) -> FVector
		{
			const FVector Lc = ClipLocal(P, Time);
			return CS.ClipLip + CS.ClipFw * (double)((Lc.X - ClipFace) * CS.ClipRatio) + CS.ClipRt * (double)(Lc.Y * CS.ClipRatio)
				+ FVector::UpVector * (double)((Lc.Z - ClipTop) * CS.ClipRatio);
		};
		auto ClipCapsule = [&](float Time) -> FVector { return ClipWorld(Clip::P_Hips, Time) - CS.ClipPelvisOff; };

		auto StartMantleTransition = [&](const FVector& TopPoint, const FVector& SurfaceWallNormal)
		{
			FVector HorizWallNorm = FVector(SurfaceWallNormal.X, SurfaceWallNormal.Y, 0.0).GetSafeNormal();
			if (HorizWallNorm.IsNearlyZero()) HorizWallNorm = -Fwd;
			const FVector WallInward = -HorizWallNorm;
			const FVector WallRight = FVector::CrossProduct(Up, WallInward).GetSafeNormal();

			CS.bMantling = true;
			CS.MantleTime = 0.f;
			const float TopH = (float)(TopPoint.Z - FeetZ);
			const bool bTallWall = TopH > 140.f;   // chest high and up: the Mixamo clip (braced hang -> pull up -> crouch on top); lower = quick vault

			// the real front face just under the lip, and how deep the top is (thin wall = climb OVER it)
			FVector Face = FVector(TopPoint.X, TopPoint.Y, TopPoint.Z) - WallInward * 12.0;
			{
				FHitResult HF;
				const FVector FA = FVector(TopPoint.X, TopPoint.Y, TopPoint.Z - 15.0) + HorizWallNorm * 70.0;
				if (GetWorld()->LineTraceSingleByChannel(HF, FA, FA - HorizWallNorm * 160.0, ECC_Visibility, ClimbParams) && !HF.bStartPenetrating)
					Face = FVector(HF.ImpactPoint.X, HF.ImpactPoint.Y, TopPoint.Z);
			}
			float Depth = -1.f;
			for (double Dd = 8.0; Dd <= 130.0; Dd += 8.0)
			{
				FHitResult HD;
				const FVector DA = Face + WallInward * Dd + Up * 15.0;
				if (!GetWorld()->LineTraceSingleByChannel(HD, DA, DA - Up * 45.0, ECC_Visibility, ClimbParams)) { Depth = (float)Dd - 4.f; break; }
			}
			CS.bOverWall = false;
			FVector FarLoc = FVector::ZeroVector, FarGround = FVector::ZeroVector;
			if (Depth > 0.f && Depth <= 80.f)
			{
				// the far side: a few probes beyond the far edge (a car / bench right behind the wall is fine to land on)
				const double Beyond[3] = { 50.0, 75.0, 35.0 };
				for (int32 Bq = 0; Bq < 3 && !CS.bOverWall; ++Bq)
				{
					FHitResult HG;
					const FVector GA = Face + WallInward * (double)(Depth + Beyond[Bq]) + Up * 20.0;
					if (GetWorld()->LineTraceSingleByChannel(HG, GA, GA - Up * 800.0, ECC_Visibility, ClimbParams)
						&& !HG.bStartPenetrating && HG.ImpactNormal.Z > 0.5f && HG.ImpactPoint.Z < TopPoint.Z - 40.0)
					{
						CS.bOverWall = true;
						FarGround = HG.ImpactPoint;
						FarLoc = HG.ImpactPoint + Up * (double)(HalfHeight + 2.f);
					}
				}
			}
			UE_LOG(LogTemp, Log, TEXT("AAA Climb: wall %.0f cm high, %.0f cm deep -> %s%s"), TopH, Depth,
				TopH > 140.f ? TEXT("Mixamo hang + pull-up") : TEXT("vault"), CS.bOverWall ? TEXT(", over to the other side") : TEXT(", onto the top"));

			CS.WallNormal = HorizWallNorm;
			CS.FacePt = Face;
			CS.MantleStartLoc = ActorLoc;
			CS.MantleEndLoc = FVector(TopPoint.X, TopPoint.Y, TopPoint.Z + HalfHeight + 4.0) + WallInward * 22.0;
			// Intermediate "chest over the ledge" position: body pressed against the front edge, hips just above the top surface
			CS.MantleApexLoc = FVector(Face.X, Face.Y, TopPoint.Z + HalfHeight * 0.55) + HorizWallNorm * 2.0;

			// Base front lip of the roof / hood / wall where hands touch (on a thin wall: the middle of its top)
			CS.LedgeEdge = Face + WallInward * (double)(Depth > 0.f ? FMath::Min(10.f, Depth * 0.4f) : 10.f);
			CS.HandAnchorL = CS.LedgeEdge - WallRight * 20.0;
			CS.HandAnchorR = CS.LedgeEdge + WallRight * 20.0;
			CS.HandNormL = Up;
			CS.HandNormR = Up;

			auto RaycastTopSurface = [&](const FVector& BaseXY, FVector& OutPos, FVector& OutNorm)
			{
				FHitResult Hit;
				const FVector Start(BaseXY.X, BaseXY.Y, TopPoint.Z + 40.0);
				const FVector End(BaseXY.X, BaseXY.Y, TopPoint.Z - 40.0);
				if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, ClimbParams))
				{
					if (Hit.bBlockingHit && !Hit.bStartPenetrating && Hit.ImpactNormal.Z > 0.45f)
					{
						OutPos = Hit.ImpactPoint;
						OutNorm = Hit.ImpactNormal;
					}
				}
			};
			RaycastTopSurface(CS.HandAnchorL, CS.HandAnchorL, CS.HandNormL);
			RaycastTopSurface(CS.HandAnchorR, CS.HandAnchorR, CS.HandNormR);

			// Foot targets: start (current soles), foothold on the wall / bumper face, and final spots on top of the surface
			const float LedgeH = FMath::Max(40.f, (float)(TopPoint.Z - FeetZ));
			if (USkeletalMeshComponent* Msh = C->GetMesh())
			{
				CS.FootStartL = Msh->GetSocketLocation(TEXT("foot_l")) - Up * 9.0;
				CS.FootStartR = Msh->GetSocketLocation(TEXT("foot_r")) - Up * 9.0;
			}
			else
			{
				CS.FootStartL = ActorLoc - Up * HalfHeight - WallRight * 12.0;
				CS.FootStartR = ActorLoc - Up * HalfHeight + WallRight * 12.0;
			}

			const double WallFootZ = bTallWall ? FMath::Max((double)FeetZ + 25.0, TopPoint.Z - 150.0) : FeetZ + FMath::Clamp(LedgeH * 0.5f, 25.f, 80.f);
			auto TraceWallFoothold = [&](double Side, FVector& OutPos, FVector& OutNorm)
			{
				OutPos = FVector(CS.LedgeEdge.X, CS.LedgeEdge.Y, WallFootZ) + WallRight * Side;
				OutNorm = HorizWallNorm;
				FHitResult Hit;
				const FVector S = FVector(ActorLoc.X, ActorLoc.Y, WallFootZ) + WallRight * Side - WallInward * 20.0;
				if (GetWorld()->SweepSingleByObjectType(Hit, S, S + WallInward * 140.0, FQuat::Identity, AllClimbObjs, FCollisionShape::MakeSphere(5.f), ClimbParams))
				{
					if (Hit.bBlockingHit && !Hit.bStartPenetrating && FMath::Abs(Hit.ImpactNormal.Z) < 0.7f)
					{
						OutPos = Hit.ImpactPoint;
						OutNorm = Hit.ImpactNormal;
					}
				}
			};
			TraceWallFoothold(-13.0, CS.FootWallL, CS.FootWallNormL);
			TraceWallFoothold( 13.0, CS.FootWallR, CS.FootWallNormR);

			const double TopIn = CS.bOverWall ? (double)(Depth * 0.5f) : 36.0;
			CS.FootTopL = Face + WallInward * TopIn - WallRight * (CS.bOverWall ? 9.0 : 12.0);
			CS.FootTopR = Face + WallInward * TopIn + WallRight * (CS.bOverWall ? 9.0 : 12.0);
			CS.FootTopNormL = Up;
			CS.FootTopNormR = Up;
			RaycastTopSurface(CS.FootTopL, CS.FootTopL, CS.FootTopNormL);
			RaycastTopSurface(CS.FootTopR, CS.FootTopR, CS.FootTopNormR);

			// ---- body timeline: vault (low) / jump-hang-pull (tall) / over the top and drop down (thin wall)
			CS.NumKeys = 0;
			auto AddKey = [&](float T, const FVector& P) { if (CS.NumKeys < 6) { CS.Key[CS.NumKeys] = P; CS.KeyT[CS.NumKeys] = T; ++CS.NumKeys; } };
			const float Base = FMath::GetMappedRangeValueClamped(FVector2D(45.f, 175.f), FVector2D(0.7f, 1.0f), TopH);
			float T = 0.f, ApexT = 0.f, WallFootT = 0.f, TopFootT = 0.f;
			AddKey(0.f, ActorLoc);
			CS.ClipA = CS.ClipB = -1.f;
			if (bTallWall)
			{
				// the clip: map its ledge onto this one, scale it to the mannequin (hips height), find the pelvis in the capsule
				CS.ClipLip = Face;
				CS.ClipFw = WallInward;
				CS.ClipRt = WallRight;
				FVector PelvisCS(0.0, 0.0, 96.0);
				if (USkeletalMeshComponent* Msh = C->GetMesh())
				{
					if (const USkeletalMesh* SkM = Msh->GetSkeletalMeshAsset())
					{
						const FReferenceSkeleton& RS = SkM->GetRefSkeleton();
						int32 Bi = RS.FindBoneIndex(FName(TEXT("pelvis")));
						if (Bi != INDEX_NONE)
						{
							FTransform Acc = FTransform::Identity;
							for (; Bi != INDEX_NONE; Bi = RS.GetParentIndex(Bi)) Acc = Acc * RS.GetRefBonePose()[Bi];
							PelvisCS = Acc.GetLocation();
						}
					}
					const FVector PelvisActor = Msh->GetRelativeTransform().TransformPosition(PelvisCS);
					CS.ClipPelvisOff = FRotator(0.0, WallInward.Rotation().Yaw, 0.0).RotateVector(PelvisActor);
				}
				else CS.ClipPelvisOff = FVector(0.0, 0.0, 6.0);
				CS.ClipRatio = FMath::Clamp((float)PelvisCS.Z / Clip::BindHipsHeight, 0.6f, 1.6f);

				T += 0.25f + FMath::Clamp((TopH - 140.f) / 220.f, 0.f, 1.f) * 0.37f; AddKey(T, ClipCapsule(0.f));   // jump (higher wall: longer) up into the clip's first pose (braced hang)
				WallFootT = T;
				CS.ClipA = T;
				T += ClipWallTime; AddKey(T, ClipCapsule(ClipEnd));   // the clip: hang -> pull up -> crouch on the top
				CS.ClipB = T;
				ApexT = T;
			}
			else
			{
				T += Base * 0.45f; AddKey(T, CS.MantleApexLoc);
				ApexT = T;
				WallFootT = Base * 0.3f;
			}
			if (CS.bOverWall)
			{
				const FVector OverTop = Face + WallInward * (double)(Depth * 0.5f) + Up * (double)(HalfHeight * 0.62f);
				T += 0.34f; AddKey(T, OverTop);                // hips over, legs swing across
				TopFootT = T - 0.04f;
				CS.HandReleaseA = (T - 0.02f); CS.HandReleaseB = (T + 0.16f);   // push off the top with the hands
				T += 0.46f; AddKey(T, FarLoc);                 // drop down the far side
			}
			else
			{
				T += bTallWall ? 0.42f : Base * 0.55f; AddKey(T, CS.MantleEndLoc);
				TopFootT = ApexT + (T - ApexT) * 0.55f;
				CS.HandReleaseA = ApexT + (T - ApexT) * 0.3f; CS.HandReleaseB = ApexT + (T - ApexT) * 0.62f;
			}
			if (CS.ClipA >= 0.f)
			{
				// the clip's hands push off the top at ~60-80% of it; afterwards the feet start from the top
				CS.HandReleaseA = CS.ClipA + (CS.ClipB - CS.ClipA) * 0.62f;
				CS.HandReleaseB = CS.ClipA + (CS.ClipB - CS.ClipA) * 0.82f;
				TopFootT = CS.ClipB;
			}
			CS.MantleDuration = T;
			for (int32 Kk = 0; Kk < CS.NumKeys; ++Kk) CS.KeyT[Kk] /= T;
			CS.HandReleaseA /= T; CS.HandReleaseB /= T;
			if (CS.ClipA >= 0.f) { CS.ClipA /= T; CS.ClipB /= T; }
			CS.ClipFootWL = CS.ClipFootWR = 0.f;

			// feet: start -> wall foothold -> top (-> landing on the far side)
			CS.NumFootKeys = 0;
			auto AddFoot = [&](float Tf, const FVector& FootL, const FVector& FootNL, const FVector& FootR, const FVector& FootNR)
			{
				if (CS.NumFootKeys >= 4) return;
				CS.FootKeyT[CS.NumFootKeys] = Tf / T;
				CS.FootKeyL[CS.NumFootKeys] = FootL; CS.FootKeyNL[CS.NumFootKeys] = FootNL;
				CS.FootKeyR[CS.NumFootKeys] = FootR; CS.FootKeyNR[CS.NumFootKeys] = FootNR;
				++CS.NumFootKeys;
			};
			AddFoot(0.f, CS.FootStartL, Up, CS.FootStartR, Up);
			AddFoot(WallFootT, CS.FootWallL, CS.FootWallNormL, CS.FootWallR, CS.FootWallNormR);
			AddFoot(TopFootT, CS.FootTopL, CS.FootTopNormL, CS.FootTopR, CS.FootTopNormR);
			if (CS.bOverWall)
				AddFoot(T * 0.97f, FarGround - WallRight * 12.0, Up, FarGround + WallRight * 12.0, Up);

			if (CM)
			{
				CM->StopMovementImmediately();
				CM->SetMovementMode(MOVE_Flying);
			}
		};

		// 1) Detect Mantle Trigger when moving towards an obstacle and pressing SpaceBar
		const bool bSpaceTrigger = bInputOk && !bBinocularsActive && !bRadialMenuOpen && CS.Cooldown <= 0.f &&
			(PC->WasInputKeyJustPressed(EKeys::SpaceBar) || (bFallingNow && PC->IsInputKeyDown(EKeys::SpaceBar) && CS.SinceEnd > 1.5f));

		if (!CS.bMantling && bSpaceTrigger)
		{
			// a) the wall in front: probes from knee to above-head height (a single band missed benches, car
			//    hoods and low walls, and missed walls when the character stood a bit too far)
			const float CapRad = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
			const FVector LookDir = (Speed2D > 30.f ? V.GetSafeNormal2D() : Fwd);
			FHitResult FrontHit;
			bool bFoundFront = false;
			const double ProbeAboveFeet[6] = { 35.0, 60.0, 90.0, 120.0, 150.0, 175.0 };
			for (int32 H = 0; H < 6 && !bFoundFront; ++H)
			{
				const FVector Start = FVector(ActorLoc.X, ActorLoc.Y, FeetZ + ProbeAboveFeet[H]);
				for (int32 Dq = 0; Dq < 2 && !bFoundFront; ++Dq)
				{
					const FVector Dir = Dq == 0 ? LookDir : Fwd;
					FHitResult Hit;
					if (GetWorld()->SweepSingleByObjectType(Hit, Start, Start + Dir * (double)(CapRad + 60.f), FQuat::Identity, AllClimbObjs, FCollisionShape::MakeSphere(14.f), ClimbParams)
						&& Hit.bBlockingHit && !Hit.bStartPenetrating && FMath::Abs(Hit.ImpactNormal.Z) < 0.55f)
					{
						FrontHit = Hit;
						bFoundFront = true;
					}
				}
			}

			if (bFoundFront)
			{
				FVector HorizNorm = FVector(FrontHit.ImpactNormal.X, FrontHit.ImpactNormal.Y, 0.0).GetSafeNormal();
				if (HorizNorm.IsNearlyZero()) HorizNorm = -Fwd;
				const FVector WallInward = -HorizNorm;

				// moving toward the surface, or standing and (roughly) facing it
				const bool bToward = Speed2D > 30.f ? FVector::DotProduct(V.GetSafeNormal2D(), WallInward) > 0.2 : FVector::DotProduct(Fwd, WallInward) > 0.5;
				if (bToward)
				{
					// b) where the face ends: march up the wall with level traces until one passes over it (a sweep down
					//    from 3.7 m hit tree crowns, awnings and balconies above the wall, so it failed in some places)
					const FVector FaceP = FrontHit.ImpactPoint;
					const FVector Side = FVector(ActorLoc.X, ActorLoc.Y, 0.0);
					const double FaceDist = FMath::Max(0.0, FVector::DotProduct(FVector(FaceP.X, FaceP.Y, 0.0) - Side, WallInward));
					double OverZ = -1.0;
					for (double Z = FaceP.Z + 10.0; Z <= FeetZ + 380.0; Z += 10.0)
					{
						FHitResult HL;
						const FVector L0 = FVector(ActorLoc.X, ActorLoc.Y, Z);
						if (!GetWorld()->LineTraceSingleByChannel(HL, L0, L0 + WallInward * (FaceDist + 30.0), ECC_Visibility, ClimbParams)) { OverZ = Z; break; }
					}
					FHitResult TopHit;
					bool bTop = false;
					if (OverZ > 0.0)
					{
						// c) the exact top just below that height, a few cm in (thin walls: 8 cm in is still on them)
						const double TopIns[3] = { 8.0, 16.0, 24.0 };
						for (int32 Ti = 0; Ti < 3 && !bTop; ++Ti)
						{
							const FVector Px = FVector(FaceP.X, FaceP.Y, 0.0) + WallInward * TopIns[Ti];
							bTop = GetWorld()->SweepSingleByObjectType(TopHit, FVector(Px.X, Px.Y, OverZ + 8.0), FVector(Px.X, Px.Y, OverZ - 30.0), FQuat::Identity, AllClimbObjs, FCollisionShape::MakeSphere(5.f), ClimbParams)
								&& !TopHit.bStartPenetrating && TopHit.ImpactNormal.Z > 0.58f;
						}
					}
					const float LedgeHeight = bTop ? (float)(TopHit.ImpactPoint.Z - FeetZ) : -1.f;
					// d) room above the top to crouch onto it (80 cm; trees / awnings higher up are fine)
					bool bRoom = false;
					if (bTop && LedgeHeight >= 45.f && LedgeHeight <= 360.f)
					{
						FHitResult CeilHit;
						const FVector Base = TopHit.ImpactPoint + WallInward * 10.0;
						bRoom = !GetWorld()->SweepSingleByObjectType(CeilHit, Base + Up * 25.0, Base + Up * 80.0, FQuat::Identity, AllClimbObjs, FCollisionShape::MakeSphere(18.f), ClimbParams);
					}
					if (bTop && bRoom) StartMantleTransition(TopHit.ImpactPoint, HorizNorm);
					else UE_LOG(LogTemp, Log, TEXT("AAA Climb: no climb (wall %.0f cm away, top %s, height %.0f cm, room %d)"), FaceDist, bTop ? TEXT("found") : TEXT("not found"), LedgeHeight, (int32)bRoom);
				}
			}
		}

		// 2) Update Active Ledge / Car / Bus Mantling (Smooth athletic vault in 0.65s)
		if (CS.bMantling)
		{
			CS.MantleTime += Dt;
			const float RawAlpha = FMath::Clamp(CS.MantleTime / FMath::Max(CS.MantleDuration, 0.2f), 0.f, 1.f);
			const float SmoothAlpha = FMath::SmoothStep(0.f, 1.f, RawAlpha);

			// body along the timeline keys (smooth between keys; the drop down the far side falls, it doesn't glide)
			FVector DesiredActorLoc = CS.NumKeys > 0 ? CS.Key[CS.NumKeys - 1] : CS.MantleEndLoc;
			for (int32 Kk = 1; Kk < CS.NumKeys; ++Kk)
			{
				if (RawAlpha > CS.KeyT[Kk]) continue;
				const float Seg = FMath::Clamp((RawAlpha - CS.KeyT[Kk - 1]) / FMath::Max(CS.KeyT[Kk] - CS.KeyT[Kk - 1], 0.001f), 0.f, 1.f);
				const bool bDrop = CS.bOverWall && Kk == CS.NumKeys - 1;
				const float u = FMath::SmoothStep(0.f, 1.f, Seg);
				DesiredActorLoc = FMath::Lerp(CS.Key[Kk - 1], CS.Key[Kk], (double)u);
				if (bDrop)
				{
					const FVector Ka = CS.Key[Kk - 1], Kb = CS.Key[Kk];
					const double Hop = 14.0 * FMath::Sin(FMath::Min(Seg / 0.35f, 1.f) * UE_PI * 0.5f) * (1.0 - Seg);   // small hop off the top
					DesiredActorLoc = FVector(FMath::Lerp(Ka.X, Kb.X, (double)u), FMath::Lerp(Ka.Y, Kb.Y, (double)u), FMath::Lerp(Ka.Z, Kb.Z, (double)(Seg * Seg)) + Hop);
				}
				break;
			}

			// the clip: before it = jump into its first pose, during it = its hips move the capsule, after it = blend out
			float ClipTime = -1.f, ClipW = 0.f;
			const bool bClipOn = CS.ClipA >= 0.f;
			const bool bInClip = bClipOn && RawAlpha >= CS.ClipA && RawAlpha <= CS.ClipB;
			if (bClipOn)
			{
				if (RawAlpha < CS.ClipA) { ClipTime = 0.f; ClipW = FMath::SmoothStep(0.f, CS.ClipA * 0.8f, RawAlpha); }
				else if (bInClip)
				{
					ClipTime = (RawAlpha - CS.ClipA) / FMath::Max(CS.ClipB - CS.ClipA, 0.001f) * ClipEnd;
					ClipW = 1.f;
					DesiredActorLoc = ClipCapsule(ClipTime);
				}
				else { ClipTime = ClipEnd; ClipW = 1.f - FMath::SmoothStep(CS.ClipB, CS.ClipB + (1.f - CS.ClipB) * 0.7f, RawAlpha); }
			}
			I.ClimbClipTime = ClipTime;
			I.ClimbClipWeight = ClipW;

			// never through the wall: while the hips are below the top the capsule keeps ~80% of its radius in front of
			// the wall face (chest / knees stay out of it); only once the hips are over the top may the body move in
			{
				const float CapRad = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
				const double TopZ = CS.FacePt.Z;
				const double Over = FMath::Clamp((DesiredActorLoc.Z - (TopZ - 10.0)) / 40.0, 0.0, 1.0);
				if (Over < 1.0)
				{
					const double MinGap = (double)CapRad * 0.8 * (1.0 - Over);
					const double Gap = FVector::DotProduct(DesiredActorLoc - CS.FacePt, CS.WallNormal);
					if (Gap < MinGap) DesiredActorLoc += CS.WallNormal * (MinGap - Gap);
				}
			}
			C->SetActorLocation(DesiredActorLoc, false, nullptr, ETeleportType::TeleportPhysics);
			if (CM) CM->Velocity = FVector::ZeroVector;

			const FRotator TargetYawRot = (-CS.WallNormal).Rotation();
			FRotator CurRot = C->GetActorRotation();
			CurRot.Yaw = FMath::FixedTurn(CurRot.Yaw, TargetYawRot.Yaw, 360.f * Dt);
			C->SetActorRotation(CurRot);

			// Hands: reach the ledge lip, stay planted while the body is within arm's reach, release once the torso is over the surface
			float HandWeightL = 0.f, HandWeightR = 0.f;
			if (USkeletalMeshComponent* Msh = C->GetMesh())
			{
				const float DistL = (float)FVector::Dist(Msh->GetSocketLocation(TEXT("upperarm_l")), CS.HandAnchorL);
				const float DistR = (float)FVector::Dist(Msh->GetSocketLocation(TEXT("upperarm_r")), CS.HandAnchorR);
				HandWeightL = 1.0f - FMath::SmoothStep(52.f, 78.f, DistL);
				HandWeightR = 1.0f - FMath::SmoothStep(52.f, 78.f, DistR);
			}
			const float HandRamp = FMath::SmoothStep(0.f, 0.14f, RawAlpha) * (1.0f - FMath::SmoothStep(CS.HandReleaseA, FMath::Max(CS.HandReleaseB, CS.HandReleaseA + 0.02f), RawAlpha));
			HandWeightL *= HandRamp;
			HandWeightR *= HandRamp;

			I.bClimbing = true;
			I.bMantling = true;
			I.MantleProgress = SmoothAlpha;
			I.ClimbSurfaceNorm = CS.WallNormal;
			I.ClimbHandPosL = CS.HandAnchorL;
			I.ClimbHandPosR = CS.HandAnchorR;
			I.ClimbHandNormL = CS.HandNormL;
			I.ClimbHandNormR = CS.HandNormR;
			I.ClimbHandWeightL = HandWeightL;
			I.ClimbHandWeightR = HandWeightR;

			// Feet: plant on the wall / bumper face, then step up onto the top surface one after the other (left leads)
			auto StepFoot = [&](float T1A, float T1B, float T2A, float T2B, const FVector& Start, const FVector& Wall, const FVector& Top,
				const FVector& WallN, const FVector& TopN, FVector& OutPos, FVector& OutNorm)
			{
				const float U1 = FMath::SmoothStep(T1A, T1B, RawAlpha);
				const float U2 = FMath::SmoothStep(T2A, T2B, RawAlpha);
				if (RawAlpha < T2A)
				{
					OutPos = FMath::Lerp(Start, Wall, U1) + Up * (FMath::Sin(U1 * UE_PI) * 6.0);
					OutNorm = FMath::Lerp(FVector(Up), WallN, U1).GetSafeNormal();
				}
				else
				{
					OutPos = FMath::Lerp(Wall, Top, U2) + Up * (FMath::Sin(U2 * UE_PI) * 12.0);
					OutNorm = FMath::Lerp(WallN, TopN, U2).GetSafeNormal();
				}
			};
			(void)StepFoot;
			// each foot steps key to key (the right one a beat after the left); lifted in an arc between holds
			auto KeyFoot = [&](bool bLeft, FVector& OutPos, FVector& OutNorm)
			{
				const float Lag = bLeft ? 0.f : 0.06f;
				const FVector* P = bLeft ? CS.FootKeyL : CS.FootKeyR;
				const FVector* Nn = bLeft ? CS.FootKeyNL : CS.FootKeyNR;
				OutPos = P[FMath::Max(CS.NumFootKeys - 1, 0)]; OutNorm = Nn[FMath::Max(CS.NumFootKeys - 1, 0)];
				for (int32 Fk = 1; Fk < CS.NumFootKeys; ++Fk)
				{
					const float Ta = CS.FootKeyT[Fk - 1] + (Fk > 1 ? Lag : 0.f), Tb = FMath::Min(CS.FootKeyT[Fk] + Lag, 1.f);
					if (RawAlpha > Tb) continue;
					// a foot stays planted, then moves in the last ~60% of the gap
					const float Mv0 = FMath::Lerp(Ta, Tb, 0.4f);
					const float Uf = FMath::SmoothStep(Mv0, Tb, RawAlpha);
					OutPos = FMath::Lerp(P[Fk - 1], P[Fk], (double)Uf) + Up * (double)(FMath::Sin(Uf * UE_PI) * (Fk == 1 ? 6.f : 12.f));
					OutNorm = FMath::Lerp(Nn[Fk - 1], Nn[Fk], (double)Uf).GetSafeNormal();
					break;
				}
			};
			KeyFoot(true, I.ClimbFootPosL, I.ClimbFootNormL);
			KeyFoot(false, I.ClimbFootPosR, I.ClimbFootNormR);
			I.ClimbFootWeightL = 1.0f;
			I.ClimbFootWeightR = 1.0f;
			if (bClipOn && RawAlpha <= CS.ClipB)
			{
				// the clip moves the legs; IK only pins a foot while the clip has it on the wall or on the top
				auto ClipFoot = [&](bool bLeft, FVector& OutPos, FVector& OutNorm, float& InOutW)
				{
					const int32 Pt = bLeft ? Clip::P_LeftToeBase : Clip::P_RightToeBase;
					const float Tc = FMath::Max(ClipTime, 0.f);
					const FVector Lc = ClipLocal(Pt, Tc);
					const bool bOnWall = FMath::Abs(Lc.X - ClipFace) < 9.f && Lc.Z < ClipTop - 8.f;
					const bool bOnTop = Lc.Z > ClipTop - 14.f && Lc.X > ClipFace + 4.f;
					FVector Wp = ClipWorld(Pt, Tc);
					if (bOnWall) { Wp -= CS.ClipFw * FVector::DotProduct(Wp - CS.ClipLip, CS.ClipFw); OutNorm = -CS.ClipFw; }
					else if (bOnTop) { Wp.Z = CS.ClipLip.Z; Wp -= CS.ClipFw * 8.0; OutNorm = FVector::UpVector; }
					else OutNorm = FVector::UpVector;
					OutPos = Wp;
					InOutW = FMath::FInterpTo(InOutW, (bOnWall || bOnTop) ? 1.f : 0.f, Dt, 12.f);
				};
				ClipFoot(true, I.ClimbFootPosL, I.ClimbFootNormL, CS.ClipFootWL);
				ClipFoot(false, I.ClimbFootPosR, I.ClimbFootNormR, CS.ClipFootWR);
				const float Pre = RawAlpha < CS.ClipA ? FMath::SmoothStep(CS.ClipA * 0.5f, CS.ClipA, RawAlpha) : 1.f;   // feet find the wall at the end of the jump
				I.ClimbFootWeightL = CS.ClipFootWL * Pre;
				I.ClimbFootWeightR = CS.ClipFootWR * Pre;
			}

			// Global IK weight stays full during the move and fades once the character stands on top
			CS.ClimbWeight = FMath::FInterpTo(CS.ClimbWeight, 1.0f - FMath::SmoothStep(0.88f, 1.0f, RawAlpha), Dt, 20.f);

			if (RawAlpha >= 1.0f)
			{
				CS.bMantling = false;
				CS.ClimbWeight = 0.f;
				CS.Cooldown = 0.6f;
				I.bClimbing = false;
				I.bMantling = false;
				I.ClimbClipTime = -1.f;
				I.ClimbClipWeight = 0.f;
				if (CM)
				{
					CM->SetMovementMode(MOVE_Walking);
					CM->Velocity = (-CS.WallNormal) * (CS.bOverWall ? 70.0 : 120.0); // small natural forward momentum (on top / away from the wall)
				}
			}
		}
		else
		{
			CS.ClimbWeight = 0.f;
			I.bClimbing = false;
			I.bMantling = false;
			I.ClimbClipTime = -1.f;
			I.ClimbClipWeight = 0.f;
		}
		I.ClimbWeight = CS.ClimbWeight;

		// ---- Adaptive Ground / Slope / Step Foot & Pelvis IK Raycasting (Active only when feet are on different heights/slopes, zero squat on flat surfaces)
		if (!bFallingNow && CS.ClimbWeight < 0.1f && C->GetMesh())
		{
			const FVector FootLocL = C->GetMesh()->GetSocketLocation(TEXT("foot_l"));
			const FVector FootLocR = C->GetMesh()->GetSocketLocation(TEXT("foot_r"));

			auto TraceGroundFoot = [&](const FVector& FootXY, float& OutHitZ, FVector& OutNorm) -> bool
			{
				FHitResult Hit;
				const FVector Start(FootXY.X, FootXY.Y, FeetZ + 42.0);
				const FVector End(FootXY.X, FootXY.Y, FeetZ - 38.0);
				if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, ClimbParams))
				{
					if (Hit.bBlockingHit && !Hit.bStartPenetrating && Hit.ImpactNormal.Z > 0.55f)
					{
						OutHitZ = (float)Hit.ImpactPoint.Z;
						OutNorm = Hit.ImpactNormal;
						return true;
					}
				}
				OutHitZ = FeetZ;
				OutNorm = Up;
				return false;
			};

			float HitZL = FeetZ, HitZR = FeetZ;
			FVector RawNormL = Up, RawNormR = Up;
			const bool bHitL = TraceGroundFoot(FootLocL, HitZL, RawNormL);
			const bool bHitR = TraceGroundFoot(FootLocR, HitZR, RawNormR);
			const float StepDiff = FMath::Abs(HitZL - HitZR);

			if (bHitL && bHitR && StepDiff > 2.2f && StepDiff < 32.0f)
			{
				// Measure relative to the higher foot (which supports the capsule) so neither knee bends on flat ground
				const float HighZ = FMath::Max(HitZL, HitZR);
				const float RawOffL = FMath::Clamp(HitZL - HighZ, -26.f, 0.f);
				const float RawOffR = FMath::Clamp(HitZR - HighZ, -26.f, 0.f);
				const float TargetPelvisZ = FMath::Min(RawOffL, RawOffR) * 0.55f;

				I.GroundOffsetL = FMath::FInterpTo(I.GroundOffsetL, RawOffL - TargetPelvisZ, Dt, 16.f);
				I.GroundOffsetR = FMath::FInterpTo(I.GroundOffsetR, RawOffR - TargetPelvisZ, Dt, 16.f);
				I.GroundPelvisOffsetZ = FMath::FInterpTo(I.GroundPelvisOffsetZ, TargetPelvisZ, Dt, 14.f);
				I.GroundNormL = FMath::VInterpTo(I.GroundNormL, RawNormL, Dt, 16.f).GetSafeNormal();
				I.GroundNormR = FMath::VInterpTo(I.GroundNormR, RawNormR, Dt, 16.f).GetSafeNormal();
				I.GroundFootWeight = FMath::FInterpTo(I.GroundFootWeight, 1.0f, Dt, 10.f);
			}
			else
			{
				I.GroundOffsetL = FMath::FInterpTo(I.GroundOffsetL, 0.f, Dt, 14.f);
				I.GroundOffsetR = FMath::FInterpTo(I.GroundOffsetR, 0.f, Dt, 14.f);
				I.GroundPelvisOffsetZ = FMath::FInterpTo(I.GroundPelvisOffsetZ, 0.f, Dt, 14.f);
				I.GroundFootWeight = FMath::FInterpTo(I.GroundFootWeight, 0.0f, Dt, 12.f);
			}
		}
		else
		{
			I.GroundFootWeight = FMath::FInterpTo(I.GroundFootWeight, 0.0f, Dt, 14.f);
		}

		// Unarmed Contextual Environment Hand-Touch Sensor (Walls, Buildings, Cars/Buses, Trees - Lateral + Forward)
		float TargetTouchL = 0.f;
		float TargetTouchR = 0.f;
		FVector RawHitPosL = I.TouchWorldPosL;
		FVector RawHitNormL = I.TouchWorldNormL;
		FVector RawHitPosR = I.TouchWorldPosR;
		FVector RawHitNormR = I.TouchWorldNormR;

		// Active when unarmed, grounded, not climbing/mantling, not using binoculars, and walking / moving near surfaces (or standing very close)
		if (!bArmed && !bBinocularsActive && !bFallingNow && CS.ClimbWeight < 0.1f && Speed2D < 460.f)
		{
			FCollisionQueryParams TouchParams(TEXT("UnarmedHandTouch"), false, C);
			FCollisionObjectQueryParams AllObjs(FCollisionObjectQueryParams::AllObjects);

			auto ProbeSideSurface = [&](float SideSign, FVector& OutPos, FVector& OutNorm, float& OutDist2D) -> bool
			{
				const FVector SideDir = Right * (double)SideSign;

				// 3-tier forward-lateral sphere sweep (strictly in front of and beside the character, never behind the hip line):
				const FVector Starts[3] = {
					ActorLoc + Up * 24.0 + Fwd * 18.0 + SideDir * 16.0,
					ActorLoc + Up * 18.0 + Fwd * 10.0 + SideDir * 16.0,
					ActorLoc + Up * 10.0 + Fwd * 14.0 + SideDir * 16.0
				};
				const FVector Ends[3] = {
					ActorLoc + Up * 20.0 + Fwd * 28.0 + SideDir * 70.0,
					ActorLoc + Up * 16.0 + Fwd * 18.0 + SideDir * 70.0,
					ActorLoc + Up * 8.0  + Fwd * 22.0 + SideDir * 68.0
				};
				const float Radii[3] = { 12.f, 13.f, 12.f };

				bool bFound = false;
				float BestScore = 1e9f;

				for (int32 P = 0; P < 3; ++P)
				{
					FHitResult Hit;
					if (GetWorld()->SweepSingleByObjectType(Hit, Starts[P], Ends[P], FQuat::Identity, AllObjs, FCollisionShape::MakeSphere(Radii[P]), TouchParams))
					{
						if (!Hit.bBlockingHit || Hit.bStartPenetrating) continue;
						// Reject flat ground, car roofs, and sloped windshields; only touch vertical/near-vertical side surfaces
						if (FMath::Abs(Hit.ImpactNormal.Z) > 0.42f) continue;
						// Ensure surface is strictly in front of the character's hips
						if (FVector::DotProduct(Hit.ImpactPoint - ActorLoc, Fwd) < 4.0) continue;
						// Ensure surface normal faces back toward character laterally
						if (FVector::DotProduct(Hit.ImpactNormal, SideDir) > 0.25) continue;

						const float Dist2D = (float)FVector::Dist2D(Hit.ImpactPoint, ActorLoc);
						if (Dist2D < 26.f || Dist2D > 58.f) continue;

						const float Score = Dist2D + (P == 2 ? 6.f : 0.f);
						if (Score < BestScore)
						{
							BestScore = Score;
							OutDist2D = Dist2D;
							// Keep hand height in a natural ergonomic band (chest/upper-waist)
							OutPos = Hit.ImpactPoint;
							OutPos.Z = FMath::Clamp(OutPos.Z, ActorLoc.Z + 8.0, ActorLoc.Z + 34.0);
							OutNorm = (Hit.ImpactNormal - Up * FVector::DotProduct(Hit.ImpactNormal, Up)).GetSafeNormal();
							if (OutNorm.IsNearlyZero()) OutNorm = -SideDir;
							bFound = true;
						}
					}
				}
				return bFound;
			};

			// Walk factor: strongest while walking (15..320 cm/s), subtle lingering touch if standing very close to wall/car/tree
			const float WalkFactor = (Speed2D > 12.f)
				? FMath::GetMappedRangeValueClamped(FVector2D(12.f, 45.f), FVector2D(0.55f, 1.0f), Speed2D) * FMath::GetMappedRangeValueClamped(FVector2D(340.f, 450.f), FVector2D(1.0f, 0.0f), Speed2D)
				: 0.65f;

			float DistL = 1e9f, DistR = 1e9f;
			if (ProbeSideSurface(-1.f, RawHitPosL, RawHitNormL, DistL))
			{
				const float ProxL = 1.0f - FMath::Clamp((DistL - 46.f) / 14.f, 0.f, 1.f);
				TargetTouchL = FMath::Clamp(ProxL * WalkFactor, 0.f, 0.95f);
			}
			if (ProbeSideSurface(1.f, RawHitPosR, RawHitNormR, DistR))
			{
				const float ProxR = 1.0f - FMath::Clamp((DistR - 46.f) / 14.f, 0.f, 1.f);
				TargetTouchR = FMath::Clamp(ProxR * WalkFactor, 0.f, 0.95f);
			}

			// If both sides detect surfaces, prioritize the closer side so posture stays natural
			if (TargetTouchL > 0.1f && TargetTouchR > 0.1f)
			{
				if (DistL <= DistR) TargetTouchR *= 0.45f;
				else TargetTouchL *= 0.45f;
			}
		}

		// Snap position immediately on new contact so hand never sweeps from an old wall 10m away
		if (I.TouchWeightL < 0.02f && TargetTouchL > 0.02f)
		{
			I.TouchWorldPosL = RawHitPosL;
			I.TouchWorldNormL = RawHitNormL;
		}
		else if (TargetTouchL > 0.02f)
		{
			I.TouchWorldPosL = FMath::VInterpTo(I.TouchWorldPosL, RawHitPosL, Dt, 20.f);
			I.TouchWorldNormL = FMath::VInterpTo(I.TouchWorldNormL, RawHitNormL, Dt, 14.f).GetSafeNormal();
		}

		if (I.TouchWeightR < 0.02f && TargetTouchR > 0.02f)
		{
			I.TouchWorldPosR = RawHitPosR;
			I.TouchWorldNormR = RawHitNormR;
		}
		else if (TargetTouchR > 0.02f)
		{
			I.TouchWorldPosR = FMath::VInterpTo(I.TouchWorldPosR, RawHitPosR, Dt, 20.f);
			I.TouchWorldNormR = FMath::VInterpTo(I.TouchWorldNormR, RawHitNormR, Dt, 14.f).GetSafeNormal();
		}

		I.TouchWeightL = FMath::FInterpTo(I.TouchWeightL, TargetTouchL, Dt, TargetTouchL > I.TouchWeightL ? 7.0f : 5.5f);
		I.TouchWeightR = FMath::FInterpTo(I.TouchWeightR, TargetTouchR, Dt, TargetTouchR > I.TouchWeightR ? 7.0f : 5.5f);
	}

	// Dynamic rangefinder raycast when using binoculars
	if (bBinocularsActive && Camera.IsValid())
	{
		FHitResult BinoHit;
		const FVector CamLoc = Camera->GetComponentLocation();
		const FVector CamFwd = Camera->GetForwardVector();
		const FVector TraceEnd = CamLoc + CamFwd * 250000.0; // 2.5 km
		FCollisionQueryParams BinoParams(TEXT("Rangefinder"), false, C);
		if (GetWorld()->LineTraceSingleByChannel(BinoHit, CamLoc, TraceEnd, ECC_Visibility, BinoParams))
		{
			RangefinderDist = (float)(BinoHit.Distance * 0.01); // cm to meters
		}
		else
		{
			RangefinderDist = -1.f;
		}
	}

	UpdateCamera(Dt, bArmed);
	if (bPoliceShoot) UpdatePolice(Dt, C);
}

void UAAAWeaponComponent::UpdateCamera(float Dt, bool bOn)
{
	ACharacter* C = Char();
	if (!C) return;
	if (!Boom.IsValid())
	{
		Boom = C->FindComponentByClass<USpringArmComponent>();
		if (Boom.IsValid()) Boom->ProbeSize = 8.f;
	}
	if (!Camera.IsValid()) { Camera = C->FindComponentByClass<UCameraComponent>(); bCamBaseSaved = false; }
	UpdateCameraShake(Dt);

	if (!bPostProcessInitialized)
	{
		SetupAaaPostProcess();
	}
	UpdateAaaPostProcess(Dt);

	// 3D Binaural Audio Listener Override: position at 75% Character Head + 25% Camera with Camera View Orientation
	// so own weapon/footsteps stay centered while external 3D gunfire, sirens & bullet whiz-bys pan accurately!
	if (APlayerController* PC = ControllingPC())
	{
		if (C->IsHidden())
		{
			PC->ClearAudioListenerOverride();
		}
		else if (Camera.IsValid())
		{
			const FVector HeadLoc = C->GetMesh() ? C->GetMesh()->GetSocketLocation(TEXT("head")) : (C->GetActorLocation() + FVector(0.0, 0.0, 65.0));
			const FVector CamLoc = Camera->GetComponentLocation();
			const FRotator CamRot = Camera->GetComponentRotation();
			const FVector ListenerLoc = FMath::Lerp(HeadLoc, CamLoc, bBinocularsActive ? 1.0 : 0.25);
			PC->SetAudioListenerOverride(nullptr, ListenerLoc, CamRot);
		}
	}

	if (!Boom.IsValid()) return;

	// climbing: lift the camera pivot so the boom clears the wall top (it hit the top edge and pulled the camera
	// into the body); only the change is applied, the armed / aiming code below sets the offset absolutely and adds it
	Boom->TargetOffset.Z -= ClimbCamLift;
	ClimbCamLift = FMath::FInterpTo(ClimbCamLift, (ClimbState.bMantling || ClimbState.SinceEnd < 1.2f) ? 45.f : 0.f, Dt, ClimbState.bMantling ? 6.f : 2.5f);
	if (ClimbCamLift < 0.05f) ClimbCamLift = 0.f;
	Boom->TargetOffset.Z += ClimbCamLift;
	// crouch: the capsule shrinks and the boom pivot jumps down with it - cancel the jump, then let it ease down;
	// lean: the camera slides out with the body
	Boom->TargetOffset.Z -= CrouchCamZ;
	Boom->SocketOffset.Y -= CrouchCamY;
	if (const UCapsuleComponent* Cap = C->GetCapsuleComponent())
	{
		const float HalfH = Cap->GetScaledCapsuleHalfHeight();
		if (LastCapsuleHalfH > 0.f && FMath::Abs(HalfH - LastCapsuleHalfH) > 0.5f) CrouchCamZ += LastCapsuleHalfH - HalfH;
		LastCapsuleHalfH = HalfH;
	}
	CrouchCamZ = FMath::FInterpTo(CrouchCamZ, 0.f, Dt, 6.f);
	CrouchCamY = FMath::FInterpTo(CrouchCamY, LeanOut * 34.f, Dt, 6.f);
	Boom->TargetOffset.Z += CrouchCamZ;
	Boom->SocketOffset.Y += CrouchCamY;
	// the camera still inside / right against the body (tight spot against a wall): hide the body from the own view
	if (Camera.IsValid() && C->GetMesh())
	{
		const FVector Head = C->GetMesh()->GetSocketLocation(TEXT("head"));
		const FVector Pelvis = C->GetMesh()->GetSocketLocation(TEXT("pelvis"));
		const float CamD = (float)FMath::PointDistToSegment(Camera->GetComponentLocation(), Pelvis, Head);
		const bool bHide = CamD < 38.f;
		if (bHide != bBodyHiddenForCam) { bBodyHiddenForCam = bHide; C->GetMesh()->SetOwnerNoSee(bHide); }
	}

	if (C->GetMesh() && C->GetMesh()->GetCollisionResponseToChannel(ECC_Camera) != ECR_Ignore)
		C->GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	if (C->GetCapsuleComponent() && C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Camera) != ECR_Ignore)
		C->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	if ((bOn || bBinocularsActive) && !bCamSaved)
	{
		SavedArm = Boom->TargetArmLength;
		SavedSocket = Boom->SocketOffset - FVector(0.0, CrouchCamY, 0.0);
		SavedTargetOffset = Boom->TargetOffset - FVector(0.0, 0.0, ClimbCamLift + CrouchCamZ);
		SavedFov = Camera.IsValid() ? Camera->FieldOfView : 90.f;
		bCamSaved = true;
	}
	CamBlend = FMath::FInterpTo(CamBlend, bOn ? 1.f : 0.f, Dt, 7.f);
	AimBlend = FMath::FInterpTo(AimBlend, bOn && bAiming ? 1.f : 0.f, Dt, 12.f);
	BinocularsBlend = FMath::FInterpTo(BinocularsBlend, bBinocularsActive ? 1.f : 0.f, Dt, 8.f);
	BinocularsZoom = FMath::FInterpTo(BinocularsZoom, TargetBinocularsZoom, Dt, 10.f);
	if (!bCamSaved) return;

	const float CurAimArm = (CurrentWeaponType == EAAAWeaponType::Pistol) ? 145.f : ((CurrentWeaponType == EAAAWeaponType::LMG) ? 195.f : AimArmLength);
	const float CurAimFOV = (CurrentWeaponType == EAAAWeaponType::Pistol) ? 58.f : ((CurrentWeaponType == EAAAWeaponType::LMG) ? 68.f : (CurrentWeaponType == EAAAWeaponType::Kar98 ? 28.f : AimFOV));   // Kar98k: 8x scope zoom
	const FVector CurAimSocket = (CurrentWeaponType == EAAAWeaponType::Pistol) ? FVector(0.0, 48.0, 18.0) : AimSocketOffset;
	const FVector CurAimTarget = (CurrentWeaponType == EAAAWeaponType::Pistol) ? FVector(0.0, 0.0, 40.0) : AimTargetOffset;

	const float BaseArm = FMath::Lerp(SavedArm, FMath::Lerp(ArmedArmLength, CurAimArm, AimBlend), CamBlend);
	const FVector BaseSocket = FMath::Lerp(SavedSocket, FMath::Lerp(ArmedSocketOffset, CurAimSocket, (double)AimBlend), (double)CamBlend);
	const FVector BaseTarget = FMath::Lerp(SavedTargetOffset, FMath::Lerp(ArmedTargetOffset, CurAimTarget, (double)AimBlend), (double)CamBlend);
	const float BaseFov = FMath::Lerp(SavedFov, FMath::Lerp(SavedFov, CurAimFOV, AimBlend), CamBlend);

	// Binoculars camera target (first-person eye view, variable optical zoom FOV)
	const float BinoFov = 90.f / FMath::Max(BinocularsZoom, 1.0f);
	const float BinoArm = 0.f; // Direct at eyes
	const FVector BinoSocket = FVector(20.0, 0.0, 68.0); // 20cm in front of head at eye level
	const FVector BinoTarget = FVector(0.0, 0.0, 0.0);

	Boom->TargetArmLength = FMath::Lerp(BaseArm, BinoArm, BinocularsBlend);
	Boom->SocketOffset = FMath::Lerp(BaseSocket, BinoSocket, (double)BinocularsBlend);
	Boom->TargetOffset = FMath::Lerp(BaseTarget, BinoTarget, (double)BinocularsBlend);
	Boom->TargetOffset.Z += ClimbCamLift + CrouchCamZ;
	Boom->SocketOffset.Y += CrouchCamY;
	if (Camera.IsValid()) Camera->SetFieldOfView(FMath::Lerp(BaseFov, BinoFov, BinocularsBlend) + CamFovPunch * (1.f - BinocularsBlend));

	if (!bOn && !bBinocularsActive && CamBlend < 0.002f && BinocularsBlend < 0.002f)
	{
		Boom->TargetArmLength = SavedArm;
		Boom->SocketOffset = SavedSocket + FVector(0.0, CrouchCamY, 0.0);
		Boom->TargetOffset = SavedTargetOffset + FVector(0.0, 0.0, ClimbCamLift + CrouchCamZ);
		if (Camera.IsValid()) Camera->SetFieldOfView(SavedFov);
		bCamSaved = false;
		CamBlend = 0.f;
		BinocularsBlend = 0.f;
	}
}

void UAAAWeaponComponent::AddCameraShake(float Strength)
{
	// one shot: the view snaps up and back a little, rolls a touch to a random side, the FOV pops,
	// and a short burst of noise (trauma) that dies out in ~0.15 s. Springs are damped (zeta ~0.7): no ringing.
	const float S = Strength * CameraShakeScale;
	if (S <= 0.f) return;
	CamKickPV    += 22.f * S;                                                   // deg/s  -> ~0.6 deg up
	CamKickYV    += FMath::FRandRange(-1.f, 1.f) * 7.f * S;                     // tiny sideways
	CamKickRV    += (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(0.5f, 1.f) * 26.f * S;   // ~0.8 deg roll
	CamPushV     += 55.f * S;                                                   // cm/s   -> ~1.4 cm back toward the shooter
	CamFovPunchV += 30.f * S;                                                   // FOV +~0.9 deg pop
	CamTrauma = FMath::Min(1.f, CamTrauma + 0.18f * S);
}

void UAAAWeaponComponent::UpdateCameraShake(float Dt)
{
	if (!Camera.IsValid()) return;
	if (!bCamBaseSaved) { CamBaseRel = Camera->GetRelativeTransform(); bCamBaseSaved = true; }
	for (int32 CamStep = 0; CamStep < 2; ++CamStep)   // sub-stepped: stable at low frame rates
	{
		const float Sd = Dt * 0.5f;
		CamKickPV    += (-400.f * CamKickP - 28.f * CamKickPV) * Sd;       CamKickP    += CamKickPV * Sd;      // 20 rad/s, zeta 0.7
		CamKickYV    += (-400.f * CamKickY - 28.f * CamKickYV) * Sd;       CamKickY    += CamKickYV * Sd;
		CamKickRV    += (-324.f * CamKickR - 25.2f * CamKickRV) * Sd;      CamKickR    += CamKickRV * Sd;      // 18 rad/s, zeta 0.7
		CamPushV     += (-625.f * CamPush - 35.f * CamPushV) * Sd;         CamPush     += CamPushV * Sd;       // 25 rad/s, zeta 0.7
		CamFovPunchV += (-900.f * CamFovPunch - 42.f * CamFovPunchV) * Sd; CamFovPunch += CamFovPunchV * Sd;   // 30 rad/s, zeta 0.7
	}
	CamKickP = FMath::Clamp(CamKickP, -3.f, 3.f); CamKickY = FMath::Clamp(CamKickY, -2.f, 2.f); CamKickR = FMath::Clamp(CamKickR, -3.f, 3.f);
	CamPush = FMath::Clamp(CamPush, -6.f, 6.f); CamFovPunch = FMath::Clamp(CamFovPunch, -4.f, 4.f);
	CamTrauma = FMath::Max(0.f, CamTrauma - Dt * 2.4f);
	CamNoiseT += Dt;

	// trauma^2: a single shot is barely a tremble, sustained fire / a blast nearby shakes properly
	const float Sh = CamTrauma * CamTrauma;
	const float NP = FMath::PerlinNoise1D(CamNoiseT * 13.f) * 1.4f * Sh;
	const float NY = FMath::PerlinNoise1D(CamNoiseT * 13.f + 37.f) * 1.0f * Sh;
	const float NR = FMath::PerlinNoise1D(CamNoiseT * 11.f + 71.f) * 1.6f * Sh;

	const bool bActive = Sh > 0.0001f || FMath::Abs(CamKickP) > 0.002f || FMath::Abs(CamKickR) > 0.002f || FMath::Abs(CamKickY) > 0.002f
		|| FMath::Abs(CamPush) > 0.01f || FMath::Abs(CamKickPV) > 0.01f || FMath::Abs(CamKickRV) > 0.01f;
	if (!bActive)
	{
		if (bCamShaking) { Camera->SetRelativeTransform(CamBaseRel); bCamShaking = false; }
		CamKickP = CamKickPV = CamKickY = CamKickYV = CamKickR = CamKickRV = CamPush = CamPushV = 0.f;
		if (FMath::Abs(CamFovPunch) < 0.01f && FMath::Abs(CamFovPunchV) < 0.05f) CamFovPunch = CamFovPunchV = 0.f;
		return;
	}
	bCamShaking = true;
	// in the camera's own frame (pitch up, yaw, roll around the view axis, pushed back along -X), then the base offset
	const FTransform Shake(FRotator(CamKickP + NP, CamKickY + NY, CamKickR + NR), FVector(-CamPush, 0.0, 0.0));
	Camera->SetRelativeTransform(Shake * CamBaseRel);
}

void UAAAWeaponComponent::SetupAaaPostProcess()
{
	bPostProcessInitialized = true;

	if (!GlobalPPV && GetWorld())
	{
		for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
		{
			if (It->bUnbound)
			{
				GlobalPPV = *It;
				break;
			}
		}
		if (!GlobalPPV)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GlobalPPV = GetWorld()->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			if (GlobalPPV)
			{
				GlobalPPV->bUnbound = true;
				GlobalPPV->Priority = 100.0f;
			}
		}
	}

	auto ConfigureAaaSettings = [](FPostProcessSettings& S)
	{
		// 1. ACES Film Curve & Tonemapping (Activision Modern Warfare IW Engine)
		S.bOverride_FilmSlope = true;
		S.FilmSlope = 0.88f;
		S.bOverride_FilmToe = true;
		S.FilmToe = 0.55f;
		S.bOverride_FilmShoulder = true;
		S.FilmShoulder = 0.93f;
		S.bOverride_FilmBlackClip = true;
		S.FilmBlackClip = 0.0f;
		S.bOverride_FilmWhiteClip = true;
		S.FilmWhiteClip = 0.04f;

		// 2. Color Grading & Tactical Split-Toning (Ubisoft The Division / Ghost Recon)
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(1.02f, 1.00f, 0.94f, 1.0f);
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(1.16f, 1.13f, 1.16f, 1.0f);
		S.bOverride_ColorGamma = true;
		S.ColorGamma = FVector4(0.97f, 0.97f, 0.99f, 1.0f);
		S.bOverride_ColorGain = true;
		S.ColorGain = FVector4(1.05f, 1.03f, 0.97f, 1.0f);
		S.bOverride_ColorContrastShadows = true;
		S.ColorContrastShadows = FVector4(1.04f, 1.06f, 1.10f, 1.0f);
		S.bOverride_ColorGainHighlights = true;
		S.ColorGainHighlights = FVector4(1.06f, 1.03f, 0.96f, 1.0f);

		// 3. Ambient Occlusion (Grounded contact shadows for vehicles, curbs, foliage)
		S.bOverride_AmbientOcclusionIntensity = true;
		S.AmbientOcclusionIntensity = 0.90f;
		S.bOverride_AmbientOcclusionRadius = true;
		S.AmbientOcclusionRadius = 180.0f;
		S.bOverride_AmbientOcclusionQuality = true;
		S.AmbientOcclusionQuality = 100.0f;
		S.bOverride_AmbientOcclusionFadeDistance = true;
		S.AmbientOcclusionFadeDistance = 12000.0f;

		// 4. Optics & Lens Imperfections (35mm Cinematic Optics)
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = 0.0f;   // (screen filter removed on request: no vignette / fringe / grain)
		S.bOverride_SceneFringeIntensity = true;
		S.SceneFringeIntensity = 0.0f;
		S.bOverride_FilmGrainIntensity = true;
		S.FilmGrainIntensity = 0.0f;
		S.bOverride_FilmGrainIntensityShadows = true;
		S.FilmGrainIntensityShadows = 0.0f;
		S.bOverride_FilmGrainIntensityMidtones = true;
		S.FilmGrainIntensityMidtones = 0.0f;
		S.bOverride_FilmGrainIntensityHighlights = true;
		S.FilmGrainIntensityHighlights = 0.0f;
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = 0.70f;
		S.bOverride_BloomThreshold = true;
		S.BloomThreshold = 0.95f;
		S.bOverride_BloomSizeScale = true;
		S.BloomSizeScale = 3.5f;
		S.bOverride_LensFlareIntensity = true;
		S.LensFlareIntensity = 0.45f;

		// 5. Motion Blur: Disabled to completely eliminate vehicle ghosting & glass smearing
		S.bOverride_MotionBlurAmount = true;
		S.MotionBlurAmount = 0.0f;
		S.bOverride_MotionBlurMax = true;
		S.MotionBlurMax = 0.0f;
		S.bOverride_MotionBlurPerObjectSize = true;
		S.MotionBlurPerObjectSize = 0.0f;

		// 6. Local Exposure & Eye Adaptation (Balanced Dynamic Range - Anti-Glare)
		S.bOverride_LocalExposureHighlightContrastScale = true;
		S.LocalExposureHighlightContrastScale = 0.55f;
		S.bOverride_LocalExposureShadowContrastScale = true;
		S.LocalExposureShadowContrastScale = 0.65f;
		S.bOverride_AutoExposureMethod = true;
		S.AutoExposureMethod = AEM_Histogram;
		S.bOverride_AutoExposureMinBrightness = true;
		S.AutoExposureMinBrightness = 0.25f;
		S.bOverride_AutoExposureMaxBrightness = true;
		S.AutoExposureMaxBrightness = 4.0f;
		S.bOverride_AutoExposureSpeedUp = true;
		S.AutoExposureSpeedUp = 3.5f;
		S.bOverride_AutoExposureSpeedDown = true;
		S.AutoExposureSpeedDown = 3.0f;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = -0.35f;

		// 7. Depth of Field (ADS Aiming Ready)
		S.bOverride_DepthOfFieldFstop = true;
		S.DepthOfFieldFstop = 32.0f;
		S.bOverride_DepthOfFieldFocalDistance = true;
		S.DepthOfFieldFocalDistance = 1500.0f;
	};

	if (GlobalPPV)
	{
		GlobalPPV->bUnbound = true;
		GlobalPPV->Priority = 100.0f;
		GlobalPPV->BlendWeight = 1.0f;
		ConfigureAaaSettings(GlobalPPV->Settings);
	}

	if (Camera.IsValid())
	{
		Camera->PostProcessBlendWeight = 1.0f;
		ConfigureAaaSettings(Camera->PostProcessSettings);
	}

	if (GEngine && GetWorld())
	{
		GEngine->Exec(GetWorld(), TEXT("r.MotionBlurQuality 0"));
		GEngine->Exec(GetWorld(), TEXT("r.Translucent.OutputVelocity 1"));
	}
}

void UAAAWeaponComponent::UpdateAaaPostProcess(float Dt)
{
	if (!Camera.IsValid()) return;

	FPostProcessSettings& S = Camera->PostProcessSettings;

	// 1. Dynamic ADS Vignette & Tactical Focus (Call of Duty MW / Ghost Recon ADS)
	const float BaseVignette = 0.f;   // (no base / ADS vignette: clean render; only the low-health pulse remains)
	float HurtExtraVignette = 0.f;
	float DesatFactor = 1.f;
	if (Health < MaxHealth * 0.4f && !bDead)
	{
		const float Danger = 1.f - (Health / (MaxHealth * 0.4f));
		const float Heartbeat = 0.5f + 0.5f * FMath::Sin((float)GetWorld()->GetTimeSeconds() * 6.5f);
		HurtExtraVignette = Danger * 0.25f * Heartbeat;
		DesatFactor = FMath::Lerp(1.f, 0.65f, Danger);
	}
	S.VignetteIntensity = FMath::FInterpTo(S.VignetteIntensity, BaseVignette + HurtExtraVignette, Dt, 8.f);

	// 2. Dynamic Saturation (Low health desaturation + tactical grading)
	S.ColorSaturation = FVector4(1.02f * DesatFactor, 1.00f * DesatFactor, 0.94f * DesatFactor, 1.0f);

	// 3. Dynamic ADS Depth of Field: Cinematic target focus
	if (AimBlend > 0.05f)
	{
		S.DepthOfFieldFstop = FMath::FInterpTo(S.DepthOfFieldFstop, FMath::Lerp(32.0f, 4.5f, AimBlend), Dt, 10.f);
		FHitResult Hit;
		const FVector CamLoc = Camera->GetComponentLocation();
		const FVector CamFwd = Camera->GetForwardVector();
		float FocalDist = 2000.f;
		FCollisionQueryParams Params(TEXT("DoFTrace"), false, Char());
		if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + CamFwd * 12000.f, ECC_Visibility, Params))
		{
			FocalDist = (float)Hit.Distance;
		}
		S.DepthOfFieldFocalDistance = FMath::FInterpTo(S.DepthOfFieldFocalDistance, FocalDist, Dt, 12.f);
	}
	else
	{
		S.DepthOfFieldFstop = FMath::FInterpTo(S.DepthOfFieldFstop, 32.0f, Dt, 8.f);
	}

	// 4. Muzzle Flash Exposure Kick & Grenade Flash
	float ExposureBonus = 0.0f;
	if (SinceShot < 0.06f)
	{
		ExposureBonus += 0.18f;
	}
	if (ExposureFlashT > 0.01f)
	{
		ExposureBonus += ExposureFlashT * 1.8f;
	}
	S.AutoExposureBias = FMath::FInterpTo(S.AutoExposureBias, -0.35f + ExposureBonus, Dt, 18.f);

	// 5. Dynamic Bloom boost
	const float TargetBloom = 0.70f + (SinceShot < 0.06f ? 0.45f : 0.0f) + ExposureFlashT * 1.2f;
	S.BloomIntensity = FMath::FInterpTo(S.BloomIntensity, TargetBloom, Dt, 15.f);

	if (GlobalPPV)
	{
		GlobalPPV->Settings = S;
	}
}

/* ============================================================================================ grenade */

void UAAAWeaponComponent::StartCookingGrenade()
{
	ACharacter* C = Char();
	if (!C || Grenades <= 0 || ThrowT >= 0.f || bCookingGrenade || bDead) return;
	bCookingGrenade = true;
	CookTimer = 0.f;
	PlaySound(ImpactSound, C->GetActorLocation(), 0.5f, 1.8f);
}

void UAAAWeaponComponent::ReleaseGrenade()
{
	bCookingGrenade = false;
	TrajectoryPoints.Reset();
	PredictedLandingLoc = FVector::ZeroVector;

	ACharacter* C = Char();
	if (!C || Grenades <= 0 || ThrowT >= 0.f || bDead) return;

	PendingFuse = FMath::Max(0.4f, 3.2f - CookTimer - 0.80f);   // the fuse keeps burning through the wind-up
	--Grenades;
	ThrowT = 0.f;
	bGrenadeSpawned = false;

	if (CookTimer >= 3.2f)
	{
		ThrowT = -1.f;
		if (UWorld* W = GetWorld())
		{
			FActorSpawnParameters P;
			P.Owner = C;
			P.Instigator = C;
			if (AAAAGrenade* G = W->SpawnActor<AAAAGrenade>(C->GetActorLocation() + FVector(0.0, 0.0, 40.0), FRotator::ZeroRotator, P))
			{
				G->Launch(FVector::ZeroVector, C, 0.05f);
			}
		}
	}
}

void UAAAWeaponComponent::SpawnThrownGrenade()
{
	bGrenadeSpawned = true;
	ACharacter* C = Char();
	APlayerController* PC = ControllingPC();
	UWorld* W = GetWorld();
	if (!C || !W || !PC) return;

	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector HandLoc = C->GetMesh() ? C->GetMesh()->GetSocketLocation(TEXT("hand_l")) : C->GetActorLocation() + FVector(0.0, 0.0, 60.0);
	const FVector LaunchDir = (CamRot.Vector() + FVector(0.0, 0.0, 0.18)).GetSafeNormal();
	const FVector LaunchVel = LaunchDir * GrenadeThrowSpeed + C->GetVelocity() * 0.4;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = C;
	SpawnParams.Instigator = C;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AAAAGrenade* Grenade = W->SpawnActor<AAAAGrenade>(HandLoc, CamRot, SpawnParams))
	{
		Grenade->Launch(LaunchVel, C, PendingFuse);
	}
}

void UAAAWeaponComponent::UpdateTrajectory(ACharacter* C, APlayerController* PC)
{
	TrajectoryPoints.Reset();
	PredictedLandingLoc = FVector::ZeroVector;
	if (!C || !PC || !GetWorld()) return;

	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector Start = C->GetMesh() ? C->GetMesh()->GetSocketLocation(TEXT("hand_l")) : C->GetActorLocation() + FVector(0.0, 0.0, 50.0);
	const FVector LaunchDir = (CamRot.Vector() + FVector(0.0, 0.0, 0.18)).GetSafeNormal();
	const FVector Vel = LaunchDir * GrenadeThrowSpeed + C->GetVelocity() * 0.4;

	FPredictProjectilePathParams PathParams(7.0f, Start, Vel, 3.0f, ECC_WorldStatic, C);
	PathParams.bTraceWithCollision = true;
	PathParams.DrawDebugType = EDrawDebugTrace::None;

	FPredictProjectilePathResult PathResult;
	if (UGameplayStatics::PredictProjectilePath(GetWorld(), PathParams, PathResult))
	{
		for (const FPredictProjectilePathPointData& Pt : PathResult.PathData)
		{
			TrajectoryPoints.Add(Pt.Location);
		}
		if (PathResult.HitResult.bBlockingHit)
		{
			PredictedLandingLoc = PathResult.HitResult.ImpactPoint;
		}
		else if (TrajectoryPoints.Num() > 0)
		{
			PredictedLandingLoc = TrajectoryPoints.Last();
		}
	}
}

/* ============================================================================================ shooting */

void UAAAWeaponComponent::FireShot(APlayerController* PC)
{
	ACharacter* C = Char();
	UWorld* W = GetWorld();
	if (!C || !W || !PC) return;
	--Ammo;
	OnAmmoChanged.Broadcast(Ammo, ReserveAmmo);
	const float GapSinceLast = SinceShot;   // for the in-hand give: rapid fire = braced grip
	SinceShot = 0.f;
	FlashT = 0.04f;
	if (Flash) Flash->SetRelativeRotation(FRotator(90.0, 0.0, FMath::FRandRange(0.0, 360.0)));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AAAWeaponAim), false, C);
	const FCollisionObjectQueryParams Obj = BulletObjects();
	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Fwd = CamRot.Vector();
	// aim trace from the camera, starting level with the character (nothing between the camera and me counts)
	const FVector Pc = C->GetActorLocation();
	const FVector S = CamLoc + Fwd * FMath::Max(0.0, FVector::DotProduct(Pc - CamLoc, Fwd));
	FVector AimPt = S + Fwd * 150000.0;
	FHitResult Hit;
	if (W->LineTraceSingleByObjectType(Hit, S, AimPt, Obj, Params)) AimPt = Hit.ImpactPoint;
	FVector From = MuzzleTransform().GetLocation();
	FHitResult Block;   // muzzle through a wall (standing against it): fire from just in front of the body
	if (W->LineTraceSingleByObjectType(Block, Pc + FVector(0.0, 0.0, 45.0), From, Obj, Params)) From = Block.ImpactPoint + Block.ImpactNormal * 4.0;
	FVector Dir = (AimPt - From).GetSafeNormal();
	if (Dir.IsNearlyZero()) Dir = Fwd;
	const float Moving = C->GetVelocity().Size2D() > 60.f ? 0.7f : 0.f;
	const float Cone = (bAiming ? SpreadAim : SpreadHip) + Spread + Moving;
	Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(Cone));
	FBullet B; B.P = From; B.V = Dir * (MuzzleVelocity * 100.f); B.bPolice = false;
	UStaticMeshComponent* Tr = Pooled(Tracers, NextTracer, MAX_TRACERS, false, TracerMat, false, B.Tracer);
	Tr->SetVisibility(false);
	Bullets.Add(B);
	OnWeaponFired.Broadcast(CurrentWeaponData, From, Dir);
	ReceiveOnFireVisuals(CurrentWeaponData, From, Dir);
	Spread = FMath::Min(SpreadMax, Spread + SpreadPerShot);

	// camera recoil: controlled vertical kick + centered micro-jitter (no rightward drift)
	const float Kp = RecoilPitch * (bAiming ? 0.45f : 0.65f) * FMath::FRandRange(0.85f, 1.15f);
	const float Ky = RecoilYaw * FMath::FRandRange(-0.45f, 0.45f);
	FRotator R = PC->GetControlRotation();
	R.Pitch += Kp; R.Yaw += Ky;
	PC->SetControlRotation(R.GetNormalized());
	RecoilAccum += Kp * FMath::Max(RecoilRecovery, 0.88f);
	KickV += 14.f;
	KickSide = FMath::FRandRange(-1.f, 1.f);
	{
		const bool bPistolShot = CurrentWeaponType == EAAAWeaponType::Pistol;
		if (bPistolShot)
		{
			GPistolSlideAnim = 1.0f; // kick slide fully back!
			GPistolHammerRot = 0.0f; // hammer strikes forward on primer
		}
		const float InHandFollow = GapSinceLast < 0.4f ? 0.4f : 1.f;     // follow-up shots: the grip is already tight
		InHandPitchV += (bPistolShot ? 150.f : 60.f) * InHandFollow;      // deg/s: ~1.5 deg (pistol) / ~0.6 deg (rifle) in the palm
		InHandYawV   += KickSide * (bPistolShot ? 40.f : 18.f) * InHandFollow;
		// camera shake: heavier gun = more, aiming down sights = tighter; follow-up shots a bit less (braced)
		const float ShakeWeapon = bPistolShot ? 0.7f : (CurrentWeaponType == EAAAWeaponType::LMG ? 1.3f : 1.f);
		AddCameraShake(ShakeWeapon * (bAiming ? 0.6f : 1.f) * (GapSinceLast < 0.4f ? 0.75f : 1.f) * (bBinocularsActive ? 0.f : 1.f));
	}

	// spent casing out of the ejection port (Kar98k: later, when the bolt is pulled back - see the tick)
	if (CurrentWeaponType == EAAAWeaponType::Kar98)
	{
		SniperFireT = 0.f; bBoltSoundPlayed = false; bBoltCasingOut = false;
	}
	else SpawnCasing(C);
	// Authentic gunshot audio from real recorded assets (M4 / Beretta M9) - no artificial synthetic processing
	USoundBase* ActiveShot = (CurrentWeaponType == EAAAWeaponType::Pistol) ? PistolShotSound.Get() : (M4ShotSound ? M4ShotSound.Get() : ShotSound.Get());
	PlaySound(ActiveShot, From, CurrentWeaponType == EAAAWeaponType::Kar98 ? 1.0f : 1.0f, CurrentWeaponType == EAAAWeaponType::Kar98 ? 0.72f : 1.0f);

	if (ATrafficAIManager* T = Traffic()) T->ReportGunfire(From);
	if (UAAAWeaponsSubsystem* Sq = GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>()) Sq->OnPlayerGunshot(From);
}

void UAAAWeaponComponent::SpawnCasing(ACharacter* C)
{
	if (Rifle && bRifleAttached && C)
	{
		int32 Ci = -1;
		UStaticMeshComponent* Cs = Pooled(Casings, NextCasing, MAX_CASINGS, true, BrassMat, false, Ci);
		const FTransform RT = Rifle->GetComponentTransform();
		FFx F; F.Idx = Ci; F.T = 0.f; F.Life = 2.f;
		F.V = RT.GetUnitAxis(EAxis::Y) * FMath::FRandRange(180.0, 260.0) + FVector(0.0, 0.0, FMath::FRandRange(120.0, 200.0)) - RT.GetUnitAxis(EAxis::X) * 40.0;
		F.Base = FVector(0.0, 0.0, C->GetActorLocation().Z - C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 0.6);
		Cs->SetWorldLocationAndRotation(RT.TransformPosition(EjectLocal), FRotator(0.0, RT.Rotator().Yaw, 90.0));
		Cs->SetWorldScale3D(FVector(0.011, 0.011, 0.045));
		Cs->SetVisibility(true);
		CasingFx.Add(F);
	}
}

void UAAAWeaponComponent::FirePolice(const FVector& From, ACharacter* C)
{
	const FVector Chest = C->GetActorLocation() + FVector(0.0, 0.0, 25.0);
	const float Dm = (float)FVector::Dist(From, Chest) / 100.f;
	const float Sp = (float)C->GetVelocity().Size() / 100.f;
	FireIncoming(From, FMath::Clamp(0.5f - Dm / 130.f - Sp / 22.f, 0.06f, 0.55f), PoliceDamage);
}

void UAAAWeaponComponent::FireIncoming(const FVector& From, float HitChance, float BulletDamage)
{
	ACharacter* C = Char();
	if (!C || bDead) return;
	const FVector Chest = C->GetActorLocation() + FVector(0.0, 0.0, 25.0);
	const float P = HitChance;
	FVector Aim = Chest + FMath::VRand() * 12.0;
	if (FMath::FRand() > P)
	{
		const FVector Dn = (Chest - From).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Dn, FVector::UpVector).GetSafeNormal();
		Aim = Chest + Side * (FMath::FRandRange(60.0, 190.0) * (FMath::RandBool() ? 1.0 : -1.0)) + FVector(0.0, 0.0, FMath::FRandRange(-40.0, 90.0));
	}
	FBullet B; B.P = From; B.V = (Aim - From).GetSafeNormal() * 52000.0; B.bPolice = true; B.Dmg = BulletDamage;
	UStaticMeshComponent* Tr = Pooled(Tracers, NextTracer, MAX_TRACERS, false, PoliceTracerMat, false, B.Tracer);
	Tr->SetVisibility(false);
	Bullets.Add(B);
	PlaySound(ShotSound, From, 0.75f, FMath::FRandRange(0.82f, 0.9f));
}

void UAAAWeaponComponent::FireAI(const FVector& From, const FVector& Aim, float BulletDmg, AActor* ShotBy, uint8 ShooterTeam, float SuppressAmt)
{
	// a soldier's round: flown like the others (real hits on whoever is in the way, tracers, cracks, suppression)
	if (!GetWorld()) return;
	FBullet B;
	B.P = From;
	B.V = (Aim - From).GetSafeNormal() * 52000.0;
	B.Dmg = BulletDmg;
	B.bAI = true;
	B.Team = ShooterTeam;
	B.Shooter = ShotBy;
	B.Sup = SuppressAmt;
	UAAAWeaponsSubsystem* Sq = GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>();
	// rounds of a side hostile to the leader can hit him too (the police only while WANTED)
	B.bPolice = AAAAPoliceOfficer::Hostile(ShooterTeam, AAATeam::Player, Sq ? Sq->WantedStars() : 0);
	UStaticMeshComponent* Tr = Pooled(Tracers, NextTracer, MAX_TRACERS, false, ShooterTeam == AAATeam::Player ? TracerMat : PoliceTracerMat, false, B.Tracer);
	if (Tr) Tr->SetVisibility(false);
	Bullets.Add(B);
	// the shot: quieter far away (PlaySound attenuates by distance)
	PlaySound(ShotSound, From, 0.6f, FMath::FRandRange(0.86f, 0.98f));
}

void UAAAWeaponComponent::UpdatePolice(float Dt, ACharacter* C)
{
	ATrafficAIManager* T = Traffic();
	if (!T || bDead) return;
	if (BurstLeft > 0)
	{
		BurstT -= Dt;
		if (BurstT <= 0.f) { FirePolice(BurstFrom, C); --BurstLeft; BurstT = 0.12f; }
		return;
	}
	PoliceT -= Dt;
	if (PoliceT > 0.f) return;
	TArray<FVector> Guns;
	T->GetPoliceShooters(PoliceRange, Guns);
	if (!Guns.Num()) { PoliceT = 0.5f; return; }
	Shooter = (Shooter + 1) % Guns.Num();
	const FVector Chest = C->GetActorLocation() + FVector(0.0, 0.0, 25.0);
	const FVector Dn = (Chest - Guns[Shooter]).GetSafeNormal();
	const FVector Out = Guns[Shooter] + Dn * 160.0;   // out of the car window
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AAAPoliceSight), false, C);
	FHitResult H;
	if (GetWorld()->LineTraceSingleByChannel(H, Out, Chest, ECC_Visibility, Params)) { PoliceT = 0.4f; return; }   // no line of sight
	BurstFrom = Out;
	BurstLeft = FMath::RandRange(2, 4);
	BurstT = FMath::FRandRange(0.f, 0.15f);
	PoliceT = FMath::Max(0.35f, PoliceBurstInterval / Guns.Num()) * FMath::FRandRange(0.8f, 1.3f);
}

void UAAAWeaponComponent::StepBullets(float Dt)
{
	UWorld* W = GetWorld();
	ACharacter* C = Char();
	if (!W || !C || !Bullets.Num()) return;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AAABullet), false, C);
	const FCollisionObjectQueryParams Obj = BulletObjects();
	const UCapsuleComponent* Cap = C->GetCapsuleComponent();
	for (int32 I = Bullets.Num() - 1; I >= 0; --I)
	{
		if (!Bullets.IsValidIndex(I)) continue;
		FBullet& B = Bullets[I];
		B.Life += Dt;
		const FVector P0 = B.P;
		B.V.Z -= 980.0 * BulletGravity * Dt;
		const FVector P1 = P0 + B.V * Dt;
		FHitResult Hit;
		bool bHit;
		if (B.bAI)
		{
			// a soldier's round never hits the man who fired it
			FCollisionQueryParams Pa(SCENE_QUERY_STAT(AAABulletAI), false, C);
			if (AActor* Sh = B.Shooter.Get()) Pa.AddIgnoredActor(Sh);
			bHit = W->LineTraceSingleByObjectType(Hit, P0, P1, Obj, Pa);
		}
		else bHit = W->LineTraceSingleByObjectType(Hit, P0, P1, Obj, Params);
		const FVector Dir = (P1 - P0).GetSafeNormal();
		// police bullets against the player's capsule
		if (B.bPolice && !bDead && Cap && !C->IsHidden())
		{
			const double Hh = Cap->GetScaledCapsuleHalfHeight(), Rr = Cap->GetScaledCapsuleRadius();
			const FVector Ctr = Cap->GetComponentLocation(), A = Ctr + FVector(0.0, 0.0, Hh - Rr), Bt = Ctr - FVector(0.0, 0.0, Hh - Rr);
			FVector X1, X2;
			FMath::SegmentDistToSegmentSafe(P0, P1, A, Bt, X1, X2);
			if (FVector::Dist(X1, X2) < Rr && (!bHit || FVector::Dist(P0, X1) < Hit.Distance))
			{
				const float Dmg = B.bAI ? PoliceDamage * 1.1f : (B.Dmg > 0.f ? B.Dmg : PoliceDamage);
				if (Tracers.IsValidIndex(B.Tracer)) Tracers[B.Tracer]->SetVisibility(false);
				Bullets.RemoveAtSwap(I);
				LastHitFrom = P0 - Dir * 600.0;   // the shooter's side: picks the flinch / death clip
				LastHitFromT = 0.f;
				UGameplayStatics::ApplyDamage(C, Dmg, nullptr, nullptr, UDamageType::StaticClass());
				if (bDead) break;
				continue;
			}

			// 3D Supersonic Bullet Whiz-By / Crack when an incoming round zips past the player's head
			const FVector HeadLoc = Ctr + FVector(0.0, 0.0, 62.0);
			if (FVector::DotProduct(P0 - HeadLoc, Dir) < 0.0 && FVector::DotProduct(P1 - HeadLoc, Dir) >= 0.0)
			{
				const FVector ClosestPt = FMath::ClosestPointOnSegment(HeadLoc, P0, P1);
				const float MissDist = (float)FVector::Dist(HeadLoc, ClosestPt);
				if (MissDist < 340.f)
				{
					const float CrackVol = FMath::Lerp(0.85f, 0.22f, MissDist / 340.f);
					PlaySound(ImpactSound, ClosestPt, CrackVol, FMath::FRandRange(1.75f, 2.15f));
					if (MissDist < 180.f) PlaySound(DrySound, ClosestPt, CrackVol * 0.55f, FMath::FRandRange(1.50f, 1.85f));
				}
			}
		}
		if (bHit)
		{
			const bool bPol = B.bPolice || B.bAI;
			if (B.bAI)
			{
				// a soldier's round: damage whoever it hit (the shooter is the causer: the victim knows who did it)
				AActor* Victim = Hit.GetActor();
				if (Victim && Victim != B.Shooter.Get() && Victim != C && Cast<APawn>(Victim))
					UGameplayStatics::ApplyPointDamage(Victim, B.Dmg, Dir, Hit, nullptr, B.Shooter.Get(), UDamageType::StaticClass());
			}
			if (Tracers.IsValidIndex(B.Tracer)) Tracers[B.Tracer]->SetVisibility(false);
			Bullets.RemoveAtSwap(I);
			Impact(Hit, Dir, bPol);
			if (bDead) break;
			continue;
		}
		if (UAAAWeaponsSubsystem* Sq = W->GetSubsystem<UAAAWeaponsSubsystem>())
		{
			if (B.bAI) Sq->OnBulletPathTeam(P0, P1, B.Team, B.Sup);
			else if (!B.bPolice) Sq->OnBulletPath(P0, P1);
		}
		B.P = P1;
		if (Tracers.IsValidIndex(B.Tracer))
		{
			UStaticMeshComponent* Tr = Tracers[B.Tracer];
			const double Len = FMath::Min(FVector::Dist(P0, P1), 420.0);
			Tr->SetWorldLocationAndRotation(P1 - Dir * Len * 0.5, Dir.Rotation());
			Tr->SetWorldScale3D(FVector(Len / 100.0, B.bPolice ? 0.018 : 0.014, B.bPolice ? 0.018 : 0.014));
			Tr->SetVisibility(B.Life > Dt * 0.5f);
		}
		if (B.Life > 2.5f)
		{
			if (Tracers.IsValidIndex(B.Tracer)) Tracers[B.Tracer]->SetVisibility(false);
			Bullets.RemoveAtSwap(I);
		}
	}
}

void UAAAWeaponComponent::Impact(const FHitResult& Hit, const FVector& Dir, bool bPolice)
{
	AActor* A = Hit.GetActor();
	UPrimitiveComponent* Comp = Hit.GetComponent();
	bool bMark = false;
	if (AAAATrainingTarget* Tg = Cast<AAAATrainingTarget>(A))
	{
		const int32 Pts = Tg->OnHit(Hit.ImpactPoint, Dir);
		if (Pts > 0 && !bPolice) { Score += Pts; LastPoints = Pts; PointsT = 1.2f; bMark = true; }
	}
	else if (ATrafficAIManager* T = Traffic())
	{
		if (A == T && T->OnBulletHit(Comp, !bPolice) && !bPolice) bMark = true;
	}
	// a round's push: cars / props take the full kick; a body (ragdoll) only a shove at the bone it hit
	if (Comp && Comp->IsSimulatingPhysics()) Comp->AddImpulseAtLocation(Dir * (Cast<USkeletalMeshComponent>(Comp) ? 1500.f : ImpactImpulse), Hit.ImpactPoint, Hit.BoneName);
	if (A && A != GetOwner() && !bPolice && !Cast<AAAATrainingTarget>(A))
	{
		UGameplayStatics::ApplyPointDamage(A, Damage, Dir, Hit, ControllingPC(), GetOwner(), UDamageType::StaticClass());
		if (Cast<APawn>(A)) bMark = true;
	}
	if (bMark) HitMarkT = 0.16f;

	// bullet hole on fixed surfaces
	const bool bFixed = Comp && Comp->Mobility != EComponentMobility::Movable;
	if (bFixed)
	{
		int32 Hi = -1;
		UStaticMeshComponent* H = Pooled(Holes, NextHole, MAX_HOLES, true, HoleMat, false, Hi);
		H->SetWorldLocationAndRotation(Hit.ImpactPoint + Hit.ImpactNormal * 0.2, FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator());
		H->SetWorldScale3D(FVector(FMath::FRandRange(0.026, 0.036), FMath::FRandRange(0.026, 0.036), 0.003));
		H->SetVisibility(true);
	}
	// dust puff
	int32 Pi = -1;
	UStaticMeshComponent* Pf = Pooled(Puffs, NextPuff, MAX_PUFFS, false, PuffMat, false, Pi);
	Pf->SetStaticMesh(AAAShapes::Sphere());
	Pf->SetWorldLocation(Hit.ImpactPoint + Hit.ImpactNormal * 3.0);
	Pf->SetVisibility(true);
	FFx F; F.Idx = Pi; F.T = 0.f; F.Life = 0.32f; F.V = Hit.ImpactNormal * 70.0 + FVector(0.0, 0.0, 25.0);
	F.Base = FVector(bFixed ? 0.2 : 0.12);
	PuffFx.Add(F);
	PlaySound(ImpactSound, Hit.ImpactPoint, bPolice ? 0.6f : 0.45f, FMath::FRandRange(0.9f, 1.15f));
}

void UAAAWeaponComponent::UpdateFx(float Dt)
{
	// 3D Positional Footsteps synchronized with walking/jogging cadence
	if (ACharacter* C = Char())
	{
		if (!bDead && !C->IsHidden())
		{
			const bool bFallingNow = C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling();
			const FVector CurLoc = C->GetActorLocation();
			USkeletalMeshComponent* M = C->GetMesh();

			if (G3DAudio.bWasFalling && !bFallingNow && !ClimbState.bMantling)
			{
				G3DAudio.StepDistAccum = 0.f;
			}
			G3DAudio.bWasFalling = bFallingNow;
			G3DAudio.bWasMantling = ClimbState.bMantling;

			// Real 3D Footsteps: stride calibrated for single foot strikes (82cm walk, 146cm jog)
			if (!bFallingNow && !ClimbState.bMantling)
			{
				const float Speed2D = (float)C->GetVelocity().Size2D();
				if (Speed2D > 35.f && !G3DAudio.LastFootPos.IsZero())
				{
					G3DAudio.StepDistAccum += (float)FVector::Dist2D(CurLoc, G3DAudio.LastFootPos);
					const float StepStride = FMath::GetMappedRangeValueClamped(FVector2D(160.f, 500.f), FVector2D(82.f, 146.f), Speed2D);
					if (G3DAudio.StepDistAccum >= StepStride)
					{
						G3DAudio.StepDistAccum = FMath::Fmod(G3DAudio.StepDistAccum, StepStride);
						const FName FootBone = G3DAudio.bLeftStep ? TEXT("foot_l") : TEXT("foot_r");
						G3DAudio.bLeftStep = !G3DAudio.bLeftStep;
						const FVector FootWorldPos = M ? M->GetSocketLocation(FootBone) : (CurLoc - FVector(0.0, 0.0, 88.0));
						const float StepVol = FMath::GetMappedRangeValueClamped(FVector2D(150.f, 520.f), FVector2D(0.35f, 0.65f), Speed2D);

						USoundBase* StepSfx = ImpactSound;
						if (FootstepSounds.Num() > 0)
						{
							static int32 LastStepIdx = -1;
							int32 StepIdx = FMath::RandRange(0, FootstepSounds.Num() - 1);
							if (FootstepSounds.Num() > 1 && StepIdx == LastStepIdx)
							{
								StepIdx = (StepIdx + 1) % FootstepSounds.Num();
							}
							LastStepIdx = StepIdx;
							if (FootstepSounds[StepIdx]) StepSfx = FootstepSounds[StepIdx];
						}
						// Play pure recorded footstep sound with authentic pitch 1.0
						PlaySound(StepSfx, FootWorldPos, StepVol, 1.0f);
					}
				}
				else
				{
					G3DAudio.StepDistAccum = 0.f;
				}
				G3DAudio.LastFootPos = CurLoc;
			}
		}
	}

	for (int32 I = PuffFx.Num() - 1; I >= 0; --I)
	{
		FFx& F = PuffFx[I];
		F.T += Dt;
		if (!Puffs.IsValidIndex(F.Idx)) { PuffFx.RemoveAtSwap(I); continue; }
		UStaticMeshComponent* P = Puffs[F.Idx];
		const float K = F.T / F.Life;
		if (K >= 1.f) { P->SetVisibility(false); PuffFx.RemoveAtSwap(I); continue; }
		F.V *= FMath::Max(0.0, 1.0 - 6.0 * Dt);
		P->SetWorldLocation(P->GetComponentLocation() + F.V * Dt);
		const double S = F.Base.X * (0.25 + 1.2 * K) * (1.0 - K * K);
		P->SetWorldScale3D(FVector(S));
	}
	for (int32 I = CasingFx.Num() - 1; I >= 0; --I)
	{
		FFx& F = CasingFx[I];
		F.T += Dt;
		if (!Casings.IsValidIndex(F.Idx)) { CasingFx.RemoveAtSwap(I); continue; }
		UStaticMeshComponent* P = Casings[F.Idx];
		if (F.T >= F.Life) { P->SetVisibility(false); CasingFx.RemoveAtSwap(I); continue; }
		FVector L = P->GetComponentLocation();
		if (L.Z > F.Base.Z || F.V.Z > 0.0)
		{
			F.V.Z -= 980.0 * Dt;
			L += F.V * Dt;
			if (L.Z <= F.Base.Z)
			{
				L.Z = F.Base.Z;
				if (F.V.Z < -150.0)
				{
					F.V = FVector(F.V.X * 0.3, F.V.Y * 0.3, -F.V.Z * 0.3);
				}
				else
				{
					F.V = FVector::ZeroVector;
				}
			}
			P->SetWorldLocationAndRotation(L, P->GetComponentRotation() + FRotator(Dt * 900.0, Dt * 400.0, 0.0));
		}
	}
}

/* ============================================================================================ targets */

void UAAAWeaponComponent::SpawnTargets(ACharacter* C)
{
	UWorld* W = GetWorld();
	if (!W) return;
	for (const TWeakObjectPtr<AActor>& T : SpawnedTargets) if (T.IsValid()) T->Destroy();
	SpawnedTargets.Reset();
	const FVector F = C->GetActorForwardVector().GetSafeNormal2D(), R = C->GetActorRightVector().GetSafeNormal2D();
	const float Dist[5] = { 9.f, 14.f, 20.f, 28.f, 40.f }, Side[5] = { -2.5f, 2.f, -0.5f, 3.f, -1.5f };
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AAATargets), false, C);
	for (int32 K = 0; K < 5; ++K)
	{
		FVector P = C->GetActorLocation() + F * (Dist[K] * 100.0) + R * (Side[K] * 100.0);
		FHitResult H;
		if (W->LineTraceSingleByChannel(H, P + FVector(0.0, 0.0, 400.0), P - FVector(0.0, 0.0, 2500.0), ECC_Visibility, Params)) P = H.ImpactPoint;
		const FRotator Face = (C->GetActorLocation() - P).GetSafeNormal2D().Rotation();
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AAAATrainingTarget* T = W->SpawnActor<AAAATrainingTarget>(AAAATrainingTarget::StaticClass(), P, FRotator(0.0, Face.Yaw, 0.0), Sp)) SpawnedTargets.Add(T);
	}
	Score = 0;
}

/* ============================================================================================ health */

void UAAAWeaponComponent::OnOwnerDamaged(AActor* DamagedActor, float Amount, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (bDead || Amount <= 0.f) return;
	Health -= Amount;
	SinceHurt = 0.f;
	// flinch (and the death clip) from the side the hit came from, harder for bigger hits
	if (const ACharacter* Me = Char())
	{
		const bool bKnownFrom = DamageCauser && DamageCauser != Me;
		const FVector From = bKnownFrom ? DamageCauser->GetActorLocation() : (LastHitFromT < 0.5f ? LastHitFrom : Me->GetActorLocation() + Me->GetActorForwardVector() * 100.0);
		PlayerHitDir = UAAARifleAnimInstance::DirFrom(Me, From);
		PlayerHitStr = FMath::Clamp(Amount / 35.f, 0.2f, 1.f);
		PlayerHitIdx = FMath::RandRange(0, 3);
		PlayerHitT = 0.f;
		LastDamage = Amount;
	}
	HurtFlash = FMath::Min(1.f, HurtFlash + 0.45f);
	if (const ACharacter* C = Char()) PlaySound(HurtSound, C->GetActorLocation(), 0.7f, FMath::FRandRange(0.95f, 1.05f));
	if (Health <= 0.f) { Health = 0.f; Die(); }
}

void UAAAWeaponComponent::Die()
{
	ACharacter* C = Char();
	if (!C || bDead) return;
	SetArmed(false);
	bDead = true; DeadT = 0.f;
	for (const FBullet& B : Bullets)
		if (Tracers.IsValidIndex(B.Tracer)) Tracers[B.Tracer]->SetVisibility(false);
	Bullets.Reset();
	USkeletalMeshComponent* M = C->GetMesh();
	const bool bInAir = C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling();
	if (UCharacterMovementComponent* CM = C->GetCharacterMovement()) { CM->StopMovementImmediately(); CM->DisableMovement(); }
	if (APlayerController* PC = ControllingPC()) { PC->SetIgnoreMoveInput(true); bIgnoringMove = true; }
	if (M)
	{
		SavedMeshProfile = M->GetCollisionProfileName();
		SavedMeshLoc = M->GetRelativeLocation();
		SavedMeshRot = M->GetRelativeRotation();
	}
	// shot: the death clip for the side the hit came from, then the ragdoll; blast / falling: ragdoll right away
	// (the hero always falls on his back - a "front" death clip - so he can raise his finger to the sky; in the air: ragdoll)
	DeathIdx = (Anim && !bInAir) ? UAAARifleAnimInstance::PickDeath(0.f) : INDEX_NONE;
	DeathAnimLen = UAAARifleAnimInstance::GetDeathLength(DeathIdx);
	bRagdolled = false;
	if (DeathIdx == INDEX_NONE || DeathAnimLen < 0.2f) StartRagdoll();
	if (UCapsuleComponent* Cap = C->GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
}

void UAAAWeaponComponent::StartRagdoll()
{
	ACharacter* C = Char();
	if (!C || bRagdolled) return;
	bRagdolled = true;
	USkeletalMeshComponent* M = C->GetMesh();
	if (M)
	{
		M->SetCollisionProfileName(TEXT("Ragdoll"));
		M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		if (!M->GetBodyInstance() || !M->GetBodyInstance()->IsValidBodyInstance()) M->RecreatePhysicsState();
		M->SetAllBodiesSimulatePhysics(true);
		M->SetSimulatePhysics(true);
		// joints with a little friction: the body folds and settles instead of jellying / spinning
		for (FBodyInstance* BI : M->Bodies) if (BI) { BI->LinearDamping = 0.12f; BI->AngularDamping = 0.9f; BI->UpdateDampingProperties(); }
		M->WakeAllRigidBodies();
		if (M->IsSimulatingPhysics()) M->AddImpulse(C->GetVelocity() * 0.3 + FVector(0.0, 0.0, -200.0), NAME_None, true);
	}
	if (UCapsuleComponent* Cap = C->GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
}

void UAAAWeaponComponent::Respawn()
{
	ACharacter* C = Char();
	if (!C || !GetWorld()) return;
	if (Anim) { Anim->Inputs.DeathTime = -1.f; Anim->Inputs.ShahadaT = -1.f; }
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
	DeathIdx = INDEX_NONE; bRagdolled = false; PlayerHitT = -1.f; LastDamage = 0.f;

	USkeletalMeshComponent* M = C->GetMesh();
	UCapsuleComponent* Cap = C->GetCapsuleComponent();
	UCharacterMovementComponent* CM = C->GetCharacterMovement();
	APlayerController* PC = ControllingPC();

	// 1. Immediately halt any residual velocity
	if (CM)
	{
		CM->StopMovementImmediately();
		CM->Velocity = FVector::ZeroVector;
	}

	// 2. Complete physics ragdoll teardown and rebuild
	if (M)
	{
		M->PutAllRigidBodiesToSleep();
		M->SetAllBodiesSimulatePhysics(false);
		M->SetSimulatePhysics(false);
		M->RecreatePhysicsState();
		M->SetCollisionProfileName(SavedMeshProfile.IsNone() ? TEXT("CharacterMesh") : SavedMeshProfile);
		M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

		if (Cap)
		{
			M->AttachToComponent(Cap, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		M->SetRelativeLocationAndRotation(DefaultMeshLoc, DefaultMeshRot);
		M->RefreshBoneTransforms();
		M->UpdateComponentToWorld();

		if (SavedAnimClass)
		{
			M->SetAnimInstanceClass(SavedAnimClass);
		}
	}

	// 3. Reactivate capsule collision
	if (Cap)
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Cap->UpdateComponentToWorld();
	}

	// 4. Safe PlayerStart location with safety elevation to prevent floor intersection
	FVector SpawnLoc = C->GetActorLocation() + FVector(0.0, 0.0, 100.0);
	FRotator SpawnRot = C->GetActorRotation();
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		SpawnLoc = It->GetActorLocation();
		SpawnRot = It->GetActorRotation();
		break;
	}
	const float HalfHeight = Cap ? Cap->GetScaledCapsuleHalfHeight() : 90.f;
	SpawnLoc.Z += HalfHeight + 6.0f;

	C->TeleportTo(SpawnLoc, FRotator(0.0, SpawnRot.Yaw, 0.0));

	// 5. Restore walking movement mode
	if (CM)
	{
		CM->SetMovementMode(MOVE_Walking);
		CM->Velocity = FVector::ZeroVector;
		CM->UpdateComponentVelocity();
	}

	// 6. Reset player input and control rotation
	if (PC)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		PC->SetControlRotation(FRotator(0.0, SpawnRot.Yaw, 0.0));
	}
	bIgnoringMove = false;

	// 7. Reset camera boom
	if (Boom.IsValid())
	{
		Boom->TargetArmLength = SavedArm;
		Boom->SocketOffset = SavedSocket;
		Boom->TargetOffset = SavedTargetOffset;
	}
	CamBlend = 0.f;
	AimBlend = 0.f;
	bCamSaved = false;

	// 8. Restore health and ammo
	Health = MaxHealth;
	Ammo = MagazineSize;
	bDead = false;
	DeadT = 0.f;
	HurtFlash = 0.f;

	// 9. Reset Wanted level and clear pursuit
	if (ATrafficAIManager* T = Traffic())
	{
		T->SetPlayerWanted(0);
	}
}

void UAAAWeaponComponent::CreateSmartHud()
{
	if (SmartHudWidget) return;
	if (APlayerController* PC = ControllingPC())
	{
		SmartHudWidget = NewObject<UAAASmartHudWidget>(PC, UAAASmartHudWidget::StaticClass());
		if (SmartHudWidget)
		{
			SmartHudWidget->Initialize();
			SmartHudWidget->InitWeapon(this);
			SmartHudWidget->AddToViewport(15);
		}
	}
}

void UAAAWeaponComponent::SwitchWeapon(EAAAWeaponType NewType)
{
	if (bBinocularsActive) SetBinoculars(false);
	const EAAAWeaponType OldType = CurrentWeaponType;
	CurrentWeaponType = NewType;
	MuzzleVelocity = 720.f; BulletGravity = 1.f; SniperFireT = -1.f;   // (the sniper below overrides these)
	if (NewType == EAAAWeaponType::Kar98)
	{
		MagazineSize = 5;
		Ammo = FMath::Clamp(Ammo, 0, 5);
		if (OldType != EAAAWeaponType::Kar98) Ammo = 5;
		Damage = 115.f;          // one round kills at most ranges
		FireRate = 0.7f;         // bolt action: the 1.45 s fire + bolt cycle is the limit
		MuzzleVelocity = 920.f;
		BulletGravity = 0.45f;
		ImpactImpulse = 16000.f;
		SpreadHip = 2.2f;
		SpreadAim = 0.02f;
		SpreadPerShot = 0.6f;
		RecoilPitch = 1.45f;
		RecoilYaw = 0.25f;
	}
	else if (NewType == EAAAWeaponType::Pistol)
	{
		MagazineSize = 8;
		Ammo = FMath::Clamp(Ammo, 0, 8);
		if (OldType != EAAAWeaponType::Pistol) Ammo = 8;
		Damage = 48.f; // Heavy .45 ACP stopping power
		FireRate = 7.5f; // Rapid semi-auto
		ImpactImpulse = 9000.f;
		SpreadHip = 1.0f;
		SpreadAim = 0.08f;
		SpreadPerShot = 0.38f;
		RecoilPitch = 0.65f;
		RecoilYaw = 0.18f;
	}
	else if (NewType == EAAAWeaponType::LMG)
	{
		MagazineSize = 100;
		Ammo = FMath::Clamp(Ammo, 0, 100);
		if (OldType != EAAAWeaponType::LMG) Ammo = 100;
		Damage = 28.f;
		FireRate = 13.5f; // 810 RPM heavy suppression
		ImpactImpulse = 14000.f;
		SpreadHip = 2.4f;
		SpreadAim = 0.35f;
		SpreadPerShot = 0.22f;
		RecoilPitch = 0.75f;
		RecoilYaw = 0.35f;
	}
	else // Rifle
	{
		MagazineSize = 30;
		Ammo = FMath::Clamp(Ammo, 0, 30);
		if (OldType != EAAAWeaponType::Rifle) Ammo = 30;
		Damage = 26.f;
		FireRate = 10.0f; // 600 RPM tactical precision
		ImpactImpulse = 9000.f;
		SpreadHip = 1.6f;
		SpreadAim = 0.25f;
		SpreadPerShot = 0.22f;
		RecoilPitch = 0.62f;
		RecoilYaw = 0.28f;
	}
	ArmT = 0.f;
	ReloadT = -1.f;
	ReloadStage = 0;
	RebuildWeaponModel();
	PlaySound(EquipSound, Char() ? Char()->GetActorLocation() : FVector::ZeroVector, 0.75f, NewType == EAAAWeaponType::Pistol ? 1.28f : (NewType == EAAAWeaponType::LMG ? 0.82f : 1.05f));
}

void UAAAWeaponComponent::EquipWeaponByDataAsset(UAAAWeaponDataAsset* NewWeaponData)
{
	if (!NewWeaponData) return;
	if (bBinocularsActive) SetBinoculars(false);

	CurrentWeaponData = NewWeaponData;
	switch (NewWeaponData->Category)
	{
	case EAAAWeaponCategory::Pistol:
		CurrentWeaponType = EAAAWeaponType::Pistol;
		break;
	case EAAAWeaponCategory::LMG:
		CurrentWeaponType = EAAAWeaponType::LMG;
		break;
	default:
		CurrentWeaponType = EAAAWeaponType::Rifle;
		break;
	}

	Damage = NewWeaponData->Damage;
	FireRate = NewWeaponData->FireRate;
	MagazineSize = NewWeaponData->MagazineSize;
	Ammo = FMath::Clamp(Ammo, 0, MagazineSize);
	if (Ammo == 0) Ammo = MagazineSize;
	ReserveAmmo = NewWeaponData->DefaultReserveAmmo;
	MuzzleVelocity = NewWeaponData->MuzzleVelocity;
	BulletGravity = NewWeaponData->BulletGravity;
	ImpactImpulse = NewWeaponData->ImpactImpulse;
	RecoilPitch = NewWeaponData->RecoilPitch;
	RecoilYaw = NewWeaponData->RecoilYaw;
	RecoilRecovery = NewWeaponData->RecoilRecovery;
	SpreadHip = NewWeaponData->SpreadHip;
	SpreadAim = NewWeaponData->SpreadAim;
	SpreadPerShot = NewWeaponData->SpreadPerShot;
	SpreadMax = NewWeaponData->SpreadMax;
	GripOffset = NewWeaponData->GripOffset;
	GripRotation = NewWeaponData->GripRotation;
	MuzzleLocal = NewWeaponData->MuzzleLocation;
	EjectLocal = NewWeaponData->EjectionLocation;

	if (NewWeaponData->ShotSound) ShotSound = NewWeaponData->ShotSound;
	if (NewWeaponData->ReloadSound) ReloadSound = NewWeaponData->ReloadSound;
	if (NewWeaponData->MagOutSound) MagOutSound = NewWeaponData->MagOutSound;
	if (NewWeaponData->MagInSound) MagInSound = NewWeaponData->MagInSound;
	if (NewWeaponData->BoltSound) BoltSound = NewWeaponData->BoltSound;
	if (NewWeaponData->DrySound) DrySound = NewWeaponData->DrySound;

	ArmT = 0.f;
	ReloadT = -1.f;
	ReloadStage = 0;
	RebuildWeaponModel();
	PlaySound(EquipSound, Char() ? Char()->GetActorLocation() : FVector::ZeroVector, 0.75f, 1.0f);

	if (OnWeaponSwitched.IsBound())
	{
		OnWeaponSwitched.Broadcast(CurrentWeaponData);
	}
	if (OnAmmoChanged.IsBound())
	{
		OnAmmoChanged.Broadcast(Ammo, ReserveAmmo);
	}
}

void UAAAWeaponComponent::EquipWeaponByIndex(int32 WeaponIndex)
{
	if (AvailableWeapons.IsValidIndex(WeaponIndex) && AvailableWeapons[WeaponIndex])
	{
		EquipWeaponByDataAsset(AvailableWeapons[WeaponIndex]);
	}
}

void UAAAWeaponComponent::AddWeaponDataAsset(UAAAWeaponDataAsset* NewWeapon)
{
	if (NewWeapon && !AvailableWeapons.Contains(NewWeapon))
	{
		AvailableWeapons.Add(NewWeapon);
	}
}

void UAAAWeaponComponent::TriggerFire()
{
	if (APlayerController* PC = ControllingPC())
	{
		FireShot(PC);
	}
}

void UAAAWeaponComponent::TriggerReload()
{
	const bool bCanReload = Ammo < MagazineSize && (bInfiniteReserve || ReserveAmmo > 0) && bRifleAttached;
	if (ReloadT < 0.f && bCanReload)
	{
		ReloadT = 0.f;
		ReloadStage = 0;
		if (!MagOutSound)
		{
			const float RL = Anim ? Anim->GetReloadLength() : 2.2f;
			PlaySound(ReloadSound, Char() ? Char()->GetActorLocation() : FVector::ZeroVector, 0.8f, 2.2f / FMath::Max(RL, 0.5f));
		}
	}
}

/* ============================================================================================ HUD */

void UAAAWeaponComponent::DrawHud(UCanvas* Cv, APlayerController* /*PC*/)
{
	if (!Cv || !bHudOn || !GEngine) return;

	// When Smart UMG Widget is active, it handles the AAA HUD rendering cleanly
	if (SmartHudWidget)
	{
		return;
	}
	const float Wd = (float)Cv->SizeX, Ht = (float)Cv->SizeY, S = Ht / 1080.f;
	const float Cx = Wd * 0.5f, Cy = Ht * 0.5f;
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	auto Rect = [Cv](float X, float Y, float W, float H, const FLinearColor& C) { FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), C); T.BlendMode = SE_BLEND_Translucent; Cv->DrawItem(T); };
	auto Line = [Cv](float X0, float Y0, float X1, float Y1, float Th, const FLinearColor& C) { Cv->K2_DrawLine(FVector2D(X0, Y0), FVector2D(X1, Y1), Th, C); };
	auto Text = [Cv](UFont* F, const FString& T, float X, float Y, float Sc, const FLinearColor& C, bool bCenter)
	{
		Cv->K2_DrawText(F, T, FVector2D(X, Y), FVector2D(Sc, Sc), C, 0.f, FLinearColor(0.f, 0.f, 0.f, 0.7f), FVector2D(1.0, 1.0), bCenter, bCenter, false, FLinearColor::Black);
	};

	// hurt feedback is handled dynamically via AAA Post-Process Vignette, Adrenaline Desaturation & Heartbeat pulse
	// explosion exposure flash: blinding white-orange screen flare
	if (ExposureFlashT > 0.01f)
	{
		const FLinearColor ExposureCol(1.0f, 0.92f, 0.72f, FMath::Min(0.85f, ExposureFlashT * 0.9f));
		Rect(0.f, 0.f, Wd, Ht, ExposureCol);
	}
	if (bDead)
	{
		Rect(0.f, 0.f, Wd, Ht, FLinearColor(0.f, 0.f, 0.f, FMath::Min(0.55f, DeadT * 0.25f)));
		// (no "WASTED": the hero's death is the shahada scene - AAAMissionSubsystem)
		return;
	}

	// health bar (bottom left)
	const float Bx = 40.f * S, By = Ht - 60.f * S, Bw = 240.f * S, Bh = 12.f * S;
	const float Hf = FMath::Clamp(Health / FMath::Max(MaxHealth, 1.f), 0.f, 1.f);
	Rect(Bx - 2.f * S, By - 2.f * S, Bw + 4.f * S, Bh + 4.f * S, FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Rect(Bx, By, Bw * Hf, Bh, FLinearColor(FMath::Lerp(0.85f, 0.25f, Hf), FMath::Lerp(0.15f, 0.75f, Hf), 0.2f, 0.9f));

	if (HelpT > 0.f) Text(Mid, TEXT("1 rifle   RMB aim   LMB fire   R reload   G grenade   T training targets"), Cx, Ht - 34.f * S, 1.1f * S, FLinearColor(1.f, 1.f, 1.f, FMath::Min(1.f, HelpT)), true);
	if (Score > 0) Text(Mid, FString::Printf(TEXT("Targets  %d"), Score), Wd - 40.f * S, 40.f * S, 1.4f * S, FLinearColor(1.f, 0.85f, 0.3f), false);

	// grenade HUD indicator (bottom right above ammo)
	const FString GrenadeTxt = FString::Printf(TEXT("[G] GRENADES  %d"), Grenades);
	Text(Mid, GrenadeTxt, Wd - 190.f * S, Ht - 110.f * S, 1.15f * S, Grenades > 0 ? FLinearColor(0.4f, 0.95f, 0.45f) : FLinearColor(0.6f, 0.6f, 0.6f), false);

	// grenade cooking indicator
	if (bCookingGrenade)
	{
		const float Rem = FMath::Max(0.f, 3.2f - CookTimer);
		const bool bFlashCook = Rem < 1.0f && FMath::Sin((float)GetWorld()->GetTimeSeconds() * 16.f) > 0.f;
		Text(Big, FString::Printf(TEXT("COOKING: %.1fs"), Rem), Cx, Cy + 80.f * S, 1.4f * S, bFlashCook ? FLinearColor(1.f, 0.1f, 0.1f) : FLinearColor(1.f, 0.8f, 0.2f), true);
	}

	// incoming grenade danger warning if near player
	if (ACharacter* C = Char())
	{
		const FVector PlayerLoc = C->GetActorLocation();
		float ClosestDist = 1e9f;
		for (TActorIterator<AAAAGrenade> It(GetWorld()); It; ++It)
		{
			if (*It && !It->IsExploded())
			{
				const float D = (float)FVector::Dist(PlayerLoc, It->GetActorLocation());
				if (D < ClosestDist) ClosestDist = D;
			}
		}
		if (ClosestDist < 1200.f)
		{
			const float Pulse = 0.7f + 0.3f * FMath::Sin((float)GetWorld()->GetTimeSeconds() * 10.f);
			Text(Big, FString::Printf(TEXT("[ ! ] GRENADE (%.1fm)"), ClosestDist / 100.f), Cx, Cy - 90.f * S, 1.5f * S, FLinearColor(1.f, 0.15f, 0.1f, Pulse), true);
		}
	}
	if (!bArmed) return;

	// ammo (bottom right)
	const FString AmmoT = bInfiniteReserve ? FString::Printf(TEXT("%d / --"), Ammo) : FString::Printf(TEXT("%d / %d"), Ammo, ReserveAmmo);
	Text(Big, AmmoT, Wd - 190.f * S, Ht - 78.f * S, 1.6f * S, Ammo > 0 ? FLinearColor::White : FLinearColor(1.f, 0.3f, 0.2f), false);
	if (ReloadT >= 0.f)
	{
		const float RL = Anim ? Anim->GetReloadLength() : 2.2f;
		Rect(Cx - 60.f * S, Cy + 40.f * S, 120.f * S, 5.f * S, FLinearColor(0.f, 0.f, 0.f, 0.5f));
		Rect(Cx - 60.f * S, Cy + 40.f * S, 120.f * S * FMath::Clamp(ReloadT / RL, 0.f, 1.f), 5.f * S, FLinearColor(1.f, 1.f, 1.f, 0.85f));
		Text(Mid, TEXT("RELOADING"), Cx, Cy + 58.f * S, 1.f * S, FLinearColor(1.f, 1.f, 1.f, 0.85f), true);
	}

	// crosshair: the gap shows the current spread
	const float Cone = (bAiming ? SpreadAim : SpreadHip) + Spread;
	const float Fov = Camera.IsValid() ? Camera->FieldOfView : 90.f;
	const float Gap = FMath::Max(3.f * S, FMath::Tan(FMath::DegreesToRadians(Cone)) / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)) * Wd * 0.5f);
	const float L = 9.f * S, Th = 2.f * S;
	const FLinearColor Cc(1.f, 1.f, 1.f, 0.9f);
	Line(Cx - Gap - L, Cy, Cx - Gap, Cy, Th, Cc); Line(Cx + Gap, Cy, Cx + Gap + L, Cy, Th, Cc);
	Line(Cx, Cy - Gap - L, Cx, Cy - Gap, Th, Cc); Line(Cx, Cy + Gap, Cx, Cy + Gap + L, Th, Cc);
	Rect(Cx - 1.f * S, Cy - 1.f * S, 2.f * S, 2.f * S, Cc);
	if (HitMarkT > 0.f)
	{
		const FLinearColor Hm(1.f, 0.25f, 0.2f, 1.f);
		const float A0 = 6.f * S, A1 = 14.f * S;
		Line(Cx - A1, Cy - A1, Cx - A0, Cy - A0, Th, Hm); Line(Cx + A0, Cy + A0, Cx + A1, Cy + A1, Th, Hm);
		Line(Cx + A1, Cy - A1, Cx + A0, Cy - A0, Th, Hm); Line(Cx - A0, Cy + A0, Cx - A1, Cy + A1, Th, Hm);
	}
	if (PointsT > 0.f) Text(Big, FString::Printf(TEXT("+%d"), LastPoints), Cx + 40.f * S, Cy - 50.f * S - (1.2f - PointsT) * 30.f * S, 1.4f * S, FLinearColor(1.f, 0.85f, 0.3f, FMath::Min(1.f, PointsT * 2.f)), false);
}

/* ============================================================================================ crouch & cover */

void UAAAWeaponComponent::ScanCover(ACharacter* C, APlayerController* PC)
{
	// what is around the player at knee / chest / head height, in the directions he is looking: a low obstacle
	// (wall, car, planter, crate: hit low, clear high) within arm's reach is cover he can duck behind and fire over
	bLowCover = false;
	UWorld* W = GetWorld();
	const UCapsuleComponent* Cap = C->GetCapsuleComponent();
	if (!W || !Cap) return;
	const float Half = Cap->GetScaledCapsuleHalfHeight(), Rad = Cap->GetScaledCapsuleRadius();
	const FVector Base = C->GetActorLocation() - FVector(0.0, 0.0, Half);   // the feet
	const float CamYaw = PC->GetControlRotation().Yaw;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAACoverScan), false, C);
	float BestAng = 999.f;
	for (const float Off : { 0.f, -35.f, 35.f, -70.f, 70.f, 180.f })
	{
		const FVector Dir = FRotator(0.0, CamYaw + Off, 0.0).Vector();
		const double Reach = Rad + 62.0;
		FHitResult Lo, Hi;
		if (!W->LineTraceSingleByChannel(Lo, Base + FVector(0.0, 0.0, 50.0), Base + FVector(0.0, 0.0, 50.0) + Dir * Reach, ECC_Visibility, Q)) continue;
		if (Cast<APawn>(Lo.GetActor())) continue;
		if (W->LineTraceSingleByChannel(Hi, Base + FVector(0.0, 0.0, 152.0), Base + FVector(0.0, 0.0, 152.0) + Dir * (Reach + 20.0), ECC_Visibility, Q)) continue;   // a wall, not low cover
		// its top: down from above, just behind the face we hit
		FHitResult Top;
		const FVector Over = Lo.ImpactPoint + Dir * 10.0;
		if (!W->LineTraceSingleByChannel(Top, FVector(Over.X, Over.Y, Base.Z + 175.0), FVector(Over.X, Over.Y, Base.Z + 35.0), ECC_Visibility, Q)) continue;
		const float H = (float)(Top.ImpactPoint.Z - Base.Z);
		if (H < 55.f || H > 150.f) continue;
		// prefer cover in front of the camera; behind him only counts when standing still (back to the wall)
		const float Ang = FMath::Abs(Off) + (Off > 100.f ? 60.f : 0.f);
		if (Ang < BestAng) { BestAng = Ang; bLowCover = true; CoverTopH = H; }
	}

	// leaning: aiming, and the line of fire from the shoulder is blocked right in front (a corner / wall edge)
	// while the same line a step to one side is clear -> lean out that way (the camera's shoulder side first)
	float LeanTo = 0.f;
	if (bArmed && bAiming)
	{
		const FRotator Ctl = PC->GetControlRotation();
		const FVector Fwd = Ctl.Vector();
		const FVector Right = FRotator(0.0, Ctl.Yaw, 0.0).RotateVector(FVector(0.0, 1.0, 0.0));
		const FVector Sh = Base + FVector(0.0, 0.0, FMath::Lerp(145.f, 105.f, CrouchOut));
		FHitResult Hb;
		if (W->LineTraceSingleByChannel(Hb, Sh, Sh + Fwd * 150.0, ECC_Visibility, Q) && !Cast<APawn>(Hb.GetActor()))
		{
			for (const float Sd : { 1.f, -1.f })
			{
				const FVector S2 = Sh + Right * (double)(55.f * Sd);
				FHitResult Hs, Hp;
				if (W->LineTraceSingleByChannel(Hp, Sh, S2, ECC_Visibility, Q)) continue;                 // no room to lean that way
				if (!W->LineTraceSingleByChannel(Hs, S2, S2 + Fwd * 400.0, ECC_Visibility, Q)) { LeanTo = Sd; break; }
			}
		}
	}
	LeanOut = LeanTo;
}

void UAAAWeaponComponent::UpdateCrouchCover(ACharacter* C, APlayerController* PC, float Dt, bool bInputOk)
{
	UCharacterMovementComponent* CM = C->GetCharacterMovement();
	if (!CM) return;
	if (!bCrouchInit)
	{
		bCrouchInit = true;
		CM->GetNavAgentPropertiesRef().bCanCrouch = true;
		CM->SetCrouchedHalfHeight(58.f);
		CM->bCanWalkOffLedgesWhenCrouching = true;
	}
	const bool bClimbingNow = ClimbState.bMantling || ClimbState.ClimbWeight > 0.05f;
	const bool bFalling = CM->IsFalling();
	const float Speed = (float)C->GetVelocity().Size2D();
	const bool bShift = bInputOk && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));

	// C: crouch on / off. Sprinting or jumping stands him up (and keeps the auto cover off for a moment)
	if (bInputOk && PC->WasInputKeyJustPressed(EKeys::C)) { bCrouchToggle = !bCrouchToggle; CoverOffT = bCrouchToggle ? 0.f : 0.9f; }
	if (bShift && Speed > 150.f) { bCrouchToggle = false; CoverOffT = 0.6f; }
	if (bInputOk && PC->WasInputKeyJustPressed(EKeys::SpaceBar) && (C->bIsCrouched || CrouchOut > 0.3f)) { bCrouchToggle = false; CoverOffT = 0.9f; }
	CoverOffT = FMath::Max(0.f, CoverOffT - Dt);
	StillT = Speed < 40.f ? StillT + Dt : 0.f;

	if ((CoverScanT -= Dt) <= 0.f) { CoverScanT = 0.08f; ScanCover(C, PC); }

	// smart cover: armed, slow (not sprinting), low cover in reach -> duck behind it. When he aims, he rises
	// only as far as the sights need to clear the top (crouched eyes ~115 cm, standing ~160 cm)
	const bool bSlow = Speed < 210.f && !bShift;
	const bool bAuto = bArmed && bLowCover && bSlow && CoverOffT <= 0.f && (Speed > 5.f || StillT > 0.15f);
	bInCover = bLowCover && (bAuto || bCrouchToggle);
	float Target = (bCrouchToggle || bAuto) ? 1.f : 0.f;
	if (bArmed && bAiming && bInCover)
	{
		const float EyeNeed = CoverTopH + 24.f;
		Target = FMath::Clamp((160.f - EyeNeed) / 45.f, 0.f, 1.f);
	}
	if (bClimbingNow || bFalling || bDead || bBinocularsActive) Target = 0.f;
	if (bClimbingNow || bDead) LeanOut = 0.f;
	CrouchOut = Target;

	// the collision capsule follows with a little hysteresis (short capsule = bullets and eyes pass over him)
	if (CrouchOut > 0.6f && !C->bIsCrouched && !bFalling) C->Crouch();
	else if (CrouchOut < 0.4f && C->bIsCrouched) C->UnCrouch();
}
