#include "AmmoPickup.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ForestStatAccess.h"
#include "HunterCharacter.h"

AAmmoPickup::AAmmoPickup()
{
	
	bAutoCenterMesh = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> AmmoMeshFinder(
		TEXT("/Game/Fab/7_62mm_ammo_box/bullets-box-7-62.bullets-box-7-62"));
	if (AmmoMeshFinder.Succeeded() && PickupMesh)
	{
		PickupMesh->SetStaticMesh(AmmoMeshFinder.Object);
	}

	Glow->SetLightColor(FLinearColor(1.0f, 0.75f, 0.15f));
	Glow->SetIntensity(2200.f);
}

bool AAmmoPickup::ApplyToCharacter(APawn* Character)
{
	if (!Character) { return false; }

	double Cur = 0.0, Max = 0.0;
	if (ForestStats::GetAmmo(Character, Cur, Max))
	{
		const double Gain = FMath::CeilToDouble(Max * RestoreFraction);

		// The player loads the ammo with the reload animation; other pawns just get it
		if (AHunterCharacter* Hunter = Cast<AHunterCharacter>(Character))
		{
			Hunter->ReloadWithAmmo(static_cast<int32>(Gain));
		}
		else
		{
			ForestStats::SetAmmo(Character, FMath::Min(Max, Cur + Gain));
		}
	}

	// Always collect on touch, even at full ammo
	return true;
}
