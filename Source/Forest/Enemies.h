#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/TimerHandle.h"
#include "Enemies.generated.h"

class UAnimationAsset;
class UParticleSystem;
class USoundBase;

UCLASS()
class FOREST_API AEnemies : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemies();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement")
	float MovementSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement")
	float AttackDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (MakeEditWidget = true))
	TArray<FVector> TargetPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Animation")
	UAnimationAsset* WalkAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Animation")
	UAnimationAsset* AttackAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Animation")
	UAnimationAsset* DeathAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Effects")
	UParticleSystem* DeathVFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Effects")
	USoundBase* DeathSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|State")
	bool bIsDying;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float AttackHitDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float TimeBeforeAnimEndToHit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float AttackInterval;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|State")
	bool bIsAttacking;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float AttackReach;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float AttackFrontHalfAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float PointBlankRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float DetectionRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float DamageAmount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float MaxHealth;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Stats")
	void TakeDamageCustom(float DamageAmountIn);

	virtual float TakeDamage(float DamageAmountIn, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Stats")
	void UpdateHealthBar();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void Die();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	FVector GetAttackImpactLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	FVector GetAttackBodyLocation() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Combat")
	void OnAttackHit();

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

protected:
	virtual void BeginPlay() override;

private:
	void MoveToNextPoint(float DeltaTime);
	void PlayAttackAnimation();
	void OnAttackAnimationFinished();
	void TriggerAttackHit();
	void DeactivateAttackHit();
	void CheckAndApplyAttackDamage();
	void DrawDebugFrontalCone(UWorld* World, const FVector& Origin, const FVector& Forward, float Reach, float HalfAngleDeg, const FColor& Color, float LifeTime, float Thickness);

	TArray<FVector> WorldTargetPoints;
	int32 CurrentTargetIndex;
	float TimeSinceLastAttack;
	float TimeInAttack;
	bool bHasLeaped;
	bool bIsMoving;
	bool bIsWalking;
	bool bAttackHitActive;
	bool bHitDealtThisAttack;
	bool bHealthBarInitialized;
	FTimerHandle AnimationTimerHandle;
	FTimerHandle AttackHitTimerHandle;
	FTimerHandle AttackHitActiveTimerHandle;
};