#include "RSMatchSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"

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

	// время шагами по 15 с: мельче крутить в меню бессмысленно
	int32 GetRoundSeconds()           { return GetInt(TEXT("RoundSeconds"), 120, 30, 300); }
	void SetRoundSeconds(int32 Value) { SetInt(TEXT("RoundSeconds"), FMath::Clamp(Value, 30, 300)); }

	int32 GetBuySeconds()             { return GetInt(TEXT("BuySeconds"), 15, 5, 60); }
	void SetBuySeconds(int32 Value)   { SetInt(TEXT("BuySeconds"), FMath::Clamp(Value, 5, 60)); }

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

	namespace
	{
		float GetOpt(const TCHAR* Key, float Def, float Min, float Max)
		{
			float Value = Def;
			GConfig->GetFloat(TEXT("RageStrike"), Key, Value, GGameUserSettingsIni);
			return FMath::Clamp(Value, Min, Max);
		}
		void SetOpt(const TCHAR* Key, float Value)
		{
			GConfig->SetFloat(TEXT("RageStrike"), Key, Value, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	float GetFov()                  { return GetOpt(TEXT("Fov"), 90.f, 70.f, 120.f); }
	void SetFov(float V)            { SetOpt(TEXT("Fov"), FMath::Clamp(V, 70.f, 120.f)); }

	float GetMusicVolume()          { return GetOpt(TEXT("VolMusic"), 1.f, 0.f, 1.f); }
	void SetMusicVolume(float V)    { SetOpt(TEXT("VolMusic"), FMath::Clamp(V, 0.f, 1.f)); }
	float GetFxVolume()             { return GetOpt(TEXT("VolFx"), 1.f, 0.f, 1.f); }
	void SetFxVolume(float V)       { SetOpt(TEXT("VolFx"), FMath::Clamp(V, 0.f, 1.f)); }

	float GetVmOffset()             { return GetOpt(TEXT("VmOffset"), 0.f, -8.f, 8.f); }
	void SetVmOffset(float V)       { SetOpt(TEXT("VmOffset"), FMath::Clamp(V, -8.f, 8.f)); }
	float GetVmOffsetZ()            { return GetOpt(TEXT("VmOffsetZ"), 0.f, -8.f, 8.f); }
	void SetVmOffsetZ(float V)      { SetOpt(TEXT("VmOffsetZ"), FMath::Clamp(V, -8.f, 8.f)); }
	float GetVmOffsetX()            { return GetOpt(TEXT("VmOffsetX"), 0.f, -8.f, 8.f); }
	void SetVmOffsetX(float V)      { SetOpt(TEXT("VmOffsetX"), FMath::Clamp(V, -8.f, 8.f)); }

	bool GetHideViewmodel()
	{
		bool bValue = false;
		GConfig->GetBool(TEXT("RageStrike"), TEXT("HideVm"), bValue, GGameUserSettingsIni);
		return bValue;
	}
	void SetHideViewmodel(bool bValue)
	{
		GConfig->SetBool(TEXT("RageStrike"), TEXT("HideVm"), bValue, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	int32 GetLoadoutPrimary()
	{
		int32 V = -1; // -1 — не выбрано
		GConfig->GetInt(TEXT("RageStrike"), TEXT("LoadoutPrimary"), V, GGameUserSettingsIni);
		return V;
	}
	void SetLoadoutPrimary(int32 Weapon)
	{
		GConfig->SetInt(TEXT("RageStrike"), TEXT("LoadoutPrimary"), Weapon, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	int32 GetLoadoutSecondary()
	{
		int32 V = -1;
		GConfig->GetInt(TEXT("RageStrike"), TEXT("LoadoutSecondary"), V, GGameUserSettingsIni);
		return V;
	}
	void SetLoadoutSecondary(int32 Weapon)
	{
		GConfig->SetInt(TEXT("RageStrike"), TEXT("LoadoutSecondary"), Weapon, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	bool GetAutoBuy()
	{
		bool bValue = false;
		GConfig->GetBool(TEXT("RageStrike"), TEXT("AutoBuy"), bValue, GGameUserSettingsIni);
		return bValue;
	}
	void SetAutoBuy(bool bValue)
	{
		GConfig->SetBool(TEXT("RageStrike"), TEXT("AutoBuy"), bValue, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	bool GetLoadoutArmor()
	{
		bool bValue = true;
		GConfig->GetBool(TEXT("RageStrike"), TEXT("LoadoutArmor"), bValue, GGameUserSettingsIni);
		return bValue;
	}
	void SetLoadoutArmor(bool bValue)
	{
		GConfig->SetBool(TEXT("RageStrike"), TEXT("LoadoutArmor"), bValue, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	float GetGamma()                { return GetOpt(TEXT("Gamma"), 2.2f, 1.6f, 3.2f); }
	void SetGamma(float V)          { SetOpt(TEXT("Gamma"), FMath::Clamp(V, 1.6f, 3.2f)); ApplyGamma(); }

	void ApplyGamma()
	{
		// гамма живёт в движке, а не в GameUserSettings — ставим напрямую
		if (GEngine)
		{
			GEngine->DisplayGamma = GetGamma();
		}
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
