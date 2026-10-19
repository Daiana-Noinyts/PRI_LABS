#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemies.h"
#include "SpawnEnemiesManager.generated.h"

UCLASS()
class FOREST_API ASpawnEnemiesManager : public AActor
{
	GENERATED_BODY()

public:
	ASpawnEnemiesManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Enemies Manager")
	TSubclassOf<class AEnemies> BaseEnemyBP;

protected:
	virtual void BeginPlay() override;
};