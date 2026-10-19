#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "ForestNPCCharacter.generated.h"

class UWidgetComponent;
class UAnimMontage;
class AAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCDeathSignature, AForestNPCCharacter*, DeadCharacter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCDodgeSignature, FVector, DodgeDirection, bool, bSuccess);

// Where the weapon is: on the belt, being drawn, ready in the hand, or being put away
UENUM(BlueprintType)
enum class ENPCWeaponState : uint8
{
	Sheathed  UMETA(DisplayName = "Sheathed (on belt)"),
	Drawing   UMETA(DisplayName = "Drawing"),
	Drawn     UMETA(DisplayName = "Drawn (ready)"),
	Sheathing UMETA(DisplayName = "Sheathing")
};

UCLASS()
class FOREST_API AForestNPCCharacter : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AForestNPCCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(1); } // 1 = NPC Team
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	// Movement states
	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void Walk();

	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void Run();

	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void SetSpeedCustom(float InSpeed);

	// Combat
	UFUNCTION(BlueprintCallable, Category = "NPC|Combat")
	void AttackPlayer(AActor* TargetPlayer);

	UFUNCTION(BlueprintCallable, Category = "NPC|Combat")
	bool TryDodgeAttack(AActor* Attacker, const FVector& AttackOrigin);

	UFUNCTION(BlueprintCallable, Category = "NPC|Combat")
	float CalculateDodgeProbability(float AttackerDistance) const;

	// Called when a shot is fired along a path that will hit this NPC: rolls the dodge chance
	// from the shooter's distance and, on success, side-steps just before the shot arrives
	void ReactToIncomingAttack(AActor* Attacker, const FVector& AttackOrigin, float TimeToImpact);

	UFUNCTION(BlueprintCallable, Category = "NPC|Combat")
	void Die();

	// Smoke / Hazard debuff (Obstacle system)
	UFUNCTION(BlueprintCallable, Category = "NPC|Obstacle")
	void ApplySmokeDebuff(float SlowFactor = 0.45f, bool bBlindSight = true);

	UFUNCTION(BlueprintCallable, Category = "NPC|Obstacle")
	void RemoveSmokeDebuff();

	// Stats Getters
	UFUNCTION(BlueprintPure, Category = "NPC|Stats")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "NPC|Stats")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "NPC|Stats")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "NPC|Combat")
	bool IsDying() const { return bIsDying; }

	UFUNCTION(BlueprintPure, Category = "NPC|Combat")
	bool IsDodging() const { return bIsDodging; }

	UFUNCTION(BlueprintPure, Category = "NPC|Obstacle")
	bool IsInSmoke() const { return bIsInSmoke; }

	UFUNCTION(BlueprintPure, Category = "NPC|Combat")
	float GetWeaponEffectiveRange() const { return WeaponEffectiveRange; }

	// ---- Weapon interaction: belt <-> hand ----
	// Stops, plays the draw animation, takes the weapon from the belt into the hand and holds it there
	UFUNCTION(BlueprintCallable, Category = "NPC|Weapon")
	void DrawWeapon();

	// Stops, plays the sheath animation and puts the weapon back on the belt
	UFUNCTION(BlueprintCallable, Category = "NPC|Weapon")
	void SheathWeapon();

	UFUNCTION(BlueprintPure, Category = "NPC|Weapon")
	ENPCWeaponState GetWeaponState() const { return WeaponState; }

	UFUNCTION(BlueprintPure, Category = "NPC|Weapon")
	bool IsWeaponReady() const { return WeaponState == ENPCWeaponState::Drawn; }

	UFUNCTION(BlueprintPure, Category = "NPC|Weapon")
	bool IsChangingWeapon() const { return WeaponState == ENPCWeaponState::Drawing || WeaponState == ENPCWeaponState::Sheathing; }

	// Player closer than this (and seen) -> draw the weapon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Weapon")
	float DrawWeaponRange = 1000.0f;

	// Player further than this, or lost -> put the weapon away
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Weapon")
	float SheathWeaponRange = 1600.0f;

protected:
	virtual void BeginPlay() override;

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Components")
	UStaticMeshComponent* WeaponMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Components")
	UWidgetComponent* HealthWidgetComponent;

	// Socket for weapon attachment (in the hand)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat")
	FName WeaponSocketName = FName("WeaponSocket");

	// Bone (or socket) on the belt where the weapon is carried when not in use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Weapon")
	FName SheathSocketName = FName("Hips");

	// Weapon placement relative to SheathSocketName (taken from the draw animation at the grab frame)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Weapon")
	FTransform SheathAttachOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Weapon")
	UAnimMontage* DrawWeaponMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Weapon")
	UAnimMontage* SheathWeaponMontage;

	// Point in the draw animation (0..1) where the hand grabs the weapon from the belt
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Weapon", meta = (ClampMin = "0", ClampMax = "1"))
	float DrawGrabFraction = 0.4f;

	// Point in the sheath animation (0..1) where the hand lets go of the weapon on the belt
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Weapon", meta = (ClampMin = "0", ClampMax = "1"))
	float SheathReleaseFraction = 0.6f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Weapon")
	ENPCWeaponState WeaponState = ENPCWeaponState::Drawn;

	// Stats
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Stats")
	float CurrentHealth = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Stats")
	float MaxHealth = 120.0f;

	// Movement Speeds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Movement")
	float WalkSpeed = 150.0f;

	// Matches the run sample in BS_NPC_Locomotion (380, played at 1.35x so the feet keep up)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Movement")
	float RunSpeed = 380.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
	bool bIsRunning = false;

	// Combat parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat")
	float WeaponEffectiveRange = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat")
	float AttackDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat")
	float AttackCooldown = 1.2f;

	// Dodge parameters.
	// Chance = MinDodgeProbability at MinDodgeDistance, rising linearly (proportionally to the attack
	// distance) up to MaxDodgeProbability at MaxDodgeDistance. See CalculateDodgeProbability().
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat|Dodge")
	float MinDodgeDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat|Dodge")
	float MaxDodgeDistance = 2000.0f;

	// Base chance: dodge chance for point-blank attacks
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat|Dodge")
	float MinDodgeProbability = 0.05f;

	// Upper cap reached at MaxDodgeDistance and beyond
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat|Dodge")
	float MaxDodgeProbability = 0.60f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat|Dodge")
	float DodgeImpulseStrength = 800.0f;

	// How many seconds before an incoming shot arrives the NPC side-steps
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat|Dodge")
	float DodgeReactionLead = 0.6f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Combat|Dodge")
	bool bIsDodging = false;

	// Obstacle / Smoke status
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Obstacle")
	bool bIsInSmoke = false;

	// Speed multiplier applied while inside smoke (set by the hazard zone)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Obstacle")
	float SmokeSlowFactor = 1.0f;

	// Montages
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
	UAnimMontage* AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
	UAnimMontage* DodgeMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
	UAnimMontage* HitMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
	UAnimMontage* DeathMontage;

	// Death state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Combat")
	bool bIsDying = false;

	// Events for Blueprint hooks
	UPROPERTY(BlueprintAssignable, Category = "NPC|Events")
	FOnNPCDeathSignature OnNPCDeath;

	UPROPERTY(BlueprintAssignable, Category = "NPC|Events")
	FOnNPCDodgeSignature OnNPCDodge;

	UFUNCTION(BlueprintImplementableEvent, Category = "NPC|Events")
	void BP_OnAttackExecuted();

	UFUNCTION(BlueprintImplementableEvent, Category = "NPC|Events")
	void BP_OnDodgeExecuted(const FVector& DodgeDir);

	UFUNCTION(BlueprintImplementableEvent, Category = "NPC|Events")
	void BP_OnSmokeEntered();

	UFUNCTION(BlueprintImplementableEvent, Category = "NPC|Events")
	void BP_OnSmokeExited();

	UFUNCTION(BlueprintImplementableEvent, Category = "NPC|Events")
	void BP_OnHealthChanged(float NewHealth, float InMaxHealth);

private:
	FTimerHandle DodgeResetTimerHandle;
	FTimerHandle PendingDodgeTimerHandle;
	FTimerHandle AttackCooldownTimerHandle;
	FVector PendingDodgeOrigin = FVector::ZeroVector;
	bool bCanAttack = true;

	bool RollDodge(const FVector& AttackOrigin) const;
	void PerformDodge(const FVector& AttackOrigin);
	void PerformPendingDodge();
	void ResetDodgeState();

	FTimerHandle WeaponSwapTimerHandle;
	FTimerHandle WeaponFinishTimerHandle;
	void AttachWeaponToSocket(FName Socket);
	void PauseAIForWeapon();
	void ResumeAIAfterWeapon();
	void OnWeaponGrabbed();
	void OnWeaponStowed();
	void FinishWeaponChange();
	void ResetAttackCooldown();
	float GetSpeedMultiplier() const { return bIsInSmoke ? SmokeSlowFactor : 1.0f; }
};
