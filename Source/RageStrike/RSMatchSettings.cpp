#include "RSMatchSettings.h"
#include "Misc/ConfigCacheIni.h"

namespace RSMatch
{
	namespace
	{
		int32 GetInt(const TCHAR* Key, int32 Default, int32 Min, int32 Max)
		{
			int32 Value = Default;
			GConfig->GetInt(TEXT("RageStrike"), Key, Value, GGameUserSettingsIni);
			return FMath::Clamp(Value, Min, Max);
		}

		void SetInt(const TCHAR* Key, int32 Value)
		{
			GConfig->SetInt(TEXT("RageStrike"), Key, Value, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	int32 GetTeamSize()             { return GetInt(TEXT("TeamSize"), 5, 1, 5); }
	void SetTeamSize(int32 Value)   { SetInt(TEXT("TeamSize"), FMath::Clamp(Value, 1, 5)); }

	int32 GetRoundsToWin()           { return GetInt(TEXT("RoundsToWin"), 13, 3, 16); }
	void SetRoundsToWin(int32 Value) { SetInt(TEXT("RoundsToWin"), FMath::Clamp(Value, 3, 16)); }

	bool GetUseBots()
	{
		bool bValue = true;
		GConfig->GetBool(TEXT("RageStrike"), TEXT("UseBots"), bValue, GGameUserSettingsIni);
		return bValue;
	}

	void SetUseBots(bool bValue)
	{
		GConfig->SetBool(TEXT("RageStrike"), TEXT("UseBots"), bValue, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

namespace RSOptions
{
	int32 GetPerfMode()
	{
		int32 Value = 0;
		GConfig->GetInt(TEXT("RageStrike"), TEXT("PerfMode"), Value, GGameUserSettingsIni);
		return FMath::Clamp(Value, 0, 2);
	}

	void SetPerfMode(int32 Value)
	{
		// список замкнут в круг: щёлкать можно в любую сторону
		const int32 Wrapped = ((Value % 3) + 3) % 3;
		GConfig->SetInt(TEXT("RageStrike"), TEXT("PerfMode"), Wrapped, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	const TCHAR* PerfModeName(int32 Value)
	{
		switch (FMath::Clamp(Value, 0, 2))
		{
		case 1:  return TEXT("Только FPS");
		case 2:  return TEXT("FPS, ЦП и ГП");
		default: return TEXT("Выключено");
		}
	}
}
