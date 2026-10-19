#include "KickableBall.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "ForestVisuals.h"

using namespace ForestVisuals;

AKickableBall::AKickableBall()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<USphereComponent>(TEXT("Body"));
	Body->InitSphereRadius(40.f);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->SetNotifyRigidBodyCollision(true);
	Body->SetHiddenInGame(true);
	RootComponent = Body;

	Visual = MakePart(this, Body, TEXT("Visual"), EShape::Sphere, FVector::ZeroVector, FVector(0.8f, 0.8f, 0.8f));
}

void AKickableBall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyColor(Visual, BallColor);
}

void AKickableBall::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	Body->SetLinearDamping(0.1f);
	Body->SetAngularDamping(0.4f);
}

void AKickableBall::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
	FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	const ACharacter* Character = Cast<ACharacter>(Other);
	if (!Character || !Character->IsPlayerControlled())
	{
		// Physical bounce reflection off obstacles and terrain
		if (bSelfMoved && Bounciness > 0.05f && MyComp)
		{
			const FVector CurVel = MyComp->GetComponentVelocity();
			if (CurVel.Size() > 80.f && HitNormal.SizeSquared() > 0.5f)
			{
				const FVector Reflected = FMath::GetReflectionVector(CurVel, HitNormal);
				MyComp->SetPhysicsLinearVelocity(Reflected * Bounciness);
			}
		}
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastKickTime < KickCooldown) { return; }

	const float Speed = Character->GetVelocity().Size2D();
	if (Speed < MinKickSpeed) { return; }

	FVector Direction = GetActorLocation() - Character->GetActorLocation();
	Direction.Z = 0.f;
	Direction = Direction.GetSafeNormal();

	FVector Launch = Direction * FMath::Min(Speed * KickSpeedMultiplier, MaxKickSpeed);
	Launch.Z = LoftSpeed;

	MyComp->AddImpulse(Launch, NAME_None, /*bVelChange*/ true);
	LastKickTime = Now;
}
