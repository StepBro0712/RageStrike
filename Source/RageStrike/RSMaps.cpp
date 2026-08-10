#include "RSMaps.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

		// Секция в Config/DefaultGame.ini: этот файл уезжает в сборку (из
		// проектного конфига стажатся только DefaultEngine/DefaultGame/DefaultInput),
		// поэтому размеченные точки доезжают до собранной игры.
		const TCHAR* SpawnSection = TEXT("RageStrike.Spawns");

		FString ProjectGameIni()
		{
			return FPaths::ConvertRelativePathToFull(
				FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini"));
		}

		// Правим одну строку текстом, а не через GConfig->Flush: тот переписывает
		// ini целиком и теряет комментарии, а рядом лежит DirectoriesToAlwaysCook,
		// без которого из сборки пропадают звук и иконки.
		bool WriteSpawnToProjectIni(const FString& Key, const FString& Value)
		{
			const FString Path = ProjectGameIni();
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				// в собранной игре файл лежит внутри пака и не пишется —
				// там разметка остаётся только в настройках этой копии
				return false;
			}

			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines, false);

			const FString Header = FString::Printf(TEXT("[%s]"), SpawnSection);
			const FString Entry = FString::Printf(TEXT("%s=%s"), *Key, *Value);

			int32 SectionStart = INDEX_NONE;
			for (int32 i = 0; i < Lines.Num(); i++)
			{
				if (Lines[i].TrimStartAndEnd() == Header)
				{
					SectionStart = i;
					break;
				}
			}

			if (SectionStart == INDEX_NONE)
			{
				Lines.Add(TEXT(""));
				Lines.Add(Header);
				Lines.Add(Entry);
			}
			else
			{
				const FString Prefix = Key + TEXT("=");
				int32 i = SectionStart + 1;
				bool bReplaced = false;
				for (; i < Lines.Num(); i++)
				{
					const FString Trimmed = Lines[i].TrimStartAndEnd();
					if (Trimmed.StartsWith(TEXT("[")))
					{
						break; // началась следующая секция
					}
					if (Trimmed.StartsWith(Prefix))
					{
						Lines[i] = Entry;
						bReplaced = true;
						break;
					}
				}
				if (!bReplaced)
				{
					Lines.Insert(Entry, i);
				}
			}

			return FFileHelper::SaveStringToFile(
				FString::Join(Lines, TEXT("\n")) + TEXT("\n"), *Path);
		}
	}

	bool GetCustomSpawn(int32 MapIndex, bool bCT, FVector& OutLocation)
	{
		const FString Key = SpawnKey(MapIndex, bCT);

		// откуда взялась точка — печатаем один раз на ключ, иначе строка
		// уходила бы в лог на каждый спавн каждого бота
		static TSet<FString> Reported;
		const bool bFirst = !Reported.Contains(Key);
		if (bFirst)
		{
			Reported.Add(Key);
		}

		// сначала разметка этой копии игры: её мог сделать игрок прямо в сборке,
		// и она должна перебивать то, что приехало из проекта
		FString Value;
		if (GConfig->GetString(TEXT("RageStrike"), *Key, Value, GGameUserSettingsIni)
			&& OutLocation.InitFromString(Value))
		{
			UE_CLOG(bFirst, LogTemp, Log, TEXT("RS/спавн: %s — из настроек этой копии игры"), *Key);
			return true;
		}

		// затем разметка, уехавшая в сборку вместе с DefaultGame.ini
		if (GConfig->GetString(SpawnSection, *Key, Value, GGameIni)
			&& OutLocation.InitFromString(Value))
		{
			UE_CLOG(bFirst, LogTemp, Log, TEXT("RS/спавн: %s — из разметки проекта"), *Key);
			return true;
		}

		UE_CLOG(bFirst, LogTemp, Log, TEXT("RS/спавн: %s не размечен, автоподбор по геометрии"), *Key);
		return false;
	}

	void SetCustomSpawn(int32 MapIndex, bool bCT, const FVector& Location)
	{
		const FString Key = SpawnKey(MapIndex, bCT);
		const FString Value = Location.ToString();

		// эта копия игры — работает и в редакторе, и в собранной
		GConfig->SetString(TEXT("RageStrike"), *Key, *Value, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);

		// и проект, чтобы точка попала в следующую сборку
		WriteSpawnToProjectIni(Key, Value);
	}
}
