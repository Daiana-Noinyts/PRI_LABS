#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupBase.generated.h"

class USphereComponent;
class USceneComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class USoundBase;

/**
 * Base class for collectible pickups.
 * Handles trigger collision, mesh display, bobbing/spinning presentation,
 * pickup collection, audio feedback, and optional respawn.
 */
UCLASS(Abstract)
class FOREST_API APickupBase : public AActor
{
	GENERATED_BODY()

public:
	APickupBase();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	/** Return true if the pickup was successfully consumed. */
	virtual bool ApplyToCharacter(APawn* Character) PURE_VIRTUAL(APickupBase::ApplyToCharacter, return false;);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<UPointLightComponent> Glow;

	/** Fraction of the character's MAX stat restored. 1.0 = full refill. */
	UPROPERTY(EditAnywhere, Category = "Pickup|Effect", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float RestoreFraction = 1.0f;

	/** Automatically centers the mesh by offsetting by its geometric bounds. */
	UPROPERTY(EditAnywhere, Category = "Pickup|Transform")
	bool bAutoCenterMesh = true;

	UPROPERTY(EditAnywhere, Category = "Pickup|Transform")
	FVector MeshScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Pickup|Transform")
	FVector MeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Pickup|Transform")
	FRotator MeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Pickup|Presentation")
	float SpinDegreesPerSecond = 90.f;

	UPROPERTY(EditAnywhere, Category = "Pickup|Presentation")
	float BobHeight = 6.f;

	UPROPERTY(EditAnywhere, Category = "Pickup|Presentation")
	float BobSpeed = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Pickup|Respawn")
	bool bRespawn = true;

	UPROPERTY(EditAnywhere, Category = "Pickup|Respawn", meta = (EditCondition = "bRespawn", ClampMin = "0.0"))
	float RespawnDelay = 20.f;

	UPROPERTY(EditAnywhere, Category = "Pickup|Audio")
	TObjectPtr<USoundBase> PickupSound;

private:
	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void TryConsume(APawn* Pawn);
	void SetPickupActive(bool bActive);
	void Respawn() { SetPickupActive(true); }

	FTimerHandle RespawnTimer;
	TWeakObjectPtr<APawn> WaitingPawn;
	FVector VisualBaseLocation = FVector::ZeroVector;
	float Age = 0.f;
	bool bPickupActive = true;
};
