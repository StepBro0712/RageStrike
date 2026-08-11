#include "RSIcons.h"
#include "Engine/Texture2D.h"

namespace RSIcons
{
	namespace
	{
		// имена совпадают с ключами в convert_icons.ps1
		const TCHAR* WeaponIconName(ERSWeapon W)
		{
			switch (W)
			{
			case ERSWeapon::Knife:        return TEXT("Knife");
			case ERSWeapon::Glock:        return TEXT("Glock");
			case ERSWeapon::USP:          return TEXT("USP");
			case ERSWeapon::P250:         return TEXT("P250");
			case ERSWeapon::Deagle:       return TEXT("Deagle");
			case ERSWeapon::Tec9:         return TEXT("Tec9");
			case ERSWeapon::FiveSeven:    return TEXT("FiveSeven");
			case ERSWeapon::MP9:          return TEXT("MP9");
			case ERSWeapon::MAC10:        return TEXT("MAC10");
			case ERSWeapon::UMP45:        return TEXT("UMP45");
			case ERSWeapon::P90:          return TEXT("P90");
			case ERSWeapon::Nova:         return TEXT("Nova");
			case ERSWeapon::XM1014:       return TEXT("XM1014");
			case ERSWeapon::GalilAR:      return TEXT("GalilAR");
			case ERSWeapon::FAMAS:        return TEXT("FAMAS");
			case ERSWeapon::AK47:         return TEXT("AK47");
			case ERSWeapon::M4A4:         return TEXT("M4A4");
			case ERSWeapon::AUG:          return TEXT("AUG");
			case ERSWeapon::SG553:        return TEXT("SG553");
			case ERSWeapon::SSG08:        return TEXT("SSG08");
			case ERSWeapon::AWP:          return TEXT("AWP");
			case ERSWeapon::HEGrenade:    return TEXT("HEGrenade");
			case ERSWeapon::Flashbang:    return TEXT("Flashbang");
			case ERSWeapon::SmokeGrenade: return TEXT("Smoke");
			case ERSWeapon::Molotov:      return TEXT("Molotov");
			case ERSWeapon::Incendiary:   return TEXT("Incendiary");
			default:                      return nullptr;
			}
		}

		// Загрузка по полному имени ассета. Иконки оружия названы Icon_<...>,
		// а значки killfeed — KF<...>, поэтому префикс задаёт вызывающий.
		UTexture2D* LoadAsset(const TCHAR* AssetName)
		{
			if (!AssetName)
			{
				return nullptr;
			}
			// кеш на имя: одна и та же иконка запрашивается каждый кадр
			static TMap<FString, UTexture2D*> Cache;
			static TSet<FString> Tried;

			const FString Key(AssetName);
			if (UTexture2D** Found = Cache.Find(Key))
			{
				return *Found;
			}
			if (Tried.Contains(Key))
			{
				return nullptr;
			}
			Tried.Add(Key);

			const FString Path = FString::Printf(TEXT("/Game/UI/Icons/%s.%s"), AssetName, AssetName);
			if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path))
			{
				Tex->AddToRoot();
				Cache.Add(Key, Tex);
				return Tex;
			}
			return nullptr;
		}

		// иконки оружия и снаряжения импортированы с префиксом Icon_
		UTexture2D* LoadIcon(const TCHAR* Name)
		{
			return Name ? LoadAsset(*FString::Printf(TEXT("Icon_%s"), Name)) : nullptr;
		}
	}

	UTexture2D* ForWeapon(ERSWeapon Weapon) { return LoadIcon(WeaponIconName(Weapon)); }
	UTexture2D* Kevlar()                    { return LoadIcon(TEXT("Kevlar")); }
	UTexture2D* Helmet()                    { return LoadIcon(TEXT("Helmet")); }

	UTexture2D* KillHeadshot()              { return LoadAsset(TEXT("KFHeadshot")); }
	UTexture2D* KillNoscope()               { return LoadAsset(TEXT("KFNoscope")); }
	UTexture2D* KillBlind()                 { return LoadAsset(TEXT("KFBlind")); }
	UTexture2D* KillSmoke()                 { return LoadAsset(TEXT("KFSmoke")); }
}
