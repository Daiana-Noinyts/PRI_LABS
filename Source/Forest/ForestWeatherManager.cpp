#include "ForestWeatherManager.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

AForestWeatherManager::AForestWeatherManager()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	RainComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RainComponent"));
	RainComponent->SetupAttachment(RootComponent);
	RainComponent->bAutoActivate = false;
	// Rain follows the camera, so keep it in world space independent of this actor
	RainComponent->SetUsingAbsoluteLocation(true);

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> RainFinder(TEXT("/Game/Weather/NS_Rain.NS_Rain"));
	if (RainFinder.Succeeded())
	{
		RainSystem = RainFinder.Object;
	}
}

void AForestWeatherManager::BeginPlay()
{
	Super::BeginPlay();

	FindSceneActors();
	BindDebugKeys();

	if (RainSystem)
	{
		RainComponent->SetAsset(RainSystem);
	}

	WeatherTimeLeft = FMath::FRandRange(DryTimeMin, DryTimeMax);
	UpdateSun();
}

void AForestWeatherManager::FindSceneActors()
{
	UWorld* World = GetWorld();

	for (TActorIterator<ADirectionalLight> It(World); It && !Sun; ++It)
	{
		Sun = *It;
	}
	for (TActorIterator<ASkyLight> It(World); It && !SkyLightActor; ++It)
	{
		SkyLightActor = *It;
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It && !Fog; ++It)
	{
		Fog = *It;
	}

	if (Sun && Sun->GetLightComponent())
	{
		// The sun has to move at runtime
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		SunBaseIntensity = Sun->GetLightComponent()->Intensity;
		SunYaw = Sun->GetActorRotation().Yaw;

		// Start at the time of day the level was designed with (sun elevation -> hour)
		const float Elevation = -Sun->GetActorRotation().Pitch;
		TimeOfDay = FMath::Fmod(6.0f + Elevation / 360.0f * 24.0f + 24.0f, 24.0f);
	}
	if (SkyLightActor && SkyLightActor->GetLightComponent())
	{
		SkyLightActor->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		SkyBaseIntensity = SkyLightActor->GetLightComponent()->Intensity;
	}
	if (Fog && Fog->GetComponent())
	{
		FogBaseDensity = Fog->GetComponent()->FogDensity;
	}

	UE_LOG(LogTemp, Log, TEXT("[Weather] Sun=%s SkyLight=%s Fog=%s Rain=%s start=%.1fh"),
		Sun ? TEXT("yes") : TEXT("no"), SkyLightActor ? TEXT("yes") : TEXT("no"), Fog ? TEXT("yes") : TEXT("no"),
		RainSystem ? TEXT("yes") : TEXT("no"), TimeOfDay);
}

void AForestWeatherManager::BindDebugKeys()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || bKeysBound)
	{
		return;
	}

	EnableInput(PC);
	if (InputComponent)
	{
		bKeysBound = true;
		InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &AForestWeatherManager::ToggleDayNight);
		InputComponent->BindKey(EKeys::NumPadEight, IE_Pressed, this, &AForestWeatherManager::ToggleDayNight);
		InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &AForestWeatherManager::ToggleRain);
		InputComponent->BindKey(EKeys::NumPadNine, IE_Pressed, this, &AForestWeatherManager::ToggleRain);
	}
}

void AForestWeatherManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// The player controller may not exist yet at BeginPlay
	BindDebugKeys();

	if (bTimeTransition)
	{
		// Fast-forward (always forwards in time) to noon / midnight after pressing 8
		const float Step = FMath::Min(TransitionRate * DeltaTime, TransitionRemaining);
		TimeOfDay += Step;
		TransitionRemaining -= Step;
		if (TransitionRemaining <= KINDA_SMALL_NUMBER)
		{
			TimeOfDay = TransitionTarget;
			bTimeTransition = false;
		}
	}
	else
	{
		TimeOfDay += 24.0f / DayLengthSeconds * DeltaTime;
	}
	TimeOfDay = FMath::Fmod(TimeOfDay + 24.0f, 24.0f);

	UpdateRain(DeltaTime);
	UpdateSun();

}

void AForestWeatherManager::UpdateSun()
{
	// Hour -> sun elevation: 06:00 sunrise (0 deg), 12:00 overhead (90), 18:00 sunset (180), 00:00 below (270)
	const float Elevation = (TimeOfDay - 6.0f) / 24.0f * 360.0f;
	const float SinElevation = FMath::Sin(FMath::DegreesToRadians(Elevation));
	const float DayFactor = FMath::SmoothStep(-0.08f, 0.2f, SinElevation);
	const float RainDim = FMath::Lerp(1.0f, RainSunScale, RainAlpha);

	if (Sun && Sun->GetLightComponent())
	{
		Sun->SetActorRotation(FRotator(-Elevation, SunYaw, 0.0f));
		Sun->GetLightComponent()->SetIntensity(SunBaseIntensity * DayFactor * RainDim);
	}

	if (SkyLightActor && SkyLightActor->GetLightComponent())
	{
		const float SkyScale = FMath::Lerp(NightSkyLightScale, 1.0f, DayFactor) * FMath::Lerp(1.0f, 0.6f, RainAlpha);
		SkyLightActor->GetLightComponent()->SetIntensity(SkyBaseIntensity * SkyScale);
	}
}

void AForestWeatherManager::UpdateRain(float DeltaTime)
{
	// Random weather: alternate dry and rainy spells of random length
	WeatherTimeLeft -= DeltaTime;
	if (WeatherTimeLeft <= 0.0f)
	{
		if (bRaining)
		{
			StopRain(false);
		}
		else
		{
			StartRain(false);
		}
	}

	// Smooth fade of the rain mood (light, fog)
	RainAlpha = FMath::FInterpConstantTo(RainAlpha, bRaining ? 1.0f : 0.0f, DeltaTime, 1.0f / 3.0f);

	if (Fog && Fog->GetComponent())
	{
		Fog->GetComponent()->SetFogDensity(FogBaseDensity * FMath::Lerp(1.0f, RainFogMultiplier, RainAlpha));
	}

	// Keep the rain volume above the player's head
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			RainComponent->SetWorldLocation(PC->PlayerCameraManager->GetCameraLocation() + FVector(0.0f, 0.0f, RainHeight));
		}
	}
}

void AForestWeatherManager::StartRain(bool bForced)
{
	bRaining = true;
	RainComponent->Activate(true);
	WeatherTimeLeft = FMath::FRandRange(RainTimeMin, RainTimeMax);
	UE_LOG(LogTemp, Log, TEXT("[Weather] Rain starts (%s), lasts %.0fs"), bForced ? TEXT("key 9") : TEXT("random"), WeatherTimeLeft);
	ShowMessage(bForced ? TEXT("Rain started (key 9)") : TEXT("It started to rain"), FColor::Cyan);
}

void AForestWeatherManager::StopRain(bool bForced)
{
	bRaining = false;
	RainComponent->Deactivate();
	WeatherTimeLeft = FMath::FRandRange(DryTimeMin, DryTimeMax);
	UE_LOG(LogTemp, Log, TEXT("[Weather] Rain stops (%s), next rain in %.0fs"), bForced ? TEXT("key 9") : TEXT("random"), WeatherTimeLeft);
	ShowMessage(bForced ? TEXT("Rain stopped (key 9)") : TEXT("The rain stopped"), FColor::Cyan);
}

void AForestWeatherManager::ToggleRain()
{
	if (bRaining)
	{
		StopRain(true);
	}
	else
	{
		StartRain(true);
	}
}

void AForestWeatherManager::ToggleDayNight()
{
	const float Elevation = (TimeOfDay - 6.0f) / 24.0f * 360.0f;
	const bool bIsDay = FMath::Sin(FMath::DegreesToRadians(Elevation)) > 0.0f;

	TransitionTarget = bIsDay ? 0.0f : 12.0f;
	TransitionRemaining = FMath::Fmod(TransitionTarget - TimeOfDay + 24.0f, 24.0f);
	TransitionRate = TransitionRemaining / FMath::Max(ToggleTransitionSeconds, 0.1f);
	bTimeTransition = TransitionRemaining > KINDA_SMALL_NUMBER;

	ShowMessage(bIsDay ? TEXT("Switching to night (key 8)") : TEXT("Switching to day (key 8)"), FColor::Yellow);
}

void AForestWeatherManager::ShowMessage(const FString& Text, const FColor& Color) const
{
}
