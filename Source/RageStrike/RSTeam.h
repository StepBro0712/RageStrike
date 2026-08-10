#pragma once

#include "CoreMinimal.h"
#include "RSTeam.generated.h"

UENUM()
enum class ERSTeam : uint8
{
	CT UMETA(DisplayName = "Counter-Terrorists"),
	T  UMETA(DisplayName = "Terrorists")
};

inline const TCHAR* TeamName(ERSTeam Team)
{
	return Team == ERSTeam::CT ? TEXT("CT") : TEXT("T");
}
