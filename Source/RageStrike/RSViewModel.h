#pragma once

#include "CoreMinimal.h"
#include "RSWeaponData.h"

class USkeletalMesh;
class UAnimSequence;

// Вьюмодель от первого лица со скелетом и настоящими анимациями CS2.
// Есть только у тех стволов, чьи модели приехали в glTF вместе с ригом:
// остальные показываются статик-мешем и процедурной анимацией.
struct FRSViewModel
{
	USkeletalMesh* Mesh = nullptr;
	UAnimSequence* Idle = nullptr;
	UAnimSequence* Draw = nullptr;
	UAnimSequence* Reload = nullptr;
	UAnimSequence* Shoot[3] = { nullptr, nullptr, nullptr };
	UAnimSequence* Inspect = nullptr;

	bool IsValid() const { return Mesh != nullptr; }
};

namespace RSViewModel
{
	// nullptr-структура, если у оружия нет скелетной вьюмодели
	const FRSViewModel* Get(ERSWeapon Weapon);
}
