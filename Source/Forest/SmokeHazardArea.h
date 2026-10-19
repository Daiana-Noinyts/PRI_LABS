#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SmokeHazardArea.generated.h"

class USphereComponent;
class UNiagaraComponent;
class UParticleSystemComponent;

UCLASS()
class FOREST_API ASmokeHazardArea : public AActor
{
	GENERATED_BODY()

public:
	ASmokeHazardArea();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard|Components")
	USphereComponent* TriggerSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard|Components")
	UNiagaraComponent* SmokeNiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard|Components")
	UParticleSystemComponent* SmokeCascadeComponent;

	// Slow penalty multiplier (e.g. 0.45 means NPC moves at 45% speed while inside smoke)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard|Properties")
	float SpeedPenaltyFactor = 0.45f;

	// Whether entering smoke blinds sensory perception
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard|Properties")
	bool bBlindPerception = true;

	// Poison damage applied per second to the NPC and the player while inside the smoke
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard|Properties")
	float PoisonDamagePerSecond = 8.0f;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// Seconds between poison damage ticks
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard|Properties", meta = (ClampMin = "0.1"))
	float PoisonTickInterval = 1.0f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Hazard|Events")
	void BP_OnActorEnteredSmoke(AActor* AffectedActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hazard|Events")
	void BP_OnActorExitedSmoke(AActor* AffectedActor);

private:
	FTimerHandle PoisonTimerHandle;

	void ApplyPoisonTick();
};
