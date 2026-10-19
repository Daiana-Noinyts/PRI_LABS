#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestTeleporter.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;
class UParticleSystem;

UCLASS()
class FOREST_API AForestTeleporter : public AActor
{
	GENERATED_BODY()
	
public:	
	AForestTeleporter();

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport")
	AActor* TargetDestination;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	FVector TargetOffset = FVector(0.0f, 0.0f, 50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	float CooldownDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport|Effects")
	USoundBase* TeleportSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport|Effects")
	UParticleSystem* TeleportFX;

	void StartCooldown();
	bool IsOnCooldown() const { return bIsOnCooldown; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TeleportTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bIsOnCooldown = false;
	FTimerHandle CooldownTimerHandle;

	void ResetCooldown();
};
