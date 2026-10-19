#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ForestAttack.generated.h"

UCLASS()
class FOREST_API UBTTask_ForestAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ForestAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
