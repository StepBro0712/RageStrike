#include "RSAudio.h"
#include "Sound/SoundBase.h"
#include "UObject/SoftObjectPath.h"

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

	USoundBase* GetFireSound(ERSWeapon Weapon)
	{
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
