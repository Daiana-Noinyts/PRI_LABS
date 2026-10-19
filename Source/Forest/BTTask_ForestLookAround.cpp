#include "BTTask_ForestLookAround.h"
#include "ForestAIController.h"
#include "GameFramework/Pawn.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

UBTTask_ForestLookAround::UBTTask_ForestLookAround()
{
	NodeName = TEXT("Forest Look Around & Survey");
	bNotifyTick = true;

	RouteKey.SelectedKeyName = FName("PatrolTarget");
	RouteKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_ForestLookAround, RouteKey));
}

float UBTTask_ForestLookAround::GetStepYawOffset(int32 StepIndex) const
{
	// Look left, look right, then back to the centre
	switch (StepIndex)
	{
	case 0:  return -LookAroundAngle;
	case 1:  return LookAroundAngle;
	default: return 0.0f;
	}
}

void UBTTask_ForestLookAround::FocusStep(AAIController* AIController, const FBTForestLookAroundMemory& Memory) const
{
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const FRotator LookRot(0.0f, Memory.BaseYaw + GetStepYawOffset(Memory.StepIndex), 0.0f);
	// Move priority so a detected player (Gameplay priority focus) always wins over the survey
	AIController->SetFocalPoint(Pawn->GetActorLocation() + LookRot.Vector() * 1000.0f, EAIFocusPriority::Move);
}

float UBTTask_ForestLookAround::ScoreDirection(const APawn* Pawn, const FVector& Direction) const
{
	const FVector Start = Pawn->GetPawnViewLocation();
	const FVector End = Start + Direction * ProbeDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestLookAround), false, Pawn);
	FHitResult Hit;
	const bool bBlocked = Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	// Free distance in this direction, with a little jitter so equal options are not always picked the same way
	return (bBlocked ? Hit.Distance : ProbeDistance) + FMath::FRandRange(0.0f, 50.0f);
}

EBTNodeResult::Type UBTTask_ForestLookAround::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	// Stand still while surveying
	AIController->StopMovement();

	FBTForestLookAroundMemory* Memory = CastInstanceNodeMemory<FBTForestLookAroundMemory>(NodeMemory);
	*Memory = FBTForestLookAroundMemory();
	Memory->BaseYaw = Pawn->GetActorRotation().Yaw;

	FocusStep(AIController, *Memory);
	return EBTNodeResult::InProgress;
}

void UBTTask_ForestLookAround::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTForestLookAroundMemory* Memory = CastInstanceNodeMemory<FBTForestLookAroundMemory>(NodeMemory);
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	Memory->StepElapsed += DeltaSeconds;
	if (Memory->StepElapsed < StepDuration)
	{
		return;
	}

	// Done looking in this direction: rate it as a possible route
	const FVector Direction = FRotator(0.0f, Memory->BaseYaw + GetStepYawOffset(Memory->StepIndex), 0.0f).Vector();
	const float Score = ScoreDirection(Pawn, Direction);
	if (Score > Memory->BestScore)
	{
		Memory->BestScore = Score;
		Memory->BestDirection = Direction;
	}

	Memory->StepIndex++;
	Memory->StepElapsed = 0.0f;

	if (Memory->StepIndex >= 3)
	{
		FinishSurvey(OwnerComp, *Memory);
		return;
	}

	FocusStep(AIController, *Memory);
}

void UBTTask_ForestLookAround::FinishSurvey(UBehaviorTreeComponent& OwnerComp, FBTForestLookAroundMemory& Memory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();

	if (Pawn && BB && RouteKey.SelectedKeyName != NAME_None && Memory.BestScore > 200.0f)
	{
		// Stop a bit short of the obstacle and snap the point to the NavMesh
		const FVector Desired = Pawn->GetActorLocation() + Memory.BestDirection * (Memory.BestScore * 0.85f);

		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld()))
		{
			FNavLocation NavLocation;
			if (NavSys->ProjectPointToNavigation(Desired, NavLocation, FVector(200.0f, 200.0f, 500.0f)))
			{
				BB->SetValueAsVector(RouteKey.SelectedKeyName, NavLocation.Location);
			}
		}
	}

	if (AIController)
	{
		AIController->ClearFocus(EAIFocusPriority::Move);
	}

	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTTask_ForestLookAround::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		AIController->ClearFocus(EAIFocusPriority::Move);
	}
	return EBTNodeResult::Aborted;
}

FString UBTTask_ForestLookAround::GetStaticDescription() const
{
	return FString::Printf(TEXT("Survey +/-%.0f deg, %.1fs per look\nBest route -> %s"),
		LookAroundAngle, StepDuration, *RouteKey.SelectedKeyName.ToString());
}
