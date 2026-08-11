#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSWeaponData.h"
#include "RSTeam.h"
#include "RSGrenade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class USoundBase;

// Радиус дыма. Одно число на всё: по нему считается масштаб видимого купола,
// он же задаёт границу, за которой дым перестаёт тушить огонь и закрывать
// обзор ботам. Разводить видимую и игровую границы нельзя — игрок будет
// стоять в дыму, которого механика не видит, или наоборот.
static constexpr float RSSmokeRadius = 325.f;

// Накрыта ли точка дымом. Нужна и огню (дым тушит горящую зону), и гранате
// (молотов, прилетевший в дым, не загорается вовсе).
bool RSSmokeCovers(const UWorld* World, const FVector& Point);

// Летящая граната любого типа. Сервер решает, когда и как она сработает,
// клиенты получают эффекты мультикастом.
UCLASS()
class RAGESTRIKE_API ARSGrenade : public AActor
{
	GENERATED_BODY()

public:
	ARSGrenade();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// сервер задаёт при спавне
	void InitThrow(ERSWeapon InType, AActor* InThrower, ERSTeam InTeam, const FVector& Velocity);

	UPROPERTY(Replicated)
	ERSWeapon Type = ERSWeapon::HEGrenade;

	TWeakObjectPtr<AActor> Thrower;

	UPROPERTY(Replicated)
	ERSTeam Team = ERSTeam::CT;

	UPROPERTY(VisibleAnywhere)
	USphereComponent* Sphere;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	UPROPERTY(VisibleAnywhere)
	UProjectileMovementComponent* Movement;

	// дым уже распустился — только тогда он что-то перекрывает и тушит
	bool IsSmokeActive() const { return bSmokeDeployed; }

	// Грузит и кеширует эффект взрыва. Зовётся заранее, на старте матча.
	static class UNiagaraSystem* PreloadFX();

protected:
	virtual void BeginPlay() override;

private:
	void Detonate();

	UFUNCTION()
	void OnBounce(const FHitResult& Hit, const FVector& ImpactVelocity);

	// эффекты на клиентах: вспышка, взрыв, дым
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDetonate(FVector Where);

	void SpawnSmokeCloud(const FVector& Where);
	void ApplyLocalFlash(const FVector& Where);

	UPROPERTY()
	USoundBase* BlastSound = nullptr;

	UPROPERTY()
	UStaticMesh* SphereMesh = nullptr;

	UPROPERTY()
	UMaterialInterface* BaseMat = nullptr;

	FTimerHandle FuseTimer;
	float ThrownAt = 0.f;
	bool bDetonated = false;
	bool bSmokeDeployed = false;
};
