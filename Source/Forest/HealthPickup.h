#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "HealthPickup.generated.h"

/**
 * Health pickup: displays the first-aid kit static mesh (TM_lab5_final).
 * Restores player's health when touched.
 */
UCLASS()
class FOREST_API AHealthPickup : public APickupBase
{
	GENERATED_BODY()

public:
	AHealthPickup();

protected:
	virtual bool ApplyToCharacter(APawn* Character) override;
};
