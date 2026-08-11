#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RSTeam.h"
#include "RSGameMode.generated.h"

class ARSGameState;

UCLASS()
class RAGESTRIKE_API ARSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARSGameMode();

	virtual void BeginPlay() override;

	// вызывается при смерти игрока или бота — проверяет, не кончился ли раунд
	void OnCombatantDied();

	void SpawnBotAt(const FVector& Location, ERSTeam Team);

	// перечитать правила из настроек: вызывается на старте уровня и когда
	// хост меняет их в меню, иначе матч продолжал играть по старым цифрам
	void ApplyMatchSettings();
	// Игрок сменил сторону — состав перекосило, добираем заново.
	// Сторону читаем из мира: пешка проставляет Team до вызова.
	void OnPlayerTeamChanged();

	// правила матча берутся из настроек хоста, значения ниже — запасные
	int32 RoundsTotal = 24;
	int32 RoundsToWin = 13;
	int32 HalfTimeRound = 12;

	// время раунда и закупки задаёт хост в меню; значения ниже — запасные
	float RoundSeconds = 120.f;
	float BuySeconds = 15.f;
	static constexpr float RoundEndSeconds = 5.f;  // показ итога раунда
	static constexpr float ExtraBuySeconds = 15.f; // докупка после начала раунда
	static constexpr float MatchOverSeconds = 8.f; // показ итога матча

private:
	UFUNCTION()
	void PlaceEveryone();

	UFUNCTION()
	void PlaceInLobby();

	UFUNCTION()
	void StartBuyPhase();

	UFUNCTION()
	void StartRound();

	UFUNCTION()
	void FinishMatch();

	UFUNCTION()
	void OnRoundTimeout();

	void EndRound(ERSTeam Winner, const FString& Reason);
	void SwapSides();
	// единственное место, где меняется состав: добирает ботов до размера
	// команды и выгоняет лишних. Должна быть идемпотентной — её зовут
	// и на старте, и при смене стороны, и перед каждым раундом
	void RebalanceRoster();
	int32 CountAlive(ERSTeam Team) const;
	ARSGameState* RSState() const;

	// диагностика состава: поимённо кто в какой команде, вместе с теми,
	// кому уже вызвали Destroy — уничтожение в Unreal отложенное,
	// и такой «уходящий» бот мог бы попадать в подсчёт
	FString RosterDump() const;
	// Внутри смены сторон состав перевёрнут наполовину: игрокам команду уже
	// поменяли, ботам ещё нет. Считать по такому миру нельзя — на этом
	// и плодился лишний бот, поэтому на время свапа перебор состава заперт.
	bool bInSwapSides = false;

	FTimerHandle StartTimer;
	FTimerHandle PhaseTimer;
	int32 NextBotNumber = 0;

	// серия поражений для лосс-бонуса, [0] — CT, [1] — T
	int32 LossStreak[2] = { 0, 0 };
};
