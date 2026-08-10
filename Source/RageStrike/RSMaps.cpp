#include "RSMaps.h"
#include "Misc/ConfigCacheIni.h"

namespace RSMaps
{
	const TArray<FRSMapDef>& All()
	{
		// первая в списке — карта по умолчанию
		static const TArray<FRSMapDef> Maps = {
			{ TEXT("Dust 2"), TEXT("/Game/Maps/Dust2"),  1.5f, 3800.f, 50.f },
			{ TEXT("Mirage"), TEXT("/Game/Maps/Mirage"), 1.0f, 6000.f, 50.f },
			{ TEXT("Арена"),  nullptr,                   1.0f, 3300.f,  0.f },
		};
		return Maps;
	}

	int32 GetSelectedIndex()
	{
		int32 Index = 0;
		GConfig->GetInt(TEXT("RageStrike"), TEXT("MapIndex"), Index, GGameUserSettingsIni);
		return FMath::Clamp(Index, 0, All().Num() - 1);
	}

	void SetSelectedIndex(int32 Index)
	{
		Index = FMath::Clamp(Index, 0, All().Num() - 1);
		GConfig->SetInt(TEXT("RageStrike"), TEXT("MapIndex"), Index, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	const FRSMapDef& Selected()
	{
		return All()[GetSelectedIndex()];
	}

	namespace
	{
		FString SpawnKey(int32 MapIndex, bool bCT)
		{
			return FString::Printf(TEXT("Spawn%s_%d"), bCT ? TEXT("CT") : TEXT("T"), MapIndex);
		}
	}

	bool GetCustomSpawn(int32 MapIndex, bool bCT, FVector& OutLocation)
	{
		FString Value;
		if (!GConfig->GetString(TEXT("RageStrike"), *SpawnKey(MapIndex, bCT), Value, GGameUserSettingsIni))
		{
			return false;
		}
		return OutLocation.InitFromString(Value);
	}

	void SetCustomSpawn(int32 MapIndex, bool bCT, const FVector& Location)
	{
		GConfig->SetString(TEXT("RageStrike"), *SpawnKey(MapIndex, bCT),
			*Location.ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
