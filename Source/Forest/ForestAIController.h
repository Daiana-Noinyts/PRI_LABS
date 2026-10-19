#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "ForestAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBehaviorTree;
class UBlackboardData;
class AForestNPCCharacter;

UCLASS()
class FOREST_API AForestAIController : public AAIController
{
	GENERATED_BODY()

public:
	AForestAIController();

	virtual void Tick(float DeltaTime) override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	// Blackboard Key Getters
	const FName& GetPatrolTargetKey() const { return PatrolTargetKey; }
	const FName& GetHasLineOfSightKey() const { return HasLineOfSightKey; }
	const FName& GetDetectedEnemyKey() const { return DetectedEnemyKey; }
	const FName& GetLastKnownLocationKey() const { return LastKnownLocationKey; }
	const FName& GetIsEffectiveRangeKey() const { return IsEffectiveRangeKey; }
	const FName& GetIsDyingKey() const { return IsDyingKey; }
	const FName& GetIsDodgingKey() const { return IsDodgingKey; }
	const FName& GetIsInSmokeKey() const { return IsInSmokeKey; }

	float GetPatrolRadius() const { return PatrolRadius; }
	AActor* GetPerceivedActor() const { return PerceivedActor; }
	bool HasLineOfSight() const { return bHasLineOfSight; }

	// Utility to update smoke status in blackboard.
	// bBlindSight: while inside, sight perception is ignored and the current target is lost (blind zone)
	UFUNCTION(BlueprintCallable, Category = "AI|State")
	void SetInSmoke(bool bInSmoke, bool bBlindSight = true);

	UFUNCTION(BlueprintPure, Category = "AI|State")
	bool IsBlindedBySmoke() const { return bBlindedBySmoke; }

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	UAIPerceptionComponent* AIPerceptionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	UAISenseConfig_Sight* SightConfig;

	// Assets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Behavior")
	UBehaviorTree* BehaviorTreeAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Behavior")
	UBlackboardData* BlackboardAsset;

	// Blackboard Key Names
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|BlackboardKeys")
	FName PatrolTargetKey = FName("PatrolTarget");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|BlackboardKeys")
	FName HasLineOfSightKey = FName("HasLineOfSight");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|BlackboardKeys")
	FName DetectedEnemyKey = FName("DetectedEnemy");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|BlackboardKeys")
	FName LastKnownLocationKey = FName("LastKnownLocation");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|BlackboardKeys")
	FName IsEffectiveRangeKey = FName("IsEffectiveRange");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|BlackboardKeys")
	FName IsDyingKey = FName("IsDying");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|BlackboardKeys")
	FName IsDodgingKey = FName("IsDodging");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|BlackboardKeys")
	FName IsInSmokeKey = FName("IsInSmoke");

	// Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Parameters")
	float PatrolRadius = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Parameters")
	float TargetLostTime = 3.5f;

	// Draw the current AI state and speed above the NPC (for testing / demonstrating the behaviour)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Debug")
	bool bShowStateDebug = true;

	// Player must stay out of sight this long before the NPC stops chasing (stops trees from causing stop-and-go)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Parameters")
	float LoseSightGraceTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Parameters")
	float LastLocationUpdateTime = 0.4f;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
	UPROPERTY()
	AActor* PerceivedActor = nullptr;

	UPROPERTY()
	AForestNPCCharacter* ControlledNPC = nullptr;

	bool bHasLineOfSight = false;
	bool bBlindedBySmoke = false;
	bool bSightLossPending = false;
	float SightLossElapsed = 0.0f;
	FVector PendingLostLocation = FVector::ZeroVector;
	float TargetLostElapsed = 0.0f;

	FString LastDebugState;

	void ShowStateDebug();
	void UpdateWeaponReadiness();
	void AcquireTarget(AActor* Actor);
	void LoseSightOfTarget(const FVector& LastSeenLocation);
	float LastLocationElapsed = 0.0f;
};
