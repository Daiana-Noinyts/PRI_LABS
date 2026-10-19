#include "HealthPickup.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ForestStatAccess.h"

AHealthPickup::AHealthPickup()
{
	// Default scale suitable for the TM_lab5_final mesh (~168x336x136 cm in original FBX)
	MeshScale = FVector(0.25f, 0.25f, 0.25f);
	bAutoCenterMesh = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> HealthMeshFinder(
		TEXT("/Game/UI/TM_lab5_final.TM_lab5_final"));
	if (HealthMeshFinder.Succeeded() && PickupMesh)
	{
		PickupMesh->SetStaticMesh(HealthMeshFinder.Object);
	}

	Glow->SetLightColor(FLinearColor(0.15f, 0.95f, 0.25f));
	Glow->SetIntensity(2200.f);
}

bool AHealthPickup::ApplyToCharacter(APawn* Character)
{
	if (!Character) { return false; }

	double Cur = 0.0, Max = 0.0;
	if (ForestStats::GetHealth(Character, Cur, Max))
	{
		const double Restored = FMath::Min(Max, Cur + (Max * RestoreFraction));
		ForestStats::SetHealth(Character, Restored);
	}

	// Always collect on touch, even at full health
	return true;
}
