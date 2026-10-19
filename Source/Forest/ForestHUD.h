#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ForestHUD.generated.h"

UCLASS()
class FOREST_API AForestHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void DrawHUD() override;

private:
	UPROPERTY(EditDefaultsOnly)
	class UTexture2D* CrosshairTex;
};