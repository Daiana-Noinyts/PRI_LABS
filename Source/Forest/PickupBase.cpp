#include "PickupBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

APickupBase::APickupBase()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(90.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Trigger);
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, 50.f));

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(VisualRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetGenerateOverlapEvents(false);
	PickupMesh->SetCastShadow(true);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(VisualRoot);
	Glow->SetIntensity(2000.f);
	Glow->SetAttenuationRadius(350.f);
	Glow->SetCastShadows(false);
}

void APickupBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (PickupMesh)
	{
		PickupMesh->SetRelativeScale3D(MeshScale);
		PickupMesh->SetRelativeRotation(MeshRotation);

		FVector FinalOffset = MeshOffset;
		if (bAutoCenterMesh && PickupMesh->GetStaticMesh())
		{
			const FBoxSphereBounds Bounds = PickupMesh->GetStaticMesh()->GetBounds();
			FinalOffset -= (Bounds.Origin * MeshScale);
		}
		PickupMesh->SetRelativeLocation(FinalOffset);
	}
}

void APickupBase::BeginPlay()
{
	Super::BeginPlay();
	VisualBaseLocation = VisualRoot->GetRelativeLocation();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APickupBase::OnTriggerBegin);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &APickupBase::OnTriggerEnd);
}

void APickupBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPickupActive) { return; }

	Age += DeltaSeconds;
	VisualRoot->AddLocalRotation(FRotator(0.f, SpinDegreesPerSecond * DeltaSeconds, 0.f));
	FVector Loc = VisualBaseLocation;
	Loc.Z += FMath::Sin(Age * BobSpeed) * BobHeight;
	VisualRoot->SetRelativeLocation(Loc);

	if (APawn* Waiting = WaitingPawn.Get())
	{
		TryConsume(Waiting);
	}
}

void APickupBase::OnTriggerBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		TryConsume(Pawn);
	}
}

void APickupBase::OnTriggerEnd(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (WaitingPawn.Get() == OtherActor)
	{
		WaitingPawn.Reset();
	}
}

void APickupBase::TryConsume(APawn* Pawn)
{
	if (!bPickupActive || !Pawn) { return; }

	if (!ApplyToCharacter(Pawn))
	{
		WaitingPawn = Pawn;
		return;
	}

	WaitingPawn.Reset();
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	if (bRespawn)
	{
		SetPickupActive(false);
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &APickupBase::Respawn, RespawnDelay, false);
	}
	else
	{
		Destroy();
	}
}

void APickupBase::SetPickupActive(bool bActive)
{
	bPickupActive = bActive;
	SetActorHiddenInGame(!bActive);
	Trigger->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}
