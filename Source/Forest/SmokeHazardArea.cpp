#include "SmokeHazardArea.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "ForestNPCCharacter.h"
#include "HunterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ASmokeHazardArea::ASmokeHazardArea()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	RootComponent = TriggerSphere;
	TriggerSphere->InitSphereRadius(350.0f);
	TriggerSphere->SetCollisionProfileName(TEXT("Trigger"));
	TriggerSphere->SetGenerateOverlapEvents(true);

	SmokeNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SmokeNiagaraComponent"));
	SmokeNiagaraComponent->SetupAttachment(RootComponent);
	SmokeNiagaraComponent->bAutoActivate = true;

	SmokeCascadeComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("SmokeCascadeComponent"));
	SmokeCascadeComponent->SetupAttachment(RootComponent);
	SmokeCascadeComponent->bAutoActivate = false;
}

void ASmokeHazardArea::BeginPlay()
{
	Super::BeginPlay();

	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ASmokeHazardArea::OnOverlapBegin);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &ASmokeHazardArea::OnOverlapEnd);

	if (PoisonDamagePerSecond > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PoisonTimerHandle, this, &ASmokeHazardArea::ApplyPoisonTick, PoisonTickInterval, true);
	}
}

void ASmokeHazardArea::ApplyPoisonTick()
{
	TArray<AActor*> Inside;
	TriggerSphere->GetOverlappingActors(Inside, APawn::StaticClass());

	const float TickDamage = PoisonDamagePerSecond * PoisonTickInterval;
	for (AActor* Actor : Inside)
	{
		const AForestNPCCharacter* NPC = Cast<AForestNPCCharacter>(Actor);
		if ((NPC && !NPC->IsDying()) || Cast<AHunterCharacter>(Actor))
		{
			// No instigator: environmental damage, so the NPC will not try to dodge it
			UGameplayStatics::ApplyDamage(Actor, TickDamage, nullptr, this, UDamageType::StaticClass());
		}
	}
}

void ASmokeHazardArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	// NPC entered poisonous smoke / blind zone
	if (AForestNPCCharacter* NPC = Cast<AForestNPCCharacter>(OtherActor))
	{
		// Slows the NPC and (if bBlindPerception) blinds its AI controller
		NPC->ApplySmokeDebuff(SpeedPenaltyFactor, bBlindPerception);
		BP_OnActorEnteredSmoke(NPC);
	}
	// Player entered smoke
	else if (AHunterCharacter* Player = Cast<AHunterCharacter>(OtherActor))
	{
		BP_OnActorEnteredSmoke(Player);
	}
}

void ASmokeHazardArea::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	// NPC exited smoke zone
	if (AForestNPCCharacter* NPC = Cast<AForestNPCCharacter>(OtherActor))
	{
		NPC->RemoveSmokeDebuff();
		BP_OnActorExitedSmoke(NPC);
	}
	// Player exited smoke
	else if (AHunterCharacter* Player = Cast<AHunterCharacter>(OtherActor))
	{
		BP_OnActorExitedSmoke(Player);
	}
}
