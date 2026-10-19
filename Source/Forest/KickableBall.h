#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KickableBall.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * Physics interaction #2 - KICK. Light, bouncy simulated ball. Running into it at speed launches it
 * away from the player (velocity-change impulse, so mass doesn't matter); walking slowly just rolls it.
 */
UCLASS()
class FOREST_API AKickableBall : public AActor
{
	GENERATED_BODY()

public:
	AKickableBall();
	virtual void PostInitializeComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
		FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	UPROPERTY(VisibleAnywhere, Category = "Ball") TObjectPtr<USphereComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Ball") TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(EditAnywhere, Category = "Ball|Look") FLinearColor BallColor = FLinearColor(0.9f, 0.45f, 0.08f);

	UPROPERTY(EditAnywhere, Category = "Ball|Physics", meta = (ClampMin = "0.1")) float MassKg = 12.f;
	UPROPERTY(EditAnywhere, Category = "Ball|Physics", meta = (ClampMin = "0.0", ClampMax = "1.0")) float Bounciness = 0.7f;
	UPROPERTY(EditAnywhere, Category = "Ball|Physics") float Friction = 0.5f;

	/** Player must be moving at least this fast (cm/s, horizontal) to kick rather than nudge. */
	UPROPERTY(EditAnywhere, Category = "Ball|Kick") float MinKickSpeed = 250.f;
	/** Ball launch speed = player speed * this, clamped to MaxKickSpeed. */
	UPROPERTY(EditAnywhere, Category = "Ball|Kick") float KickSpeedMultiplier = 2.2f;
	UPROPERTY(EditAnywhere, Category = "Ball|Kick") float MaxKickSpeed = 1600.f;
	/** Extra upward speed added to every kick. */
	UPROPERTY(EditAnywhere, Category = "Ball|Kick") float LoftSpeed = 280.f;
	UPROPERTY(EditAnywhere, Category = "Ball|Kick") float KickCooldown = 0.25f;

private:
	double LastKickTime = -100.0;
};
