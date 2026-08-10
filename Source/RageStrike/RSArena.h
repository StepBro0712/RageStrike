#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSTeam.h"
#include "RSArena.generated.h"

class UStaticMesh;
class UMaterialInterface;

// Реплицируемый «строитель» карты: сервер задаёт Seed и номер карты,
// каждый клиент строит то же самое у себя локально.
UCLASS()
class RAGESTRIKE_API ARSArena : public AActor
{
	GENERATED_BODY()

public:
	ARSArena();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// сервер вызывает после установки Seed и MapIndex
	void Build();

	UPROPERTY(ReplicatedUsing = OnRep_Build)
	int32 Seed = 0;

	UPROPERTY(Replicated)
	int32 MapIndex = 0;

	// Точка появления с посадкой на землю. Команды разводятся по
	// противоположным концам карты вдоль её длинной стороны.
	static FVector FindSpawnPoint(UWorld* World, ERSTeam Team, bool bTeamZone = true);
	static float GetMapFloor(UWorld* World);

	FVector MapCenter = FVector::ZeroVector;
	float PlayRadius = 3300.f;
	float GroundLevel = 0.f; // высота основного пола карты
	FBox MapBounds = FBox(ForceInit);

	// готовые точки появления, разнесённые по краям проходимой части карты
	TArray<FVector> SpawnsCT;
	TArray<FVector> SpawnsT;

	// все проходимые точки — боты используют их как маршрутные
	TArray<FVector> NavPoints;
	static FVector GetRandomNavPoint(UWorld* World, const FVector& Near, float MinDist);

private:
	UFUNCTION()
	void OnRep_Build();

	void BuildProcedural();
	void BuildFromContent(const TCHAR* Folder, float Scale, float GroundZ);
	void SpawnBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Color);
	void SpawnLighting();
	void ClearTemplateGeometry();

	UFUNCTION()
	void MeasureGroundLevel();

	void BuildSpawnZones();

	FTimerHandle MeasureTimer;

	UPROPERTY()
	UStaticMesh* CubeMesh = nullptr;

	UPROPERTY()
	UMaterialInterface* BaseMat = nullptr;

	bool bBuilt = false;
};
