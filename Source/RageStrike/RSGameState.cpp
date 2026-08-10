#include "RSGameState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

void ARSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARSGameState, RoundNumber);
	DOREPLIFETIME(ARSGameState, ScoreCT);
	DOREPLIFETIME(ARSGameState, ScoreT);
	DOREPLIFETIME(ARSGameState, PhaseEndsAt);
	DOREPLIFETIME(ARSGameState, Phase);
	DOREPLIFETIME(ARSGameState, Announcement);
	DOREPLIFETIME(ARSGameState, RoundsTotal);
	DOREPLIFETIME(ARSGameState, RoundsToWin);
	DOREPLIFETIME(ARSGameState, HalfTimeRound);
	DOREPLIFETIME(ARSGameState, TeamSize);
	DOREPLIFETIME(ARSGameState, BuyEndsAt);
}

bool ARSGameState::IsBuyTime() const
{
	// в перерыве всегда, а в бою — пока не вышло докупочное время
	if (Phase == ERSPhase::Intermission)
	{
		return true;
	}
	return Phase == ERSPhase::Live && GetWorld()->GetTimeSeconds() < BuyEndsAt;
}

float ARSGameState::GetTimeLeft() const
{
	return FMath::Max(0.f, PhaseEndsAt - GetWorld()->GetTimeSeconds());
}

void ARSGameState::MulticastAddKill_Implementation(const FString& Killer, const FString& Victim,
	const FString& Weapon, bool bHeadshot, uint8 KillerTeam, uint8 VictimTeam)
{
	FRSKillEntry E;
	E.Killer = Killer;
	E.Victim = Victim;
	E.Weapon = Weapon;
	E.bHeadshot = bHeadshot;
	E.KillerTeam = KillerTeam;
	E.VictimTeam = VictimTeam;
	E.Time = GetWorld()->GetTimeSeconds();
	KillFeed.Add(E);
	if (KillFeed.Num() > 8)
	{
		KillFeed.RemoveAt(0);
	}
}
