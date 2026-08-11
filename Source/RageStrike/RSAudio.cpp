#include "RSAudio.h"
#include "RSMatchSettings.h"
#include "Sound/SoundBase.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/Engine.h"
#include "AudioDevice.h"
#include "AudioDeviceHandle.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"

namespace RSAudio
{
	namespace
	{
		const TCHAR* Paths[(int32)ESound::COUNT] =
		{
			TEXT("/Game/Audio/Fire_Rifle.Fire_Rifle"),
			TEXT("/Game/Audio/Fire_SMG.Fire_SMG"),
			TEXT("/Game/Audio/Fire_Pistol.Fire_Pistol"),
			TEXT("/Game/Audio/Fire_Sniper.Fire_Sniper"),
			TEXT("/Game/Audio/Fire_Shotgun.Fire_Shotgun"),
			TEXT("/Game/Audio/Knife_Slash.Knife_Slash"),
			TEXT("/Game/Audio/Knife_Hit.Knife_Hit"),
			TEXT("/Game/Audio/Reload.Reload"),
			TEXT("/Game/Audio/Step_Walk.Step_Walk"),
			TEXT("/Game/Audio/Step_Run.Step_Run"),
			TEXT("/Game/Audio/Land.Land"),
			TEXT("/Game/Audio/Nade_Throw.Nade_Throw"),
			TEXT("/Game/Audio/Nade_Bounce.Nade_Bounce"),
			TEXT("/Game/Audio/Explode.Explode"),
			TEXT("/Game/Audio/Flash.Flash"),
			TEXT("/Game/Audio/Smoke.Smoke"),
			TEXT("/Game/Audio/Burn.Burn"),
			TEXT("/Game/Audio/Buy.Buy"),
			TEXT("/Game/Audio/HitMarker.HitMarker"),
			TEXT("/Game/Audio/Music_Menu.Music_Menu"),
			TEXT("/Game/Audio/Music_Buy.Music_Buy")
		};

		// кеш живёт до конца процесса; ассеты держим корневыми, чтобы их
		// не собрал сборщик мусора между раундами
		USoundBase* Cache[(int32)ESound::COUNT] = { nullptr };
		bool bTried[(int32)ESound::COUNT] = { false };
	}

	USoundBase* Get(ESound Which)
	{
		const int32 Index = (int32)Which;
		if (Index < 0 || Index >= (int32)ESound::COUNT)
		{
			return nullptr;
		}
		if (!bTried[Index])
		{
			bTried[Index] = true;
			if (USoundBase* Loaded = LoadObject<USoundBase>(nullptr, Paths[Index]))
			{
				Loaded->AddToRoot();
				Cache[Index] = Loaded;
			}
		}
		return Cache[Index];
	}

	namespace
	{
		float MasterVolume = 1.f;

		USoundAttenuation* AttenCache[4] = { nullptr };
	}

	USoundAttenuation* GetAttenuation(ERange Range)
	{
		const int32 Index = FMath::Clamp((int32)Range, 0, 3);
		if (AttenCache[Index])
		{
			return AttenCache[Index];
		}

		// радиус полной громкости и дистанция затухания в сантиметрах
		float Radius = 200.f;
		float Falloff = 1500.f;
		switch (Range)
		{
		case ERange::Step:      Radius = 150.f;  Falloff = 1500.f;  break;
		case ERange::Gun:       Radius = 700.f;  Falloff = 6000.f;  break;
		case ERange::Explosion: Radius = 1200.f; Falloff = 11000.f; break;
		case ERange::Ambient:   Radius = 250.f;  Falloff = 1800.f;  break;
		}

		USoundAttenuation* Att = NewObject<USoundAttenuation>();
		Att->AddToRoot();

		FSoundAttenuationSettings& S = Att->Attenuation;
		S.bAttenuate = true;
		S.bSpatialize = true;
		S.AttenuationShape = EAttenuationShape::Sphere;
		// у сферы радиус лежит в X
		S.AttenuationShapeExtents = FVector(Radius, 0.f, 0.f);
		S.FalloffDistance = Falloff;
		S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		S.dBAttenuationAtMax = -60.f;

		// стены глушат звук: без этого враг за домом слышен как в упор
		S.bEnableOcclusion = true;
		S.OcclusionLowPassFilterFrequency = 500.f;
		S.OcclusionVolumeAttenuation = 0.4f;
		S.OcclusionInterpolationTime = 0.1f;
		S.bUseComplexCollisionForOcclusion = false;

		AttenCache[Index] = Att;
		return Att;
	}

	void PlayAt(const UObject* WorldContext, USoundBase* Sound,
		const FVector& Location, float Volume, ERange Range, bool bFromOther)
	{
		if (!Sound)
		{
			return;
		}
		// Громкость эффектов отдельная от музыки: одним ползунком нельзя было
		// приглушить выстрелы, не убив заодно и музыку в меню.
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location,
			FRotator::ZeroRotator,
			Volume * (bFromOther ? 3.f : 1.f) * RSOptions::GetFxVolume(), 1.f, 0.f,
			GetAttenuation(Range));
	}

	void SetMasterVolume(float Volume)
	{
		MasterVolume = FMath::Clamp(Volume, 0.f, 1.f);
		if (GEngine)
		{
			if (FAudioDeviceHandle Device = GEngine->GetMainAudioDevice())
			{
				Device->SetTransientPrimaryVolume(MasterVolume);
			}
		}
	}

	float GetMasterVolume()
	{
		return MasterVolume;
	}

	namespace
	{
		// ключ совпадает с именами ассетов, которые сложил import_cs_audio.py
		const TCHAR* AssetKey(ERSWeapon W)
		{
			switch (W)
			{
			case ERSWeapon::Knife:     return TEXT("Knife");
			case ERSWeapon::Glock:     return TEXT("Glock");
			case ERSWeapon::USP:       return TEXT("USP");
			case ERSWeapon::P250:      return TEXT("P250");
			case ERSWeapon::Deagle:    return TEXT("Deagle");
			case ERSWeapon::Tec9:      return TEXT("Tec9");
			case ERSWeapon::FiveSeven: return TEXT("FiveSeven");
			case ERSWeapon::MP9:       return TEXT("MP9");
			case ERSWeapon::MAC10:     return TEXT("MAC10");
			case ERSWeapon::UMP45:     return TEXT("UMP45");
			case ERSWeapon::P90:       return TEXT("P90");
			case ERSWeapon::Nova:      return TEXT("Nova");
			case ERSWeapon::XM1014:    return TEXT("XM1014");
			case ERSWeapon::GalilAR:   return TEXT("GalilAR");
			case ERSWeapon::FAMAS:     return TEXT("FAMAS");
			case ERSWeapon::AK47:      return TEXT("AK47");
			case ERSWeapon::M4A4:      return TEXT("M4A4");
			case ERSWeapon::AUG:       return TEXT("AUG");
			case ERSWeapon::SG553:     return TEXT("SG553");
			case ERSWeapon::SSG08:     return TEXT("SSG08");
			case ERSWeapon::AWP:       return TEXT("AWP");
			default:                   return nullptr;
			}
		}

		USoundBase* LoadCS(const FString& AssetName)
		{
			static TMap<FString, USoundBase*> CSCache;
			if (USoundBase** Found = CSCache.Find(AssetName))
			{
				return *Found;
			}
			const FString Path = FString::Printf(TEXT("/Game/Audio/CS/%s.%s"),
				*AssetName, *AssetName);
			USoundBase* Loaded = LoadObject<USoundBase>(nullptr, *Path);
			if (Loaded)
			{
				Loaded->AddToRoot();
			}
			CSCache.Add(AssetName, Loaded);
			return Loaded;
		}
	}

	USoundBase* GetReloadSound(ERSWeapon Weapon)
	{
		if (const TCHAR* Key = AssetKey(Weapon))
		{
			if (USoundBase* Real = LoadCS(FString::Printf(TEXT("Reload_%s"), Key)))
			{
				return Real;
			}
		}
		return Get(ESound::Reload);
	}

	USoundBase* GetStepSound(bool bRunning)
	{
		// вариант выбираем случайно: восемь записей бетона
		const int32 Index = FMath::RandRange(1, 8);
		if (USoundBase* Real = LoadCS(FString::Printf(TEXT("Step_%d"), Index)))
		{
			return Real;
		}
		return Get(bRunning ? ESound::StepRun : ESound::StepWalk);
	}

	USoundBase* GetFireSound(ERSWeapon Weapon)
	{
		if (const TCHAR* Key = AssetKey(Weapon))
		{
			if (USoundBase* Real = LoadCS(FString::Printf(TEXT("Fire_%s"), Key)))
			{
				return Real;
			}
		}

		const FRSWeaponDef& Def = RSWeapons::Get(Weapon);
		if (Def.Slot == ERSSlot::Knife)
		{
			return Get(ESound::KnifeSlash);
		}
		if (Def.Pellets > 1)
		{
			return Get(ESound::FireShotgun);
		}
		if (Def.Mesh == ERSMeshKind::Sniper)
		{
			return Get(ESound::FireSniper);
		}
		if (Def.Slot == ERSSlot::Secondary)
		{
			return Get(ESound::FirePistol);
		}
		// пистолеты-пулемёты стреляют быстрее винтовок — по темпу и различаем
		return (Def.Interval < 0.085f) ? Get(ESound::FireSMG) : Get(ESound::FireRifle);
	}
}
