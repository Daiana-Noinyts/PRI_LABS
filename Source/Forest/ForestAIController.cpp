#include "ForestAIController.h"
#include "ForestNPCCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"

AForestAIController::AForestAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	if (SightConfig)
	{
		SightConfig->SightRadius = 2600.0f;
		SightConfig->LoseSightRadius = 3100.0f;
		SightConfig->PeripheralVisionAngleDegrees = 85.0f;
		SightConfig->SetMaxAge(5.0f);
		SightConfig->AutoSuccessRangeFromLastSeenLocation = 400.0f;

		// Detect Enemies (Player) and Neutrals
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

		AIPerceptionComp->ConfigureSense(*SightConfig);
		AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
	}
}

void AForestAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledNPC = Cast<AForestNPCCharacter>(InPawn);

	if (AIPerceptionComp)
	{
		AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AForestAIController::OnTargetPerceptionUpdated);
	}

	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
	}
}

void AForestAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB || !ControlledNPC)
	{
		return;
	}

	// Sync Death state
	if (ControlledNPC->IsDying())
	{
		BB->SetValueAsBool(IsDyingKey, true);
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	// Sync Dodge & Smoke states
	BB->SetValueAsBool(IsDodgingKey, ControlledNPC->IsDodging());
	BB->SetValueAsBool(IsInSmokeKey, ControlledNPC->IsInSmoke());

	ShowStateDebug();
	UpdateWeaponReadiness();

	// Not looking at the player (lost him, searching, patrolling): turn the body the way he is going.
	// Without any focus nothing rotates him, so he would keep facing the old target and run backwards.
	// Lowest focus priority, so the player (Gameplay focus) or the path (Move focus) still win when set.
	if (!bHasLineOfSight && !ControlledNPC->IsDodging())
	{
		const FVector Vel = ControlledNPC->GetVelocity();
		const FVector Flat(Vel.X, Vel.Y, 0.0f);
		if (Flat.SizeSquared() > FMath::Square(20.0f))
		{
			SetFocalPoint(ControlledNPC->GetActorLocation() + Flat.GetSafeNormal() * 500.0f, EAIFocusPriority::Default);
		}
	}

	// If currently in sight, continuously refresh player's last known location
	if (bHasLineOfSight && PerceivedActor)
	{
		LastLocationElapsed += DeltaTime;
		if (LastLocationElapsed >= LastLocationUpdateTime)
		{
			LastLocationElapsed = 0.0f;
			BB->SetValueAsVector(LastKnownLocationKey, PerceivedActor->GetActorLocation());
		}

		// Update effective weapon range flag
		const float DistToTarget = FVector::Dist(ControlledNPC->GetActorLocation(), PerceivedActor->GetActorLocation());
		const bool bInRange = DistToTarget <= ControlledNPC->GetWeaponEffectiveRange();
		BB->SetValueAsBool(IsEffectiveRangeKey, bInRange);
	}

	// Line of sight was broken: only give up the chase if the player stays hidden for the whole grace period
	if (bSightLossPending)
	{
		SightLossElapsed += DeltaTime;
		if (SightLossElapsed >= LoseSightGraceTime)
		{
			LoseSightOfTarget(PendingLostLocation);
		}
	}

	// If player broke line of sight, track memory decay timer
	if (!bHasLineOfSight && PerceivedActor)
	{
		TargetLostElapsed += DeltaTime;
		if (TargetLostElapsed >= TargetLostTime)
		{
			// NPC has "forgotten" the player and returns to patrolling
			PerceivedActor = nullptr;
			TargetLostElapsed = 0.0f;
			BB->SetValueAsObject(DetectedEnemyKey, nullptr);
			BB->SetValueAsBool(HasLineOfSightKey, false);
			BB->SetValueAsBool(IsEffectiveRangeKey, false);
			ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
}

void AForestAIController::UpdateWeaponReadiness()
{
	if (ControlledNPC->IsDodging() || ControlledNPC->IsChangingWeapon())
	{
		return;
	}

	const float Dist = PerceivedActor ? FVector::Dist(ControlledNPC->GetActorLocation(), PerceivedActor->GetActorLocation()) : TNumericLimits<float>::Max();

	// Player seen and close enough -> raise the weapon and keep it in the hands
	if (ControlledNPC->GetWeaponState() == ENPCWeaponState::Sheathed && bHasLineOfSight && Dist <= ControlledNPC->DrawWeaponRange)
	{
		ControlledNPC->DrawWeapon();
	}
	// Player got away (too far, or forgotten) -> put the weapon away again
	else if (ControlledNPC->GetWeaponState() == ENPCWeaponState::Drawn && (!PerceivedActor || Dist > ControlledNPC->SheathWeaponRange))
	{
		ControlledNPC->SheathWeapon();
	}
}

ETeamAttitude::Type AForestAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (const IGenericTeamAgentInterface* OtherTeamAgent = Cast<IGenericTeamAgentInterface>(&Other))
	{
		// Player Team = 0 -> Hostile
		if (OtherTeamAgent->GetGenericTeamId() == FGenericTeamId(0))
		{
			return ETeamAttitude::Hostile;
		}
		// NPCs Team = 1 -> Friendly
		if (OtherTeamAgent->GetGenericTeamId() == FGenericTeamId(1))
		{
			return ETeamAttitude::Friendly;
		}
	}
	return ETeamAttitude::Neutral;
}

void AForestAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB || !Actor)
	{
		return;
	}

	// Verify target is hostile (player)
	if (GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		// Blind zone: the NPC cannot see anything while standing in the smoke
		if (bBlindedBySmoke)
		{
			return;
		}
		AcquireTarget(Actor);
	}
	else if (bHasLineOfSight)
	{
		// Trees break the view for a moment all the time in the forest. Keep chasing for a short
		// grace period instead of instantly dropping to the (walking) search branch.
		bSightLossPending = true;
		SightLossElapsed = 0.0f;
		PendingLostLocation = Stimulus.StimulusLocation;
	}
}

void AForestAIController::ShowStateDebug()
{
	const UCharacterMovementComponent* MoveComp = ControlledNPC->GetCharacterMovement();
	const float Speed = ControlledNPC->GetVelocity().Size2D();
	const float MaxSpeed = MoveComp ? MoveComp->MaxWalkSpeed : 0.0f;

	// Same priority order as the Behavior Tree branches
	FString State;
	if (ControlledNPC->IsDodging())   { State = TEXT("DODGE"); }
	else if (bHasLineOfSight)          { State = TEXT("CHASE (run)"); }
	else if (PerceivedActor)           { State = TEXT("SEARCH (walk)"); }
	else                               { State = TEXT("PATROL (walk)"); }
	if (bBlindedBySmoke)
	{
		State += TEXT(" | BLIND (smoke)");
	}

	switch (ControlledNPC->GetWeaponState())
	{
	case ENPCWeaponState::Sheathed:  State += TEXT(" | axe: belt"); break;
	case ENPCWeaponState::Drawing:   State += TEXT(" | axe: DRAWING"); break;
	case ENPCWeaponState::Drawn:     State += TEXT(" | axe: READY"); break;
	case ENPCWeaponState::Sheathing: State += TEXT(" | axe: SHEATHING"); break;
	}

	if (State != LastDebugState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPC State] %s -> %s (speed %.0f, max %.0f)"), *LastDebugState, *State, Speed, MaxSpeed);
		LastDebugState = State;
	}

}

void AForestAIController::AcquireTarget(AActor* Actor)
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB || !Actor)
	{
		return;
	}

	bHasLineOfSight = true;
	bSightLossPending = false;
	PerceivedActor = Actor;
	TargetLostElapsed = 0.0f;
	BB->SetValueAsBool(HasLineOfSightKey, true);
	BB->SetValueAsObject(DetectedEnemyKey, PerceivedActor);
	BB->SetValueAsVector(LastKnownLocationKey, Actor->GetActorLocation());
	SetFocus(Actor);
}

void AForestAIController::LoseSightOfTarget(const FVector& LastSeenLocation)
{
	bHasLineOfSight = false;
	bSightLossPending = false;
	TargetLostElapsed = 0.0f;

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsBool(HasLineOfSightKey, false);
		BB->SetValueAsBool(IsEffectiveRangeKey, false);
		BB->SetValueAsVector(LastKnownLocationKey, LastSeenLocation);
	}
	ClearFocus(EAIFocusPriority::Gameplay);
}

void AForestAIController::SetInSmoke(bool bInSmoke, bool bBlindSight)
{
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsBool(IsInSmokeKey, bInSmoke);
	}

	const bool bWasBlinded = bBlindedBySmoke;
	bBlindedBySmoke = bInSmoke && bBlindSight;

	if (bBlindedBySmoke && bHasLineOfSight && PerceivedActor)
	{
		// Smoke cuts the view: NPC falls back to searching the last place it saw the player
		LoseSightOfTarget(PerceivedActor->GetActorLocation());
	}
	else if (bWasBlinded && !bBlindedBySmoke && AIPerceptionComp)
	{
		// Leaving the smoke: perception won't re-fire for a player that stayed in view, so check now
		TArray<AActor*> VisibleActors;
		AIPerceptionComp->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), VisibleActors);
		for (AActor* Visible : VisibleActors)
		{
			if (Visible && GetTeamAttitudeTowards(*Visible) == ETeamAttitude::Hostile)
			{
				AcquireTarget(Visible);
				break;
			}
		}
	}
}
