#include "RSBinds.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"

namespace RSBinds
{
	const TArray<FEntry>& All()
	{
		// Порядок = порядок строк в меню. Движение — оси со знаком: у одной
		// оси две клавиши, поэтому «вперёд» и «назад» это одно имя с разным
		// знаком, иначе они бы затирали друг друга.
		static const TArray<FEntry> Entries = {
			{ TEXT("Вперёд"),        TEXT("MoveForward"),   true,  1.f,  EKeys::W },
			{ TEXT("Назад"),         TEXT("MoveForward"),   true, -1.f,  EKeys::S },
			{ TEXT("Влево"),         TEXT("MoveRight"),     true, -1.f,  EKeys::A },
			{ TEXT("Вправо"),        TEXT("MoveRight"),     true,  1.f,  EKeys::D },
			{ TEXT("Прыжок"),        TEXT("Jump"),          false, 0.f,  EKeys::SpaceBar },
			{ TEXT("Присесть"),      TEXT("CrouchAction"),  false, 0.f,  EKeys::LeftControl },
			{ TEXT("Идти тихо"),     TEXT("Sprint"),        false, 0.f,  EKeys::LeftShift },
			{ TEXT("Огонь"),         TEXT("Fire"),          false, 0.f,  EKeys::LeftMouseButton },
			{ TEXT("Перезарядка"),   TEXT("Reload"),        false, 0.f,  EKeys::R },
		};
		return Entries;
	}

	FKey GetKey(const FEntry& Entry)
	{
		const UInputSettings* Settings = UInputSettings::GetInputSettings();
		if (Entry.bAxis)
		{
			for (const FInputAxisKeyMapping& M : Settings->GetAxisMappings())
			{
				if (M.AxisName == FName(Entry.Mapping) && FMath::IsNearlyEqual(M.Scale, Entry.Scale))
				{
					return M.Key;
				}
			}
		}
		else
		{
			for (const FInputActionKeyMapping& M : Settings->GetActionMappings())
			{
				if (M.ActionName == FName(Entry.Mapping))
				{
					return M.Key;
				}
			}
		}
		return Entry.Default;
	}

	namespace
	{
		void RebuildKeymaps()
		{
			// без пересборки старые привязки живут до перезапуска уровня
			for (TObjectIterator<UPlayerInput> It; It; ++It)
			{
				It->ForceRebuildingKeyMaps(true);
			}
		}
	}

	void SetKey(const FEntry& Entry, const FKey& NewKey)
	{
		UInputSettings* Settings = UInputSettings::GetInputSettings();
		if (Entry.bAxis)
		{
			TArray<FInputAxisKeyMapping> Existing = Settings->GetAxisMappings();
			for (const FInputAxisKeyMapping& M : Existing)
			{
				if (M.AxisName == FName(Entry.Mapping) && FMath::IsNearlyEqual(M.Scale, Entry.Scale))
				{
					Settings->RemoveAxisMapping(M);
				}
			}
			Settings->AddAxisMapping(FInputAxisKeyMapping(FName(Entry.Mapping), NewKey, Entry.Scale));
		}
		else
		{
			TArray<FInputActionKeyMapping> Existing = Settings->GetActionMappings();
			for (const FInputActionKeyMapping& M : Existing)
			{
				if (M.ActionName == FName(Entry.Mapping))
				{
					Settings->RemoveActionMapping(M);
				}
			}
			Settings->AddActionMapping(FInputActionKeyMapping(FName(Entry.Mapping), NewKey));
		}

		Settings->SaveKeyMappings();
		RebuildKeymaps();
	}

	void ResetAll()
	{
		for (const FEntry& Entry : All())
		{
			SetKey(Entry, Entry.Default);
		}
	}
}

namespace RSCrosshair
{
	namespace
	{
		const TCHAR* Sec = TEXT("RageStrike");

		float GetFloat(const TCHAR* Key, float Def)
		{
			float Value = Def;
			GConfig->GetFloat(Sec, Key, Value, GGameUserSettingsIni);
			return Value;
		}
		void SetFloat(const TCHAR* Key, float Value)
		{
			GConfig->SetFloat(Sec, Key, Value, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		bool GetBoolOpt(const TCHAR* Key, bool Def)
		{
			bool Value = Def;
			GConfig->GetBool(Sec, Key, Value, GGameUserSettingsIni);
			return Value;
		}
		void SetBoolOpt(const TCHAR* Key, bool Value)
		{
			GConfig->SetBool(Sec, Key, Value, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	float GetLength()            { return GetFloat(TEXT("CrossLength"), 9.f); }
	void SetLength(float V)      { SetFloat(TEXT("CrossLength"), FMath::Clamp(V, 0.f, 30.f)); }
	float GetThickness()         { return GetFloat(TEXT("CrossThick"), 2.f); }
	void SetThickness(float V)   { SetFloat(TEXT("CrossThick"), FMath::Clamp(V, 1.f, 8.f)); }
	float GetGap()               { return GetFloat(TEXT("CrossGap"), 5.f); }
	void SetGap(float V)         { SetFloat(TEXT("CrossGap"), FMath::Clamp(V, 0.f, 25.f)); }
	int32 GetChannel(int32 Which)
	{
		static const TCHAR* Keys[] = { TEXT("CrossR"), TEXT("CrossG"), TEXT("CrossB"), TEXT("CrossA") };
		static const int32 Defaults[] = { 25, 250, 65, 255 };
		const int32 I = FMath::Clamp(Which, 0, 3);
		int32 V = Defaults[I];
		GConfig->GetInt(Sec, Keys[I], V, GGameUserSettingsIni);
		return FMath::Clamp(V, 0, 255);
	}

	void SetChannel(int32 Which, int32 Value)
	{
		static const TCHAR* Keys[] = { TEXT("CrossR"), TEXT("CrossG"), TEXT("CrossB"), TEXT("CrossA") };
		const int32 I = FMath::Clamp(Which, 0, 3);
		GConfig->SetInt(Sec, Keys[I], FMath::Clamp(Value, 0, 255), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	FLinearColor GetColor()
	{
		return FLinearColor(GetChannel(0) / 255.f, GetChannel(1) / 255.f,
			GetChannel(2) / 255.f, GetChannel(3) / 255.f);
	}
	bool GetOutline()            { return GetBoolOpt(TEXT("CrossOutline"), true); }
	void SetOutline(bool V)      { SetBoolOpt(TEXT("CrossOutline"), V); }
	bool GetDot()                { return GetBoolOpt(TEXT("CrossDot"), false); }
	void SetDot(bool V)          { SetBoolOpt(TEXT("CrossDot"), V); }
	bool GetDynamic()            { return GetBoolOpt(TEXT("CrossDynamic"), true); }
	void SetDynamic(bool V)      { SetBoolOpt(TEXT("CrossDynamic"), V); }

	const TCHAR* ChannelName(int32 Which)
	{
		switch (Which)
		{
		case 0:  return TEXT("Красный");
		case 1:  return TEXT("Зелёный");
		case 2:  return TEXT("Синий");
		default: return TEXT("Прозрачность");
		}
	}
}
