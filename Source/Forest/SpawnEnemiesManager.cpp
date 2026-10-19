#include "SpawnEnemiesManager.h"

#include "SpawnEnemyTargetPoint.h"
#include "Kismet/GameplayStatics.h"

ASpawnEnemiesManager::ASpawnEnemiesManager()
{
	// Tick is not needed - we only spawn enemies once in BeginPlay
	PrimaryActorTick.bCanEverTick = false;
}

void ASpawnEnemiesManager::BeginPlay()
{
	Super::BeginPlay();

	if (!BaseEnemyBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("BaseEnemyBP is not set!"));
		return;
	}

	TArray<AActor*> FoundTargetPoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASpawnEnemyTargetPoint::StaticClass(), FoundTargetPoints);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (AActor* FoundTargetPoint : FoundTargetPoints)
	{
		GetWorld()->SpawnActor<AEnemies>(BaseEnemyBP, FoundTargetPoint->GetActorTransform(), SpawnParams);
	}
}