#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TeleporterPad.generated.h"

class UBoxComponent;
class USceneComponent;
class UPointLightComponent;
class USoundBase;
class ACharacter;

/**
 * Trigger-based teleporter. Place two pads far apart, set Destination on one and (with bLinkBothWays)
 * the editor links the other automatically. Walking into a pad's trigger volume moves the player to its Destination.
 */
class UStaticMeshComponent;

UCLASS()
class FOREST_API ATeleporterPad : public AActor
{
	GENERATED_BODY()

public:
	ATeleporterPad();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void SetTeleporterEnabled(bool bEnabled) { bTeleporterEnabled = bEnabled; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleporter") TObjectPtr<ATeleporterPad> Destination;
	UPROPERTY(EditAnywhere, Category = "Teleporter") bool bLinkBothWays = true;
	UPROPERTY(EditAnywhere, Category = "Teleporter") bool bTeleporterEnabled = true;

	/** Where the player lands, relative to this pad's origin, when it is used as a destination (capsule half-height is added). */
	UPROPERTY(EditAnywhere, Category = "Teleporter") FVector ArrivalOffset = FVector(0.f, 0.f, 10.f);
	/** Face the same way as the destination pad after arriving (control rotation is turned by the same amount). */
	UPROPERTY(EditAnywhere, Category = "Teleporter") bool bAlignToDestinationFacing = true;
	UPROPERTY(EditAnywhere, Category = "Teleporter") bool bPreserveVelocity = false;
	UPROPERTY(EditAnywhere, Category = "Teleporter", meta = (ClampMin = "0.0")) float FadeInDuration = 0.4f;
	/** Only used for a warning: pads closer than this (cm) aren't a 'significant distance'. */
	UPROPERTY(EditAnywhere, Category = "Teleporter") float MinRecommendedDistance = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Teleporter|Look") FLinearColor PadColor = FLinearColor(0.15f, 0.55f, 1.f);
	UPROPERTY(EditAnywhere, Category = "Teleporter|Audio") TObjectPtr<USoundBase> TeleportSound;

	UPROPERTY(VisibleAnywhere, Category = "Teleporter") TObjectPtr<UBoxComponent> Trigger;

private:
	UFUNCTION() void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Teleport(ACharacter* Character);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Rim;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY() TObjectPtr<USceneComponent> Spinner;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Crystal;
	UPROPERTY() TObjectPtr<UPointLightComponent> Glow;

	/** Actors that just arrived here; ignored until they leave the trigger so they don't bounce straight back. */
	TMap<TWeakObjectPtr<AActor>, double> Suppressed;
	float Age = 0.f;
};
