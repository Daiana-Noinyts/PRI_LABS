#include "MyGameMode.h"
#include "ForestHUD.h"
#include "HunterCharacter.h"
#include "Projectile.h"
#include "ForestWeatherManager.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"

AMyGameMode::AMyGameMode()
{
	HUDClass = AForestHUD::StaticClass();
	DefaultPawnClass = AHunterCharacter::StaticClass();
}

void AMyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// No debug text on screen (also hides Print String output from Blueprints)
	GAreScreenMessagesEnabled = false;

	// Day/night + rain: use the one placed in the level, otherwise create it
	TActorIterator<AForestWeatherManager> It(GetWorld());
	if (!It)
	{
		GetWorld()->SpawnActor<AForestWeatherManager>(AForestWeatherManager::StaticClass(), FTransform::Identity);
	}
}
