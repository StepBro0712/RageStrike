#pragma once

#include "CoreMinimal.h"

// Настройки матча, которые хост задаёт перед созданием сервера.
// Живут в GameUserSettings.ini, читаются игровым режимом на старте уровня
// и разъезжаются клиентам через GameState.
namespace RSMatch
{
	int32 GetTeamSize();          // игроков на сторону, 1..5
	void SetTeamSize(int32 Value);

	bool GetUseBots();            // добирать состав ботами
	void SetUseBots(bool bValue);

	int32 GetRoundsToWin();       // сколько раундов нужно для победы, 3..16
	void SetRoundsToWin(int32 Value);

	int32 GetRoundSeconds();      // длительность раунда, 30..300 с
	void SetRoundSeconds(int32 Value);

	int32 GetBuySeconds();        // закупка перед раундом, 5..60 с
	void SetBuySeconds(int32 Value);

	// производные правила
	inline int32 RoundsTotalFor(int32 RoundsToWin) { return (RoundsToWin - 1) * 2; }
	inline int32 HalfTimeFor(int32 RoundsToWin)    { return RoundsToWin - 1; }
}

// Прочие опции интерфейса
namespace RSOptions
{
	// 0 — выключено, 1 — только FPS, 2 — FPS с загрузкой процессора и видеокарты
	int32 GetPerfMode();
	void SetPerfMode(int32 Value);
	const TCHAR* PerfModeName(int32 Value);

	float GetFov();               // обзор, 70..120
	void SetFov(float Value);

	float GetMusicVolume();       // музыка отдельно от эффектов
	void SetMusicVolume(float Value);
	float GetFxVolume();
	void SetFxVolume(float Value);

	float GetVmOffset();          // сдвиг оружия вправо-влево, -8..8
	void SetVmOffset(float Value);
	float GetVmOffsetZ();         // вверх-вниз
	void SetVmOffsetZ(float Value);
	float GetVmOffsetX();         // и вперёд-назад, ближе к камере или дальше
	void SetVmOffsetX(float Value);
	bool GetHideViewmodel();      // убрать оружие с экрана
	void SetHideViewmodel(bool bValue);

	float GetGamma();             // яркость, 1.6..3.2
	void SetGamma(float Value);
	void ApplyGamma();            // разово при старте и после правки
}
