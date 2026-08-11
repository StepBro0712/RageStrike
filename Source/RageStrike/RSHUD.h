#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RSWeaponData.h"
#include "RSHUD.generated.h"

class ARSCharacter;
class ARSBot;

// Кликабельная область меню закупки. Меню рисуется на канвасе, поэтому
// зоны для мыши собираем прямо во время отрисовки.
struct FRSBuyHotspot
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	int32 Category = -1;
	ERSWeapon Weapon = ERSWeapon::Knife;
	// -2 заголовок категории, -1 оружие, 0 кевлар, 1 кевлар со шлемом
	int8 Kind = -1;

	bool Contains(const FVector2D& P) const
	{
		return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y;
	}
};

UCLASS()
class RAGESTRIKE_API ARSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// клик мышью по меню закупки; true — попали в карточку
	bool HandleBuyClick(const FVector2D& Mouse, ARSCharacter* Player);
	// клик по строке оверлея читов; true — переключили чит
	bool HandleCheatClick(const FVector2D& Mouse, ARSCharacter* Player);

private:
	TArray<FRSBuyHotspot> BuyHotspots;
	// строки оверлея читов: индекс чита и его область на экране
	TArray<TPair<int32, FBox2D>> CheatHotspots;
	// стрелки в настройках читов: ключ = номер настройки * 2, плюс единица
	// для «больше». У переключателей зона одна, на всю строку.
	TArray<TPair<int32, FBox2D>> CheatSettingHotspots;
	bool GetMouseOnCanvas(FVector2D& Out) const;

	void DrawCrosshair(const ARSCharacter* Player);
	void DrawSniperScope(const ARSCharacter* Player);
	void DrawESP(const ARSCharacter* Player);
	// лента событий чита: урон, промахи и причины отказа стрелять
	void DrawCheatLog(const ARSCharacter* Player);
	// полоски под прицелом: заряд двойного выстрела, шанс попадания, урон
	void DrawCheatStatus(const ARSCharacter* Player);
	void DrawCheatPanel(const ARSCharacter* Player);
	void DrawHealthArmor(const ARSCharacter* Player);
	void DrawAmmo(const ARSCharacter* Player);
	void DrawMoney(const ARSCharacter* Player);
	void DrawRadar(const ARSCharacter* Player);
	void DrawKillFeed(const ARSCharacter* Player);
	// Весь текст HUD идёт через эту обёртку: она рисует крупным шрифтом,
	// а масштаб делит на отношение размеров, чтобы вёрстка осталась прежней.
	void DrawTextScaled(const FString& Text, FLinearColor Color, float X, float Y,
		class UFont* Font, float Scale);
	// замер текста тем же масштабом, иначе панели считают ширину неверно
	void GetTextSizeScaled(const FString& Text, float& OutW, float& OutH,
		class UFont* Font, float Scale);

	void DrawPerfStats();
	// показания пересчитываются раз в полсекунды: чаще их не прочитать
	float PerfNextUpdate = 0.f;
	int32 PerfFrames = 0;
	float PerfAccumTime = 0.f;
	float ShownFPS = 0.f;
	float ShownFrameMs = 0.f;
	float ShownCPUPct = 0.f;
	float ShownGPUPct = 0.f;

	void DrawBoxOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness);
	void DrawRoundInfo(const ARSCharacter* Player);
	void DrawBuyMenu(const ARSCharacter* Player);
	void DrawScoreboard(const ARSCharacter* Player);
};
