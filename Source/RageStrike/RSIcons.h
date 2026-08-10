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
}
