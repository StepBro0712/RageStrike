#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSTracer.generated.h"

class UStaticMeshComponent;

// Трассер: вытянутый светящийся цилиндр от ствола до точки попадания плюс
// вспышка у дула и искра в месте удара. Живёт несколько кадров и исчезает.
UCLASS()
class RAGESTRIKE_API ARSTracer : public AActor
{
	GENERATED_BODY()

public:
	ARSTracer();

	virtual void Tick(float DeltaTime) override;

	// вызывается сразу после спавна на каждом клиенте
	void Show(const FVector& Start, const FVector& End, bool bBigCaliber);

	static void Spawn(UWorld* World, const FVector& Start, const FVector& End, bool bBigCaliber);

private:
	UPROPERTY()
	UStaticMeshComponent* Beam = nullptr;

	UPROPERTY()
	UStaticMeshComponent* Muzzle = nullptr;

	UPROPERTY()
	UStaticMeshComponent* Impact = nullptr;

	UPROPERTY()
	UMaterialInterface* BaseMat = nullptr;

	UPROPERTY()
	UStaticMesh* CylinderMesh = nullptr;

	UPROPERTY()
	UStaticMesh* SphereMesh = nullptr;

	float Life = 0.f;
	float Duration = 0.09f;
};
