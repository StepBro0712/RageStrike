#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RSTeam.h"
#include "RSWeaponData.h"
#include "RSBot.generated.h"

class ARSCharacter;
class USkeletalMesh;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class RAGESTRIKE_API ARSBot : public ACharacter
{
	GENERATED_BODY()

public:
	ARSBot();

	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// вместо уничтожения возвращаем на карту
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;
	virtual void OutsideWorldBounds() override;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Team)
	ERSTeam Team = ERSTeam::T;

	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

	// номер закреплён за ботом на весь матч, чтобы счёт не сбрасывался
	UPROPERTY(Replicated)
	int32 BotNumber = 0;

	// сервер помечает последний хит хедшотом для killfeed
	bool bLastHitHeadshot = false;

	// оружие раунда: влияет на урон, темп и killfeed
	ERSWeapon Weapon = ERSWeapon::AK47;

	// флешка ослепляет бота — он перестаёт видеть цели
	float BlindUntil = -10.f;

	// между раундами бот воскресает, а не создаётся заново
	void RespawnForRound(const FVector& Location);

	void SetTeam(ERSTeam NewTeam);

	// камера для наблюдения: без неё вид становится в центр модели —
	// зритель оказывается внутри головы бота
	UPROPERTY(VisibleAnywhere)
	USpringArmComponent* SpectateArm;

	UPROPERTY(VisibleAnywhere)
	UCameraComponent* SpectateCam;

private:
	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_Team();

	void ApplyTeamVisuals();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShot(FVector Start, FVector End);

	void Ragdoll();

	UPROPERTY()
	USoundBase* FireSound = nullptr;

	UPROPERTY()
	USkeletalMesh* CTBodyMesh = nullptr;

	UPROPERTY()
	USkeletalMesh* TBodyMesh = nullptr;

	float FireCooldown = 1.f;
	float StrafeTimer = 0.f;
	float StrafeDir = 1.f;
	bool bRagdolled = false;

	// патрулирование
	FVector MoveTarget = FVector::ZeroVector;
	bool bHasTarget = false;
	float StuckTime = 0.f;
	FVector LastLocation = FVector::ZeroVector;

	void PickNewTarget(const FVector& PreferNear);
	bool IsPathClear(const FVector& Dir, float Distance) const;
	FVector SteerTowards(const FVector& Destination) const;

	// цель — любой живой противник: игрок или вражеский бот
	AActor* FindNearestEnemy() const;
	bool CanSee(const AActor* Target) const;
	void ShootAt(AActor* Target);
	bool IsEnemy(const AActor* Other) const;
};
