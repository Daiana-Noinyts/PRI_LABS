#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "AmmoPickup.generated.h"

/**
 * Ammo pickup: displays the military ammo box static mesh (bullets-box-7-62).
 * Restores player's bullets when touched.
 */
UCLASS()
class FOREST_API AAmmoPickup : public APickupBase
{
	GENERATED_BODY()

public:
	AAmmoPickup();

protected:
	virtual bool ApplyToCharacter(APawn* Character) override;
};
