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
	// боты всегда играют против команды хозяина игры
	void OnPlayerTeamChanged();

	int32 BotCount = 5;      // противников
	int32 TeammateCount = 4; // напарников, чтобы раунд не решался в одиночку

	// правила матча берутся из настроек хоста, значения ниже — запасные
	int32 RoundsTotal = 24;
	int32 RoundsToWin = 13;
	int32 HalfTimeRound = 12;

	static constexpr float RoundSeconds = 120.f;
	static constexpr float RoundEndSeconds = 5.f;  // показ итога раунда
	static constexpr float BuySeconds = 15.f;      // закупка перед раундом
	static constexpr float MatchOverSeconds = 8.f; // показ итога матча

private:
	UFUNCTION()
	void PlaceEveryone();

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
	void EnsureBots();
	int32 CountAlive(ERSTeam Team) const;
	ERSTeam GetBotTeam() const;
	ARSGameState* RSState() const;

	FTimerHandle StartTimer;
	FTimerHandle PhaseTimer;
	int32 NextBotNumber = 0;

	// серия поражений для лосс-бонуса, [0] — CT, [1] — T
	int32 LossStreak[2] = { 0, 0 };
};
