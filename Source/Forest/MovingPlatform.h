#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MovingPlatform.generated.h"

class UStaticMeshComponent;

/**
 * Physics interaction #3 - RIDE. A kinematic platform that shuttles between its start and start+TargetOffset.
 * The character standing on it is carried automatically (CharacterMovement based-movement).
 */
UCLASS()
class FOREST_API AMovingPlatform : public AActor
{
	GENERATED_BODY()

public:
	AMovingPlatform();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Platform")
	void SetPlatformMoving(bool bMove) { bMoving = bMove; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Platform") TObjectPtr<UStaticMeshComponent> Slab;

	/** End point relative to the actor's placed transform (local space, cm). Drag the gizmo in the viewport. */
	UPROPERTY(EditAnywhere, Category = "Platform", meta = (MakeEditWidget = true)) FVector TargetOffset = FVector(0.f, 0.f, 400.f);
	/** Seconds for one leg of the trip. */
	UPROPERTY(EditAnywhere, Category = "Platform", meta = (ClampMin = "0.1")) float TravelTime = 4.f;
	UPROPERTY(EditAnywhere, Category = "Platform", meta = (ClampMin = "0.0")) float PauseAtEnds = 1.5f;
	UPROPERTY(EditAnywhere, Category = "Platform") bool bMoving = true;
	UPROPERTY(EditAnywhere, Category = "Platform|Look") FLinearColor SlabColor = FLinearColor(0.32f, 0.20f, 0.10f);

private:
	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;
	float Progress = 0.f;   // 0 = start, 1 = end
	float Direction = 1.f;
	float PauseRemaining = 0.f;
};
