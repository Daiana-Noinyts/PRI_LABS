#include "Projectile.h"
#include "Enemies.h"
#include "ForestNPCCharacter.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Components/DecalComponent.h"
#include "UObject/ConstructorHelpers.h"

AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DecalMatFinder(TEXT("/Game/M_BulletHole.M_BulletHole"));
	if (DecalMatFinder.Succeeded())
	{
		HitDecalMaterial = DecalMatFinder.Object;
	}

	bLaunched = false;
	ElapsedTime = 0.0f;
	TotalFlightTime = 1.0f;
	AngularSpeed = 0.0f;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	CollisionComponent->InitSphereRadius(15.0f);
	RootComponent = CollisionComponent;

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->SetUpdatedComponent(RootComponent);
	ProjectileMovementComponent->InitialSpeed = 800.0f;
	ProjectileMovementComponent->MaxSpeed = 0.0f;
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->bShouldBounce = false;
	ProjectileMovementComponent->ProjectileGravityScale = 0.0f;

	ProjectileMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMeshComponent"));
	ProjectileMeshComponent->SetupAttachment(RootComponent);

	InitialLifeSpan = LifeSpan;

	CollisionComponent->BodyInstance.SetCollisionProfileName(TEXT("Projectile"));
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->OnComponentHit.AddDynamic(this, &AProjectile::OnHit);
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* MyOwner = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(MyOwner, true);
	}
	if (AActor* MyInstigator = GetInstigator())
	{
		CollisionComponent->IgnoreActorWhenMoving(MyInstigator, true);
	}
}

void AProjectile::FireInDirection(const FVector& Direction, float DistanceToTarget)
{
	StartLocation = GetActorLocation();
	ForwardDir = Direction.GetSafeNormal();

	RightDir = FVector::CrossProduct(FVector::UpVector, ForwardDir);
	if (RightDir.IsNearlyZero())
	{
		RightDir = FVector::RightVector;
	}
	RightDir.Normalize();
	UpDir = FVector::CrossProduct(ForwardDir, RightDir).GetSafeNormal();

	const float Speed = ProjectileMovementComponent->InitialSpeed;
	TotalFlightTime = FMath::Max(DistanceToTarget / Speed, 0.05f);
	AngularSpeed = 2.0f * PI * NumSpiralTurns / TotalFlightTime;

	ElapsedTime = 0.0f;
	bLaunched = true;

	// Ensure the projectile doesn't despawn before reaching the target
	SetLifeSpan(TotalFlightTime + 2.0f);

	if (AActor* MyOwner = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(MyOwner, true);
	}

	// Deactivate ProjectileMovementComponent's autonomous movement to prevent it from conflicting with the exact spiral trajectory
	ProjectileMovementComponent->Deactivate();

	SetActorRotation(ForwardDir.Rotation());

	// Where the shot was fired from: the NPC's dodge chance depends on this distance
	ShooterLocation = GetInstigator() ? GetInstigator()->GetActorLocation()
		: (GetOwner() ? GetOwner()->GetActorLocation() : StartLocation);
}

void AProjectile::WarnNPCsInPath()
{
	const float Speed = FMath::Max(ProjectileMovementComponent->InitialSpeed, 1.0f);
	const FVector Location = GetActorLocation();

	for (TActorIterator<AForestNPCCharacter> It(GetWorld()); It; ++It)
	{
		AForestNPCCharacter* NPC = *It;
		if (!NPC || NPC->IsDying() || WarnedNPCs.Contains(NPC))
		{
			continue;
		}

		// Is the NPC just ahead on the shot's path (about to be hit)?
		const FVector ToNPC = NPC->GetActorLocation() - Location;
		const float Along = FVector::DotProduct(ToNPC, ForwardDir);
		if (Along <= 0.0f || Along > Speed * DodgeWarningTime)
		{
			continue;
		}

		const float MissDistance = (ToNPC - ForwardDir * Along).Size();
		if (MissDistance > 120.0f + SpiralRadius)
		{
			continue;
		}

		// One reaction (one dodge roll) per shot
		WarnedNPCs.Add(NPC);
		NPC->ReactToIncomingAttack(GetInstigator(), ShooterLocation, Along / Speed);
	}
}

FVector AProjectile::ComputeOffset(float Time) const
{
	if (Time >= TotalFlightTime || TotalFlightTime <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float NormalizedTime = FMath::Clamp(Time / TotalFlightTime, 0.0f, 1.0f);
	const float SinVal = FMath::Sin(PI * NormalizedTime);
	// Sin squared dampens smoothly near start and end so impact is dead center
	const float Envelope = SinVal * SinVal;

	const float Angle = AngularSpeed * Time;
	return (RightDir * FMath::Cos(Angle) + UpDir * FMath::Sin(Angle)) * (SpiralRadius * Envelope);
}

void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bLaunched || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ElapsedTime += DeltaTime;

	// Give NPCs a chance to see the shot coming before it reaches them
	WarnNPCsInPath();

	// If reached max flight time and hasn't hit anything, destroy cleanly
	if (ElapsedTime >= TotalFlightTime + 0.1f)
	{
		Destroy();
		return;
	}

	const float Speed = ProjectileMovementComponent->InitialSpeed;
	const FVector DesiredLocation = StartLocation + ForwardDir * (Speed * ElapsedTime) + ComputeOffset(ElapsedTime);

	const FVector PrevLocation = GetActorLocation();
	FHitResult Hit;
	SetActorLocation(DesiredLocation, true, &Hit);

	const FVector CurrentVelocity = (DesiredLocation - PrevLocation) / DeltaTime;
	ProjectileMovementComponent->Velocity = CurrentVelocity;

	if (!CurrentVelocity.IsNearlyZero())
	{
		SetActorRotation(CurrentVelocity.Rotation());
	}

	if (Hit.bBlockingHit)
	{
		if (Hit.GetActor() != this && Hit.GetActor() != GetOwner() && Hit.GetActor() != GetInstigator())
		{
			OnHit(CollisionComponent, Hit.GetActor(), Hit.GetComponent(), -Hit.ImpactNormal, Hit);
		}
	}
}

void AProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHitExecuted)
	{
		Destroy();
		return;
	}

	if (OtherActor == this || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}

	// The NPC side-stepped this shot: let it fly past instead of hitting him
	const AForestNPCCharacter* HitNPC = Cast<AForestNPCCharacter>(OtherActor);
	if (HitNPC && HitNPC->IsDodging())
	{
		CollisionComponent->IgnoreActorWhenMoving(OtherActor, true);
		return;
	}

	bHitExecuted = true;
	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	static const float Impulse = 100.0f;

	// Apply damage once to hit actors (calls TakeDamage)
	if (OtherActor)
	{
		UGameplayStatics::ApplyDamage(OtherActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
	}

	if (OtherComponent && OtherComponent->IsSimulatingPhysics())
	{
		OtherComponent->AddImpulseAtLocation(ProjectileMovementComponent->Velocity * Impulse, Hit.ImpactPoint);
	}

	if (HitVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitVFX, Hit.ImpactPoint, FRotator::ZeroRotator);
	}

	UMaterialInterface* DecalToSpawn = HitDecalMaterial;
	if (!DecalToSpawn)
	{
		DecalToSpawn = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/M_BulletHole.M_BulletHole"));
	}

	if (DecalToSpawn)
	{
		const FRotator DecalRotation = (-Hit.ImpactNormal).Rotation();

		if (OtherComponent)
		{
			UGameplayStatics::SpawnDecalAttached(
				DecalToSpawn,
				DecalSize,
				OtherComponent,
				NAME_None,
				Hit.ImpactPoint,
				DecalRotation,
				EAttachLocation::KeepWorldPosition,
				DecalLifeSpan
			);
		}
		else
		{
			UGameplayStatics::SpawnDecalAtLocation(
				GetWorld(),
				DecalToSpawn,
				DecalSize,
				Hit.ImpactPoint,
				DecalRotation,
				DecalLifeSpan
			);
		}
	}

	Destroy();
}