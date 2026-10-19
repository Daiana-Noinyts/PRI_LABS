#include "BTTask_SetNPCSpeed.h"
#include "ForestAIController.h"
#include "ForestNPCCharacter.h"

UBTTask_SetNPCSpeed::UBTTask_SetNPCSpeed()
{
	NodeName = TEXT("Forest Set NPC Speed");
}

EBTNodeResult::Type UBTTask_SetNPCSpeed::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AForestAIController* AIController = Cast<AForestAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	AForestNPCCharacter* NPC = Cast<AForestNPCCharacter>(AIController->GetPawn());
	if (!NPC)
	{
		return EBTNodeResult::Failed;
	}

	if (SpeedMode == EForestSpeedMode::Run)
	{
		NPC->Run();
	}
	else
	{
		NPC->Walk();
	}

	return EBTNodeResult::Succeeded;
}
