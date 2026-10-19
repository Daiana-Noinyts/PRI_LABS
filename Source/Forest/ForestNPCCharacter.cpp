#include "ForestNPCCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "ForestAIController.h"
#include "BrainComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Projectile.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

AForestNPCCharacter::AForestNPCCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Weapon mesh attached to character skeleton socket
	WeaponMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComponent"));
	WeaponMeshComponent->SetupAttachment(GetMesh(), WeaponSocketName);
	WeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMeshComponent->SetCastShadow(true);

	// Dedicated overhead Health Bar widget component
	HealthWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidgetComponent"));
	HealthWidgetComponent->SetupAttachment(RootComponent);
	HealthWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	HealthWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidgetComponent->SetDrawSize(FVector2D(140.0f, 18.0f));

	// Movement settings
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = true;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->MaxWalkSpeed = WalkSpeed;
		MoveComp->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	}

	bUseControllerRotationYaw = false;
}

void AForestNPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	// The weapon starts on the belt; without a belt socket it simply stays in the hand
	if (GetMesh() && WeaponMeshComponent)
	{
		if (GetMesh()->DoesSocketExist(SheathSocketName))
		{
			AttachWeaponToSocket(SheathSocketName);
			WeaponState = ENPCWeaponState::Sheathed;
		}
		else
		{
			AttachWeaponToSocket(WeaponSocketName);
			WeaponState = ENPCWeaponState::Drawn;
		}
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
	}

	BP_OnHealthChanged(CurrentHealth, MaxHealth);
}

void AForestNPCCharacter::AttachWeaponToSocket(FName Socket)
{
	if (GetMesh() && WeaponMeshComponent && GetMesh()->DoesSocketExist(Socket))
	{
		WeaponMeshComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);

		// On the belt: offset from the hip bone so the weapon hangs where the hand grabs it
		if (Socket == SheathSocketName)
		{
			WeaponMeshComponent->SetRelativeLocationAndRotation(SheathAttachOffset.GetLocation(), SheathAttachOffset.GetRotation());
		}
	}
}

void AForestNPCCharacter::PauseAIForWeapon()
{
	// Stand still while the weapon is being drawn / put away
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->PauseLogic(TEXT("Weapon"));
		}
		if (UPathFollowingComponent* PathFollowing = AIC->GetPathFollowingComponent())
		{
			PathFollowing->PauseMove(FAIRequestID::CurrentRequest, EPathFollowingVelocityMode::Reset);
		}
	}
}

void AForestNPCCharacter::ResumeAIAfterWeapon()
{
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UPathFollowingComponent* PathFollowing = AIC->GetPathFollowingComponent())
		{
			PathFollowing->ResumeMove(FAIRequestID::CurrentRequest);
		}
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->ResumeLogic(TEXT("Weapon"));
		}
	}
}

void AForestNPCCharacter::DrawWeapon()
{
	if (WeaponState != ENPCWeaponState::Sheathed || bIsDying || bIsDodging)
	{
		return;
	}

	WeaponState = ENPCWeaponState::Drawing;
	PauseAIForWeapon();

	const float Length = DrawWeaponMontage ? PlayAnimMontage(DrawWeaponMontage) : 0.0f;
	if (Length <= 0.0f)
	{
		// No animation: switch instantly
		OnWeaponGrabbed();
		FinishWeaponChange();
		return;
	}

	// The weapon moves from the belt to the hand at the moment the hand grabs it in the animation
	GetWorldTimerManager().SetTimer(WeaponSwapTimerHandle, this, &AForestNPCCharacter::OnWeaponGrabbed, Length * DrawGrabFraction, false);
	GetWorldTimerManager().SetTimer(WeaponFinishTimerHandle, this, &AForestNPCCharacter::FinishWeaponChange, Length, false);
	UE_LOG(LogTemp, Warning, TEXT("[Weapon] Drawing (%.2fs)"), Length);
}

void AForestNPCCharacter::SheathWeapon()
{
	if (WeaponState != ENPCWeaponState::Drawn || bIsDying || bIsDodging || !GetMesh()->DoesSocketExist(SheathSocketName))
	{
		return;
	}

	WeaponState = ENPCWeaponState::Sheathing;
	PauseAIForWeapon();

	const float Length = SheathWeaponMontage ? PlayAnimMontage(SheathWeaponMontage) : 0.0f;
	if (Length <= 0.0f)
	{
		OnWeaponStowed();
		FinishWeaponChange();
		return;
	}

	GetWorldTimerManager().SetTimer(WeaponSwapTimerHandle, this, &AForestNPCCharacter::OnWeaponStowed, Length * SheathReleaseFraction, false);
	GetWorldTimerManager().SetTimer(WeaponFinishTimerHandle, this, &AForestNPCCharacter::FinishWeaponChange, Length, false);
	UE_LOG(LogTemp, Warning, TEXT("[Weapon] Sheathing (%.2fs)"), Length);
}

void AForestNPCCharacter::OnWeaponGrabbed()
{
	AttachWeaponToSocket(WeaponSocketName);
}

void AForestNPCCharacter::OnWeaponStowed()
{
	AttachWeaponToSocket(SheathSocketName);
}

void AForestNPCCharacter::FinishWeaponChange()
{
	if (WeaponState == ENPCWeaponState::Drawing)
	{
		WeaponState = ENPCWeaponState::Drawn;
	}
	else if (WeaponState == ENPCWeaponState::Sheathing)
	{
		WeaponState = ENPCWeaponState::Sheathed;
	}

	if (!bIsDying)
	{
		ResumeAIAfterWeapon();
	}
}

void AForestNPCCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AForestNPCCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

ETeamAttitude::Type AForestNPCCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(&Other))
	{
		// Player is Team 0 -> Hostile
		if (TeamAgent->GetGenericTeamId() == FGenericTeamId(0))
		{
			return ETeamAttitude::Hostile;
		}
		// Fellow NPCs or Team 1 -> Friendly
		if (TeamAgent->GetGenericTeamId() == GetGenericTeamId())
		{
			return ETeamAttitude::Friendly;
		}
	}
	return ETeamAttitude::Neutral;
}

void AForestNPCCharacter::Walk()
{
	bIsRunning = false;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed * GetSpeedMultiplier();
	}
}

void AForestNPCCharacter::Run()
{
	bIsRunning = true;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = RunSpeed * GetSpeedMultiplier();
	}
}

void AForestNPCCharacter::SetSpeedCustom(float InSpeed)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = InSpeed * GetSpeedMultiplier();
	}
}

float AForestNPCCharacter::CalculateDodgeProbability(float AttackerDistance) const
{
	// Success probability increases proportionally to the distance of the player's attack:
	//   P(d) = MinProb + (MaxProb - MinProb) * (d - MinDist) / (MaxDist - MinDist), clamped to [MinDist, MaxDist]
	// With the defaults: 2 m -> 5%, 5 m -> 14%, 10 m -> 29%, 15 m -> 43%, 20 m+ -> 60%
	const float ClampedDist = FMath::Clamp(AttackerDistance, MinDodgeDistance, MaxDodgeDistance);
	const float Alpha = (ClampedDist - MinDodgeDistance) / FMath::Max(MaxDodgeDistance - MinDodgeDistance, 1.0f);
	return FMath::Lerp(MinDodgeProbability, MaxDodgeProbability, Alpha);
}

bool AForestNPCCharacter::RollDodge(const FVector& AttackOrigin) const
{
	const float Distance = FVector::Dist(GetActorLocation(), AttackOrigin);
	const float DodgeProb = CalculateDodgeProbability(Distance);
	const float Roll = FMath::FRand();
	const bool bSuccess = Roll <= DodgeProb;

	UE_LOG(LogTemp, Warning, TEXT("[Dodge] Dist=%.0f Prob=%.2f Roll=%.2f -> %s"), Distance, DodgeProb, Roll, bSuccess ? TEXT("DODGE") : TEXT("hit"));
	return bSuccess;
}

bool AForestNPCCharacter::TryDodgeAttack(AActor* Attacker, const FVector& AttackOrigin)
{
	if (bIsDodging || bIsDying || bIsInSmoke)
	{
		return false;
	}

	if (RollDodge(AttackOrigin))
	{
		PerformDodge(AttackOrigin);
		return true;
	}

	OnNPCDodge.Broadcast(FVector::ZeroVector, false);
	return false;
}

void AForestNPCCharacter::ReactToIncomingAttack(AActor* Attacker, const FVector& AttackOrigin, float TimeToImpact)
{
	// A blinded NPC (smoke) cannot see the shot coming; only one dodge can be queued at a time
	if (bIsDodging || bIsDying || bIsInSmoke || IsChangingWeapon() || GetWorldTimerManager().IsTimerActive(PendingDodgeTimerHandle))
	{
		return;
	}

	// Chance is decided by how far away the shooter is (further = easier to react)
	if (!RollDodge(AttackOrigin))
	{
		OnNPCDodge.Broadcast(FVector::ZeroVector, false);
		return;
	}

	PendingDodgeOrigin = AttackOrigin;
	// Wait until the shot is close, then side-step out of its path
	const float Delay = TimeToImpact - DodgeReactionLead;
	if (Delay <= KINDA_SMALL_NUMBER)
	{
		PerformDodge(AttackOrigin);
	}
	else
	{
		GetWorldTimerManager().SetTimer(PendingDodgeTimerHandle, this, &AForestNPCCharacter::PerformPendingDodge, Delay, false);
	}
}

void AForestNPCCharacter::PerformPendingDodge()
{
	if (!bIsDying)
	{
		PerformDodge(PendingDodgeOrigin);
	}
}

void AForestNPCCharacter::PerformDodge(const FVector& AttackOrigin)
{
	bIsDodging = true;

	// Calculate lateral evade direction
	const FVector ThreatDir = (GetActorLocation() - AttackOrigin).GetSafeNormal2D();
	const FVector RightDir = FVector::CrossProduct(ThreatDir, FVector::UpVector);
	// Randomly dodge left or right
	const float SideSign = (FMath::RandBool()) ? 1.0f : -1.0f;
	const FVector DodgeDirection = (RightDir * SideSign + ThreatDir * 0.2f).GetSafeNormal();

	// Freeze the AI for the duration of the dodge so new move requests don't cancel the side-step
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->PauseLogic(TEXT("Dodge"));
		}
		if (UPathFollowingComponent* PathFollowing = AIC->GetPathFollowingComponent())
		{
			PathFollowing->PauseMove(FAIRequestID::CurrentRequest, EPathFollowingVelocityMode::Keep);
		}
	}

	// The dive animation goes forward, so the body must face the dive direction for the whole dive.
	// Drop the AI focus on the target and point the control rotation along the dive, otherwise the
	// controller keeps turning him back to the player and the dive plays sideways.
	const FRotator DiveRotation(0.0f, DodgeDirection.Rotation().Yaw, 0.0f);
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
		AIC->ClearFocus(EAIFocusPriority::Move);
		AIC->SetControlRotation(DiveRotation);
	}
	SetActorRotation(DiveRotation);
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = false;
	}

	// Physical dive out of the line of fire
	LaunchCharacter(DodgeDirection * DodgeImpulseStrength + FVector(0.0f, 0.0f, 150.0f), true, true);

	float DodgeDuration = 0.9f;
	if (DodgeMontage)
	{
		const float MontageLength = PlayAnimMontage(DodgeMontage);
		if (MontageLength > 0.0f)
		{
			// Stay in the dodge (untouchable, AI paused) while the dive plays, within sensible limits
			DodgeDuration = FMath::Clamp(MontageLength, 0.6f, 1.6f);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Dodge] Dive performed (%.2fs)"), DodgeDuration);

	BP_OnDodgeExecuted(DodgeDirection);
	OnNPCDodge.Broadcast(DodgeDirection, true);

	// Reset dodge state after recovery window (NPC is untouchable while dodging)
	GetWorldTimerManager().SetTimer(DodgeResetTimerHandle, this, &AForestNPCCharacter::ResetDodgeState, DodgeDuration, false);
}

void AForestNPCCharacter::ResetDodgeState()
{
	bIsDodging = false;

	// Face the target again (controller focus drives rotation)
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = true;
	}

	if (AForestAIController* ForestAIC = Cast<AForestAIController>(GetController()))
	{
		// Look at the player again if he is still in sight
		if (ForestAIC->HasLineOfSight() && ForestAIC->GetPerceivedActor())
		{
			ForestAIC->SetFocus(ForestAIC->GetPerceivedActor());
		}
	}

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UPathFollowingComponent* PathFollowing = AIC->GetPathFollowingComponent())
		{
			PathFollowing->ResumeMove(FAIRequestID::CurrentRequest);
		}
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->ResumeLogic(TEXT("Dodge"));
		}
	}
}

float AForestNPCCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDying)
	{
		return 0.0f;
	}

	// Use the SHOOTER's position for distance, not the projectile's
	FVector AttackOrigin = GetActorLocation();
	AActor* Attacker = nullptr;
	if (EventInstigator && EventInstigator->GetPawn())
	{
		Attacker = EventInstigator->GetPawn();
		AttackOrigin = Attacker->GetActorLocation();
	}
	else if (DamageCauser)
	{
		Attacker = DamageCauser;
		AttackOrigin = DamageCauser->GetActorLocation();
	}

	// Mid-dodge the NPC is out of the line of fire: no damage, no hit reaction
	if (bIsDodging)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPC] Damage %.1f BLOCKED (dodging)"), DamageAmount);
		return 0.0f;
	}

	// Projectiles already rolled their dodge when fired (ReactToIncomingAttack); poison has no instigator and can't be dodged
	const bool bCanDodge = !Cast<AProjectile>(DamageCauser) && EventInstigator && EventInstigator->GetPawn();

	// Check if NPC successfully dodges incoming attack
	if (bCanDodge && TryDodgeAttack(Attacker, AttackOrigin))
	{
		// Damage completely avoided via agile dodge!
		return 0.0f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	CurrentHealth = FMath::Clamp(CurrentHealth - ActualDamage, 0.0f, MaxHealth);
	UE_LOG(LogTemp, Warning, TEXT("[NPC] Took %.1f damage, health %.0f/%.0f"), ActualDamage, CurrentHealth, MaxHealth);
	BP_OnHealthChanged(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
	else if (HitMontage && !bIsDodging)
	{
		PlayAnimMontage(HitMontage);
	}

	return ActualDamage;
}

void AForestNPCCharacter::AttackPlayer(AActor* TargetPlayer)
{
	if (!bCanAttack || bIsDying || bIsDodging || !IsWeaponReady() || !TargetPlayer)
	{
		return;
	}

	bCanAttack = false;

	if (AttackMontage)
	{
		PlayAnimMontage(AttackMontage);
	}

	BP_OnAttackExecuted();

	// Deal minor damage to player when in weapon range
	UGameplayStatics::ApplyDamage(TargetPlayer, AttackDamage, GetController(), this, UDamageType::StaticClass());

	GetWorldTimerManager().SetTimer(AttackCooldownTimerHandle, this, &AForestNPCCharacter::ResetAttackCooldown, AttackCooldown, false);
}

void AForestNPCCharacter::ResetAttackCooldown()
{
	bCanAttack = true;
}

void AForestNPCCharacter::Die()
{
	if (bIsDying)
	{
		return;
	}

	bIsDying = true;
	GetWorldTimerManager().ClearTimer(PendingDodgeTimerHandle);
	GetWorldTimerManager().ClearTimer(WeaponSwapTimerHandle);
	GetWorldTimerManager().ClearTimer(WeaponFinishTimerHandle);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
		MoveComp->StopMovementImmediately();
	}

	OnNPCDeath.Broadcast(this);
	SetLifeSpan(8.0f);
}

void AForestNPCCharacter::ApplySmokeDebuff(float SlowFactor, bool bBlindSight)
{
	bIsInSmoke = true;
	SmokeSlowFactor = FMath::Clamp(SlowFactor, 0.05f, 1.0f);
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = (bIsRunning ? RunSpeed : WalkSpeed) * SmokeSlowFactor;
	}

	if (AForestAIController* AIC = Cast<AForestAIController>(GetController()))
	{
		AIC->SetInSmoke(true, bBlindSight);
	}
	BP_OnSmokeEntered();
}

void AForestNPCCharacter::RemoveSmokeDebuff()
{
	bIsInSmoke = false;
	SmokeSlowFactor = 1.0f;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bIsRunning ? RunSpeed : WalkSpeed;
	}

	if (AForestAIController* AIC = Cast<AForestAIController>(GetController()))
	{
		AIC->SetInSmoke(false);
	}
	BP_OnSmokeExited();
}
