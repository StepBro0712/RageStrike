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

	// Общая громкость. FApp::SetVolumeMultiplier в собранной игре молчит,
	// поэтому крутим громкость прямо у аудиоустройства.
	void SetMasterVolume(float Volume);
	float GetMasterVolume();

	// Дальность слышимости, как в CS: шаги слышно вблизи, выстрелы далеко,
	// взрывы почти по всей карте. Без затухания все звуки играли одинаково
	// громко в любой точке.
	enum class ERange : uint8
	{
		Step,      // ~15 м
		Gun,       // ~60 м
		Explosion, // ~110 м
		Ambient    // огонь, дым — небольшой радиус
	};

	class USoundAttenuation* GetAttenuation(ERange Range);

	// Проиграть звук в точке мира с нужным затуханием. bFromOther поднимает
	// громкость: чужие шаги и выстрелы должны быть отчётливо слышны, свои
	// и так звучат вплотную к слушателю.
	void PlayAt(const UObject* WorldContext, class USoundBase* Sound,
		const FVector& Location, float Volume, ERange Range, bool bFromOther = false);

	// Настоящие звуки CS лежат в /Game/Audio/CS под именем оружия; если для
	// ствола записи нет, откатываемся на синтезированный звук его класса.
	USoundBase* GetFireSound(ERSWeapon Weapon);
	USoundBase* GetReloadSound(ERSWeapon Weapon);
	// шаги: восемь вариантов по бетону, чтобы не долбило одним и тем же
	USoundBase* GetStepSound(bool bRunning);
}
