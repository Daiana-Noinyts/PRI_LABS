#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystem.h"
#include "Projectile.generated.h"

UCLASS()
class FOREST_API AProjectile : public AActor
{
	GENERATED_BODY()

public:
	AProjectile();

	virtual void Tick(float DeltaTime) override;

	void FireInDirection(const FVector& Direction, float DistanceToTarget);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleDefaultsOnly, Category = Projectile)
	USphereComponent* CollisionComponent;

	UPROPERTY(EditAnywhere, Category = "Effects")
	class UMaterialInterface* HitDecalMaterial;

	UPROPERTY(EditAnywhere, Category = "Effects")
	FVector DecalSize = FVector(30.f, 15.f, 15.f);

	UPROPERTY(EditAnywhere, Category = "Effects")
	float DecalLifeSpan = 30.f;

	UPROPERTY(VisibleAnywhere, Category = Movement)
	UProjectileMovementComponent* ProjectileMovementComponent;

	UPROPERTY(VisibleAnywhere, Category = Projectile)
	UStaticMeshComponent* ProjectileMeshComponent;

	UPROPERTY(EditAnywhere, Category = Projectile)
	float LifeSpan = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Effects")
	UParticleSystem* HitVFX;

	UPROPERTY(EditAnywhere, Category = "Trajectory")
	float SpiralRadius = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory")
	float NumSpiralTurns = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float Damage = 20.0f;

private:
	FVector ComputeOffset(float Time) const;
	void WarnNPCsInPath();

	// NPCs notice a shot this many seconds before it would reach them
	float DodgeWarningTime = 1.0f;
	FVector ShooterLocation;
	TArray<TWeakObjectPtr<AActor>> WarnedNPCs;
	bool bHitExecuted = false;

	FVector StartLocation;
	FVector ForwardDir;
	FVector RightDir;
	FVector UpDir;

	float TotalFlightTime;
	float AngularSpeed;
	float ElapsedTime;
	bool bLaunched;
};