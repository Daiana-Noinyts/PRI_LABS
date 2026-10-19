#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetNPCSpeed.generated.h"

UENUM(BlueprintType)
enum class EForestSpeedMode : uint8
{
	Walk UMETA(DisplayName = "Walk"),
	Run UMETA(DisplayName = "Run")
};

UCLASS()
class FOREST_API UBTTask_SetNPCSpeed : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetNPCSpeed();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
	EForestSpeedMode SpeedMode = EForestSpeedMode::Walk;
};
