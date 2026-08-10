#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RSHUD.generated.h"

class ARSCharacter;
class ARSBot;

UCLASS()
class RAGESTRIKE_API ARSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCrosshair(const ARSCharacter* Player);
	void DrawSniperScope(const ARSCharacter* Player);
	void DrawESP(const ARSCharacter* Player);
	void DrawCheatPanel(const ARSCharacter* Player);
	void DrawHealthArmor(const ARSCharacter* Player);
	void DrawAmmo(const ARSCharacter* Player);
	void DrawMoney(const ARSCharacter* Player);
	void DrawRadar(const ARSCharacter* Player);
	void DrawKillFeed(const ARSCharacter* Player);
	void DrawBoxOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness);
	void DrawRoundInfo(const ARSCharacter* Player);
	void DrawBuyMenu(const ARSCharacter* Player);
	void DrawScoreboard(const ARSCharacter* Player);
};
