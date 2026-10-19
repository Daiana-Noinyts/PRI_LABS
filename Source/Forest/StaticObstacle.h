#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StaticObstacle.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USphereComponent;
class UCapsuleComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class EObstacleCollision : uint8
{
	/** Use the simple collision authored on the mesh asset (convex hulls / boxes). Mesh must have it. */
	MeshAsAuthored UMETA(DisplayName = "Mesh (authored simple collision)"),
	/** Invisible box - crisp, cheap blocker for walls, logs, crates. */
	Box,
	/** Invisible sphere - boulders; the player slides around it. */
	Sphere,
	/** Invisible capsule - trunks, stumps, pillars; smooth slide-along behaviour. */
	Capsule
};

/**
 * Immovable scenery the player has to walk around. One class, many looks: pick any static mesh, choose the
 * collision primitive, and decide which channels it blocks (e.g. tree canopies that don't shove the camera).
 */
UCLASS()
class FOREST_API AStaticObstacle : public AActor
{
	GENERATED_BODY()

public:
	AStaticObstacle();
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Obstacle") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Obstacle") TObjectPtr<UBoxComponent> BoxShape;
	UPROPERTY(VisibleAnywhere, Category = "Obstacle") TObjectPtr<USphereComponent> SphereShape;
	UPROPERTY(VisibleAnywhere, Category = "Obstacle") TObjectPtr<UCapsuleComponent> CapsuleShape;

	UPROPERTY(EditAnywhere, Category = "Obstacle") TObjectPtr<UStaticMesh> ObstacleMesh;

	UPROPERTY(EditAnywhere, Category = "Obstacle|Collision") EObstacleCollision CollisionType = EObstacleCollision::MeshAsAuthored;

	/** Offset of the collision primitive from the actor origin (local space, cm). */
	UPROPERTY(EditAnywhere, Category = "Obstacle|Collision", meta = (EditCondition = "CollisionType != EObstacleCollision::MeshAsAuthored", EditConditionHides))
	FVector ShapeOffset = FVector(0.f, 0.f, 60.f);

	UPROPERTY(EditAnywhere, Category = "Obstacle|Collision", meta = (EditCondition = "CollisionType == EObstacleCollision::Box", EditConditionHides))
	FVector BoxHalfExtent = FVector(60.f, 60.f, 60.f);

	UPROPERTY(EditAnywhere, Category = "Obstacle|Collision", meta = (EditCondition = "CollisionType == EObstacleCollision::Sphere", EditConditionHides, ClampMin = "1.0"))
	float SphereRadius = 60.f;

	UPROPERTY(EditAnywhere, Category = "Obstacle|Collision", meta = (EditCondition = "CollisionType == EObstacleCollision::Capsule", EditConditionHides, ClampMin = "1.0"))
	float CapsuleRadius = 40.f;

	UPROPERTY(EditAnywhere, Category = "Obstacle|Collision", meta = (EditCondition = "CollisionType == EObstacleCollision::Capsule", EditConditionHides, ClampMin = "1.0"))
	float CapsuleHalfHeight = 120.f;

	/** Which channels this obstacle blocks. */
	UPROPERTY(EditAnywhere, Category = "Obstacle|Response") bool bBlockPawns = true;
	UPROPERTY(EditAnywhere, Category = "Obstacle|Response") bool bBlockCamera = true;
	/** Line traces on the Visibility channel (hitscan, interaction traces). */
	UPROPERTY(EditAnywhere, Category = "Obstacle|Response") bool bBlockTraces = true;
	/** If false the character can't step up onto it (it acts as a wall, not a curb). */
	UPROPERTY(EditAnywhere, Category = "Obstacle|Response") bool bAllowStepUp = false;

private:
	void ApplyBlocking(UPrimitiveComponent* Comp) const;
	static void DisableCollision(UPrimitiveComponent* Comp);
};
