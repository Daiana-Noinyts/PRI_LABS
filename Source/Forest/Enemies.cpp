#include "Enemies.h"
#include "HunterCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Animation/AnimationAsset.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "Camera/PlayerCameraManager.h"

AEnemies::AEnemies()
{
	PrimaryActorTick.bCanEverTick = true;

	MovementSpeed = 150.0f;
	AttackDuration = 1.15f; // Native Primary_A swipe duration
	AttackHitDelay = 0.45f; // Strike impact connects ~0.45s into swing
	TimeBeforeAnimEndToHit = 0.70f;
	AttackInterval = 4.0f; // Performs attack periodically while patrolling
	TimeSinceLastAttack = 0.0f;
	TimeInAttack = 0.0f;
	bHasLeaped = false;
	CurrentTargetIndex = 0;
	bIsMoving = true;
	bIsWalking = false;
	bIsDying = false;
	bIsAttacking = false;
	bAttackHitActive = false;
	bHitDealtThisAttack = false;
	bHealthBarInitialized = false;

	AttackReach = 450.0f; // Increased melee claw swipe reach (4.5m)
	AttackFrontHalfAngle = 75.0f; // Wide 150-degree frontal swipe
	PointBlankRadius = 260.0f; // Danger radius around body/feet
	DetectionRadius = 450.0f;
	DamageAmount = 15.0f;

	CurrentHealth = 100.0f;
	MaxHealth = 100.0f;

	TargetPoints.Add(FVector(300.f, 300.f, 0.f));
	TargetPoints.Add(FVector(300.f, -300.f, 0.f));
	TargetPoints.Add(FVector(-300.f, -300.f, 0.f));
	TargetPoints.Add(FVector(-300.f, 300.f, 0.f));

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> DeathAnimFinder(TEXT("/Game/UI/BorisAnims/Death_From_The_Back1.Death_From_The_Back1"));
	if (DeathAnimFinder.Succeeded())
	{
		DeathAnimation = DeathAnimFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> DeathSoundFinder(TEXT("/Game/ParagonBoris/Audio/Cues/Boris_Death.Boris_Death"));
	if (DeathSoundFinder.Succeeded())
	{
		DeathSound = DeathSoundFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UParticleSystem> DeathVFXFinder(TEXT("/Game/ParagonBoris/FX/Particles/Boris/Abilities/AOE/FX/Proto/P_Proto_Boris_Ball_Explode.P_Proto_Boris_Ball_Explode"));
	if (DeathVFXFinder.Succeeded())
	{
		DeathVFX = DeathVFXFinder.Object;
	}
}

void AEnemies::BeginPlay()
{
	Super::BeginPlay();

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetGenerateOverlapEvents(true);
		MeshComp->SetNotifyRigidBodyCollision(true);
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetGenerateOverlapEvents(true);
		Capsule->SetNotifyRigidBodyCollision(true);
	}

	// Respect blueprint values or update to increased reach
	if (PointBlankRadius <= 220.0f) PointBlankRadius = 260.0f;
	if (AttackReach <= 360.0f) AttackReach = 450.0f;
	if (AttackFrontHalfAngle <= 65.0f) AttackFrontHalfAngle = 75.0f;
	if (DetectionRadius <= 0.0f) DetectionRadius = 450.0f;

	if (AttackAnimation)
	{
		const float PlayLength = AttackAnimation->GetPlayLength();
		if (PlayLength > 0.4f)
		{
			// Always sync duration to the actual animation asset length so Boris never freezes in end pose!
			AttackDuration = PlayLength;
		}
	}

	if (AttackDuration <= 0.0f)
	{
		AttackDuration = 1.15f;
	}

	// If AttackHitDelay was unset, or if it is greater than the animation duration,
	// automatically scale hit delay to ~35% of the attack animation (ideal for melee strikes)!
	if (AttackHitDelay <= 0.0f || AttackHitDelay >= AttackDuration)
	{
		AttackHitDelay = AttackDuration * 0.35f;
	}
	TimeBeforeAnimEndToHit = FMath::Max(0.2f, AttackDuration - AttackHitDelay);

	if (AttackInterval <= 0.0f)
	{
		AttackInterval = 5.0f;
	}
	TimeSinceLastAttack = 5.0f; // Allow attacking at the first waypoint reached

	if (TargetPoints.Num() == 0)
	{
		TargetPoints.Add(FVector(400.f, 400.f, 0.f));
		TargetPoints.Add(FVector(400.f, -400.f, 0.f));
		TargetPoints.Add(FVector(-400.f, -400.f, 0.f));
		TargetPoints.Add(FVector(-400.f, 400.f, 0.f));
	}

	WorldTargetPoints.Empty();
	const FTransform SpawnTransform = GetActorTransform();
	for (const FVector& LocalPoint : TargetPoints)
	{
		WorldTargetPoints.Add(SpawnTransform.TransformPositionNoScale(LocalPoint));
	}

	UpdateHealthBar();
}

void AEnemies::Tick(float DeltaTime)
{
	// Note: We deliberately do NOT call Super::Tick(DeltaTime) because AActor::Tick invokes
	// Blueprint ReceiveTick, which in BP_Enemies has legacy logic that turns to the player
	// and triggers duplicate attacks on sight. Boris is fully updated below.

	if (bIsDying)
	{
		return;
	}

	// Ensure widget component is fully ready and initialized with initial color
	if (!bHealthBarInitialized)
	{
		UpdateHealthBar();
		bHealthBarInitialized = true;
	}

	if (bIsAttacking)
	{
		TimeInAttack += DeltaTime;

		if (bAttackHitActive && !bHitDealtThisAttack)
		{
			CheckAndApplyAttackDamage();
		}
	}
	else if (bIsMoving)
	{
		MoveToNextPoint(DeltaTime);
	}
}

void AEnemies::DrawDebugFrontalCone(UWorld* World, const FVector& Origin, const FVector& Forward, float Reach, float HalfAngleDeg, const FColor& Color, float LifeTime, float Thickness)
{
	if (!World) return;

	const float CapsuleHalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
	const FVector GroundOrigin = Origin - FVector(0.f, 0.f, CapsuleHalfHeight - 6.0f);

	FVector FlatForward = Forward;
	FlatForward.Z = 0.0f;
	if (FlatForward.IsNearlyZero())
	{
		FlatForward = FVector::ForwardVector;
	}
	FlatForward.Normalize();

	// Circular zone around body / feet
	DrawDebugCircle(World, GroundOrigin, PointBlankRadius, 36, Color, false, LifeTime, 0, Thickness * 0.8f, FVector(1, 0, 0), FVector(0, 1, 0), false);

	const FRotator LeftRot(0.f, -HalfAngleDeg, 0.f);
	const FRotator RightRot(0.f, HalfAngleDeg, 0.f);

	const FVector LeftDir = LeftRot.RotateVector(FlatForward);
	const FVector RightDir = RightRot.RotateVector(FlatForward);

	const FVector LeftPt = GroundOrigin + LeftDir * Reach;
	const FVector RightPt = GroundOrigin + RightDir * Reach;
	const FVector CenterPt = GroundOrigin + FlatForward * Reach;

	// Boundary lines from origin to reach
	DrawDebugLine(World, GroundOrigin, LeftPt, Color, false, LifeTime, 0, Thickness);
	DrawDebugLine(World, GroundOrigin, RightPt, Color, false, LifeTime, 0, Thickness);
	DrawDebugLine(World, GroundOrigin, CenterPt, Color, false, LifeTime, 0, Thickness * 0.7f);

	// Arc connecting the front boundaries
	const int32 NumSteps = 24;
	FVector PrevArcPt = LeftPt;
	for (int32 i = 1; i <= NumSteps; ++i)
	{
		const float Fraction = (float)i / (float)NumSteps;
		const float Angle = FMath::Lerp(-HalfAngleDeg, HalfAngleDeg, Fraction);
		const FVector StepDir = FRotator(0.f, Angle, 0.f).RotateVector(FlatForward);
		const FVector NextArcPt = GroundOrigin + StepDir * Reach;
		DrawDebugLine(World, PrevArcPt, NextArcPt, Color, false, LifeTime, 0, Thickness);
		PrevArcPt = NextArcPt;
	}
}

void AEnemies::MoveToNextPoint(float DeltaTime)
{
	if (WorldTargetPoints.Num() == 0) return;

	TimeSinceLastAttack += DeltaTime;

	FVector CurrentLocation = GetActorLocation();

	FVector TargetLocation = WorldTargetPoints[CurrentTargetIndex];
	TargetLocation.Z = CurrentLocation.Z;

	float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	// When Boris reaches a waypoint and has walked for at least 1.5s since the last attack:
	// stop, perform attack, then move to the next point
	if (Distance < 70.f && TimeSinceLastAttack >= 1.5f)
	{
		CurrentTargetIndex = (CurrentTargetIndex + 1) % WorldTargetPoints.Num();
		bIsMoving = false;
		bIsWalking = false;
		TimeSinceLastAttack = 0.0f;
		PlayAttackAnimation();
		return;
	}
	else
	{
		FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
		FRotator NewRotation = Direction.Rotation();
		SetActorRotation(NewRotation);

		USkeletalMeshComponent* MeshComp = GetMesh();
		if (WalkAnimation && MeshComp && !bIsWalking)
		{
			MeshComp->PlayAnimation(WalkAnimation, true);
			bIsWalking = true;
		}

		SetActorLocation(CurrentLocation + Direction * MovementSpeed * DeltaTime);
	}
}

FVector AEnemies::GetAttackImpactLocation() const
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		const FVector HandL = MeshComp->GetSocketLocation(FName("hand_l"));
		const FVector HandR = MeshComp->GetSocketLocation(FName("hand_r"));
		if (!HandL.IsNearlyZero() && !HandR.IsNearlyZero())
		{
			return (HandL + HandR) * 0.5f;
		}

		const FVector Pelvis = MeshComp->GetSocketLocation(FName("pelvis"));
		if (!Pelvis.IsNearlyZero())
		{
			return Pelvis;
		}
	}

	return GetActorLocation() + GetActorForwardVector() * 300.0f;
}

FVector AEnemies::GetAttackBodyLocation() const
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		const FVector Pelvis = MeshComp->GetSocketLocation(FName("pelvis"));
		if (!Pelvis.IsNearlyZero())
		{
			return Pelvis;
		}
	}
	return GetActorLocation();
}

void AEnemies::PlayAttackAnimation()
{
	if (bIsAttacking || bIsDying)
	{
		return;
	}

	bIsAttacking = true;
	bAttackHitActive = false;
	bHitDealtThisAttack = false;
	TimeInAttack = 0.0f;
	bHasLeaped = false;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (AttackAnimation && MeshComp && !bIsDying)
	{
		MeshComp->PlayAnimation(AttackAnimation, false);
	}

	const float HitDelay = FMath::Clamp(AttackHitDelay, 0.15f, FMath::Max(0.2f, AttackDuration - 0.15f));

	// Orange telegraph cone showing where Boris will strike
	DrawDebugFrontalCone(GetWorld(), GetActorLocation(), GetActorForwardVector(), AttackReach, AttackFrontHalfAngle, FColor::Orange, HitDelay, 3.5f);

	GetWorld()->GetTimerManager().SetTimer(AttackHitTimerHandle, this, &AEnemies::TriggerAttackHit, HitDelay, false);
	GetWorld()->GetTimerManager().SetTimer(AnimationTimerHandle, this, &AEnemies::OnAttackAnimationFinished, AttackDuration, false);
}

void AEnemies::TriggerAttackHit()
{
	if (!bIsAttacking || bIsDying) return;

	// Deliberately do NOT call OnAttackHit() to prevent legacy BP node from subtracting 25 HP
	// OnAttackHit();

	bAttackHitActive = true;

	// Red impact cone showing the entire active attack zone
	DrawDebugFrontalCone(GetWorld(), GetActorLocation(), GetActorForwardVector(), AttackReach, AttackFrontHalfAngle, FColor::Red, 1.2f, 5.0f);
	DrawDebugSphere(GetWorld(), GetAttackImpactLocation(), 75.0f, 16, FColor::Red, false, 1.2f, 0, 4.0f);

	CheckAndApplyAttackDamage();

	// Keep hit window active for the remainder of the claw swing so it connects reliably!
	const float RemainingTime = FMath::Max(0.4f, AttackDuration - TimeInAttack);
	GetWorld()->GetTimerManager().SetTimer(AttackHitActiveTimerHandle, this, &AEnemies::DeactivateAttackHit, RemainingTime, false);
}

void AEnemies::DeactivateAttackHit()
{
	bAttackHitActive = false;
}

void AEnemies::CheckAndApplyAttackDamage()
{
	if (!bIsAttacking || bIsDying || bHitDealtThisAttack) return;

	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!PlayerChar) return;

	AHunterCharacter* Hunter = Cast<AHunterCharacter>(PlayerChar);
	if (!Hunter) return;

	const FVector PlayerLoc = PlayerChar->GetActorLocation();
	const FVector CapsuleLoc = GetActorLocation();
	const FVector FistsLoc = GetAttackImpactLocation();
	const FVector BodyLoc = GetAttackBodyLocation();

	// Distance from Boris's center to Player
	const FVector ToPlayer = PlayerLoc - CapsuleLoc;
	const float Dist2D = FVector(ToPlayer.X, ToPlayer.Y, 0.f).Size();
	const bool bInHeight = FMath::Abs(PlayerLoc.Z - CapsuleLoc.Z) <= 300.0f;

	// Check frontal orientation from Boris
	FVector FlatForward = GetActorForwardVector();
	FlatForward.Z = 0.0f;
	FlatForward.Normalize();
	FVector FlatDirToPlayer = ToPlayer;
	FlatDirToPlayer.Z = 0.0f;
	FlatDirToPlayer.Normalize();
	const float Dot = FVector::DotProduct(FlatForward, FlatDirToPlayer);

	// 1. Natural frontal claw arc (if player is within AttackReach and generally in front of Boris)
	const bool bHitFrontSwipe = (Dist2D <= AttackReach && Dot >= 0.20f && bInHeight);

	// 2. Point-blank zone around feet/body
	const bool bHitPointBlank = (Dist2D <= PointBlankRadius && bInHeight);

	// 3. Proximity to Boris's fists/claws
	const FVector ToPlayerFists = PlayerLoc - FistsLoc;
	const float DistFists2D = FVector(ToPlayerFists.X, ToPlayerFists.Y, 0.f).Size();
	const bool bInFistsHeight = FMath::Abs(PlayerLoc.Z - FistsLoc.Z) <= 300.0f;
	const bool bHitFists = (DistFists2D <= (AttackReach * 0.9f) && bInFistsHeight);

	// 4. Proximity to Boris's body
	const FVector ToPlayerBody = PlayerLoc - BodyLoc;
	const float DistBody2D = FVector(ToPlayerBody.X, ToPlayerBody.Y, 0.f).Size();
	const bool bHitBody = (DistBody2D <= PointBlankRadius && bInHeight);

	if (bHitFrontSwipe || bHitPointBlank || bHitFists || bHitBody)
	{
		bHitDealtThisAttack = true;

		DrawDebugSphere(GetWorld(), PlayerLoc, 50.0f, 16, FColor::Red, false, 1.5f, 0, 4.0f);

		Hunter->ApplyDamageDirect(DamageAmount);
	}
}

void AEnemies::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (bIsDying || !OtherActor) return;

	// "якщо гравець під час атаки потрапляє у колізію ворога то гравець отримує шкоду"
	if (AHunterCharacter* Hunter = Cast<AHunterCharacter>(OtherActor))
	{
		if (bIsAttacking && !bHitDealtThisAttack)
		{
			bHitDealtThisAttack = true;
			Hunter->ApplyDamageDirect(DamageAmount);
		}
	}
}

void AEnemies::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	if (bIsDying || !Other) return;

	// "якщо гравець під час атаки потрапляє у колізію ворога то гравець отримує шкоду"
	if (AHunterCharacter* Hunter = Cast<AHunterCharacter>(Other))
	{
		if (bIsAttacking && !bHitDealtThisAttack)
		{
			bHitDealtThisAttack = true;
			Hunter->ApplyDamageDirect(DamageAmount);
		}
	}
}

void AEnemies::OnAttackAnimationFinished()
{
	bIsAttacking = false;
	bAttackHitActive = false;
	TimeSinceLastAttack = 0.0f;
	TimeInAttack = 0.0f;
	bHasLeaped = false;

	if (bIsDying || WorldTargetPoints.Num() == 0) return;

	bIsMoving = true;
}

float AEnemies::TakeDamage(float DamageAmountIn, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmountIn, DamageEvent, EventInstigator, DamageCauser);
	TakeDamageCustom(ActualDamage > 0.f ? ActualDamage : DamageAmountIn);
	return ActualDamage;
}

void AEnemies::TakeDamageCustom(float DamageAmountIn)
{
	if (bIsDying) return;

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmountIn, 0.0f, MaxHealth);
	UpdateHealthBar();

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void AEnemies::UpdateHealthBar()
{
	const float HealthPercent = FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f);

	// Try to update any attached UWidgetComponent (like BP_DummyActorComponent with BP_DummyWidget)
	TArray<UWidgetComponent*> WidgetComps;
	GetComponents<UWidgetComponent>(WidgetComps);
	for (UWidgetComponent* Comp : WidgetComps)
	{
		if (!Comp) continue;

		if (UUserWidget* UserWidget = Comp->GetUserWidgetObject())
		{
			bool bHandledByBlueprint = false;
			if (UFunction* Func = UserWidget->FindFunction(FName("SetNewHealthValue")))
			{
				struct { double Val; } ParamsDouble = { (double)HealthPercent };
				struct { float Val; } ParamsFloat = { HealthPercent };
				if (FProperty* Prop = Func->FindPropertyByName(FName("NewHealthValue")))
				{
					if (Prop->IsA<FDoubleProperty>())
					{
						UserWidget->ProcessEvent(Func, &ParamsDouble);
					}
					else
					{
						UserWidget->ProcessEvent(Func, &ParamsFloat);
					}
				}
				else
				{
					UserWidget->ProcessEvent(Func, &ParamsDouble);
				}
				bHandledByBlueprint = true;
			}

			// Only fallback to direct C++ percent update if widget blueprint doesn't have custom logic
			if (!bHandledByBlueprint)
			{
				if (UProgressBar* Bar = Cast<UProgressBar>(UserWidget->GetWidgetFromName(FName("HealthProgressBar"))))
				{
					Bar->SetPercent(HealthPercent);
				}
			}
		}
	}
}

void AEnemies::Die()
{
	if (bIsDying) return;

	bIsDying = true;
	bIsMoving = false;
	bIsWalking = false;
	bIsAttacking = false;
	bAttackHitActive = false;
	TimeInAttack = 0.0f;
	bHasLeaped = false;

	GetWorld()->GetTimerManager().ClearTimer(AnimationTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(AttackHitTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(AttackHitActiveTimerHandle);

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (DeathAnimation && GetMesh())
	{
		GetMesh()->PlayAnimation(DeathAnimation, false);
	}

	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), DeathSound, GetActorLocation());
	}

	if (DeathVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DeathVFX, GetActorLocation(), FRotator::ZeroRotator);
	}

	SetLifeSpan(3.5f);
}