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
	// Руки, запечённые в позу привязки именно этого ствола: у каждой
	// вьюмодели Source своя поза привязки с уже сложенным хватом.
	USkeletalMesh* Arms = nullptr;

	bool IsValid() const { return Mesh != nullptr; }
};

// Посадка вьюмодели у камеры. Автоподбор по габаритам не годится: у скелета
// AK-47 размеры в исходнике меньше сантиметра, и вычисленный масштаб раздувал
// модель на пол-экрана. Поэтому цифры задаются вручную на каждый ствол.
struct FRSVMPlace
{
	float Scale = 1.f;
	FVector Loc = FVector::ZeroVector;   // относительно камеры
	FRotator Rot = FRotator::ZeroRotator;
};

namespace RSViewModel
{
	// nullptr-структура, если у оружия нет скелетной вьюмодели
	const FRSViewModel* Get(ERSWeapon Weapon);

	// Посадка: сначала из настроек игрока (их пишет режим подгонки),
	// иначе значение по умолчанию из кода.
	FRSVMPlace GetPlace(ERSWeapon Weapon);
	void SetPlace(ERSWeapon Weapon, const FRSVMPlace& P);
	// строка с текущими цифрами — её показывает HUD и печатает лог,
	// чтобы подобранные значения можно было вписать в код
	FString PlaceToString(ERSWeapon Weapon);
}
