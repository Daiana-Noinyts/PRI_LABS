#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GenericTeamAgentInterface.h"
#include "AnimNotify_HunterReload.h"
#include "HunterCharacter.generated.h"

UCLASS()
class FOREST_API AHunterCharacter : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AHunterCharacter();

	// IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(0); }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float HealthCounter = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 BulletCounter = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 MaxBullets = 10;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Stats")
	void AddHealth(float Amount);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Stats")
	void AddBullets(int32 Amount);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UUserWidget> HUDWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	class UUserWidget* HUDWidgetInstance;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat")
	void TakeHit(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyDamageDirect(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddHealthDirect(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddBulletsDirect(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateHUD();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void PlayDamageFlash();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamageCustom(float DamageAmount);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stats")
	void OnStatsChanged();

	TArray<class UUserWidget*> GetAllHUDWidgets();




protected:
	virtual void BeginPlay() override;
	void MoveForward(float value);
	void MoveRight(float value);

	UFUNCTION()
	void Fire();

	UPROPERTY(VisibleAnywhere)
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* GunMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MuzzleOffset = FVector(30.f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<class AProjectile> ProjectileClass;

	// ---------------- Reload (key R) ----------------
	// Phases: lower the weapon -> load ammunition -> raise it back to the rest pose.
	// With ReloadMontage set: the montage plays on the first-person mesh and its notifies
	// (TakeMagazine, ThrowMagazine, TakeMagazine_FromPocket, InsertMagazine, EndReloading) drive the
	// magazine: out of the gun into the left hand, thrown away, new one from the pocket, inserted.
	// Without a montage: a fully procedural fallback that loads shells one by one.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	class UAnimMontage* ReloadMontage;

	// Magazine in the gun / in the left hand during the reload
	UPROPERTY(VisibleAnywhere, Category = "Weapon|Reload")
	class UStaticMeshComponent* GunMagazine;

	UPROPERTY(VisibleAnywhere, Category = "Weapon|Reload")
	class UStaticMeshComponent* HandMagazine;

	// Bone of the first-person mesh that holds the magazine, and the magazine offset from it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	FName HandMagazineBone = FName("hand_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	FVector HandMagazineOffset = FVector(2.0f, 7.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	FRotator HandMagazineRotation = FRotator(0.0f, 0.0f, 90.0f);

	// Colour of both magazines (in the gun and in the hand)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	FLinearColor MagazineColor = FLinearColor(0.03f, 0.18f, 0.70f);

	// The new magazine is inserted as soon as the hand brings it this close (cm) to the magazine well,
	// instead of waiting for a fixed moment of the animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	float InsertDistance = 12.0f;

	// Seconds to lower the weapon / raise it back
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	float ReloadLowerTime = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	float ReloadRaiseTime = 0.45f;

	// Seconds per loaded shell (appear -> slide up -> push into the port)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	float RoundInsertTime = 0.55f;

	// Most shells shown being loaded (ammo is split between them)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	int32 MaxVisibleRounds = 4;

	// Tilt of the lowered weapon, relative to the rest pose (camera space).
	// Rolled so the underside (loading port) turns towards the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	FRotator ReloadLoweredRotation = FRotator(-18.0f, 8.0f, 30.0f);

	// Extra manual offset of the lowered weapon (camera space), added to the automatic one
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	FVector ReloadLoweredOffset = FVector::ZeroVector;

	// Loading port on the gun mesh, in the gun's local space (underside of the receiver)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	FVector ReloadPortLocal = FVector(-6.0f, 0.9f, -18.0f);

	// Where the loading port is brought on screen while loading (camera space: forward, right, up)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	FVector ReloadPortScreenTarget = FVector(42.0f, 4.0f, -11.0f);

	// The shell that is spawned and pushed into the gun: red plastic body + brass base
	UPROPERTY(VisibleAnywhere, Category = "Weapon|Reload")
	class USceneComponent* ReloadShellRoot;

	UPROPERTY(VisibleAnywhere, Category = "Weapon|Reload")
	class UStaticMeshComponent* ReloadShellBody;

	UPROPERTY(VisibleAnywhere, Category = "Weapon|Reload")
	class UStaticMeshComponent* ReloadShellBase;

public:

	// Plays the reload animation and loads Amount bullets (capped at MaxBullets). Called by ammo pickups.
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ReloadWithAmmo(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bIsReloading; }

	// Manual reload on a key (R / gamepad X): tops the magazine up to the maximum
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ReloadPressed();

	// Called by the reload montage notifies (and by a timer backup for each notify)
	void HandleReloadStage(EHunterReloadStage Stage);

	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnFireEvent();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	bool bIsReloading = false;
	float ReloadElapsed = 0.0f;
	int32 ReloadInserts = 0;      // visible rounds in this reload
	int32 ReloadInsertsDone = 0;  // rounds already in the gun
	int32 ReloadAmmoToAdd = 0;    // bullets still to add

	FVector WeaponRestLocation = FVector::ZeroVector;
	FRotator WeaponRestRotation = FRotator::ZeroRotator;
	FVector ReloadAutoOffset = FVector::ZeroVector; // brings the loading port to ReloadPortScreenTarget

	// Where the magazine sits in the gun. Taken from the GunMagazine component as placed in the Blueprint
	// viewport, so it can be moved there by hand instead of being overwritten by a hard-coded number.
	FTransform GunMagazineRest = FTransform::Identity;
	float PocketStageTime = 0.0f; // ReloadElapsed when the new magazine was taken from the pocket

	// Montage reload state
	bool bMontageReload = false;
	bool bRaising = false;
	float RaiseElapsed = 0.0f;
	float PoseScale = 1.0f;     // how far the rig is lowered (montage path lowers less)
	uint8 StagesDone = 0;       // bit per EHunterReloadStage
	TArray<FTimerHandle> StageTimers;

	UPROPERTY()
	class UAnimationAsset* IdleAnimation = nullptr;

	// The gun is re-parented to the right hand while the left hand handles the magazine
	FName GunRestSocket = NAME_None;
	FTransform GunRestRelative = FTransform::Identity;

	void RunReloadStage(EHunterReloadStage Stage);
	void DropOldMagazine();
	void UpdateMontageReload(float DeltaTime);

	FVector GetReloadPortWorld() const;
	void ShowShell(bool bVisible);
	void UpdateReload(float DeltaTime);
	void SetWeaponPose(float LowerAlpha, float ExtraPitch);
	void FinishReload();
};