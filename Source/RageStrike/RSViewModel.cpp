#include "RSViewModel.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Misc/ConfigCacheIni.h"

namespace RSViewModel
{
	namespace
	{
		// Одна запись таблицы: путь к мешу и к каждой анимации. Имена анимаций
		// у стволов не унифицированы — у AK это ak47_draw, у AWP awp_draw,
		// у M4A4 просто draw, — поэтому шаблоном их не собрать, и таблица
		// генерируется по факту импорта.
		struct FRSVMEntry
		{
			ERSWeapon Weapon;
			const TCHAR* Mesh;
			const TCHAR* Idle;
			const TCHAR* Draw;
			const TCHAR* Reload;
			const TCHAR* Shoot[3];
			const TCHAR* Inspect;
		};

		#include "RSViewModelTable.h"

		template <typename T>
		T* LoadPart(const TCHAR* Path)
		{
			if (!Path)
			{
				return nullptr;
			}
			T* Loaded = LoadObject<T>(nullptr, Path);
			if (Loaded)
			{
				Loaded->AddToRoot();
			}
			return Loaded;
		}

		FRSViewModel Build(const FRSVMEntry& E)
		{
			FRSViewModel VM;
			VM.Mesh = LoadPart<USkeletalMesh>(E.Mesh);
			VM.Idle = LoadPart<UAnimSequence>(E.Idle);
			VM.Draw = LoadPart<UAnimSequence>(E.Draw);
			VM.Reload = LoadPart<UAnimSequence>(E.Reload);
			for (int32 i = 0; i < 3; i++)
			{
				VM.Shoot[i] = LoadPart<UAnimSequence>(E.Shoot[i]);
			}
			VM.Inspect = LoadPart<UAnimSequence>(E.Inspect);

			// У части стволов вариант выстрела один: подставляем его на
			// остальные слоты, иначе каждый второй выстрел был бы без анимации.
			for (int32 i = 1; i < 3; i++)
			{
				if (!VM.Shoot[i])
				{
					VM.Shoot[i] = VM.Shoot[0];
				}
			}
			return VM;
		}
	}

	const FRSViewModel* Get(ERSWeapon Weapon)
	{
		const FRSVMEntry* Entry = nullptr;
		for (const FRSVMEntry& E : GVMTable)
		{
			if (E.Weapon == Weapon)
			{
				Entry = &E;
				break;
			}
		}
		if (!Entry)
		{
			return nullptr; // нет вьюмодели — покажется статик-меш
		}

		static TMap<uint8, FRSViewModel> Cache;
		const uint8 Key = (uint8)Weapon;
		if (FRSViewModel* Found = Cache.Find(Key))
		{
			return Found->IsValid() ? Found : nullptr;
		}

		const FRSViewModel Built = Build(*Entry);
		FRSViewModel& Stored = Cache.Add(Key, Built);
		return Stored.IsValid() ? &Stored : nullptr;
	}

	FRSVMPlace GetPlace(ERSWeapon Weapon)
	{
		// Значения по умолчанию. Подбираются режимом подгонки в игре и
		// вписываются сюда; до подбора это просто стартовая точка.
		// Scale = 0 означает «посчитать из габаритов меша». Жёсткое число
		// здесь уже стоило ошибки в восемьдесят раз: меш USP оказался длиной
		// 42.75 единицы, а не меньше сантиметра, как считалось раньше, и
		// масштаб 40 давал семнадцатиметровый пистолет. Замер надёжнее.
		// Цифры подобраны в игре режимом F8 и перенесены сюда, иначе они
		// остались бы только в настройках одной машины и в сборку не уехали.
		FRSVMPlace P;
		P.Scale = 0.f;
		switch (Weapon)
		{
		case ERSWeapon::AK47:
			P.Scale = 0.84f;
			P.Loc = FVector(31.4f, 9.f, -9.f);
			P.Rot = FRotator(0.f, -95.f, 0.f);
			break;
		case ERSWeapon::USP:
			P.Scale = 0.51f;
			P.Loc = FVector(31.6f, 9.f, -14.f);
			P.Rot = FRotator(0.f, -90.f, 0.f);
			break;
		default:
			break;
		}

		// Поверх кладём то, что игрок подобрал сам: ключ на ствол, чтобы
		// у каждого была своя посадка.
		// Без точки в имени: секция «RageStrike.ViewModel» молча не
		// сохранялась — точка трактуется движком как путь к классу для
		// пообъектных настроек. Прицел пишется в «RageStrike» и работает.
		const TCHAR* Sec = TEXT("RageStrikeViewModel");
		const FString Pre = FString::Printf(TEXT("VM%d_"), (int32)Weapon);
		GConfig->GetFloat(Sec, *(Pre + TEXT("Scale")), P.Scale, GGameUserSettingsIni);
		GConfig->GetVector(Sec, *(Pre + TEXT("Loc")), P.Loc, GGameUserSettingsIni);
		GConfig->GetRotator(Sec, *(Pre + TEXT("Rot")), P.Rot, GGameUserSettingsIni);
		return P;
	}

	void SetPlace(ERSWeapon Weapon, const FRSVMPlace& P)
	{
		// Без точки в имени: секция «RageStrike.ViewModel» молча не
		// сохранялась — точка трактуется движком как путь к классу для
		// пообъектных настроек. Прицел пишется в «RageStrike» и работает.
		const TCHAR* Sec = TEXT("RageStrikeViewModel");
		const FString Pre = FString::Printf(TEXT("VM%d_"), (int32)Weapon);
		GConfig->SetFloat(Sec, *(Pre + TEXT("Scale")), P.Scale, GGameUserSettingsIni);
		GConfig->SetVector(Sec, *(Pre + TEXT("Loc")), P.Loc, GGameUserSettingsIni);
		GConfig->SetRotator(Sec, *(Pre + TEXT("Rot")), P.Rot, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	FString PlaceToString(ERSWeapon Weapon)
	{
		const FRSVMPlace P = GetPlace(Weapon);
		return FString::Printf(
			TEXT("Scale %.2f  Loc %.1f %.1f %.1f  Rot %.1f %.1f %.1f"),
			P.Scale, P.Loc.X, P.Loc.Y, P.Loc.Z, P.Rot.Pitch, P.Rot.Yaw, P.Rot.Roll);
	}
}
