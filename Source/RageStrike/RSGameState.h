#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RSWeaponData.h"
#include "RSGameState.generated.h"

UENUM()
enum class ERSPhase : uint8
{
	RoundEnd,     // итог раунда, покупать нельзя
	Intermission, // закупка перед раундом
	Live,         // идёт раунд
	MatchOver,    // матч сыгран
	Lobby         // матч не начат: персонаж стоит на карте, поверх меню
};

// запись killfeed, живёт локально на каждом клиенте
struct FRSKillEntry
{
	FString Killer;
	FString Victim;
	// Имя оружия оставлено запасным вариантом: killfeed рисует иконку, но
	// убить может и то, у чего иконки нет (падение, огонь, добивание миром).
	FString Weapon;
	// COUNT = оружие неизвестно, рисуем текстом
	ERSWeapon WeaponType = ERSWeapon::COUNT;
	// битовая маска обстоятельств, см. namespace RSKill в RSCharacter.h
	uint8 Flags = 0;
	bool bHeadshot = false;
	uint8 KillerTeam = 0;
	uint8 VictimTeam = 0;
	float Time = 0.f;
};

// Состояние матча живёт здесь, а не в GameMode: GameMode существует только
// на сервере, а счёт нужно показывать и подключившимся игрокам.
UCLASS()
class RAGESTRIKE_API ARSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	int32 RoundNumber = 0;

	UPROPERTY(Replicated)
	int32 ScoreCT = 0;

	UPROPERTY(Replicated)
	int32 ScoreT = 0;

	UPROPERTY(Replicated)
	float PhaseEndsAt = 0.f; // время сервера, когда закончится текущая фаза

	UPROPERTY(Replicated)
	ERSPhase Phase = ERSPhase::Intermission;

	UPROPERTY(Replicated)
	FString Announcement;

	// правила матча задаёт хост, клиенты получают их отсюда
	UPROPERTY(Replicated)
	int32 RoundsTotal = 24;

	UPROPERTY(Replicated)
	int32 RoundsToWin = 13;

	UPROPERTY(Replicated)
	int32 HalfTimeRound = 12;

	UPROPERTY(Replicated)
	int32 TeamSize = 5;

	// Закупка не заканчивается вместе с фазой: как в CS, докупиться можно
	// ещё 15 секунд после начала раунда.
	UPROPERTY(Replicated)
	float BuyEndsAt = 0.f;

	bool IsBuyTime() const;

	float GetTimeLeft() const;

	// killfeed: сервер рассылает, клиенты копят локально, HUD рисует
	UFUNCTION(NetMulticast, Reliable)
	// WeaponType передаётся числом: ERSWeapon не UENUM, а параметры RPC
	// должны быть реплицируемых типов. ERSWeapon::COUNT = неизвестно.
	void MulticastAddKill(const FString& Killer, const FString& Victim,
		const FString& Weapon, bool bHeadshot, uint8 KillerTeam, uint8 VictimTeam,
		uint8 WeaponType, uint8 Flags);

	TArray<FRSKillEntry> KillFeed;
};
