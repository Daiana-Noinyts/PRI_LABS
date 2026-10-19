#include "MovingPlatform.h"
#include "Components/StaticMeshComponent.h"
#include "ForestVisuals.h"

using namespace ForestVisuals;

AMovingPlatform::AMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root part: the engine cube already has box collision, so the character can stand on it.
	Slab = MakePart(this, nullptr, TEXT("Slab"), EShape::Cube, FVector::ZeroVector, FVector(4.f, 4.f, 0.3f));
	Slab->SetMobility(EComponentMobility::Movable);
	Slab->SetCollisionProfileName(TEXT("BlockAll"));
	Slab->SetGenerateOverlapEvents(false);
	RootComponent = Slab;
}

void AMovingPlatform::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyColor(Slab, SlabColor);
}

void AMovingPlatform::BeginPlay()
{
	Super::BeginPlay();
	StartLocation = GetActorLocation();
	EndLocation = GetActorTransform().TransformPosition(TargetOffset);
	PauseRemaining = PauseAtEnds;   // brief pause before the first trip
}

void AMovingPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMoving) { return; }

	if (PauseRemaining > 0.f)
	{
		PauseRemaining -= DeltaSeconds;
		return;
	}

	Progress += Direction * DeltaSeconds / TravelTime;
	if (Progress >= 1.f)      { Progress = 1.f; Direction = -1.f; PauseRemaining = PauseAtEnds; }
	else if (Progress <= 0.f) { Progress = 0.f; Direction =  1.f; PauseRemaining = PauseAtEnds; }

	const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, Progress, 2.f);
	SetActorLocation(FMath::Lerp(StartLocation, EndLocation, Alpha));
}
