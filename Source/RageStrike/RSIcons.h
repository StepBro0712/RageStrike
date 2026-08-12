#pragma once

#include "CoreMinimal.h"
#include "RSWeaponData.h"

class UTexture2D;

// Иконки оружия и снаряжения из CS2. Грузятся лениво и кешируются,
// как и звуки; если иконки нет, вернётся nullptr и HUD нарисует текст.
namespace RSIcons
{
	UTexture2D* ForWeapon(ERSWeapon Weapon);
	UTexture2D* Kevlar();
	UTexture2D* Helmet();

	// Значки обстоятельств убийства в killfeed. Исходники webp лежат в
	// ImportSource\killfied, в PNG их переводит Scripts\convert_killfeed_icons.ps1
	// (там же инверсия в белый — оригиналы чёрные).
	UTexture2D* KillHeadshot();
	UTexture2D* KillNoscope();
	UTexture2D* KillBlind();
	UTexture2D* KillSmoke();
	UTexture2D* KillPenetrate();
}
