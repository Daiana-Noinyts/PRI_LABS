#include "TeleporterPad.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "ForestVisuals.h"

using namespace ForestVisuals;

ATeleporterPad::ATeleporterPad()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->SetBoxExtent(FVector(100.f, 100.f, 120.f));
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Trigger->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	Trigger->SetGenerateOverlapEvents(true);

	Rim  = MakePart(this, Root, TEXT("Rim"),  EShape::Cylinder, FVector(0, 0, 2), FVector(2.6f, 2.6f, 0.04f));
	Disc = MakePart(this, Root, TEXT("Disc"), EShape::Cylinder, FVector(0, 0, 5), FVector(2.2f, 2.2f, 0.06f));

	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(Root);
	Spinner->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	Crystal = MakePart(this, Spinner, TEXT("Crystal"), EShape::Cube, FVector::ZeroVector, FVector(0.35f, 0.35f, 0.35f), FRotator(35.f, 0.f, 45.f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Spinner);
	Glow->SetIntensity(3000.f);
	Glow->SetAttenuationRadius(500.f);
	Glow->SetCastShadows(false);
}

void ATeleporterPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyColor(Rim, FLinearColor(0.08f, 0.08f, 0.1f));
	ApplyColor(Disc, PadColor * 0.6f);
	ApplyColor(Crystal, PadColor);
	Glow->SetLightColor(PadColor);
}

void ATeleporterPad::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATeleporterPad::OnTriggerBegin);

	if (!Destination)
	{
		UE_LOG(LogTemp, Warning, TEXT("TeleporterPad %s has no Destination set."), *GetName());
	}
	else if (FVector::Dist(GetActorLocation(), Destination->GetActorLocation()) < MinRecommendedDistance)
	{
		UE_LOG(LogTemp, Warning, TEXT("TeleporterPad %s is only %.0f cm from %s - consider placing them further apart."),
			*GetName(), FVector::Dist(GetActorLocation(), Destination->GetActorLocation()), *Destination->GetName());
	}
}

void ATeleporterPad::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	Spinner->AddLocalRotation(FRotator(0.f, 90.f * DeltaSeconds, 0.f));
	Spinner->SetRelativeLocation(FVector(0.f, 0.f, 110.f + FMath::Sin(Age * 2.f) * 8.f));

	// Forget arrivals once they have left the trigger (small grace period so the arrival overlap can register).
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto It = Suppressed.CreateIterator(); It; ++It)
	{
		AActor* Actor = It.Key().Get();
		if (!Actor || (Now - It.Value() > 0.25 && !Trigger->IsOverlappingActor(Actor)))
		{
			It.RemoveCurrent();
		}
	}
}

void ATeleporterPad::OnTriggerBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32, bool, const FHitResult&)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!bTeleporterEnabled || !Destination || !Destination->bTeleporterEnabled || !Character) { return; }
	if (!Character->IsPlayerControlled() || OtherComp != Character->GetCapsuleComponent())    { return; }
	if (Suppressed.Contains(Character)) { return; }

	Teleport(Character);
}

void ATeleporterPad::Teleport(ACharacter* Character)
{
	const FVector OldVelocity = Character->GetVelocity();
	const float   CurrentYaw  = Character->GetActorRotation().Yaw;
	const float   NewYaw      = bAlignToDestinationFacing ? Destination->GetActorRotation().Yaw : CurrentYaw;
	const float   YawDelta    = NewYaw - CurrentYaw;

	const FVector DestLoc = Destination->GetActorLocation() + Destination->ArrivalOffset
		+ FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());

	// Mark BEFORE moving: the arrival overlap on the destination fires synchronously inside TeleportTo.
	Destination->Suppressed.Add(Character, GetWorld()->GetTimeSeconds());

	if (!Character->TeleportTo(DestLoc, FRotator(0.f, NewYaw, 0.f), /*bIsATest*/ false, /*bNoCheck*/ false))
	{
		Destination->Suppressed.Remove(Character);
		UE_LOG(LogTemp, Warning, TEXT("TeleporterPad %s: destination %s is blocked, teleport cancelled."), *GetName(), *Destination->GetName());
		return;
	}

	if (AController* Controller = Character->GetController())
	{
		FRotator Control = Controller->GetControlRotation();
		Control.Yaw += YawDelta;
		Controller->SetControlRotation(Control);

		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			if (PC->PlayerCameraManager && FadeInDuration > 0.f)
			{
				PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeInDuration, FLinearColor::Black);
			}
		}
	}

	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->Velocity = bPreserveVelocity ? FRotator(0.f, YawDelta, 0.f).RotateVector(OldVelocity) : FVector::ZeroVector;
		Move->UpdateComponentVelocity();
	}

	UGameplayStatics::PlaySoundAtLocation(this, TeleportSound, GetActorLocation());
	UGameplayStatics::PlaySoundAtLocation(this, TeleportSound, DestLoc);
}

#if WITH_EDITOR
void ATeleporterPad::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ATeleporterPad, Destination)
		&& bLinkBothWays && Destination && Destination->Destination != this)
	{
		Destination->Modify();
		Destination->Destination = this;
	}
}
#endif
