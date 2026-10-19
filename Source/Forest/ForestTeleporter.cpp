#include "ForestTeleporter.h"
#include "HunterCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"


AForestTeleporter::AForestTeleporter()
{
	PrimaryActorTick.bCanEverTick = false;

	TeleportTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("TeleportTrigger"));
	TeleportTrigger->InitBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	TeleportTrigger->SetCollisionProfileName(TEXT("Trigger"));
	TeleportTrigger->SetGenerateOverlapEvents(true);
	RootComponent = TeleportTrigger;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bIsOnCooldown = false;
	CooldownDuration = 2.0f;
	TargetOffset = FVector(0.0f, 0.0f, 50.0f);
}

void AForestTeleporter::BeginPlay()
{
	Super::BeginPlay();
	TeleportTrigger->OnComponentBeginOverlap.AddDynamic(this, &AForestTeleporter::OnOverlapBegin);
}

void AForestTeleporter::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsOnCooldown || !OtherActor || OtherActor == this)
	{
		return;
	}

	AHunterCharacter* Hunter = Cast<AHunterCharacter>(OtherActor);
	if (!Hunter)
	{
		return;
	}

	if (!TargetDestination)
	{
		return;
	}

	FVector DestLocation = TargetDestination->GetActorLocation() + TargetOffset;
	FRotator DestRotation = TargetDestination->GetActorRotation();

	if (AForestTeleporter* DestTeleporter = Cast<AForestTeleporter>(TargetDestination))
	{
		DestTeleporter->StartCooldown();
	}
	StartCooldown();

	if (TeleportSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TeleportSound, GetActorLocation());
	}
	if (TeleportFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TeleportFX, GetActorLocation(), GetActorRotation());
	}

	Hunter->TeleportTo(DestLocation, DestRotation);

	if (TeleportSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TeleportSound, DestLocation);
	}
	if (TeleportFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TeleportFX, DestLocation, DestRotation);
	}
}

void AForestTeleporter::StartCooldown()
{
	bIsOnCooldown = true;
	GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &AForestTeleporter::ResetCooldown, CooldownDuration, false);
}

void AForestTeleporter::ResetCooldown()
{
	bIsOnCooldown = false;
}
