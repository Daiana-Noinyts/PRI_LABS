#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_HunterReload.generated.h"

// Stages of the player's reload, placed as notifies on the reload montage
UENUM(BlueprintType)
enum class EHunterReloadStage : uint8
{
	TakeMagazine            UMETA(DisplayName = "TakeMagazine"),
	ThrowMagazine           UMETA(DisplayName = "ThrowMagazine"),
	TakeMagazineFromPocket  UMETA(DisplayName = "TakeMagazine_FromPocket"),
	InsertMagazine          UMETA(DisplayName = "InsertMagazine"),
	EndReloading            UMETA(DisplayName = "EndReloading")
};

/** Montage notify that tells AHunterCharacter which reload stage the animation has reached. */
UCLASS(meta = (DisplayName = "Hunter Reload Stage"))
class FOREST_API UAnimNotify_HunterReload : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reload")
	EHunterReloadStage Stage = EHunterReloadStage::TakeMagazine;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
