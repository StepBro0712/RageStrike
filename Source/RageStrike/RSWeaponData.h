#pragma once

#include "CoreMinimal.h"
#include "RSTeam.h"

// Весь арсенал CS. Значения UENUM попадают в репликацию — порядок не менять.
UENUM()
enum class ERSWeapon : uint8
{
	Knife,
	// пистолеты
	Glock, USP, P250, Deagle, Tec9, FiveSeven,
	// пистолеты-пулемёты
	MP9, MAC10, UMP45, P90,
	// дробовики
	Nova, XM1014,
	// винтовки
	GalilAR, FAMAS, AK47, M4A4, AUG, SG553,
	// снайперки
	SSG08, AWP,
	// гранаты
	HEGrenade, Flashbang, SmokeGrenade, Molotov, Incendiary,
	COUNT
};

enum class ERSSlot : uint8 { Knife, Secondary, Primary, Grenade };

// какой ассет показываем: своих моделей на весь арсенал нет,
// используем четыре имеющихся + сферу для гранат
enum class ERSMeshKind : uint8 { Knife, Pistol, RifleAK, RifleM4, Sniper, Grenade };

enum class ERSTeamLock : uint8 { Any, CT, T };

struct FRSWeaponDef
{
	const TCHAR* Name;
	ERSSlot Slot;
	ERSMeshKind Mesh;
	ERSTeamLock Lock;
	int32 Price;
	float BodyDamage;    // за одну пулю/дробину
	float HeadMult;
	float Interval;      // сек между выстрелами
	bool bAuto;          // автоматический огонь
	int32 Pellets;       // дробовики стреляют пучком
	float Speed;         // макс. скорость бега, юниты UE
	float ReloadTime;
	int32 Mag;
	int32 ReserveMax;
	int32 KillReward;
	bool bScope;
	float ScopeFOV;
	float KickBase;      // отдача: старт, прирост, амплитуда «змейки»
	float KickRamp;
	float YawAmp;
	float BaseSpread;    // базовый разброс стоя, градусы
};

namespace RSWeapons
{
	constexpr int32 Count = (int32)ERSWeapon::COUNT;
	constexpr int32 GrenadeTypes = 5; // HE, флешка, дым, молотов, зажигалка

	const FRSWeaponDef& Get(ERSWeapon W);

	inline bool IsGrenade(ERSWeapon W)
	{
		return W >= ERSWeapon::HEGrenade && W <= ERSWeapon::Incendiary;
	}

	// 0..4 для гранат, иначе -1
	inline int32 GrenadeIndex(ERSWeapon W)
	{
		return IsGrenade(W) ? (int32)W - (int32)ERSWeapon::HEGrenade : -1;
	}

	inline ERSWeapon GrenadeByIndex(int32 I)
	{
		return (ERSWeapon)((int32)ERSWeapon::HEGrenade + I);
	}

	inline bool AllowedFor(ERSWeapon W, ERSTeam Team)
	{
		const ERSTeamLock L = Get(W).Lock;
		return L == ERSTeamLock::Any
			|| (L == ERSTeamLock::CT && Team == ERSTeam::CT)
			|| (L == ERSTeamLock::T && Team == ERSTeam::T);
	}

	// Модель оружия из /Game/Weapons/CS2 (кеш внутри). Общая и для игрока,
	// и для ботов: у ботов оружие тоже должно быть видно в руках.
	class UStaticMesh* LoadWeaponMesh(ERSWeapon W);

	// реальная длина ствола в сантиметрах — по ней модели приводятся к размеру
	float RealLength(ERSWeapon W);

	// категории меню закупки: список оружия с учётом команды.
	// Индекс = цифра на клавиатуре минус один; нулевая (снаряжение) пустая,
	// броня рисуется и покупается отдельно.
	TArray<ERSWeapon> BuyCategory(int32 Category, ERSTeam Team);
	const TCHAR* BuyCategoryName(int32 Category);
	constexpr int32 BuyCategories = 6;
	constexpr int32 EquipmentCategory = 0;
}
