#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Pawn.h"

/**
 * Reads/writes the player's health and ammo through UE reflection.
 * AHunterCharacter exposes HealthCounter / MaxHealth / BulletCounter / MaxBullets;
 * going through reflection means the pickups compile no matter whether those members are
 * public or protected, float or double, int32 or int64.
 * If you prefer direct access, replace the bodies below with Cast<AHunterCharacter>(...)->HealthCounter etc.
 */
namespace ForestStats
{
	inline bool GetNumber(const UObject* Obj, const TCHAR* PropName, double& Out)
	{
		if (!Obj) { return false; }
		const FNumericProperty* Prop = CastField<FNumericProperty>(Obj->GetClass()->FindPropertyByName(FName(PropName)));
		if (!Prop) { return false; }
		const void* Addr = Prop->ContainerPtrToValuePtr<void>(Obj);
		Out = Prop->IsFloatingPoint()
			? Prop->GetFloatingPointPropertyValue(Addr)
			: static_cast<double>(Prop->GetSignedIntPropertyValue(Addr));
		return true;
	}

	inline bool SetNumber(UObject* Obj, const TCHAR* PropName, double Value)
	{
		if (!Obj) { return false; }
		const FNumericProperty* Prop = CastField<FNumericProperty>(Obj->GetClass()->FindPropertyByName(FName(PropName)));
		if (!Prop) { return false; }
		void* Addr = Prop->ContainerPtrToValuePtr<void>(Obj);
		if (Prop->IsFloatingPoint()) { Prop->SetFloatingPointPropertyValue(Addr, Value); }
		else                         { Prop->SetIntPropertyValue(Addr, static_cast<int64>(FMath::RoundToDouble(Value))); }
		return true;
	}

	inline bool GetHealth(const UObject* C, double& Cur, double& Max) { return GetNumber(C, TEXT("HealthCounter"), Cur) && GetNumber(C, TEXT("MaxHealth"), Max); }
	inline bool SetHealth(UObject* C, double V)
	{
		if (!SetNumber(C, TEXT("HealthCounter"), V)) { return false; }
		if (UFunction* Func = C->FindFunction(FName("UpdateHUD"))) { C->ProcessEvent(Func, nullptr); }
		if (UFunction* Func = C->FindFunction(FName("OnStatsChanged"))) { C->ProcessEvent(Func, nullptr); }
		return true;
	}
	inline bool GetAmmo  (const UObject* C, double& Cur, double& Max) { return GetNumber(C, TEXT("BulletCounter"), Cur) && GetNumber(C, TEXT("MaxBullets"), Max); }
	inline bool SetAmmo  (UObject* C, double V)
	{
		if (!SetNumber(C, TEXT("BulletCounter"), V)) { return false; }
		if (UFunction* Func = C->FindFunction(FName("UpdateHUD"))) { C->ProcessEvent(Func, nullptr); }
		if (UFunction* Func = C->FindFunction(FName("OnStatsChanged"))) { C->ProcessEvent(Func, nullptr); }
		return true;
	}
}
