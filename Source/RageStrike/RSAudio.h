#pragma once

#include "CoreMinimal.h"
#include "RSWeaponData.h"

class USoundBase;

// Звуки игры. Собраны в одном месте, чтобы не тащить ConstructorHelpers
// в каждый актор: ассеты грузятся лениво при первом обращении и кешируются.
namespace RSAudio
{
	enum class ESound : uint8
	{
		FireRifle, FireSMG, FirePistol, FireSniper, FireShotgun,
		KnifeSlash, KnifeHit,
		Reload, StepWalk, StepRun, Land,
		NadeThrow, NadeBounce, Explode, Flash, Smoke, Burn,
		Buy, HitMarker, MusicMenu, MusicBuy,
		COUNT
	};

	USoundBase* Get(ESound Which);

	// звук выстрела подбирается по классу оружия
	USoundBase* GetFireSound(ERSWeapon Weapon);
}
