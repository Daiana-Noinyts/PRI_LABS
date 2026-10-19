#include "BTTask_PatrolForest.h"
#include "ForestAIController.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_PatrolForest::UBTTask_PatrolForest()
{
	NodeName = TEXT("Forest Find Patrol Point");
}

EBTNodeResult::Type UBTTask_PatrolForest::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AForestAIController* AIController = Cast<AForestAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(OwnerComp.GetWorld());
	if (!NavSys)
	{
		return EBTNodeResult::Failed;
	}

	const float Radius = (CustomPatrolRadius > 0.0f) ? CustomPatrolRadius : AIController->GetPatrolRadius();

	FNavLocation NavLocation;
	const bool bFound = NavSys->GetRandomReachablePointInRadius(Pawn->GetActorLocation(), Radius, NavLocation);

	if (bFound)
	{
		AIController->GetBlackboardComponent()->SetValueAsVector(AIController->GetPatrolTargetKey(), NavLocation.Location);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
