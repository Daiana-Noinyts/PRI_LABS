#include "PushableCrate.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FVector CrateHalfExtent(50.7f, 26.f, 21.f);
}

APushableCrate::APushableCrate()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->InitBoxExtent(CrateHalfExtent);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->SetNotifyRigidBodyCollision(true);
	Body->SetHiddenInGame(true);
	Body->BodyInstance.DOFMode = EDOFMode::SixDOF;
	Body->BodyInstance.bLockXRotation = true;
	Body->BodyInstance.bLockYRotation = true;
	RootComponent = Body;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Body);
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -CrateHalfExtent.Z));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CrateMesh(
		TEXT("/Game/Fab/Wooden_crate/SM_Crate/StaticMeshes/SM_Crate.SM_Crate"));
	if (CrateMesh.Succeeded()) { Visual->SetStaticMesh(CrateMesh.Object); }
}

void APushableCrate::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	Body->SetLinearDamping(LinearDamping);
	Body->SetAngularDamping(AngularDamping);
}

void APushableCrate::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
	FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	if (!Other || !OtherComp || Other->IsA<ACharacter>() || OtherComp->IsSimulatingPhysics()) { return; }

	const FVector Velocity = Other->GetVelocity();
	if (Velocity.SizeSquared() < FMath::Square(MinImpactSpeed)) { return; }

	MyComp->AddImpulseAtLocation(Velocity * ImpactImpulseScale, HitLocation);
}
