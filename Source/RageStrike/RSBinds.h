#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

// Перебинд управления и настройки прицела.
//
// Управление меняется через штатные маппинги ввода движка: у осей (движение)
// у каждой клавиши свой знак, у действий знака нет. Правим их в рантайме и
// сохраняем — тогда клавиши переживают перезапуск и работают везде, где код
// уже слушает эти действия.
namespace RSBinds
{
	struct FEntry
	{
		const TCHAR* Display;  // как называется в меню
		const TCHAR* Mapping;  // имя оси или действия в настройках ввода
		bool bAxis;
		float Scale;           // для оси: +1 или -1
		FKey Default;
	};

	const TArray<FEntry>& All();

	FKey GetKey(const FEntry& Entry);
	void SetKey(const FEntry& Entry, const FKey& NewKey);
	void ResetAll();
}

// Настройки прицела: хранятся в GameUserSettings, читаются HUD-ом каждый кадр
namespace RSCrosshair
{
	float GetLength();
	void SetLength(float Value);
	float GetThickness();
	void SetThickness(float Value);
	float GetGap();
	void SetGap(float Value);
	// цвет задаётся каналами, как в CS2, а не выбором из списка
	int32 GetChannel(int32 Which);        // 0 R, 1 G, 2 B, 3 A
	void SetChannel(int32 Which, int32 Value);
	FLinearColor GetColor();
	bool GetOutline();
	void SetOutline(bool bValue);
	bool GetDot();
	void SetDot(bool bValue);
	bool GetDynamic();
	void SetDynamic(bool bValue);

	const TCHAR* ChannelName(int32 Which);
}
