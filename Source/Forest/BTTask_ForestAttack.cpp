#include "BTTask_ForestAttack.h"
#include "ForestAIController.h"
#include "ForestNPCCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_ForestAttack::UBTTask_ForestAttack()
{
	NodeName = TEXT("Forest Attack Target");
}

EBTNodeResult::Type UBTTask_ForestAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AForestAIController* AIController = Cast<AForestAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	AForestNPCCharacter* NPC = Cast<AForestNPCCharacter>(AIController->GetPawn());
	if (!NPC || NPC->IsDying())
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BB = AIController->GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(AIController->GetDetectedEnemyKey()));
	if (!Target)
	{
		return EBTNodeResult::Failed;
	}

	NPC->AttackPlayer(Target);
	return EBTNodeResult::Succeeded;
}
