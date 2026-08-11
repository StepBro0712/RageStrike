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

	// Ник вместо «Бот 3»: собирается из списков при создании и живёт с ботом
	// до конца матча.
	UPROPERTY(Replicated)
	FString Nick;

	// сервер выдаёт имя новому боту
	static FString MakeBotNick(int32 Seed);

	// сервер помечает последний хит хедшотом для killfeed
	bool bLastHitHeadshot = false;

	// оружие раунда: влияет на урон, темп и killfeed. Реплицируется, иначе
	// клиенты не знают, какую модель вешать боту в руку.
	UPROPERTY(ReplicatedUsing = OnRep_Weapon)
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

	// оружие в руке: у ботов оно тоже должно быть видно
	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* GunMesh;

private:
	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_Team();

	UFUNCTION()
	void OnRep_Weapon();

	void ApplyTeamVisuals();
	// подобрать модель под оружие раунда и посадить её в руку
	void ApplyWeaponVisuals();

	// шаги: без них слышно только себя, а противников и союзников нет
	void UpdateFootsteps(float DeltaTime);
	float StepDistance = 0.f;
	// память о контакте: где последний раз видели врага или откуда прилетело.
	// Без неё бот забывал цель в тот же кадр, как терял её из виду.
	FVector LastContactPos = FVector::ZeroVector;
	float LastContactTime = -1000.f;
	static constexpr float MemorySeconds = 6.f;

	FVector GunPivot = FVector::ZeroVector;
	FVector GunHandLoc = FVector::ZeroVector;
	// доворот модели под ось ствола, как у игрока: без него модели, вытянутые
	// не по той оси, висят в руке стоймя
	FQuat GunAlign = FQuat::Identity;

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
	// Возвращает ближайшего ВИДИМОГО врага; в OutNearestAny кладёт просто
	// ближайшего — он нужен как направление патруля, когда никого не видно.
	AActor* FindNearestEnemy(AActor** OutNearestAny = nullptr) const;
	bool CanSee(const AActor* Target) const;
	void ShootAt(AActor* Target);
	bool IsEnemy(const AActor* Other) const;
};
