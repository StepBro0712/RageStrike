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
#include "Engine/StaticMeshActor.h"
#include "Camera/CameraActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
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

	ApplyMatchSettings();

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

	// В лобби матч не запускаем: ни ботов, ни раундов, ни закупки —
	// персонаж просто стоит на карте, а поверх открыто меню.
	if (ARSPlayerController::IsLobby())
	{
		GetWorldTimerManager().SetTimer(StartTimer, this, &ARSGameMode::PlaceInLobby, 0.25f, false);
		return;
	}

	// расстановка ждёт, пока физика зарегистрирует коллизию карты:
	// иначе трассировки не находят пол и все проваливаются под карту
	GetWorldTimerManager().SetTimer(StartTimer, this, &ARSGameMode::PlaceEveryone, 0.25f, false);
}

void ARSGameMode::ApplyMatchSettings()
{
	// правила матча задаёт тот, кто поднял сервер
	RoundsToWin = RSMatch::GetRoundsToWin();
	RoundsTotal = RSMatch::RoundsTotalFor(RoundsToWin);
	HalfTimeRound = RSMatch::HalfTimeFor(RoundsToWin);

	const int32 TeamSize = RSMatch::GetTeamSize();
	RoundSeconds = RSMatch::GetRoundSeconds();
	BuySeconds = RSMatch::GetBuySeconds();

	UE_LOG(LogTemp, Log, TEXT("RS/состав: ApplyMatchSettings: TeamSize=%d, боты=%d, RoundsToWin=%d, состав: %s"),
		TeamSize, RSMatch::GetUseBots() ? 1 : 0, RoundsToWin, *RosterDump());

	if (ARSGameState* State = RSState())
	{
		State->RoundsTotal = RoundsTotal;
		State->RoundsToWin = RoundsToWin;
		State->HalfTimeRound = HalfTimeRound;
		State->TeamSize = TeamSize;
	}

	// состав подгоняем под новый размер команды прямо сейчас
	RebalanceRoster();
}

void ARSGameMode::PlaceInLobby()
{
	UWorld* World = GetWorld();

	// Сцена лобби — отдельная площадка высоко над картой: карта остаётся
	// фоном далеко внизу, а персонаж стоит один, как на витрине. Ставить
	// его на боевой спавн было проще, но выглядело как брошенный матч.
	const FVector Stage(0.f, 0.f, ARSArena::GetMapFloor(World) + 6000.f);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.ObjectFlags |= RF_Transient;

	if (AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(), Stage - FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, SP))
	{
		Floor->SetMobility(EComponentMobility::Movable);
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
			// куб движка — метр на метр: растягиваем в круглую площадку
			Floor->GetStaticMeshComponent()->SetWorldScale3D(FVector(4.f, 4.f, 0.4f));
		}
	}

	// ставим игрока на площадку лицом к камере и замораживаем: в лобби
	// незачем ходить, а с площадки можно было бы просто упасть
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		It->RespawnForRound(Stage);
		It->SetActorRotation(FRotator::ZeroRotator);
		// Движение выключаем, но не через FreezeUntilRound: тот прячет актёра
		// целиком, и в лобби на площадке никого не было видно.
		It->GetCharacterMovement()->DisableMovement();
	}

	// камера смотрит на персонажа спереди и чуть сверху
	if (ACameraActor* Cam = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(), Stage + FVector(260.f, 0.f, 95.f),
		FRotator(-6.f, 180.f, 0.f), SP))
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTargetWithBlend(Cam, 0.4f);
		}
	}

	if (ARSGameState* State = RSState())
	{
		State->Phase = ERSPhase::Lobby;
		State->Announcement = TEXT("Лобби — нажми «Играть» в меню");
	}
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

	RebalanceRoster();
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		It->RespawnForRound(ARSArena::FindSpawnPoint(GetWorld(), It->Team));
	}

	State->Phase = ERSPhase::Intermission;
	State->Announcement = FString::Printf(TEXT("Закупка перед раундом %d — B"), State->RoundNumber);
	State->PhaseEndsAt = GetWorld()->GetTimeSeconds() + BuySeconds;
	// докупиться можно ещё 15 секунд после начала раунда, как в CS
	State->BuyEndsAt = State->PhaseEndsAt + ExtraBuySeconds;
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

	UE_LOG(LogTemp, Log, TEXT("RS/состав: SwapSides начало: %s"), *RosterDump());
	bInSwapSides = true;

	// стороны меняют и игроки, и боты — составы команд остаются прежними
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		It->SetTeam(It->Team == ERSTeam::CT ? ERSTeam::T : ERSTeam::CT);
	}
	UE_LOG(LogTemp, Log, TEXT("RS/состав: SwapSides, игроки перевёрнуты: %s"), *RosterDump());

	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		It->SetTeam(It->Team == ERSTeam::CT ? ERSTeam::T : ERSTeam::CT);
	}

	bInSwapSides = false;
	UE_LOG(LogTemp, Log, TEXT("RS/состав: SwapSides конец: %s"), *RosterDump());

	// свап состав не меняет, но пересчёт дешёвый и чинит перекос,
	// если он всё-таки откуда-то взялся
	RebalanceRoster();
}

FString ARSGameMode::RosterDump() const
{
	FString Out;
	for (int32 Side = 0; Side < 2; Side++)
	{
		const ERSTeam Team = (Side == 0) ? ERSTeam::CT : ERSTeam::T;

		int32 Players = 0;
		for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
		{
			if (It->Team == Team)
			{
				Players++;
			}
		}

		// AllActors — чтобы увидеть и тех, кого уже уничтожили в этом кадре
		TArray<FString> Names;
		for (TActorIterator<ARSBot> It(GetWorld(), ARSBot::StaticClass(), EActorIteratorFlags::AllActors); It; ++It)
		{
			if (It->Team != Team)
			{
				continue;
			}
			Names.Add(FString::Printf(TEXT("%s%s%s"), *It->Nick,
				It->Health > 0.f ? TEXT("") : TEXT("|мёртв"),
				(!IsValid(*It) || It->IsActorBeingDestroyed()) ? TEXT("|уничтожается") : TEXT("")));
		}

		Out += FString::Printf(TEXT("%s игроков=%d ботов=%d [%s]  "),
			Side == 0 ? TEXT("CT") : TEXT("T"), Players, Names.Num(), *FString::Join(Names, TEXT(", ")));
	}
	return Out;
}

void ARSGameMode::OnPlayerTeamChanged()
{
	RebalanceRoster();
}

void ARSGameMode::RebalanceRoster()
{
	// Внутри SwapSides мир перевёрнут наполовину: игроки уже сменили сторону,
	// боты ещё нет. Пересчёт по такому состоянию убивал «лишнего» бота на
	// стороне игрока и тут же спавнил нового напротив — он и был лишним.
	if (bInSwapSides)
	{
		return;
	}

	const int32 TeamSize = RSMatch::GetTeamSize();
	// в лобби ботов нет вовсе: там никто не воюет, а состав добирается
	// заново при старте матча
	const bool bBots = RSMatch::GetUseBots() && !ARSPlayerController::IsLobby();

	for (int32 Side = 0; Side < 2; Side++)
	{
		const ERSTeam Team = (Side == 0) ? ERSTeam::CT : ERSTeam::T;

		// места в составе сначала занимают живые игроки, остальное добирают боты
		int32 Humans = 0;
		for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It) && It->Team == Team)
			{
				Humans++;
			}
		}

		TArray<ARSBot*> Bots;
		for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It) && It->Team == Team)
			{
				Bots.Add(*It);
			}
		}

		const int32 Target = bBots ? FMath::Max(0, TeamSize - Humans) : 0;
		const int32 Extra = Bots.Num() - Target;
		if (Extra == 0)
		{
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("RS/состав:   сторона %s: игроков=%d, ботов=%d, цель ботов=%d"),
			Side == 0 ? TEXT("CT") : TEXT("T"), Humans, Bots.Num(), Target);

		// лишних выгоняем, начиная с уже убитых
		if (Extra > 0)
		{
			Bots.Sort([](const ARSBot& A, const ARSBot& B) { return A.Health < B.Health; });
			for (int32 i = 0; i < Extra; i++)
			{
				UE_LOG(LogTemp, Log, TEXT("RS/состав:   убираю %s (hp=%.0f)"), *Bots[i]->Nick, Bots[i]->Health);
				Bots[i]->Destroy();
			}
		}

		// недостающих досаживаем на точки своей стороны
		for (int32 i = 0; i < -Extra; i++)
		{
			SpawnBotAt(ARSArena::FindSpawnPoint(GetWorld(), Team), Team);
		}

		UE_LOG(LogTemp, Log, TEXT("RS/состав: состав после правки: %s"), *RosterDump());
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
		Bot->Nick = ARSBot::MakeBotNick(Bot->BotNumber);

		UE_LOG(LogTemp, Log, TEXT("RS/состав:   спавн %s в %s"), *Bot->Nick,
			Team == ERSTeam::CT ? TEXT("CT") : TEXT("T"));
	}
}
