#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PushableCrate.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UStaticMesh;

/**
 * Physics interaction #1 - PUSH. Heavy simulated crate. The character's capsule pushes it by walking into it
 * (CharacterMovement::bEnablePhysicsInteraction). It stays upright and slides; non-physics movers such as
 * projectiles nudge it on impact.
 */
UCLASS()
class FOREST_API APushableCrate : public AActor
{
	GENERATED_BODY()

public:
	APushableCrate();
	virtual void PostInitializeComponents() override;

protected:
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
		FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/** The simulated body. A box is used instead of the mesh's own collision so it always works. */
	UPROPERTY(VisibleAnywhere, Category = "Crate") TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Crate") TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(EditAnywhere, Category = "Crate|Physics", meta = (ClampMin = "1.0")) float MassKg = 90.f;
	UPROPERTY(EditAnywhere, Category = "Crate|Physics") float LinearDamping = 1.2f;
	UPROPERTY(EditAnywhere, Category = "Crate|Physics") float AngularDamping = 4.f;

	/** Impulse (kg*cm/s) per cm/s of the hitting object's speed. */
	UPROPERTY(EditAnywhere, Category = "Crate|Impact") float ImpactImpulseScale = 3.f;
	UPROPERTY(EditAnywhere, Category = "Crate|Impact") float MinImpactSpeed = 500.f;
};
