#include "RSGameMode.h"
#include "RSGameState.h"
#include "RSCharacter.h"
#include "RSBot.h"
#include "RSHUD.h"
#include "RSArena.h"
#include "RSMaps.h"
#include "RSWeaponPickup.h"
#include "RSPlayerController.h"
#include "RSMatchSettings.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"

ARSGameMode::ARSGameMode()
{
	DefaultPawnClass = ARSCharacter::StaticClass();
	HUDClass = ARSHUD::StaticClass();
	PlayerControllerClass = ARSPlayerController::StaticClass();
	GameStateClass = ARSGameState::StaticClass();
}

ARSGameState* ARSGameMode::RSState() const
{
	return GetGameState<ARSGameState>();
}

void ARSGameMode::BeginPlay()
{
	Super::BeginPlay();

	// правила матча задаёт тот, кто поднял сервер
	RoundsToWin = RSMatch::GetRoundsToWin();
	RoundsTotal = RSMatch::RoundsTotalFor(RoundsToWin);
	HalfTimeRound = RSMatch::HalfTimeFor(RoundsToWin);

	const int32 TeamSize = RSMatch::GetTeamSize();
	const bool bBots = RSMatch::GetUseBots();
	// боты добирают состав: противников по размеру команды, своих на одного
	// меньше — одно место занимает сам игрок
	BotCount = bBots ? TeamSize : 0;
	TeammateCount = bBots ? FMath::Max(0, TeamSize - 1) : 0;

	if (ARSGameState* State = RSState())
	{
		State->RoundsTotal = RoundsTotal;
		State->RoundsToWin = RoundsToWin;
		State->HalfTimeRound = HalfTimeRound;
		State->TeamSize = TeamSize;
	}

	// арена: сервер задаёт Seed, клиенты строят такую же локально по репликации
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ARSArena* Arena = GetWorld()->SpawnActor<ARSArena>(ARSArena::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, SP))
	{
		Arena->Seed = FMath::Rand();
		Arena->MapIndex = RSMaps::GetSelectedIndex();
		Arena->Build();
	}

	// пешка появляется раньше, чем построена карта, поэтому замораживаем её:
	// иначе первые доли секунды игрок падает сквозь ещё не готовый уровень
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		It->FreezeUntilRound();
	}

	// расстановка ждёт, пока физика зарегистрирует коллизию карты:
	// иначе трассировки не находят пол и все проваливаются под карту
	GetWorldTimerManager().SetTimer(StartTimer, this, &ARSGameMode::PlaceEveryone, 0.25f, false);
}

void ARSGameMode::PlaceEveryone()
{
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector Spawn = ARSArena::FindSpawnPoint(GetWorld(), ERSTeam::CT);
	GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Spawn, FRotator::ZeroRotator, SP);

	// ставим игроков на карту и даём закупиться до первого раунда,
	// иначе первая возможность купить появлялась только после раунда 1
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		It->RespawnForRound(ARSArena::FindSpawnPoint(GetWorld(), It->Team));
	}

	StartBuyPhase();
}

void ARSGameMode::StartBuyPhase()
{
	ARSGameState* State = RSState();
	if (!State || State->Phase == ERSPhase::MatchOver)
	{
		return;
	}

	State->RoundNumber++;

	// после 12-го раунда команды меняются сторонами вместе со счётом,
	// чтобы очки остались у своих игроков
	if (State->RoundNumber == HalfTimeRound + 1)
	{
		SwapSides();
		Swap(State->ScoreCT, State->ScoreT);
	}

	// возрождаем всех сразу на закупку: игрок стоит на своей точке
	// и выбирает оружие перед раундом, а не над телами прошлого
	for (TActorIterator<ARSWeaponPickup> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		It->RespawnForRound(ARSArena::FindSpawnPoint(GetWorld(), It->Team));
	}

	EnsureBots();
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		It->RespawnForRound(ARSArena::FindSpawnPoint(GetWorld(), It->Team));
	}

	State->Phase = ERSPhase::Intermission;
	State->Announcement = FString::Printf(TEXT("Закупка перед раундом %d — B"), State->RoundNumber);
	State->PhaseEndsAt = GetWorld()->GetTimeSeconds() + BuySeconds;
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ARSGameMode::StartRound, BuySeconds, false);
}

int32 ARSGameMode::CountAlive(ERSTeam Team) const
{
	int32 Count = 0;
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->bAlive && It->Team == Team)
		{
			Count++;
		}
	}
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->Health > 0.f && It->Team == Team)
		{
			Count++;
		}
	}
	return Count;
}

void ARSGameMode::StartRound()
{
	ARSGameState* State = RSState();
	if (!State || State->Phase == ERSPhase::MatchOver)
	{
		return;
	}

	State->Announcement.Empty();

	UE_LOG(LogTemp, Log, TEXT("RageStrike: round %d started, score %d:%d, alive CT=%d T=%d"),
		State->RoundNumber, State->ScoreCT, State->ScoreT,
		CountAlive(ERSTeam::CT), CountAlive(ERSTeam::T));


	State->Phase = ERSPhase::Live;
	State->PhaseEndsAt = GetWorld()->GetTimeSeconds() + RoundSeconds;
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ARSGameMode::OnRoundTimeout, RoundSeconds, false);
}

void ARSGameMode::OnCombatantDied()
{
	ARSGameState* State = RSState();
	if (!State || State->Phase != ERSPhase::Live)
	{
		return;
	}

	// счётчики берём в следующем кадре: погибший ещё не успел обновиться
	if (CountAlive(ERSTeam::T) == 0)
	{
		EndRound(ERSTeam::CT, TEXT("Террористы уничтожены"));
	}
	else if (CountAlive(ERSTeam::CT) == 0)
	{
		EndRound(ERSTeam::T, TEXT("Спецназ уничтожен"));
	}
}

void ARSGameMode::OnRoundTimeout()
{
	// время вышло — побеждает команда, которой осталось больше;
	// при равенстве раунд за обороняющимися, как в CS
	const int32 AliveCT = CountAlive(ERSTeam::CT);
	const int32 AliveT = CountAlive(ERSTeam::T);
	EndRound(AliveT > AliveCT ? ERSTeam::T : ERSTeam::CT, TEXT("Время вышло"));
}

void ARSGameMode::EndRound(ERSTeam Winner, const FString& Reason)
{
	ARSGameState* State = RSState();
	if (!State || State->Phase != ERSPhase::Live)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PhaseTimer);

	int32& WinnerScore = (Winner == ERSTeam::CT) ? State->ScoreCT : State->ScoreT;
	WinnerScore++;

	// экономика CS: победителям $3250, проигравшим лосс-бонус,
	// растущий с каждым поражением подряд ($1400 → $3400)
	const ERSTeam Loser = (Winner == ERSTeam::CT) ? ERSTeam::T : ERSTeam::CT;
	LossStreak[(uint8)Winner == (uint8)ERSTeam::CT ? 0 : 1] = 0;
	int32& LoserStreak = LossStreak[Loser == ERSTeam::CT ? 0 : 1];
	LoserStreak = FMath::Min(LoserStreak + 1, 5);
	const int32 LossBonus = 1400 + 500 * (LoserStreak - 1);

	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		It->AddMoney(It->Team == Winner ? 3250 : LossBonus);
	}

	State->Phase = ERSPhase::RoundEnd;
	State->Announcement = FString::Printf(TEXT("Раунд за %s — %s"),
		Winner == ERSTeam::CT ? TEXT("CT") : TEXT("T"), *Reason);

	UE_LOG(LogTemp, Log, TEXT("RageStrike: round %d end (%s), alive CT=%d T=%d"),
		State->RoundNumber, *Reason, CountAlive(ERSTeam::CT), CountAlive(ERSTeam::T));

	const bool bSomeoneWon = State->ScoreCT >= RoundsToWin || State->ScoreT >= RoundsToWin;
	const bool bRoundsOver = State->RoundNumber >= RoundsTotal;

	if (bSomeoneWon || bRoundsOver)
	{
		State->Phase = ERSPhase::MatchOver;
		State->PhaseEndsAt = GetWorld()->GetTimeSeconds() + MatchOverSeconds;
		// показали итог — и возвращаемся в главное меню
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ARSGameMode::FinishMatch,
			MatchOverSeconds, false);

		if (State->ScoreCT == State->ScoreT)
		{
			State->Announcement = FString::Printf(TEXT("НИЧЬЯ  %d : %d"), State->ScoreCT, State->ScoreT);
		}
		else
		{
			const bool bCTWon = State->ScoreCT > State->ScoreT;
			State->Announcement = FString::Printf(TEXT("ПОБЕДА %s  %d : %d"),
				bCTWon ? TEXT("CT") : TEXT("T"),
				bCTWon ? State->ScoreCT : State->ScoreT,
				bCTWon ? State->ScoreT : State->ScoreCT);
		}
		return;
	}

	// сначала пять секунд на итог раунда, покупать в это время нельзя
	State->PhaseEndsAt = GetWorld()->GetTimeSeconds() + RoundEndSeconds;
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ARSGameMode::StartBuyPhase, RoundEndSeconds, false);
}

void ARSGameMode::FinishMatch()
{
	// матч сыгран: возвращаем игрока в стартовое меню
	ARSPlayerController::AllowStartupMenu();
	if (ARSPlayerController* PC = Cast<ARSPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->ReloadLevel();
	}
}

void ARSGameMode::SwapSides()
{
	// новая половина — экономика серий начинается заново
	LossStreak[0] = LossStreak[1] = 0;

	// стороны меняют и игроки, и боты — составы команд остаются прежними
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		It->SetTeam(It->Team == ERSTeam::CT ? ERSTeam::T : ERSTeam::CT);
	}
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		It->SetTeam(It->Team == ERSTeam::CT ? ERSTeam::T : ERSTeam::CT);
	}
}

ERSTeam ARSGameMode::GetBotTeam() const
{
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (const ARSCharacter* Player = Cast<ARSCharacter>(PC->GetPawn()))
		{
			return Player->Team == ERSTeam::CT ? ERSTeam::T : ERSTeam::CT;
		}
	}
	return ERSTeam::T;
}

void ARSGameMode::OnPlayerTeamChanged()
{
	// составы теперь постоянные: перекидывать ботов нельзя, иначе
	// вместе с командой у них сбрасывалась бы принадлежность и счёт
}

void ARSGameMode::EnsureBots()
{
	int32 Have[2] = { 0, 0 };
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		Have[It->Team == ERSTeam::CT ? 0 : 1]++;
	}

	const ERSTeam BotTeam = GetBotTeam();
	const ERSTeam PlayerTeam = (BotTeam == ERSTeam::CT) ? ERSTeam::T : ERSTeam::CT;

	const int32 NeedEnemy = BotCount - Have[BotTeam == ERSTeam::CT ? 0 : 1];
	for (int32 i = 0; i < NeedEnemy; i++)
	{
		SpawnBotAt(ARSArena::FindSpawnPoint(GetWorld(), BotTeam), BotTeam);
	}

	const int32 NeedMate = TeammateCount - Have[PlayerTeam == ERSTeam::CT ? 0 : 1];
	for (int32 i = 0; i < NeedMate; i++)
	{
		SpawnBotAt(ARSArena::FindSpawnPoint(GetWorld(), PlayerTeam), PlayerTeam);
	}
}

void ARSGameMode::SpawnBotAt(const FVector& Location, ERSTeam Team)
{
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (ARSBot* Bot = GetWorld()->SpawnActor<ARSBot>(ARSBot::StaticClass(), Location, FRotator::ZeroRotator, SP))
	{
		Bot->SetTeam(Team);
		Bot->BotNumber = ++NextBotNumber;
	}
}
