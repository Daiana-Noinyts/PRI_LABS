#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PatrolForest.generated.h"

UCLASS()
class FOREST_API UBTTask_PatrolForest : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PatrolForest();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Patrol")
	float CustomPatrolRadius = 0.0f; // If 0, uses Controller's PatrolRadius
};
