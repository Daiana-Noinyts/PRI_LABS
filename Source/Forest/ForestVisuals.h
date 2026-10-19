#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

/** Flat-shaded, low-poly props built from engine primitives so they match the LowPolyForest look with zero extra assets. */
namespace ForestVisuals
{
	enum class EShape : uint8 { Cube, Cylinder, Sphere };

	/** Constructor-only. Creates a collision-free primitive part. Scale is in metres (engine primitives are 100 cm). */
	inline UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, EShape Shape,
		const FVector& LocCm, const FVector& ScaleM, const FRotator& Rot = FRotator::ZeroRotator)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl (TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph (TEXT("/Engine/BasicShapes/Sphere.Sphere"));

		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		UStaticMesh* Mesh = (Shape == EShape::Cube) ? Cube.Object : (Shape == EShape::Cylinder) ? Cyl.Object : Sph.Object;
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(LocCm);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(ScaleM);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		return C;
	}

	/** Tints a part via the "Color" parameter of the engine's BasicShapeMaterial. Call from OnConstruction. */
	inline void ApplyColor(UStaticMeshComponent* C, const FLinearColor& Color)
	{
		if (!C) { return; }
		if (UMaterialInstanceDynamic* MID = C->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}
