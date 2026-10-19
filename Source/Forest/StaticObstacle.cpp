#include "StaticObstacle.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AStaticObstacle::AStaticObstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Static);
	RootComponent = Mesh;

	BoxShape = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxShape"));
	BoxShape->SetupAttachment(Mesh);
	BoxShape->SetMobility(EComponentMobility::Static);

	SphereShape = CreateDefaultSubobject<USphereComponent>(TEXT("SphereShape"));
	SphereShape->SetupAttachment(Mesh);
	SphereShape->SetMobility(EComponentMobility::Static);

	CapsuleShape = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleShape"));
	CapsuleShape->SetupAttachment(Mesh);
	CapsuleShape->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultRock(
		TEXT("/Game/LowPolyForest/StaticMeshes/Environment/SM_Env_rock_12.SM_Env_rock_12"));
	if (DefaultRock.Succeeded()) { ObstacleMesh = DefaultRock.Object; }
}

void AStaticObstacle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Mesh->SetStaticMesh(ObstacleMesh);

	// Start from a clean slate, then enable only the selected collision type.
	DisableCollision(Mesh);
	DisableCollision(BoxShape);
	DisableCollision(SphereShape);
	DisableCollision(CapsuleShape);

	switch (CollisionType)
	{
	case EObstacleCollision::MeshAsAuthored:
		ApplyBlocking(Mesh);
		break;

	case EObstacleCollision::Box:
		BoxShape->SetBoxExtent(BoxHalfExtent);
		BoxShape->SetRelativeLocation(ShapeOffset);
		ApplyBlocking(BoxShape);
		break;

	case EObstacleCollision::Sphere:
		SphereShape->SetSphereRadius(SphereRadius);
		SphereShape->SetRelativeLocation(ShapeOffset);
		ApplyBlocking(SphereShape);
		break;

	case EObstacleCollision::Capsule:
		CapsuleShape->SetCapsuleSize(CapsuleRadius, FMath::Max(CapsuleHalfHeight, CapsuleRadius));
		CapsuleShape->SetRelativeLocation(ShapeOffset);
		ApplyBlocking(CapsuleShape);
		break;
	}
}

void AStaticObstacle::DisableCollision(UPrimitiveComponent* Comp)
{
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCanEverAffectNavigation(false);
}

void AStaticObstacle::ApplyBlocking(UPrimitiveComponent* Comp) const
{
	Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Comp->SetCollisionObjectType(ECC_WorldStatic);
	Comp->SetCollisionResponseToAllChannels(ECR_Block);
	Comp->SetCollisionResponseToChannel(ECC_Pawn,       bBlockPawns  ? ECR_Block : ECR_Ignore);
	Comp->SetCollisionResponseToChannel(ECC_Camera,     bBlockCamera ? ECR_Block : ECR_Ignore);
	Comp->SetCollisionResponseToChannel(ECC_Visibility, bBlockTraces ? ECR_Block : ECR_Ignore);
	Comp->CanCharacterStepUpOn = bAllowStepUp ? ECB_Yes : ECB_No;
	Comp->SetCanEverAffectNavigation(bBlockPawns);
}
