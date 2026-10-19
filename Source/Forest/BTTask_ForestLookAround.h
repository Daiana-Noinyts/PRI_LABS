#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ForestLookAround.generated.h"

struct FBTForestLookAroundMemory
{
	int32 StepIndex = 0;
	float StepElapsed = 0.0f;
	float BaseYaw = 0.0f;
	float BestScore = -1.0f;
	FVector BestDirection = FVector::ZeroVector;
};

/**
 * Stops the NPC and makes it survey the area: it turns (smoothly, via the controller focal point)
 * to the left, to the right and back to the centre, pausing at each step. Every direction is probed
 * with a line trace; the most open one is projected onto the NavMesh and written to the blackboard
 * as the next route to take.
 */
UCLASS()
class FOREST_API UBTTask_ForestLookAround : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ForestLookAround();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTForestLookAroundMemory); }
	virtual FString GetStaticDescription() const override;

protected:
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Observe")
	float LookAroundAngle = 60.0f;

	// How long the NPC holds each look direction (includes turning time)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Observe", meta = (ClampMin = "0.2"))
	float StepDuration = 1.0f;

	// How far ahead each direction is probed when choosing the route
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Observe", meta = (ClampMin = "100.0"))
	float ProbeDistance = 1500.0f;

	// Vector key that receives the chosen route point (left empty = do not write)
	UPROPERTY(EditAnywhere, Category = "AI|Observe")
	FBlackboardKeySelector RouteKey;

private:
	float GetStepYawOffset(int32 StepIndex) const;
	void FocusStep(AAIController* AIController, const FBTForestLookAroundMemory& Memory) const;
	float ScoreDirection(const APawn* Pawn, const FVector& Direction) const;
	void FinishSurvey(UBehaviorTreeComponent& OwnerComp, FBTForestLookAroundMemory& Memory);
};
