#pragma once

#include "CoreMinimal.h"

// Описание карты. Чтобы добавить новую: импортировать меши в /Game/Maps/<Имя>
// и дописать одну строку в RSMaps::All() — остальное подхватится само.
struct FRSMapDef
{
	const TCHAR* DisplayName;
	const TCHAR* ContentFolder; // nullptr — процедурная арена из кубов
	float Scale;
	float PlayRadius;           // в каком радиусе от центра появляются игроки и боты
	float GroundZ;              // на какой высоте окажется низ карты
};

namespace RSMaps
{
	const TArray<FRSMapDef>& All();

	int32 GetSelectedIndex();
	void SetSelectedIndex(int32 Index);

	const FRSMapDef& Selected();
}
