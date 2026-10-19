#include "AnimNotify_HunterReload.h"
#include "HunterCharacter.h"
#include "Components/SkeletalMeshComponent.h"

FString UAnimNotify_HunterReload::GetNotifyName_Implementation() const
{
	switch (Stage)
	{
	case EHunterReloadStage::TakeMagazine:           return TEXT("TakeMagazine");
	case EHunterReloadStage::ThrowMagazine:          return TEXT("ThrowMagazine");
	case EHunterReloadStage::TakeMagazineFromPocket: return TEXT("TakeMagazine_FromPocket");
	case EHunterReloadStage::InsertMagazine:         return TEXT("InsertMagazine");
	case EHunterReloadStage::EndReloading:           return TEXT("EndReloading");
	}
	return Super::GetNotifyName_Implementation();
}

void UAnimNotify_HunterReload::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (AHunterCharacter* Hunter = MeshComp ? Cast<AHunterCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		Hunter->HandleReloadStage(Stage);
	}
}
