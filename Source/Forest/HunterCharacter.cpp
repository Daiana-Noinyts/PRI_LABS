#include "HunterCharacter.h"
#include "Projectile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "UObject/UObjectIterator.h"
#include "Components/ProgressBar.h"
#include "Components/PanelWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "ForestVisuals.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "Animation/AnimMontage.h"
#include "Engine/StaticMeshActor.h"
#include "TimerManager.h"

namespace HunterReload
{
	// Moments of the reload montage (MM_Rifle_Reload, seconds from its start) where the left hand
	// reaches the magazine, throws it, takes a new one, inserts it and lets go. The magazine follows
	// the hand between these moments, so it is always in the hand while the hand works it.
	float StageTimes[] = {
		0.30f,  // TakeMagazine:           the hand grabs the magazine in the gun
		0.50f,  // ThrowMagazine:          the hand swings away and drops the empty magazine
		0.62f,  // TakeMagazineFromPocket: the hand is at the pocket, takes a new magazine
		1.07f,  // InsertMagazine:         the hand is back at the gun and pushes it in
		1.85f   // EndReloading:           the hand is back on the grip, the rig goes back
	};
	constexpr int32 NumStages = UE_ARRAY_COUNT(StageTimes);
	constexpr float AnchorBlendTime = 0.30f;    // blend in/out of keeping the hands framed
	constexpr float MontageRaiseTime = 0.30f;   // blend back to the rest pose after the hand is on the grip again

	// The magazine in the gun hangs straight down out of the magazine well. When the left hand grabs it,
	// it slides out of the well into the palm, keeps the angle at which the hand took it, and the new
	// magazine slides from the palm back into the well the same way.
	// Magazine well of the shotgun mesh (gun space): under the receiver, just in front of the trigger
	// guard, with the top of the 17 cm magazine going into the gun
	const FVector MagazineWellLocal(-12.0f, 0.9f, -11.0f);
	constexpr float GrabBlendTime = 0.12f;      // well -> palm after the hand grabs the magazine
	constexpr float InsertBlendTime = 0.18f;    // palm -> well just before it is inserted
	FTransform HeldInHand = FTransform::Identity; // magazine relative to the left hand bone while it is held

	// Pivot of the tilt in the shell-loading fallback (camera space)
	FVector PivotInCamera = FVector::ZeroVector;

	// The camera the player actually sees through (on the head of the first-person body). During the
	// montage reload it is turned towards the gun and the hand, then handed back to the mouse.
	TWeakObjectPtr<UCameraComponent> ViewCamera;
	bool bViewCameraUsedControlRotation = true;
	FQuat ViewRotation = FQuat::Identity;
	constexpr float MaxLookYaw = 15.0f;          // the view only tilts down to the gun; sideways it turns at most this much
	constexpr float MinLookPitch = -55.0f;       // and never looks further down than this
	FVector AnchorOffset = FVector::ZeroVector;
	float AnchorWeight = 0.0f;

	float Ease(float A) { return FMath::InterpEaseInOut(0.0f, 1.0f, FMath::Clamp(A, 0.0f, 1.0f), 2.0f); }
	float EaseOut(float A) { return FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(A, 0.0f, 1.0f), 2.0f); }
}

// Sets default values
AHunterCharacter::AHunterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	check(CameraComponent != nullptr);
	CameraComponent->SetupAttachment(CastChecked<USceneComponent, UCapsuleComponent>(GetCapsuleComponent()));
	CameraComponent->bUsePawnControlRotation = true;

	GetMesh()->SetupAttachment(CameraComponent);

	GunMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMeshComponent"));
	GunMeshComponent->bCastDynamicShadow = false;
	GunMeshComponent->CastShadow = false;
	GunMeshComponent->SetupAttachment(GetMesh(), TEXT("HandGrip_l"));

	// Shotgun shell shown while reloading (built from engine cylinders, ~2.3 x 7 cm), placed in world space each frame
	ReloadShellRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ReloadShellRoot"));
	ReloadShellRoot->SetupAttachment(CameraComponent);
	ReloadShellBody = ForestVisuals::MakePart(this, ReloadShellRoot, TEXT("ReloadShellBody"),
		ForestVisuals::EShape::Cylinder, FVector(0.0f, 0.0f, 1.0f), FVector(0.022f, 0.022f, 0.055f));
	ReloadShellBase = ForestVisuals::MakePart(this, ReloadShellRoot, TEXT("ReloadShellBase"),
		ForestVisuals::EShape::Cylinder, FVector(0.0f, 0.0f, -2.6f), FVector(0.024f, 0.024f, 0.016f));
	ReloadShellBody->SetCastShadow(false);
	ReloadShellBase->SetCastShadow(false);
	ReloadShellRoot->SetVisibility(false, true);

	// Box magazine under the gun (8 x 3 x 17 cm), and a second one used in the left hand during the reload
	GunMagazine = ForestVisuals::MakePart(this, GunMeshComponent, TEXT("GunMagazine"),
		ForestVisuals::EShape::Cube, HunterReload::MagazineWellLocal, FVector(0.08f, 0.03f, 0.17f));
	GunMagazine->SetCastShadow(false);

	HandMagazine = ForestVisuals::MakePart(this, GetMesh(), TEXT("HandMagazine"),
		ForestVisuals::EShape::Cube, FVector::ZeroVector, FVector(0.08f, 0.03f, 0.17f));
	HandMagazine->SetupAttachment(GetMesh(), HandMagazineBone);
	HandMagazine->SetCastShadow(false);
	HandMagazine->SetVisibility(false);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bEnablePhysicsInteraction = true;
		MoveComp->PushForceFactor = 750000.0f;
		MoveComp->InitialPushForceFactor = 500000.0f;
	}

	static ConstructorHelpers::FClassFinder<UUserWidget> HUDWidgetFinder(TEXT("/Game/UI/HUD_Widget"));
	if (HUDWidgetFinder.Succeeded())
	{
		HUDWidgetClass = HUDWidgetFinder.Class;
	}
}


// Called when the game starts or when spawned
void AHunterCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetMesh()->AttachToComponent(CameraComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	// Rest pose of the first-person rig: the reload animation always returns here
	WeaponRestLocation = GetMesh()->GetRelativeLocation();
	WeaponRestRotation = GetMesh()->GetRelativeRotation();
	ForestVisuals::ApplyColor(ReloadShellBody, FLinearColor(0.75f, 0.05f, 0.04f)); // red plastic hull
	ForestVisuals::ApplyColor(ReloadShellBase, FLinearColor(0.85f, 0.62f, 0.2f));  // brass head
	ForestVisuals::ApplyColor(GunMagazine, MagazineColor);
	ForestVisuals::ApplyColor(HandMagazine, MagazineColor);
	// Remember where the magazine was placed in the Blueprint; every reload puts it back exactly there
	GunMagazineRest = GunMagazine->GetRelativeTransform();

	// Left-hand magazine placement (editable on the Blueprint)
	HandMagazine->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, HandMagazineBone);
	HandMagazine->SetRelativeLocationAndRotation(HandMagazineOffset, HandMagazineRotation);

	// The idle pose the first-person mesh returns to after the reload montage
	IdleAnimation = GetMesh()->AnimationData.AnimToPlay;

	if (HUDWidgetClass && !HUDWidgetInstance)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC && PC->IsLocalController())
		{
			HUDWidgetInstance = CreateWidget<UUserWidget>(PC, HUDWidgetClass);
			if (HUDWidgetInstance)
			{
				HUDWidgetInstance->AddToViewport();
			}
		}
	}

	// Auto-detect max bullets from BulletsBox if present
	TArray<UUserWidget*> Widgets = GetAllHUDWidgets();
	for (UUserWidget* Widget : Widgets)
	{
		if (!Widget) continue;
		if (UPanelWidget* BulletsPanel = Cast<UPanelWidget>(Widget->GetWidgetFromName(FName("BulletsBox"))))
		{
			const int32 NumChildren = BulletsPanel->GetChildrenCount();
			if (NumChildren > 0)
			{
				MaxBullets = NumChildren;
				BulletCounter = NumChildren;
				break;
			}
		}
	}

	HealthCounter = MaxHealth;
	BulletCounter = FMath::Clamp(BulletCounter, 0, MaxBullets);

	UpdateHUD();
	OnStatsChanged();
}


// Called every frame
void AHunterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsReloading)
	{
		if (bMontageReload)
		{
			UpdateMontageReload(DeltaTime);
		}
		else
		{
			UpdateReload(DeltaTime);
		}
	}
}

void AHunterCharacter::ReloadWithAmmo(int32 Amount)
{
	// Ammo only comes from pickups: the reload animation loads exactly this many bullets (up to the max)
	if (Amount <= 0)
	{
		return;
	}

	if (bIsReloading)
	{
		// Picked up more ammo mid-reload: it goes into the magazine being loaded
		ReloadAmmoToAdd += Amount;
		return;
	}

	const int32 ToLoad = FMath::Min(Amount, MaxBullets - BulletCounter);
	if (ToLoad <= 0)
	{
		return;
	}

	bIsReloading = true;
	ReloadElapsed = 0.0f;
	ReloadInserts = FMath::Clamp(ToLoad, 1, FMath::Max(1, MaxVisibleRounds));
	ReloadInsertsDone = 0;
	ReloadAmmoToAdd = ToLoad;

	if (ReloadMontage)
	{
		// Montage reload: the left hand really takes the magazine out, throws it, takes a new one and
		// inserts it; the magazine is carried by the hand between those moments
		bMontageReload = true;
		bRaising = false;
		RaiseElapsed = 0.0f;
		StagesDone = 0;
		HunterReload::AnchorOffset = FVector::ZeroVector;
		HunterReload::AnchorWeight = 0.0f;
		GunMagazine->SetRelativeTransform(GunMagazineRest);
		GunMagazine->SetVisibility(true);
		HandMagazine->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, HandMagazineBone);
		HandMagazine->SetRelativeLocationAndRotation(HandMagazineOffset, HandMagazineRotation);
		HandMagazine->SetVisibility(false);
		// Both magazines (in the gun / in the hand) have the same colour
		if (UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			for (UStaticMeshComponent* Magazine : { GunMagazine, HandMagazine })
			{
				Magazine->SetMaterial(0, ShapeMaterial);
				ForestVisuals::ApplyColor(Magazine, MagazineColor);
			}
		}

		// The left hand leaves the gun, so the right hand keeps holding it where it is now
		GunRestSocket = GunMeshComponent->GetAttachSocketName();
		GunRestRelative = GunMeshComponent->GetRelativeTransform();
		if (GetMesh()->DoesSocketExist(TEXT("hand_r")))
		{
			GunMeshComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
		}
		GetMesh()->PlayAnimation(ReloadMontage, false);

		// The camera the player sees through (the active one, on the head): it is turned towards the
		// gun during the reload instead of following the mouse
		HunterReload::ViewCamera = nullptr;
		TInlineComponentArray<UCameraComponent*> Cameras(this);
		for (UCameraComponent* Cam : Cameras)
		{
			if (Cam->IsActive())
			{
				HunterReload::ViewCamera = Cam;
				HunterReload::bViewCameraUsedControlRotation = Cam->bUsePawnControlRotation;
				Cam->bUsePawnControlRotation = false;
				HunterReload::ViewRotation = Cam->GetComponentQuat();
				break;
			}
		}
		return;
	}

	bMontageReload = false;
	PoseScale = 1.0f;

	// Work out how far the rig must move so that, with the lowered tilt applied, the loading port
	// sits at a fixed visible spot on screen (so the shells going in are always in view)
	GetMesh()->SetRelativeLocationAndRotation(WeaponRestLocation, WeaponRestRotation);
	HunterReload::PivotInCamera = CameraComponent->GetComponentTransform().InverseTransformPosition(GetReloadPortWorld());
	ReloadAutoOffset = (ReloadPortScreenTarget - HunterReload::PivotInCamera).GetClampedToMaxSize(45.0f);
}

FVector AHunterCharacter::GetReloadPortWorld() const
{
	if (GunMeshComponent && GunMeshComponent->GetStaticMesh())
	{
		return GunMeshComponent->GetComponentTransform().TransformPosition(ReloadPortLocal);
	}
	// No gun mesh: a point in front of the camera
	return CameraComponent->GetComponentTransform().TransformPosition(ReloadPortScreenTarget);
}

void AHunterCharacter::ShowShell(bool bVisible)
{
	ReloadShellRoot->SetVisibility(bVisible, true);
}

void AHunterCharacter::SetWeaponPose(float LowerAlpha, float ExtraPitch)
{
	// Blend between the rest pose and the lowered/tilted pose (relative to the camera).
	// The tilt turns the rig around the pivot (the magazine / loading port), not around the body root,
	// so the gun stays in front of the camera while it is tilted
	const FVector Pivot = HunterReload::PivotInCamera;
	const FQuat Tilt = (ReloadLoweredRotation * (LowerAlpha * PoseScale) + FRotator(ExtraPitch, 0.0f, 0.0f)).Quaternion();
	const FVector Location = Pivot + Tilt.RotateVector(WeaponRestLocation - Pivot) + (ReloadAutoOffset + ReloadLoweredOffset) * LowerAlpha;
	const FQuat Rotation = Tilt * WeaponRestRotation.Quaternion();
	GetMesh()->SetRelativeLocationAndRotation(Location, Rotation);
}

void AHunterCharacter::UpdateReload(float DeltaTime)
{
	ReloadElapsed += DeltaTime;

	const float LoadStart = ReloadLowerTime;
	const float LoadEnd = LoadStart + ReloadInserts * RoundInsertTime;
	const float TotalTime = LoadEnd + ReloadRaiseTime;

	// Puts the next shell into the gun and adds its share of the ammo
	auto LoadOneRound = [this]()
	{
		const int32 RoundsLeft = FMath::Max(1, ReloadInserts - ReloadInsertsDone);
		const int32 Amount = FMath::DivideAndRoundUp(ReloadAmmoToAdd, RoundsLeft);
		BulletCounter = FMath::Clamp(BulletCounter + Amount, 0, MaxBullets);
		ReloadAmmoToAdd -= Amount;
		ReloadInsertsDone++;
		UpdateHUD();
		OnStatsChanged();
	};

	if (ReloadElapsed < LoadStart)
	{
		// 1) Lower and tilt the weapon, bringing the loading port into view
		const float Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, ReloadElapsed / ReloadLowerTime, 2.0f);
		SetWeaponPose(Alpha, 0.0f);
		ShowShell(false);
	}
	else if (ReloadElapsed < LoadEnd)
	{
		// 2) Load shells one by one
		const float Local = ReloadElapsed - LoadStart;
		const int32 Index = FMath::Min(FMath::FloorToInt(Local / RoundInsertTime), ReloadInserts - 1);
		const float U = FMath::Clamp((Local - Index * RoundInsertTime) / RoundInsertTime, 0.0f, 1.0f);

		while (ReloadInsertsDone < Index)
		{
			LoadOneRound();
		}


		const FVector Up = CameraComponent->GetUpVector();
		const FVector Right = CameraComponent->GetRightVector();
		const FVector Port = GetReloadPortWorld();
		const FVector Start = Port - Up * 16.0f + Right * 5.0f;  // shell appears below the gun
		const FVector Inside = Port + Up * 4.0f;                  // pushed fully into the gun

		if (U < 0.85f)
		{
			FVector ShellLocation;
			float ShellScale = 1.0f;
			if (U < 0.15f)
			{
				// a) the shell appears (scales in) below the loading port
				ShellLocation = Start;
				ShellScale = FMath::InterpEaseOut(0.0f, 1.0f, U / 0.15f, 2.0f);
			}
			else if (U < 0.65f)
			{
				// b) it is brought up to the port
				ShellLocation = FMath::Lerp(Start, Port, FMath::InterpEaseInOut(0.0f, 1.0f, (U - 0.15f) / 0.5f, 2.0f));
			}
			else
			{
				// c) it is pushed into the port and disappears inside the gun
				ShellLocation = FMath::Lerp(Port, Inside, (U - 0.65f) / 0.2f);
			}

			// Shell axis points along the insertion direction, slightly tilted like a thumb push
			const FRotator ShellRotation = FRotationMatrix::MakeFromZ((Up * 4.0f - Right).GetSafeNormal()).Rotator();
			ReloadShellRoot->SetWorldLocationAndRotation(ShellLocation, ShellRotation);
			ReloadShellRoot->SetWorldScale3D(FVector(ShellScale));
			ShowShell(true);
			SetWeaponPose(1.0f, 0.0f);
		}
		else
		{
			// d) the shell is in: count the ammo, and the gun jolts as it seats
			if (ReloadInsertsDone == Index)
			{
				LoadOneRound();
			}
			ShowShell(false);
			const float Jolt = FMath::Sin(PI * (U - 0.85f) / 0.15f) * 3.0f;
			SetWeaponPose(1.0f, Jolt);
		}
	}
	else if (ReloadElapsed < TotalTime)
	{
		// 3) Raise the weapon back to the starting position
		while (ReloadInsertsDone < ReloadInserts)
		{
			LoadOneRound();
		}
		ShowShell(false);
		const float Alpha = 1.0f - FMath::InterpEaseInOut(0.0f, 1.0f, (ReloadElapsed - LoadEnd) / ReloadRaiseTime, 2.0f);
		SetWeaponPose(Alpha, 0.0f);
	}
	else
	{
		FinishReload();
	}
}

void AHunterCharacter::FinishReload()
{
	bIsReloading = false;
	ShowShell(false);
	SetWeaponPose(0.0f, 0.0f);

	if (bMontageReload)
	{
		for (FTimerHandle& Handle : StageTimers)
		{
			GetWorldTimerManager().ClearTimer(Handle);
		}
		StageTimers.Reset();
		HandMagazine->SetVisibility(false);
		GunMagazine->SetVisibility(true);
		bMontageReload = false;
		HunterReload::AnchorOffset = FVector::ZeroVector;
		HunterReload::AnchorWeight = 0.0f;
		GetMesh()->SetRelativeLocationAndRotation(WeaponRestLocation, WeaponRestRotation);

		// The view follows the mouse again
		if (UCameraComponent* ViewCam = HunterReload::ViewCamera.Get())
		{
			ViewCam->bUsePawnControlRotation = HunterReload::bViewCameraUsedControlRotation;
			ViewCam->SetWorldRotation(GetControlRotation());
		}
		HunterReload::ViewCamera = nullptr;

		// Back to the idle pose, gun back in its original grip
		if (IdleAnimation)
		{
			GetMesh()->PlayAnimation(IdleAnimation, true);
		}
		GunMeshComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, GunRestSocket);
		GunMeshComponent->SetRelativeTransform(GunRestRelative);
	}

	// Whatever was picked up and not loaded yet goes in now (never more than the max)
	BulletCounter = FMath::Clamp(BulletCounter + ReloadAmmoToAdd, 0, MaxBullets);
	ReloadAmmoToAdd = 0;
	UpdateHUD();
	OnStatsChanged();
}

void AHunterCharacter::HandleReloadStage(EHunterReloadStage Stage)
{
	if (!bIsReloading || !bMontageReload)
	{
		return;
	}

	// Run every stage up to this one exactly once, in order (a missed notify can't break the sequence)
	for (uint8 S = 0; S <= static_cast<uint8>(Stage); ++S)
	{
		const uint8 Bit = 1 << S;
		if (!(StagesDone & Bit))
		{
			StagesDone |= Bit;
			RunReloadStage(static_cast<EHunterReloadStage>(S));
		}
	}
}

void AHunterCharacter::RunReloadStage(EHunterReloadStage Stage)
{
	FString Text;
	switch (Stage)
	{
	case EHunterReloadStage::TakeMagazine:
		// The left hand grabs the magazine: from now on it is in the hand, not in the gun. It is held in the
		// middle of the palm, at the angle the hand took it at; UpdateMontageReload moves it there.
		{
			const FTransform Hand = GetMesh()->GetSocketTransform(HandMagazineBone);
			FVector Palm = Hand.GetLocation();
			if (GetMesh()->DoesSocketExist(TEXT("middle_01_l")))
			{
				Palm = FMath::Lerp(Palm, GetMesh()->GetSocketLocation(TEXT("middle_01_l")), 0.6f);
			}
			const FTransform Well = GunMagazine->GetComponentTransform();
			HunterReload::HeldInHand = FTransform(Well.GetRotation(), Palm, Well.GetScale3D()).GetRelativeTransform(Hand);
		}
		HandMagazine->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepWorldTransform);
		HandMagazine->SetWorldTransform(GunMagazine->GetComponentTransform());
		GunMagazine->SetVisibility(false);
		HandMagazine->SetVisibility(true);
		Text = TEXT("Magazine out");
		break;

	case EHunterReloadStage::ThrowMagazine:
		// The hand drops the empty magazine
		DropOldMagazine();
		HandMagazine->SetVisibility(false);
		Text = TEXT("Empty magazine thrown");
		break;

	case EHunterReloadStage::TakeMagazineFromPocket:
		// The hand takes a full magazine from the pocket
		HandMagazine->SetVisibility(true);
		PocketStageTime = ReloadElapsed;
		Text = TEXT("New magazine from pocket");
		break;

	case EHunterReloadStage::InsertMagazine:
		// The hand pushes it into the gun: the picked-up ammunition is loaded
		HandMagazine->SetVisibility(false);
		GunMagazine->SetVisibility(true);
		BulletCounter = FMath::Clamp(BulletCounter + ReloadAmmoToAdd, 0, MaxBullets);
		ReloadAmmoToAdd = 0;
		UpdateHUD();
		OnStatsChanged();
		Text = FString::Printf(TEXT("Magazine inserted - ammo %d/%d"), BulletCounter, MaxBullets);
		break;

	case EHunterReloadStage::EndReloading:
		// Raise the weapon back to the starting position
		bRaising = true;
		RaiseElapsed = 0.0f;
		Text = TEXT("Raising weapon");
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("[Reload] %s"), *Text);
}

void AHunterCharacter::DropOldMagazine()
{
	// The thrown magazine becomes a physics object: tossed down and to the side so it leaves the view
	// quickly instead of flying across the camera, then disappears after a while
	const FTransform From = HandMagazine->GetComponentTransform();
	AStaticMeshActor* Dropped = GetWorld()->SpawnActor<AStaticMeshActor>(From.GetLocation(), From.Rotator());
	if (!Dropped)
	{
		return;
	}

	UStaticMeshComponent* DropMesh = Dropped->GetStaticMeshComponent();
	DropMesh->SetMobility(EComponentMobility::Movable);
	DropMesh->SetStaticMesh(GunMagazine->GetStaticMesh());
	DropMesh->SetWorldScale3D(GunMagazine->GetComponentScale());
	DropMesh->SetMaterial(0, GunMagazine->GetMaterial(0));
	DropMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	DropMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	DropMesh->SetSimulatePhysics(true);
	DropMesh->AddImpulse(CameraComponent->GetRightVector() * 90.0f + CameraComponent->GetUpVector() * -120.0f + CameraComponent->GetForwardVector() * 40.0f, NAME_None, true);
	DropMesh->AddAngularImpulseInDegrees(FVector(300.0f, 150.0f, 0.0f), NAME_None, true);
	Dropped->SetLifeSpan(8.0f);
}

void AHunterCharacter::UpdateMontageReload(float DeltaTime)
{
	using namespace HunterReload;

	ReloadElapsed += DeltaTime;
	const float T = ReloadElapsed;

	// 1) Stages at the montage moments where the left hand reaches or leaves the magazine
	for (int32 S = NumStages - 1; S >= 0; --S)
	{
		// Insertion is decided by where the hand is (below), not by a fixed time
		if (S == static_cast<int32>(EHunterReloadStage::InsertMagazine))
		{
			continue;
		}
		if (T >= StageTimes[S])
		{
			HandleReloadStage(static_cast<EHunterReloadStage>(S));
			break;
		}
	}
	if (!bIsReloading)
	{
		return;
	}

	// 2) The view camera sits on the head of the first-person body, and the rifle reload montage was made
	//    for a third-person view, so the hands work below the line of sight. Like in a first-person
	//    shooter, the view is turned down to the gun while the hand works the magazine, and back afterwards.
	if (!bRaising)
	{
		AnchorWeight = FMath::Min(1.0f, AnchorWeight + DeltaTime / AnchorBlendTime);
	}
	else
	{
		RaiseElapsed += DeltaTime;
		AnchorWeight = FMath::Max(0.0f, 1.0f - RaiseElapsed / MontageRaiseTime);
	}

	if (UCameraComponent* ViewCam = ViewCamera.Get())
	{
		// Tilt the view down to the magazine well (not to the moving hand, so it does not jerk around).
		// Sideways it turns only a little, so the head never twists away from the gun.
		const FVector Focus = GunMagazine->GetComponentLocation();
		const FRotator Base = GetControlRotation();
		const FRotator LookAt = (Focus - ViewCam->GetComponentLocation()).Rotation();
		const float A = Ease(AnchorWeight);
		const float Pitch = FMath::Max(FMath::Lerp(Base.Pitch, LookAt.Pitch, A), FMath::Min(Base.Pitch, MinLookPitch));
		const float Yaw = Base.Yaw + FMath::Clamp(FRotator::NormalizeAxis(LookAt.Yaw - Base.Yaw), -MaxLookYaw, MaxLookYaw) * A;
		const FRotator Wanted(Pitch, Yaw, 0.0f);
		ViewRotation = FQuat::Slerp(ViewRotation, Wanted.Quaternion(), FMath::Min(1.0f, DeltaTime * 6.0f));
		FRotator Smoothed = ViewRotation.Rotator();
		Smoothed.Roll = 0.0f;
		ViewCam->SetWorldRotation(Smoothed);
	}

	// 3) The magazine held in the left hand: slides out of the well into the palm after the grab,
	//    stays in the palm, and slides from the palm back into the well just before it is inserted
	if (HandMagazine->IsVisible())
	{
		const FTransform Held = HeldInHand * GetMesh()->GetSocketTransform(HandMagazineBone);
		const FTransform Well = GunMagazine->GetComponentTransform();
		float ToWell = 0.0f;
		if (T < StageTimes[0] + GrabBlendTime)
		{
			ToWell = 1.0f - Ease((T - StageTimes[0]) / GrabBlendTime);
		}
		else if ((StagesDone & (1 << static_cast<uint8>(EHunterReloadStage::TakeMagazineFromPocket))) &&
			!(StagesDone & (1 << static_cast<uint8>(EHunterReloadStage::InsertMagazine))))
		{
			// The new magazine travels with the hand and slides into the well as the hand gets there;
			// the moment it arrives it is inserted
			const float Dist = FVector::Dist(Held.GetLocation(), Well.GetLocation());
			constexpr float ApproachRange = 20.0f;
			ToWell = Ease(FMath::Clamp(1.0f - (Dist - InsertDistance) / ApproachRange, 0.0f, 1.0f));
			if (Dist <= InsertDistance && (T - PocketStageTime) > 0.15f)
			{
				HandleReloadStage(EHunterReloadStage::InsertMagazine);
			}
		}
		if (HandMagazine->IsVisible())
		{
			FTransform Blended;
			Blended.Blend(Held, Well, ToWell);
			HandMagazine->SetWorldTransform(Blended);
		}
	}

	const float MontageLength = ReloadMontage ? ReloadMontage->GetPlayLength() : 2.2f;
	if ((bRaising && RaiseElapsed >= MontageRaiseTime) || T > MontageLength + 0.5f)
	{
		FinishReload();
	}
}

// Called to bind functionality to input
void AHunterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAxis("MoveForward", this, &AHunterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AHunterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &AHunterCharacter::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &AHunterCharacter::AddControllerPitchInput);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AHunterCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AHunterCharacter::StopJumping);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AHunterCharacter::Fire);

	// Manual reload
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AHunterCharacter::ReloadPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AHunterCharacter::ReloadPressed);
}

void AHunterCharacter::ReloadPressed()
{
	// Nothing to do while already reloading or when the magazine is full
	if (bIsReloading || BulletCounter >= MaxBullets)
	{
		return;
	}
	ReloadWithAmmo(MaxBullets - BulletCounter);
}

void AHunterCharacter::MoveForward(float value) {
	FVector direction = FRotationMatrix(Controller->GetControlRotation()).GetScaledAxis(EAxis::X);
	AddMovementInput(direction, value);
}

void AHunterCharacter::MoveRight(float value) {
	FVector direction = FRotationMatrix(Controller->GetControlRotation()).GetScaledAxis(EAxis::Y);
	AddMovementInput(direction, value);
}

void AHunterCharacter::Fire()
{
	if (bIsReloading)
	{
		return;
	}

	// Empty stays empty until ammo is picked up
	if (BulletCounter <= 0)
	{
		BulletCounter = 0;
		return;
	}

	if (ProjectileClass)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
		}
		else
		{
			CameraLocation = CameraComponent ? CameraComponent->GetComponentLocation() : GetPawnViewLocation();
			CameraRotation = GetViewRotation();
		}

		const float MaxAimDistance = 10000.f;
		FVector TraceEnd = CameraLocation + (CameraRotation.Vector() * MaxAimDistance);

		FHitResult Hit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FireTrace), true);
		QueryParams.AddIgnoredActor(this);
		QueryParams.bTraceComplex = true;

		// Default aim point if shooting into open sky (3000 units keeps trajectory well-behaved)
		FVector AimPoint = CameraLocation + (CameraRotation.Vector() * 3000.f);

		// 1. Prioritize Object Types so Pawns (enemies) and world geometry are both detected!
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

		if (GetWorld()->LineTraceSingleByObjectType(Hit, CameraLocation, TraceEnd, ObjectQueryParams, QueryParams))
		{
			AimPoint = Hit.ImpactPoint;
		}
		// 2. Fallback to Visibility channel (e.g. landscape or special static meshes)
		else if (GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Visibility, QueryParams))
		{
			AimPoint = Hit.ImpactPoint;
		}
		// 3. Fallback to Camera channel
		else if (GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Camera, QueryParams))
		{
			AimPoint = Hit.ImpactPoint;
		}

		FVector MuzzleLocation;
		if (GunMeshComponent && GunMeshComponent->DoesSocketExist(TEXT("Muzzle")))
		{
			MuzzleLocation = GunMeshComponent->GetSocketLocation(TEXT("Muzzle"));
		}
		else if (GetMesh() && GetMesh()->DoesSocketExist(TEXT("Muzzle")))
		{
			MuzzleLocation = GetMesh()->GetSocketLocation(TEXT("Muzzle"));
		}
		else if (GunMeshComponent)
		{
			MuzzleLocation = GunMeshComponent->GetComponentTransform().TransformPosition(MuzzleOffset);
		}
		else
		{
			const float DistToAim = FVector::Dist(CameraLocation, AimPoint);
			const float SafeForward = FMath::Clamp(MuzzleOffset.X, 10.f, FMath::Max(DistToAim - 20.f, 10.f));
			MuzzleLocation = CameraLocation + (CameraRotation.Vector() * SafeForward);
		}

		FVector LaunchDirection = (AimPoint - MuzzleLocation).GetSafeNormal();
		FRotator MuzzleRotation = LaunchDirection.Rotation();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AProjectile* Projectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, MuzzleLocation, MuzzleRotation, SpawnParams);
		if (Projectile)
		{
			const float DistanceToTarget = FVector::Dist(MuzzleLocation, AimPoint);
			Projectile->FireInDirection(LaunchDirection, DistanceToTarget);

			BulletCounter--;
			UpdateHUD();
			OnStatsChanged();

			OnFireEvent();
		}
	}
}

void AHunterCharacter::OnFireEvent_Implementation()
{

}

TArray<UUserWidget*> AHunterCharacter::GetAllHUDWidgets()
{
	TArray<UUserWidget*> FoundWidgets;
	if (HUDWidgetInstance)
	{
		FoundWidgets.AddUnique(HUDWidgetInstance);
	}

	if (FObjectProperty* Prop = FindFProperty<FObjectProperty>(GetClass(), FName("HUDInstance")))
	{
		if (UUserWidget* BPHud = Cast<UUserWidget>(Prop->GetObjectPropertyValue_InContainer(this)))
		{
			FoundWidgets.AddUnique(BPHud);
			if (!HUDWidgetInstance)
			{
				HUDWidgetInstance = BPHud;
			}
		}
	}

	if (FoundWidgets.Num() == 0 && GetWorld())
	{
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld() && It->IsInViewport() && It->GetClass()->GetName().Contains(TEXT("HUD_Widget")))
			{
				FoundWidgets.AddUnique(*It);
				if (!HUDWidgetInstance)
				{
					HUDWidgetInstance = *It;
				}
			}
		}
	}

	return FoundWidgets;
}

void AHunterCharacter::ApplyDamageDirect(float Amount)
{
	HealthCounter = FMath::Clamp(HealthCounter - Amount, 0.0f, MaxHealth);
	UpdateHUD();
	PlayDamageFlash();
	OnStatsChanged();
}

void AHunterCharacter::AddHealthDirect(float Amount)
{
	HealthCounter = FMath::Clamp(HealthCounter + Amount, 0.0f, MaxHealth);
	UpdateHUD();
	OnStatsChanged();
}

void AHunterCharacter::AddBulletsDirect(int32 Amount)
{
	// Gaining ammo always goes through the reload animation
	if (Amount > 0)
	{
		ReloadWithAmmo(Amount);
		return;
	}

	BulletCounter = FMath::Clamp(BulletCounter + Amount, 0, MaxBullets);
	UpdateHUD();
	OnStatsChanged();
}

void AHunterCharacter::TakeHit_Implementation(float Amount)
{
	PlayDamageFlash();
}

void AHunterCharacter::TakeDamageCustom(float DamageAmount)
{
	ApplyDamageDirect(DamageAmount);
}

void AHunterCharacter::UpdateHUD()
{
	TArray<UUserWidget*> Widgets = GetAllHUDWidgets();
	for (UUserWidget* Widget : Widgets)
	{
		if (!Widget) continue;

		if (UFunction* SetHealthFunc = Widget->FindFunction(FName("SetHealth")))
		{
			struct { double Health; } ParamsDouble = { (double)HealthCounter };
			struct { float Health; } ParamsFloat = { HealthCounter };

			if (FProperty* Prop = SetHealthFunc->FindPropertyByName(FName("Health")))
			{
				if (Prop->IsA<FDoubleProperty>())
				{
					Widget->ProcessEvent(SetHealthFunc, &ParamsDouble);
				}
				else
				{
					Widget->ProcessEvent(SetHealthFunc, &ParamsFloat);
				}
			}
			else
			{
				Widget->ProcessEvent(SetHealthFunc, &ParamsDouble);
			}
		}

		if (UFunction* SetBulletsFunc = Widget->FindFunction(FName("SetBullets")))
		{
			struct { int32 Bullets; } ParamsInt = { BulletCounter };
			struct { int64 Bullets; } ParamsInt64 = { BulletCounter };
			struct { double Bullets; } ParamsDouble = { (double)BulletCounter };

			if (FProperty* Prop = SetBulletsFunc->FindPropertyByName(FName("Bullets")))
			{
				if (Prop->IsA<FDoubleProperty>())
				{
					Widget->ProcessEvent(SetBulletsFunc, &ParamsDouble);
				}
				else if (Prop->IsA<FInt64Property>())
				{
					Widget->ProcessEvent(SetBulletsFunc, &ParamsInt64);
				}
				else
				{
					Widget->ProcessEvent(SetBulletsFunc, &ParamsInt);
				}
			}
			else if (FProperty* PropNum = SetBulletsFunc->FindPropertyByName(FName("BulletNum")))
			{
				if (PropNum->IsA<FDoubleProperty>())
				{
					Widget->ProcessEvent(SetBulletsFunc, &ParamsDouble);
				}
				else if (PropNum->IsA<FInt64Property>())
				{
					Widget->ProcessEvent(SetBulletsFunc, &ParamsInt64);
				}
				else
				{
					Widget->ProcessEvent(SetBulletsFunc, &ParamsInt);
				}
			}
			else
			{
				Widget->ProcessEvent(SetBulletsFunc, &ParamsInt);
			}
		}

		if (UProgressBar* BulletsBar = Cast<UProgressBar>(Widget->GetWidgetFromName(FName("BulletsProgressBar"))))
		{
			BulletsBar->SetPercent(FMath::Clamp((float)BulletCounter / (float)MaxBullets, 0.0f, 1.0f));
		}

		// Directly update BulletsBox (Horizontal Box or Wrap Box with bullet icons)
		if (UPanelWidget* BulletsPanel = Cast<UPanelWidget>(Widget->GetWidgetFromName(FName("BulletsBox"))))
		{
			const int32 NumChildren = BulletsPanel->GetChildrenCount();
			for (int32 i = 0; i < NumChildren; ++i)
			{
				if (UWidget* Child = BulletsPanel->GetChildAt(i))
				{
					Child->SetVisibility(i < BulletCounter ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
				}
			}
		}

		if (UProgressBar* HealthBar = Cast<UProgressBar>(Widget->GetWidgetFromName(FName("HealthProgressBar"))))
		{
			HealthBar->SetPercent(FMath::Clamp(HealthCounter / MaxHealth, 0.0f, 1.0f));
		}
	}
}

void AHunterCharacter::PlayDamageFlash()
{
	TArray<UUserWidget*> Widgets = GetAllHUDWidgets();
	for (UUserWidget* Widget : Widgets)
	{
		if (!Widget) continue;

		if (UFunction* DamageAnimFunc = Widget->FindFunction(FName("PlayDamageEffect")))
		{
			Widget->ProcessEvent(DamageAnimFunc, nullptr);
		}

		if (UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Widget->GetClass()))
		{
			for (UWidgetAnimation* Anim : WidgetClass->Animations)
			{
				if (Anim && Anim->GetName().Contains(TEXT("Damage")))
				{
					Widget->PlayAnimation(Anim, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f);
					break;
				}
			}
		}
	}
}

void AHunterCharacter::AddHealth_Implementation(float Amount)
{
	AddHealthDirect(Amount);
}

void AHunterCharacter::AddBullets_Implementation(int32 Amount)
{
	AddBulletsDirect(Amount);
}

ETeamAttitude::Type AHunterCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (const IGenericTeamAgentInterface* OtherTeamAgent = Cast<IGenericTeamAgentInterface>(&Other))
	{
		// Team 0 is player, any other team (e.g. 1) is hostile
		if (OtherTeamAgent->GetGenericTeamId() != GetGenericTeamId())
		{
			return ETeamAttitude::Hostile;
		}
		return ETeamAttitude::Friendly;
	}
	return ETeamAttitude::Neutral;
}

float AHunterCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	ApplyDamageDirect(ActualDamage);
	return ActualDamage;
}



