#include "RSViewModel.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"

namespace RSViewModel
{
	namespace
	{
		// В glTF из CS2 ассеты лежат по шаблону
		// /Game/Weapons/CS2/<Пакет>/Meshes/<Пакет>/SkeletalMeshes/<Пакет><Суффикс>
		const TCHAR* PackageFor(ERSWeapon W)
		{
			switch (W)
			{
			case ERSWeapon::AK47: return TEXT("AK47");
			case ERSWeapon::USP:  return TEXT("USPS");
			default:              return nullptr;
			}
		}

		template <typename T>
		T* LoadPart(const TCHAR* Pack, const FString& Suffix)
		{
			const FString Name = FString::Printf(TEXT("%s%s"), Pack, *Suffix);
			const FString Path = FString::Printf(
				TEXT("/Game/Weapons/CS2/%s/Meshes/%s/SkeletalMeshes/%s.%s"),
				Pack, Pack, *Name, *Name);
			T* Loaded = LoadObject<T>(nullptr, *Path);
			if (Loaded)
			{
				Loaded->AddToRoot();
			}
			return Loaded;
		}

		FRSViewModel Build(const TCHAR* Pack)
		{
			FRSViewModel VM;
			VM.Mesh = LoadPart<USkeletalMesh>(Pack, FString());
			VM.Idle = LoadPart<UAnimSequence>(Pack, TEXT("firstperson_idle"));
			VM.Draw = LoadPart<UAnimSequence>(Pack, TEXT("firstperson_draw"));
			VM.Reload = LoadPart<UAnimSequence>(Pack, TEXT("firstperson_reload"));
			VM.Shoot[0] = LoadPart<UAnimSequence>(Pack, TEXT("firstperson_shoot1"));
			VM.Shoot[1] = LoadPart<UAnimSequence>(Pack, TEXT("firstperson_shoot2"));
			VM.Shoot[2] = LoadPart<UAnimSequence>(Pack, TEXT("firstperson_shoot3"));
			// осмотр в CS называется lookat
			VM.Inspect = LoadPart<UAnimSequence>(Pack, TEXT("firstperson_lookat01"));
			return VM;
		}
	}

	const FRSViewModel* Get(ERSWeapon Weapon)
	{
		// Скелетная вьюмодель пока выключена: у скелета AK-47 габариты в
		// исходнике меньше сантиметра, автоподбор масштаба раздувает модель
		// на пол-экрана. Пока посадка не выставлена вручную, показываем
		// статик-меши — они сидят правильно.
		static constexpr bool bEnabled = false;
		if (!bEnabled)
		{
			return nullptr;
		}

		const TCHAR* Pack = PackageFor(Weapon);
		if (!Pack)
		{
			return nullptr;
		}

		static TMap<FString, FRSViewModel> Cache;
		const FString Key(Pack);
		if (FRSViewModel* Found = Cache.Find(Key))
		{
			return Found->IsValid() ? Found : nullptr;
		}

		const FRSViewModel Built = Build(Pack);
		FRSViewModel& Stored = Cache.Add(Key, Built);
		return Stored.IsValid() ? &Stored : nullptr;
	}
}
