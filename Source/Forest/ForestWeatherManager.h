#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestWeatherManager.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * Two natural phenomena for the forest:
 *  1) Day/night cycle - the sun (directional light) circles the sky; SkyAtmosphere colours the sky,
 *     sun and ambient light fade out at night.
 *  2) Rain at random intervals - a Niagara rain volume follows the player; while it rains the light
 *     dims and fog thickens. Dry/rain durations are random within the configured ranges.
 * Debug keys for demonstration: 8 = toggle day/night, 9 = force rain on/off.
 * Spawned automatically by AMyGameMode if the level does not contain one.
 */
UCLASS()
class FOREST_API AForestWeatherManager : public AActor
{
	GENERATED_BODY()

public:
	AForestWeatherManager();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ToggleDayNight();

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ToggleRain();

	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetTimeOfDay() const { return TimeOfDay; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	bool IsRaining() const { return bRaining; }

protected:
	virtual void BeginPlay() override;

	// ---- Day / night ----
	// Current hour, 0..24
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|DayNight", meta = (ClampMin = "0", ClampMax = "24"))
	float TimeOfDay = 10.0f;

	// Real seconds for a full 24 h cycle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|DayNight", meta = (ClampMin = "10"))
	float DayLengthSeconds = 240.0f;

	// Seconds the 8 key takes to jump to noon / midnight
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|DayNight")
	float ToggleTransitionSeconds = 4.0f;

	// Sky light brightness at night (fraction of day)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|DayNight")
	float NightSkyLightScale = 0.08f;

	// ---- Rain ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Rain")
	UNiagaraSystem* RainSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Rain")
	float DryTimeMin = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Rain")
	float DryTimeMax = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Rain")
	float RainTimeMin = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Rain")
	float RainTimeMax = 50.0f;

	// Height of the rain volume above the player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Rain")
	float RainHeight = 1500.0f;

	// Fog density multiplier and sun dimming at full rain
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Rain")
	float RainFogMultiplier = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Rain")
	float RainSunScale = 0.35f;

	UPROPERTY(VisibleAnywhere, Category = "Weather|Rain")
	UNiagaraComponent* RainComponent;

	// Show "Time / Weather" status on screen
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	bool bShowStatus = true;

private:
	UPROPERTY()
	ADirectionalLight* Sun = nullptr;

	UPROPERTY()
	ASkyLight* SkyLightActor = nullptr;

	UPROPERTY()
	AExponentialHeightFog* Fog = nullptr;

	float SunBaseIntensity = 10.0f;
	float SkyBaseIntensity = 1.0f;
	float FogBaseDensity = 0.02f;
	float SunYaw = 0.0f;

	// Day/night key transition
	bool bTimeTransition = false;
	float TransitionTarget = 12.0f;
	float TransitionRate = 0.0f;      // hours per second
	float TransitionRemaining = 0.0f; // hours still to advance

	bool bKeysBound = false;

	// Rain state
	bool bRaining = false;
	float RainAlpha = 0.0f;           // 0 = dry, 1 = full rain (smoothed)
	float WeatherTimeLeft = 0.0f;     // until next random change

	void FindSceneActors();
	void BindDebugKeys();
	void UpdateSun();
	void UpdateRain(float DeltaTime);
	void StartRain(bool bForced);
	void StopRain(bool bForced);
	void ShowMessage(const FString& Text, const FColor& Color) const;
};
