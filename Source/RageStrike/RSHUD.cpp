#include "RSHUD.h"
#include "RSCharacter.h"
#include "RSBot.h"
#include "RSGameMode.h"
#include "RSGameState.h"
#include "RSPlayerController.h"
#include "RSMatchSettings.h"
#include "RSIcons.h"
#include "Engine/Texture2D.h"
#include "RenderCore.h"
#include "RHI.h"
#include "Misc/App.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "RSBinds.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

namespace
{
	// Яркая палитра Counter-Strike 2
	const FLinearColor ColBgDark(0.02f, 0.03f, 0.05f, 0.88f);
	const FLinearColor ColBgSolid(0.06f, 0.08f, 0.11f, 0.96f);
	const FLinearColor ColBorder(1.0f, 1.0f, 1.0f, 0.22f);
	const FLinearColor ColCT(0.22f, 0.58f, 1.00f, 1.0f);
	const FLinearColor ColT(0.98f, 0.68f, 0.15f, 1.0f);
	const FLinearColor ColWhite(1.00f, 1.00f, 1.00f, 1.0f);
	const FLinearColor ColDim(0.95f, 0.96f, 0.98f, 0.60f);
	const FLinearColor ColCSGreen(0.20f, 0.95f, 0.38f, 1.0f);
	const FLinearColor ColRed(0.98f, 0.20f, 0.20f, 1.0f);
	const FLinearColor ColGold(1.00f, 0.84f, 0.18f, 1.0f);

	FLinearColor TeamColor(uint8 Team)
	{
		return Team == (uint8)ERSTeam::CT ? ColCT : ColT;
	}

	// Шрифт HUD. Раньше весь текст рисовался мелким встроенным шрифтом и
	// растягивался в полтора-два раза — отсюда пиксельность. Берём крупный,
	// а прежний размер компенсируем масштабом в DrawTextScaled.
	UFont* RSFontMain() { return GEngine->GetSmallFont(); }
	UFont* RSFontBig()  { return GEngine->GetMediumFont(); }
}

void ARSHUD::DrawTextScaled(const FString& Text, FLinearColor Color, float X, float Y,
	UFont* Font, float Scale)
{
	// Шрифт Slate через канвас не рисуется — текст пропадал целиком.
	// Возвращаемся к штатному шрифту движка: он растровый и мылится, но виден.
	DrawText(Text, Color, X, Y, Font, Scale);
}

void ARSHUD::GetTextSizeScaled(const FString& Text, float& OutW, float& OutH,
	UFont* Font, float Scale)
{
	GetTextSize(Text, OutW, OutH, Font, Scale);
}

void ARSHUD::DrawBoxOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	DrawLine(X, Y, X + W, Y, Color, Thickness);
	DrawLine(X, Y + H, X + W, Y + H, Color, Thickness);
	DrawLine(X, Y, X, Y + H, Color, Thickness);
	DrawLine(X + W, Y, X + W, Y + H, Color, Thickness);
}

// Иконка оружия из CS2. Если её нет, откатываемся на рисованный силуэт.
static void DrawWeaponIcon(UCanvas* Canvas, float X, float Y, float W, float H,
	ERSWeapon Weapon, const FLinearColor& Color)
{
	if (!Canvas)
	{
		return;
	}
	if (UTexture2D* Icon = RSIcons::ForWeapon(Weapon))
	{
		Canvas->K2_DrawTexture(Icon, FVector2D(X, Y), FVector2D(W, H),
			FVector2D::ZeroVector, FVector2D::UnitVector, Color, EBlendMode::BLEND_Translucent);
	}
}

static void DrawEquipIcon(UCanvas* Canvas, float X, float Y, float W, float H,
	UTexture2D* Icon, const FLinearColor& Color)
{
	if (Canvas && Icon)
	{
		Canvas->K2_DrawTexture(Icon, FVector2D(X, Y), FVector2D(W, H),
			FVector2D::ZeroVector, FVector2D::UnitVector, Color, EBlendMode::BLEND_Translucent);
	}
}

static void DrawWeaponSilhouette(UCanvas* Canvas, float X, float Y, float W, float H, ERSMeshKind MeshKind, const FLinearColor& Color)
{
	if (!Canvas) return;

	// Векторные иконки оружия CS2 повышенной чёткости
	switch (MeshKind)
	{
	case ERSMeshKind::Pistol:
		// Затвор и рукоятка пистолета
		Canvas->K2_DrawLine(FVector2D(X + 2.f, Y + H * 0.35f), FVector2D(X + W - 2.f, Y + H * 0.35f), 4.f, Color);
		Canvas->K2_DrawLine(FVector2D(X + 6.f, Y + H * 0.35f), FVector2D(X + 14.f, Y + H * 0.85f), 4.f, Color);
		break;
	case ERSMeshKind::RifleAK:
	case ERSMeshKind::RifleM4:
		// Приклад, корпус, ствол и магазин винтовки
		Canvas->K2_DrawLine(FVector2D(X, Y + H * 0.4f), FVector2D(X + W, Y + H * 0.4f), 4.f, Color);
		Canvas->K2_DrawLine(FVector2D(X + 4.f, Y + H * 0.4f), FVector2D(X + 10.f, Y + H * 0.85f), 3.5f, Color); // приклад
		Canvas->K2_DrawLine(FVector2D(X + W * 0.45f, Y + H * 0.4f), FVector2D(X + W * 0.36f, Y + H * 0.90f), 4.f, Color); // магазин
		break;
	case ERSMeshKind::Sniper:
		// Снайперка: ствол, оптический прицел, корпус
		Canvas->K2_DrawLine(FVector2D(X, Y + H * 0.5f), FVector2D(X + W, Y + H * 0.5f), 4.f, Color);
		Canvas->K2_DrawLine(FVector2D(X + W * 0.3f, Y + H * 0.20f), FVector2D(X + W * 0.65f, Y + H * 0.20f), 5.f, Color); // оптика
		Canvas->K2_DrawLine(FVector2D(X + W * 0.45f, Y + H * 0.5f), FVector2D(X + W * 0.40f, Y + H * 0.90f), 3.5f, Color); // магазин
		break;
	case ERSMeshKind::Knife:
		// Нож: лезвие и рукоять
		Canvas->K2_DrawLine(FVector2D(X + 4.f, Y + H * 0.5f), FVector2D(X + W * 0.45f, Y + H * 0.5f), 4.f, Color);
		Canvas->K2_DrawLine(FVector2D(X + W * 0.45f, Y + H * 0.3f), FVector2D(X + W - 4.f, Y + H * 0.6f), 5.f, Color);
		break;
	default:
		// Граната
		Canvas->K2_DrawLine(FVector2D(X + W * 0.4f, Y + 4.f), FVector2D(X + W * 0.6f, Y + 4.f), 4.f, Color);
		Canvas->K2_DrawLine(FVector2D(X + W * 0.3f, Y + H * 0.4f), FVector2D(X + W * 0.7f, Y + H * 0.4f), 6.f, Color);
		break;
	}
}

void ARSHUD::DrawHUD()
{
	Super::DrawHUD();

	ARSCharacter* Player = Cast<ARSCharacter>(GetOwningPawn());
	if (!Player || !Canvas)
	{
		return;
	}

	// пока открыто меню, HUD не рисуем: радар и патроны лезли поверх панелей
	if (const ARSPlayerController* RSPC = Cast<ARSPlayerController>(GetOwningPlayerController()))
	{
		if (RSPC->IsMenuOpen())
		{
			return;
		}
	}

	const bool bScoped = Player->bAlive && Player->bAimingNow
		&& RSWeapons::Get(Player->CurrentWeapon).Mesh == ERSMeshKind::Sniper
		&& !Player->bThirdPerson;

	if (bScoped)
	{
		DrawSniperScope(Player);
	}

	if (Player->bESP)
	{
		DrawESP(Player);
	}
	if (Player->bAlive && !bScoped)
	{
		DrawCrosshair(Player);
	}

	DrawRadar(Player);
	DrawKillFeed(Player);
	DrawHealthArmor(Player);
	DrawAmmo(Player);
	DrawMoney(Player);
	DrawRoundInfo(Player);
	DrawBuyMenu(Player);
	DrawScoreboard(Player);
	// оверлей читов рисуем последним: он должен лежать поверх закупки
	// меню читов теперь на Slate (SRSCheatMenu) — канвасу рисовать нечего
	DrawCheatLog(Player);
	DrawCheatStatus(Player);
	DrawPerfStats();

	const float Now = GetWorld()->GetTimeSeconds();

	// Заливка цветом своей стороны на время закупки: к началу раунда гаснет,
	// синяя у спецназа, оранжевая у террористов.
	if (const ARSGameState* GS = GetWorld()->GetGameState<ARSGameState>())
	{
		if (GS->Phase == ERSPhase::Intermission)
		{
			const float Left = GS->GetTimeLeft();
			// длительность закупки задаётся в меню, поэтому берём её из настроек:
			// на клиенте игрового режима нет, а константы больше не существует
			const float Fade = FMath::Clamp(Left / FMath::Max(1.f, (float)RSMatch::GetBuySeconds()), 0.f, 1.f);
			const bool bCT = (Player->Team == ERSTeam::CT);

			// у спецназа заливка ярче и синее, у террористов — тёмная рыжая
			const FLinearColor Side = bCT ? FLinearColor(0.18f, 0.45f, 1.f)
			                              : FLinearColor(0.32f, 0.16f, 0.02f);
			const float Alpha = Fade * (bCT ? 0.38f : 0.55f);
			DrawRect(FLinearColor(Side.R, Side.G, Side.B, Alpha),
				0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
		}
	}

	// подтверждение разметки спавна
	if (Now < Player->SpawnMarkUntil)
	{
		DrawTextScaled(Player->SpawnMarkMessage, ColGold,
			Canvas->SizeX * 0.5f - 200.f, Canvas->SizeY * 0.72f, RSFontMain(), 1.4f);
	}

	// Хитмаркер попадания
	if (Now - Player->LastHitMarkerTime < 0.15f)
	{
		const float CX = Canvas->SizeX * 0.5f;
		const float CY = Canvas->SizeY * 0.5f;
		const FLinearColor C = FLinearColor::White;
		DrawLine(CX - 14.f, CY - 14.f, CX - 6.f, CY - 6.f, C, 3.f);
		DrawLine(CX + 14.f, CY - 14.f, CX + 6.f, CY - 6.f, C, 3.f);
		DrawLine(CX - 14.f, CY + 14.f, CX - 6.f, CY + 6.f, C, 3.f);
		DrawLine(CX + 14.f, CY + 14.f, CX + 6.f, CY + 6.f, C, 3.f);
	}

	// Красная вспышка урона
	if (Now - Player->LastDamagedTime < 0.3f)
	{
		DrawRect(FLinearColor(1.f, 0.f, 0.f, 0.18f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
	}

	// Ослепление флешкой
	if (Now < Player->FlashEndTime)
	{
		const float Left = (Player->FlashEndTime - Now) / FMath::Max(0.3f, Player->FlashDuration);
		const float Alpha = FMath::Clamp(Left * Left * 1.6f, 0.f, 1.f);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, Alpha), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
	}

	// Строку с подсказками клавиш убрали: она занимала весь низ экрана,
	// а клавиши теперь и так видно в настройках, где их можно переназначить.
}

void ARSHUD::DrawCrosshair(const ARSCharacter* Player)
{
	if (RSWeapons::Get(Player->CurrentWeapon).Mesh == ERSMeshKind::Sniper)
	{
		return;
	}

	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	// всё берётся из настроек игрока; динамический зазор растёт от разброса
	const float Len = RSCrosshair::GetLength();
	const float Thick = RSCrosshair::GetThickness();
	const float BaseGap = RSCrosshair::GetGap();
	const float Gap = RSCrosshair::GetDynamic()
		? FMath::Clamp(BaseGap + Player->CurrentSpreadDeg * 8.f, BaseGap, BaseGap + 22.f)
		: BaseGap;

	const FLinearColor Color = RSCrosshair::GetColor();
	const FLinearColor Outline(0.f, 0.f, 0.f, 0.85f);

	if (RSCrosshair::GetOutline())
	{
		// тёмный контур: прицел читается на любом фоне
		DrawLine(CX - Gap - Len - 1.f, CY, CX - Gap + 1.f, CY, Outline, Thick + 2.f);
		DrawLine(CX + Gap - 1.f, CY, CX + Gap + Len + 1.f, CY, Outline, Thick + 2.f);
		DrawLine(CX, CY - Gap - Len - 1.f, CX, CY - Gap + 1.f, Outline, Thick + 2.f);
		DrawLine(CX, CY + Gap - 1.f, CX, CY + Gap + Len + 1.f, Outline, Thick + 2.f);
	}

	if (Len > 0.f)
	{
		DrawLine(CX - Gap - Len, CY, CX - Gap, CY, Color, Thick);
		DrawLine(CX + Gap, CY, CX + Gap + Len, CY, Color, Thick);
		DrawLine(CX, CY - Gap - Len, CX, CY - Gap, Color, Thick);
		DrawLine(CX, CY + Gap, CX, CY + Gap + Len, Color, Thick);
	}

	if (RSCrosshair::GetDot())
	{
		if (RSCrosshair::GetOutline())
		{
			DrawRect(Outline, CX - Thick * 0.5f - 1.f, CY - Thick * 0.5f - 1.f, Thick + 2.f, Thick + 2.f);
		}
		DrawRect(Color, CX - Thick * 0.5f, CY - Thick * 0.5f, Thick, Thick);
	}
}

void ARSHUD::DrawSniperScope(const ARSCharacter* Player)
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;
	const float R = FMath::Min(W, H) * 0.48f;

	// Заливаем всё, кроме круга линзы. Канвас не умеет круглую маску, поэтому
	// идём построчно: на каждой полосе считаем полуширину круга и закрываем
	// чёрным всё слева и справа от неё. Раньше закрашивались только четыре
	// прямоугольника, и по углам оставалась видна картинка за окружностью.
	const float Step = 2.f;
	for (float Y = 0.f; Y < H; Y += Step)
	{
		const float Dy = (Y + Step * 0.5f) - CY;
		const float Half = (FMath::Abs(Dy) >= R) ? 0.f : FMath::Sqrt(R * R - Dy * Dy);

		if (Half <= 0.f)
		{
			DrawRect(FLinearColor::Black, 0.f, Y, W, Step);
			continue;
		}
		DrawRect(FLinearColor::Black, 0.f, Y, CX - Half, Step);
		DrawRect(FLinearColor::Black, CX + Half, Y, W - CX - Half, Step);
	}

	// тонкая окантовка линзы, чтобы край не выглядел рваным от шага заливки
	const int32 Seg = 96;
	for (int32 i = 0; i < Seg; i++)
	{
		const float A0 = 2.f * PI * i / Seg;
		const float A1 = 2.f * PI * (i + 1) / Seg;
		DrawLine(CX + R * FMath::Cos(A0), CY + R * FMath::Sin(A0),
			CX + R * FMath::Cos(A1), CY + R * FMath::Sin(A1), FLinearColor::Black, 3.f);
	}

	DrawLine(CX - R, CY, CX + R, CY, FLinearColor::Black, 1.5f);
	DrawLine(CX, CY - R, CX, CY + R, FLinearColor::Black, 1.5f);
}

void ARSHUD::DrawESP(const ARSCharacter* Player)
{
	APawn* OwnPawn = GetOwningPawn();

	TArray<AActor*> Enemies;
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->Health > 0.f && It->Team != Player->Team)
		{
			Enemies.Add(*It);
		}
	}
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && *It != Player && It->bAlive && It->Team != Player->Team)
		{
			Enemies.Add(*It);
		}
	}

	for (AActor* Enemy : Enemies)
	{
		FVector Origin, Extent;
		Enemy->GetActorBounds(true, Origin, Extent);

		float MinX = FLT_MAX, MinY = FLT_MAX, MaxX = -FLT_MAX, MaxY = -FLT_MAX;
		bool bOnScreen = false;
		for (int32 i = 0; i < 8; i++)
		{
			const FVector Corner(
				Origin.X + ((i & 1) ? Extent.X : -Extent.X),
				Origin.Y + ((i & 2) ? Extent.Y : -Extent.Y),
				Origin.Z + ((i & 4) ? Extent.Z : -Extent.Z));
			const FVector Screen = Project(Corner);
			if (Screen.Z > 0.f)
			{
				bOnScreen = true;
				MinX = FMath::Min(MinX, (float)Screen.X);
				MinY = FMath::Min(MinY, (float)Screen.Y);
				MaxX = FMath::Max(MaxX, (float)Screen.X);
				MaxY = FMath::Max(MaxY, (float)Screen.Y);
			}
		}

		if (!bOnScreen)
		{
			continue;
		}

		const float W = MaxX - MinX;
		const float H = MaxY - MinY;
		const FLinearColor EspCol = ARSCharacter::EspPalette(Player->EspColor);

		// Заливка силуэта: рисуется поверх картинки, поэтому светится
		// сквозь стены. Настоящие chams потребовали бы пост-процесс материал.
		if (Player->bEspFill)
		{
			FLinearColor Fill = EspCol;
			Fill.A = 0.22f;
			DrawRect(Fill, MinX, MinY, W, H);
		}

		if (Player->bEspBox)
		{
			DrawBoxOutline(MinX, MinY, W, H, EspCol, 1.5f);
		}

		// Скелет по костям: теперь, когда у моделей есть физический ассет,
		// кости можно спросить напрямую и соединить линиями.
		if (Player->bEspSkeleton)
		{
			if (const ACharacter* AsChar = Cast<ACharacter>(Enemy))
			{
				if (const USkeletalMeshComponent* Mesh = AsChar->GetMesh())
				{
					static const TCHAR* Chain[][2] =
					{
						{ TEXT("head"), TEXT("neck_01") },
						{ TEXT("neck_01"), TEXT("spine_03") },
						{ TEXT("spine_03"), TEXT("pelvis") },
						{ TEXT("spine_03"), TEXT("upperarm_l") },
						{ TEXT("upperarm_l"), TEXT("lowerarm_l") },
						{ TEXT("lowerarm_l"), TEXT("hand_l") },
						{ TEXT("spine_03"), TEXT("upperarm_r") },
						{ TEXT("upperarm_r"), TEXT("lowerarm_r") },
						{ TEXT("lowerarm_r"), TEXT("hand_r") },
						{ TEXT("pelvis"), TEXT("thigh_l") },
						{ TEXT("thigh_l"), TEXT("calf_l") },
						{ TEXT("calf_l"), TEXT("foot_l") },
						{ TEXT("pelvis"), TEXT("thigh_r") },
						{ TEXT("thigh_r"), TEXT("calf_r") },
						{ TEXT("calf_r"), TEXT("foot_r") },
					};
					for (const TCHAR** Bone : Chain)
					{
						const FVector A = Project(Mesh->GetSocketLocation(FName(Bone[0])));
						const FVector B = Project(Mesh->GetSocketLocation(FName(Bone[1])));
						if (A.Z > 0.f && B.Z > 0.f)
						{
							DrawLine(A.X, A.Y, B.X, B.Y, EspCol, 1.2f);
						}
					}
				}
			}
		}

		float EnemyHealth = 100.f;
		if (const ARSBot* AsBot = Cast<ARSBot>(Enemy))
		{
			EnemyHealth = AsBot->Health;
		}
		else if (const ARSCharacter* AsPlayer = Cast<ARSCharacter>(Enemy))
		{
			EnemyHealth = AsPlayer->Health;
		}
		const float HPFrac = FMath::Clamp(EnemyHealth / 100.f, 0.f, 1.f);
		if (Player->bEspHealth)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), MinX - 7.f, MinY, 4.f, H);
			DrawRect(FLinearColor(0.1f, 1.f, 0.1f), MinX - 7.f, MinY + H * (1.f - HPFrac), 4.f, H * HPFrac);
		}

		if (Player->bEspLine)
		{
			DrawLine(Canvas->SizeX * 0.5f, Canvas->SizeY, MinX + W * 0.5f, MaxY, FLinearColor(1.f, 1.f, 1.f, 0.35f), 1.f);
		}

		if (OwnPawn && Player->bEspDist)
		{
			const int32 Meters = FMath::RoundToInt(FVector::Dist(OwnPawn->GetActorLocation(), Origin) / 100.f);
			DrawTextScaled(FString::Printf(TEXT("%dm"), Meters), FLinearColor::White, MinX, MinY - 16.f, RSFontMain(), 1.1f);
		}

		// Упреждение: жёлтая метка там, где цель окажется — по ней и префайрим.
		// Рисуем по тем же правилам, по каким целится аимбот, иначе метка
		// показывала бы не то, куда полетит пуля.
		if (Player->bPredict && Player->bEspMark && Enemy->GetVelocity().Size2D() > 100.f)
		{
			const FVector Ahead = Player->PredictPoint(Enemy, !Player->IsVisibleTo(Enemy));
			const FVector Screen = Project(Ahead);
			if (Screen.Z > 0.f)
			{
				const FLinearColor Amber(1.f, 0.75f, 0.1f);
				DrawBoxOutline(Screen.X - 9.f, Screen.Y - 9.f, 18.f, 18.f, Amber, 1.5f);
				DrawLine(MinX + W * 0.5f, MinY + H * 0.5f, Screen.X, Screen.Y, Amber, 1.f);
			}
		}
	}
}

void ARSHUD::DrawCheatStatus(const ARSCharacter* Player)
{
	if (!Player->bAlive)
	{
		return;
	}

	// показываем только то, что включено: выключенный чит на экране не следит
	const bool bShowDT = Player->bDoubleTap;
	const bool bShowChance = Player->HitChance > 0.f && (Player->bTriggerbot || Player->bAimbot);
	const bool bShowDmg = Player->bTriggerbot || Player->bAimbot;
	if (!bShowDT && !bShowChance && !bShowDmg)
	{
		return;
	}

	// у левого края, под лентой событий: по центру полоски лезли на прицел
	const float W = 190.f;
	const float X = 32.f;
	float Y = Canvas->SizeY * 0.5f - 40.f;

	auto Bar = [&](const FString& Label, float Frac, const FString& Value, const FLinearColor& Col)
	{
		DrawTextScaled(Label, ColDim, X, Y - 1.f, RSFontMain(), 1.0f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X + 46.f, Y + 2.f, W - 100.f, 8.f);
		DrawRect(Col, X + 46.f, Y + 2.f, (W - 100.f) * FMath::Clamp(Frac, 0.f, 1.f), 8.f);
		DrawTextScaled(Value, Col, X + W - 48.f, Y - 1.f, RSFontMain(), 1.0f);
		Y += 16.f;
	};

	if (bShowDT)
	{
		const float Need = RSWeapons::Get(Player->CurrentWeapon).Interval * 2.f;
		const float Frac = Player->DoubleTapCharge / FMath::Max(0.01f, Need);
		const bool bReady = Frac >= 1.f;
		Bar(TEXT("DT"), Frac, bReady ? TEXT("готов") : FString::Printf(TEXT("%.0f%%"), Frac * 100.f),
			bReady ? FLinearColor(0.3f, 0.9f, 1.f) : FLinearColor(0.45f, 0.5f, 0.6f));
	}

	if (bShowChance)
	{
		const float Chance = Player->EstimateHitChance();
		const bool bOk = Chance >= Player->HitChance;
		Bar(TEXT("HIT"), Chance / 100.f,
			FString::Printf(TEXT("%.0f/%.0f"), Chance, Player->HitChance),
			bOk ? ColCSGreen : FLinearColor(0.9f, 0.55f, 0.2f));
	}

	if (bShowDmg)
	{
		const float Dmg = Player->DamageIfFiredNow();
		const float Threshold = Player->EffectiveMinDamage();
		const bool bOk = Dmg >= Threshold;
		Bar(TEXT("DMG"), Dmg / 100.f,
			FString::Printf(TEXT("%.0f/%.0f"), Dmg, Threshold),
			bOk ? ColCSGreen : FLinearColor(0.9f, 0.55f, 0.2f));
	}
}

void ARSHUD::DrawCheatLog(const ARSCharacter* Player)
{
	if (!Player->bCheatLogs || Player->CheatLogLines.Num() == 0)
	{
		return;
	}

	// лента слева под радаром: свежие строки внизу, старые тают
	const float Now = GetWorld()->GetTimeSeconds();
	const float Life = 5.f;
	float Y = 240.f;

	for (const ARSCharacter::FRSCheatLog& Line : Player->CheatLogLines)
	{
		const float Age = Now - Line.Time;
		if (Age > Life)
		{
			continue;
		}
		FLinearColor Col = Line.Color;
		Col.A = FMath::Clamp((Life - Age) / 1.5f, 0.f, 1.f);
		DrawTextScaled(Line.Text, Col, 32.f, Y, RSFontMain(), 1.05f);
		Y += 17.f;
	}
}

void ARSHUD::DrawRadar(const ARSCharacter* Player)
{
	// Круглый радар CS2
	const float Size = 190.f;
	const float X = 30.f, Y = 30.f;
	const float CX = X + Size * 0.5f;
	const float CY = Y + Size * 0.5f;
	const float R = Size * 0.5f;
	const float Range = 4500.f;

	DrawRect(ColBgDark, X, Y, Size, Size);
	DrawBoxOutline(X, Y, Size, Size, ColBorder, 2.0f);

	// Сетка и круги дальности
	DrawLine(CX, Y + 4.f, CX, Y + Size - 4.f, FLinearColor(1.f, 1.f, 1.f, 0.10f), 1.5f);
	DrawLine(X + 4.f, CY, X + Size - 4.f, CY, FLinearColor(1.f, 1.f, 1.f, 0.10f), 1.5f);
	DrawBoxOutline(X + Size * 0.25f, Y + Size * 0.25f, Size * 0.5f, Size * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.10f), 1.f);

	// Метки плентов A и B
	UFont* Font = RSFontBig();
	DrawTextScaled(TEXT("A"), ColGold, X + 18.f, Y + 14.f, Font, 1.1f);
	DrawTextScaled(TEXT("B"), ColGold, X + Size - 28.f, Y + Size - 28.f, Font, 1.1f);

	const FVector MyLoc = Player->GetActorLocation();
	const float Yaw = FMath::DegreesToRadians(Player->GetControlRotation().Yaw);
	const FVector2D Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw));
	const FVector2D Right(-FMath::Sin(Yaw), FMath::Cos(Yaw));

	auto PlotOnRadar = [&](const AActor* Who, uint8 Team, bool bEnemy)
	{
		const FVector Rel3 = Who->GetActorLocation() - MyLoc;
		const FVector2D Rel(Rel3.X, Rel3.Y);
		float U = FVector2D::DotProduct(Rel, Right) / Range * R;
		float V = -FVector2D::DotProduct(Rel, Fwd) / Range * R;
		U = FMath::Clamp(U, -R + 8.f, R - 8.f);
		V = FMath::Clamp(V, -R + 8.f, R - 8.f);

		const FLinearColor DotCol = bEnemy ? ColRed : TeamColor(Team);
		DrawRect(DotCol, CX + U - 4.f, CY + V - 4.f, 8.f, 8.f);
	};

	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It) || *It == Player || !It->bAlive) { continue; }
		const bool bEnemy = It->Team != Player->Team;
		if (!bEnemy || Player->bESP) { PlotOnRadar(*It, (uint8)It->Team, bEnemy); }
	}
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It) || It->Health <= 0.f) { continue; }
		const bool bEnemy = It->Team != Player->Team;
		if (!bEnemy || Player->bESP) { PlotOnRadar(*It, (uint8)It->Team, bEnemy); }
	}

	// Свой маркер — яркая белая стрелка
	DrawLine(CX, CY - 9.f, CX - 6.f, CY + 6.f, FLinearColor::White, 2.5f);
	DrawLine(CX, CY - 9.f, CX + 6.f, CY + 6.f, FLinearColor::White, 2.5f);
	DrawLine(CX - 6.f, CY + 6.f, CX + 6.f, CY + 6.f, FLinearColor::White, 2.0f);
}

void ARSHUD::DrawKillFeed(const ARSCharacter* Player)
{
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State)
	{
		return;
	}

	UFont* Font = RSFontMain();
	const float Now = GetWorld()->GetTimeSeconds();
	const float Right = Canvas->SizeX - 260.f;
	float Y = 45.f;

	for (const FRSKillEntry& E : State->KillFeed)
	{
		const float Age = Now - E.Time;
		if (Age > 6.f)
		{
			continue;
		}
		const float Alpha = FMath::Clamp(1.f - (Age - 5.f), 0.f, 1.f);

		const FString WeaponPart = FString::Printf(TEXT("  [%s%s]  "),
			*E.Weapon, E.bHeadshot ? TEXT(" HS") : TEXT(""));

		float KW, KH, WW, WH, VW, VH;
		GetTextSizeScaled(E.Killer, KW, KH, Font, 1.25f);
		GetTextSizeScaled(WeaponPart, WW, WH, Font, 1.25f);
		GetTextSizeScaled(E.Victim, VW, VH, Font, 1.25f);

		const float TotalW = KW + WW + VW + 20.f;
		const float X = Right - TotalW;

		DrawRect(FLinearColor(0.04f, 0.05f, 0.07f, 0.85f * Alpha), X - 8.f, Y - 4.f, TotalW + 16.f, 28.f);
		DrawBoxOutline(X - 8.f, Y - 4.f, TotalW + 16.f, 28.f, FLinearColor(1.f, 1.f, 1.f, 0.20f * Alpha), 1.f);

		FLinearColor KC = TeamColor(E.KillerTeam); KC.A = Alpha;
		FLinearColor VC = TeamColor(E.VictimTeam); VC.A = Alpha;
		FLinearColor WC = ColWhite; WC.A = Alpha;
		if (E.bHeadshot)
		{
			WC = FLinearColor(1.f, 0.25f, 0.2f, Alpha);
		}

		DrawTextScaled(E.Killer, KC, X, Y, Font, 1.25f);
		DrawTextScaled(WeaponPart, WC, X + KW, Y, Font, 1.25f);
		DrawTextScaled(E.Victim, VC, X + KW + WW, Y, Font, 1.25f);
		Y += 32.f;
	}
}

void ARSHUD::DrawPerfStats()
{
	const int32 Mode = RSOptions::GetPerfMode();
	if (Mode <= 0)
	{
		return;
	}

	// копим кадры и раз в полсекунды пересчитываем показания
	const float Delta = FMath::Max(FApp::GetDeltaTime(), KINDA_SMALL_NUMBER);
	PerfFrames++;
	PerfAccumTime += Delta;

	const float Now = FPlatformTime::Seconds();
	if (Now >= PerfNextUpdate)
	{
		// секунда: на полусекунде цифры прыгают и их не прочитать
		PerfNextUpdate = Now + 1.f;

		if (PerfAccumTime > 0.f && PerfFrames > 0)
		{
			ShownFPS = PerfFrames / PerfAccumTime;
			ShownFrameMs = PerfAccumTime / PerfFrames * 1000.f;
		}
		PerfFrames = 0;
		PerfAccumTime = 0.f;

		// ЦП: реальная загрузка процесса, её движок считает сам
		ShownCPUPct = (float)FPlatformTime::GetCPUTime().CPUTimePct;

		// ГП в процентах: сколько времени кадра занимает видеокарта
		const float GPUMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		ShownGPUPct = (ShownFrameMs > 0.01f)
			? FMath::Clamp(GPUMs / ShownFrameMs * 100.f, 0.f, 100.f) : 0.f;
	}

	UFont* Font = RSFontMain();
	const float X = Canvas->SizeX - 150.f;
	float Y = 8.f;

	const int32 FPS = FMath::RoundToInt(ShownFPS);
	// зелёный от 90, жёлтый от 45, ниже красный
	const FLinearColor FPSColor = (FPS >= 90) ? ColCSGreen
		: (FPS >= 45) ? ColGold : ColRed;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 8.f, Y - 4.f, 148.f, (Mode >= 2) ? 58.f : 22.f);
	DrawTextScaled(FString::Printf(TEXT("%d FPS   %.1f мс"), FPS, ShownFrameMs),
		FPSColor, X, Y, Font, 1.15f);

	if (Mode >= 2)
	{
		auto LoadColor = [](float Pct)
		{
			return (Pct >= 90.f) ? ColRed : (Pct >= 70.f) ? ColGold : ColDim;
		};

		Y += 18.f;
		DrawTextScaled(FString::Printf(TEXT("ЦП   %3.0f %%"), ShownCPUPct),
			LoadColor(ShownCPUPct), X, Y, Font, 1.05f);
		Y += 16.f;
		DrawTextScaled(FString::Printf(TEXT("ГП   %3.0f %%"), ShownGPUPct),
			LoadColor(ShownGPUPct), X, Y, Font, 1.05f);
	}
}

bool ARSHUD::GetMouseOnCanvas(FVector2D& Out) const
{
	APlayerController* PC = GetOwningPlayerController();
	float MX = 0.f, MY = 0.f;
	if (PC && PC->GetMousePosition(MX, MY))
	{
		Out = FVector2D(MX, MY);
		return true;
	}
	return false;
}

bool ARSHUD::HandleBuyClick(const FVector2D& Mouse, ARSCharacter* Player)
{
	if (!Player || !Player->bBuyMenuOpen)
	{
		return false;
	}

	for (const FRSBuyHotspot& Spot : BuyHotspots)
	{
		if (!Spot.Contains(Mouse))
		{
			continue;
		}
		// клик по заголовку просто переключает категорию для цифровых клавиш
		Player->BuyCategory = Spot.Category;
		if (Spot.Kind == 0 || Spot.Kind == 1)
		{
			Player->ServerBuyArmor(Spot.Kind == 1);
		}
		else if (Spot.Kind == -1)
		{
			Player->ServerBuyWeapon(Spot.Weapon);
		}
		return true;
	}
	// клик мимо карточек внутри меню всё равно не должен стрелять
	return true;
}

void ARSHUD::DrawCheatPanel(const ARSCharacter* Player)
{
	// Оверлей открывается на Del или Insert. Пока он закрыт, на экране
	// никаких следов читов — чистый HUD.
	struct FCheatRow { const TCHAR* Key; const TCHAR* Name; const TCHAR* Desc; bool bOn; };
	const FCheatRow Rows[] =
	{
		{ TEXT("01"), TEXT("Aimbot"),      TEXT("наводит в голову"),        Player->bAimbot },
		{ TEXT("02"), TEXT("ESP / WH"),    TEXT("враги сквозь стены"),      Player->bESP },
		{ TEXT("03"), TEXT("Triggerbot"),  TEXT("стреляет сам по цели"),    Player->bTriggerbot },
		{ TEXT("04"), TEXT("NoRecoil"),    TEXT("без отдачи и разброса"),   Player->bNoRecoilSpread },
		{ TEXT("05"), TEXT("Speed + BHop"),TEXT("скорость и распрыжка"),    Player->bSpeedhack },
		{ TEXT("06"), TEXT("Silent Aim"),  TEXT("пули летят в цель"),       Player->bSilentAim },
		{ TEXT("07"), TEXT("GodMode"),     TEXT("бессмертие"),              Player->bGodMode },
		{ TEXT("08"), TEXT("Деньги"),      TEXT("кошелёк не пустеет"),      Player->bInfiniteMoney },
		{ TEXT("09"), TEXT("Anti-Aim"),    TEXT("тело смотрит не туда"),    Player->bAntiAim },
		{ TEXT("10"), TEXT("Predict"),     TEXT("упреждение и префайр"),    Player->bPredict },
	};

	UFont* Small = RSFontMain();

	// когда оверлей закрыт, на экране не остаётся ничего: состояние читов
	// смотрим по Del/Insert
	if (!Player->bCheatMenuOpen)
	{
		return;
	}

	// значения настроек: показываем словами, а не числами там, где так понятнее
	static const TCHAR* ModeNames[] = { TEXT("Спиной"), TEXT("Спин"), TEXT("Дрожь") };
	auto OnOff = [](bool b) { return b ? TEXT("вкл") : TEXT("выкл"); };

	struct FSetRow { int32 Id; const TCHAR* Name; FString Value; bool bToggle; };
	const FSetRow Sets[] =
	{
		{ 0, TEXT("Анти-аим: режим"),
			ModeNames[FMath::Clamp(Player->AntiAimMode, 0, 2)], false },
		{ 1, TEXT("Анти-аим: качание"),
			FString::Printf(TEXT("%.0f°"), Player->AntiAimSwing), false },
		{ 2, TEXT("Анти-аим: наклон головы"),
			FString::Printf(TEXT("%.0f°"), Player->AntiAimPitch), false },
		{ 3, TEXT("Анти-аим: кручение"),
			FString::Printf(TEXT("%.0f°/с"), Player->AntiAimSpin), false },
		{ 4, TEXT("Предикт: тиков"),
			FString::Printf(TEXT("%d  (%.0f мс)"), Player->PredictTicks,
				Player->PredictTicks * ARSCharacter::PredictTickSeconds * 1000.f), false },
		{ 5, TEXT("Предикт: только за стеной"), OnOff(Player->bPredictOnlyHidden), true },
		{ 11, TEXT("Триггербот: FOV"),
			Player->TriggerFov <= 0.f ? FString(TEXT("точный луч"))
				: FString::Printf(TEXT("%.1f°"), Player->TriggerFov), false },
		{ 6, TEXT("ВХ: рамка"),        OnOff(Player->bEspBox),    true },
		{ 7, TEXT("ВХ: полоса ХП"),    OnOff(Player->bEspHealth), true },
		{ 8, TEXT("ВХ: дистанция"),    OnOff(Player->bEspDist),   true },
		{ 9, TEXT("ВХ: линия к цели"), OnOff(Player->bEspLine),   true },
		{ 10, TEXT("ВХ: метка упреждения"), OnOff(Player->bEspMark), true },
	};

	const float W = 560.f;
	const float RowH = 34.f;
	const float SetH = 26.f;
	const float H = 78.f + RowH * UE_ARRAY_COUNT(Rows)
		+ 30.f + SetH * UE_ARRAY_COUNT(Sets) + 34.f;
	const float X = Canvas->SizeX * 0.5f - W * 0.5f;
	const float Y0 = Canvas->SizeY * 0.5f - H * 0.5f;

	// затемняем игру, чтобы оверлей читался поверх любой карты
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	DrawRect(ColBgSolid, X, Y0, W, H);
	DrawBoxOutline(X, Y0, W, H, ColRed, 2.f);
	DrawRect(ColRed, X, Y0, W, 3.f);

	DrawTextScaled(TEXT("RAGE MENU"), ColRed, X + 20.f, Y0 + 16.f, RSFontBig(), 1.5f);
	DrawTextScaled(TEXT("Del / Insert — закрыть"), ColDim, X + W - 190.f, Y0 + 24.f, Small, 1.1f);

	// строки — переключатели: собираем их зоны для мыши
	CheatHotspots.Reset();
	FVector2D Mouse;
	const bool bHasMouse = GetMouseOnCanvas(Mouse);

	float Y = Y0 + 62.f;
	for (int32 i = 0; i < UE_ARRAY_COUNT(Rows); i++)
	{
		const FCheatRow& Row = Rows[i];
		const FBox2D Zone(FVector2D(X + 14.f, Y), FVector2D(X + W - 14.f, Y + RowH - 6.f));
		CheatHotspots.Add(TPair<int32, FBox2D>(i, Zone));

		const bool bHover = bHasMouse && Zone.IsInside(Mouse);
		FLinearColor RowBg = Row.bOn ? FLinearColor(0.10f, 0.30f, 0.14f, 0.55f)
		                             : FLinearColor(1.f, 1.f, 1.f, 0.04f);
		if (bHover)
		{
			RowBg.A += 0.18f;
		}
		DrawRect(RowBg, Zone.Min.X, Zone.Min.Y, Zone.GetSize().X, Zone.GetSize().Y);
		if (bHover)
		{
			DrawBoxOutline(Zone.Min.X, Zone.Min.Y, Zone.GetSize().X, Zone.GetSize().Y, ColGold, 1.5f);
		}

		DrawTextScaled(Row.Key, ColGold, X + 24.f, Y + 6.f, Small, 1.2f);
		DrawTextScaled(Row.Name, ColWhite, X + 62.f, Y + 6.f, Small, 1.25f);
		DrawTextScaled(Row.Desc, ColDim, X + 190.f, Y + 7.f, Small, 1.05f);

		// сам переключатель: рамка с бегунком
		const float TogX = X + W - 78.f;
		const float TogY = Y + 7.f;
		DrawRect(Row.bOn ? ColCSGreen : FLinearColor(0.25f, 0.25f, 0.27f),
			TogX, TogY, 44.f, 18.f);
		DrawRect(FLinearColor(0.92f, 0.94f, 0.96f),
			Row.bOn ? (TogX + 26.f) : (TogX + 2.f), TogY + 2.f, 16.f, 14.f);

		Y += RowH;
	}

	// --- настройки: значение со стрелками по бокам ---
	CheatSettingHotspots.Reset();
	Y += 6.f;
	DrawTextScaled(TEXT("НАСТРОЙКИ"), ColGold, X + 24.f, Y, Small, 1.2f);
	Y += 22.f;

	for (const FSetRow& Set : Sets)
	{
		const float ArrowW = 22.f;
		const float PlusX = X + W - 40.f;
		const float MinusX = PlusX - 132.f;

		if (Set.bToggle)
		{
			// переключателю стрелки не нужны: жмём всю строку
			const FBox2D Zone(FVector2D(X + 14.f, Y - 2.f), FVector2D(X + W - 14.f, Y + SetH - 8.f));
			CheatSettingHotspots.Add(TPair<int32, FBox2D>(Set.Id * 2 + 1, Zone));
			if (bHasMouse && Zone.IsInside(Mouse))
			{
				DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.06f),
					Zone.Min.X, Zone.Min.Y, Zone.GetSize().X, Zone.GetSize().Y);
			}
			DrawTextScaled(Set.Name, ColWhite, X + 24.f, Y, Small, 1.1f);
			DrawTextScaled(Set.Value, Set.Value == TEXT("вкл") ? ColCSGreen : ColDim,
				PlusX - 6.f, Y, Small, 1.1f);
		}
		else
		{
			const FBox2D MinusZone(FVector2D(MinusX, Y - 2.f), FVector2D(MinusX + ArrowW, Y + SetH - 8.f));
			const FBox2D PlusZone(FVector2D(PlusX, Y - 2.f), FVector2D(PlusX + ArrowW, Y + SetH - 8.f));
			CheatSettingHotspots.Add(TPair<int32, FBox2D>(Set.Id * 2, MinusZone));
			CheatSettingHotspots.Add(TPair<int32, FBox2D>(Set.Id * 2 + 1, PlusZone));

			DrawTextScaled(Set.Name, ColWhite, X + 24.f, Y, Small, 1.1f);

			auto Arrow = [&](const FBox2D& Zone, const TCHAR* Sign)
			{
				const bool bHover = bHasMouse && Zone.IsInside(Mouse);
				DrawRect(bHover ? FLinearColor(0.35f, 0.35f, 0.38f) : FLinearColor(0.18f, 0.18f, 0.20f),
					Zone.Min.X, Zone.Min.Y, Zone.GetSize().X, Zone.GetSize().Y);
				DrawTextScaled(Sign, ColWhite, Zone.Min.X + 7.f, Zone.Min.Y + 2.f, Small, 1.15f);
			};
			Arrow(MinusZone, TEXT("<"));
			Arrow(PlusZone, TEXT(">"));

			DrawTextScaled(Set.Value, ColGold, MinusX + ArrowW + 12.f, Y, Small, 1.1f);
		}
		Y += SetH;
	}

	DrawTextScaled(TEXT("ЛКМ по строке — переключить, по стрелкам — изменить"), ColDim,
		X + 20.f, Y0 + H - 26.f, Small, 1.05f);
}

bool ARSHUD::HandleCheatClick(const FVector2D& Mouse, ARSCharacter* Player)
{
	if (!Player || !Player->bCheatMenuOpen)
	{
		return false;
	}
	// стрелки настроек проверяем первыми: они лежат ниже строк-переключателей
	for (const TPair<int32, FBox2D>& Spot : CheatSettingHotspots)
	{
		if (Spot.Value.IsInside(Mouse))
		{
			Player->ApplyCheatSetting(Spot.Key / 2, (Spot.Key % 2) ? 1 : -1);
			return true;
		}
	}
	for (const TPair<int32, FBox2D>& Spot : CheatHotspots)
	{
		if (Spot.Value.IsInside(Mouse))
		{
			Player->ToggleCheatByIndex(Spot.Key);
			return true;
		}
	}
	// клик мимо строк внутри оверлея всё равно не должен стрелять
	return true;
}

// ВЫРАЗИТЕЛЬНЫЙ НИЖНИЙ CS2 HUD (Скриншот 1 из примера)
void ARSHUD::DrawMoney(const ARSCharacter* Player)
{
	// Функция включена в единый DrawHealthArmor модуль для точной подгонки
}

void ARSHUD::DrawHealthArmor(const ARSCharacter* Player)
{
	UFont* Big = RSFontBig();
	UFont* Small = RSFontMain();

	const float H = Canvas->SizeY;
	const float CX = Canvas->SizeX * 0.5f;

	// 1. ДЕНЬГИ (Слева по центру $7300 с фоновой плашкой)
	const FString MoneyStr = FString::Printf(TEXT("$ %d"), Player->Money);
	float MW = 0.f, MH = 0.f;
	GetTextSizeScaled(MoneyStr, MW, MH, Big, 1.8f);
	// деньги ушли в левый нижний угол, здоровье — по центру, патроны — вправо
	const float MoneyX = 44.f;
	const float MoneyY = H - 65.f;

	DrawRect(ColBgDark, MoneyX - 12.f, MoneyY - 6.f, MW + 24.f, 44.f);
	DrawBoxOutline(MoneyX - 12.f, MoneyY - 6.f, MW + 24.f, 44.f, ColBorder, 1.5f);
	DrawRect(ColCSGreen, MoneyX - 12.f, MoneyY - 6.f, 4.f, 44.f); // зелёная полоса CS2
	DrawTextScaled(MoneyStr, ColCSGreen, MoneyX + 4.f, MoneyY, Big, 1.8f);

	// 2. ЗДОРОВЬЕ И БРОНЯ (Центр-Слева 100 с плашкой и 6px полосой)
	const int32 HP = FMath::Max(0, FMath::RoundToInt(Player->Health));
	const FString HPStr = FString::Printf(TEXT("%d"), HP);
	float HW = 0.f, HH = 0.f;
	GetTextSizeScaled(HPStr, HW, HH, Big, 2.4f);
	const float HPX = CX - 59.f; // плашка шириной 150 встаёт ровно по центру
	const float HPY = H - 75.f;

	DrawRect(ColBgDark, HPX - 16.f, HPY - 4.f, 150.f, 54.f);
	DrawBoxOutline(HPX - 16.f, HPY - 4.f, 150.f, 54.f, ColBorder, 1.5f);

	const FLinearColor HPCol = HP > 30 ? ColWhite : ColRed;
	DrawTextScaled(HPStr, HPCol, HPX, HPY, Big, 2.4f);

	// Толстая подчёркивающая полоса здоровья CS2 (6px)
	const float HPFrac = FMath::Clamp(HP / 100.f, 0.f, 1.f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.18f), HPX - 10.f, HPY + 44.f, 138.f, 6.f);
	DrawRect(HPCol, HPX - 10.f, HPY + 44.f, 138.f * HPFrac, 6.f);

	// Индикатор брони
	const int32 AP = FMath::RoundToInt(Player->Armor);
	if (AP > 0)
	{
		const FString APStr = FString::Printf(TEXT("[%d]"), AP);
		DrawTextScaled(APStr, ColCT, HPX + HW + 6.f, HPY + 16.f, Small, 1.3f);
	}

	// 3. ПАТРОНЫ И ОРУЖИЕ (Центр-Справа 12 / 24 с фоновой плашкой)
	const float AmmoX = Canvas->SizeX - 212.f;
	const float AmmoY = H - 75.f;

	DrawRect(ColBgDark, AmmoX - 12.f, AmmoY - 4.f, 180.f, 54.f);
	DrawBoxOutline(AmmoX - 12.f, AmmoY - 4.f, 180.f, 54.f, ColBorder, 1.5f);

	if (Player->bReloading)
	{
		DrawTextScaled(TEXT("ПЕРЕЗАРЯДКА..."), ColGold, AmmoX, AmmoY + 10.f, Big, 1.4f);
		return;
	}

	DrawWeaponIcon(Canvas, AmmoX + 96.f, AmmoY - 32.f, 76.f, 28.f, Player->CurrentWeapon, ColWhite);
	DrawTextScaled(Player->GetWeaponName().ToUpper(), ColDim, AmmoX + 110.f, AmmoY - 6.f, Small, 1.15f);

	if (RSWeapons::IsGrenade(Player->CurrentWeapon))
	{
		const int32 GI = RSWeapons::GrenadeIndex(Player->CurrentWeapon);
		DrawTextScaled(FString::Printf(TEXT("x %d"), Player->Grenades[GI]), ColWhite, AmmoX, AmmoY + 6.f, Big, 2.0f);
		return;
	}
	if (Player->GetMaxAmmo() == 0)
	{
		DrawTextScaled(TEXT("—"), ColWhite, AmmoX, AmmoY + 6.f, Big, 2.0f);
		return;
	}

	const int32 InMag = Player->GetAmmo();
	const FLinearColor MagCol = InMag > 4 ? ColWhite : ColRed;

	const FString AmmoMagStr = FString::Printf(TEXT("%d"), InMag);
	DrawTextScaled(AmmoMagStr, MagCol, AmmoX, AmmoY, Big, 2.4f);

	const FString AmmoResStr = FString::Printf(TEXT("/  %d"), Player->GetReserveAmmo());
	DrawTextScaled(AmmoResStr, ColDim, AmmoX + 60.f, AmmoY + 14.f, Small, 1.6f);
}

void ARSHUD::DrawAmmo(const ARSCharacter* Player)
{
	// Функция включена в единый DrawHealthArmor модуль для точной подгонки
}

void ARSHUD::DrawRoundInfo(const ARSCharacter* Player)
{
	// Верхнее CS2 Табло Игроков и Раунда (Скриншот 1 и 3)
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State)
	{
		return;
	}

	UFont* Font = RSFontMain();
	UFont* Med = RSFontBig();

	const float CX = Canvas->SizeX * 0.5f;
	const float Y = 12.f;

	int32 AliveCT = 0, AliveT = 0;
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->bAlive)
		{
			(It->Team == ERSTeam::CT ? AliveCT : AliveT)++;
		}
	}
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->Health > 0.f)
		{
			(It->Team == ERSTeam::CT ? AliveCT : AliveT)++;
		}
	}

	// 5 Крупных карточек спецназа (CT) слева
	const float CardW = 38.f;
	const float CardH = 42.f;
	const float CardGap = 4.f;
	float CTX = CX - 65.f - (5 * (CardW + CardGap));

	for (int32 i = 0; i < 5; i++)
	{
		const bool bAlive = i < AliveCT;
		const FLinearColor CardBg = bAlive ? FLinearColor(0.12f, 0.28f, 0.52f, 0.90f) : FLinearColor(0.04f, 0.05f, 0.07f, 0.65f);
		DrawRect(CardBg, CTX, Y, CardW, CardH);
		DrawRect(ColCT, CTX, Y, CardW, 4.f);

		if (bAlive)
		{
			DrawTextScaled(TEXT("CT"), ColCT, CTX + 10.f, Y + 12.f, Font, 1.2f);
		}
		else
		{
			DrawTextScaled(TEXT("X"), FLinearColor(0.4f, 0.4f, 0.4f), CTX + 13.f, Y + 12.f, Font, 1.2f);
		}
		CTX += CardW + CardGap;
	}

	// Центральный таймер и счёт
	const float CenterW = 120.f;
	const float CenterX = CX - CenterW * 0.5f;
	DrawRect(ColBgDark, CenterX, Y, CenterW, CardH + 18.f);
	DrawBoxOutline(CenterX, Y, CenterW, CardH + 18.f, ColBorder, 1.5f);

	const int32 Left = FMath::CeilToInt(State->GetTimeLeft());
	const FString TimerStr = FString::Printf(TEXT("%d:%02d"), Left / 60, Left % 60);
	const bool bUrgent = State->Phase == ERSPhase::Live && Left <= 10;

	DrawTextScaled(TimerStr, bUrgent ? ColRed : ColWhite, CenterX + 26.f, Y + 4.f, Med, 1.5f);

	const FString ScoreStr = FString::Printf(TEXT("%d   %d"), State->ScoreCT, State->ScoreT);
	DrawTextScaled(ScoreStr, ColWhite, CenterX + 40.f, Y + 28.f, Font, 1.3f);

	// живые по сторонам, а не подпись «5 vs 5» на все случаи жизни
	DrawTextScaled(FString::Printf(TEXT("%d — %d"), AliveCT, AliveT), ColDim, CenterX + 41.f, Y + 44.f, Font, 0.95f);

	// 5 Крупных карточек террористов (T) справа
	float TX = CX + 65.f;
	for (int32 i = 0; i < 5; i++)
	{
		const bool bAlive = i < AliveT;
		const FLinearColor CardBg = bAlive ? FLinearColor(0.52f, 0.36f, 0.12f, 0.90f) : FLinearColor(0.04f, 0.05f, 0.07f, 0.65f);
		DrawRect(CardBg, TX, Y, CardW, CardH);
		DrawRect(ColT, TX, Y, CardW, 4.f);

		if (bAlive)
		{
			DrawTextScaled(TEXT("T"), ColT, TX + 13.f, Y + 12.f, Font, 1.2f);
		}
		else
		{
			DrawTextScaled(TEXT("X"), FLinearColor(0.4f, 0.4f, 0.4f), TX + 13.f, Y + 12.f, Font, 1.2f);
		}
		TX += CardW + CardGap;
	}

	// Плашка времени закупки
	if (State->Phase == ERSPhase::Intermission && !Player->bBuyMenuOpen)
	{
		const FString BuyMsg = FString::Printf(TEXT("ВРЕМЯ ЗАКУПКИ (%d с) — Удерживай [ B ]"), Left);
		float TW = 0.f, TH = 0.f;
		GetTextSizeScaled(BuyMsg, TW, TH, Font, 1.3f);
		const float BX = CX - TW * 0.5f;
		const float BY = Y + CardH + 28.f;

		DrawRect(FLinearColor(0.04f, 0.05f, 0.07f, 0.90f), BX - 16.f, BY - 4.f, TW + 32.f, 28.f);
		DrawBoxOutline(BX - 16.f, BY - 4.f, TW + 32.f, 28.f, ColGold, 1.5f);
		DrawTextScaled(BuyMsg, ColGold, BX, BY, Font, 1.3f);
	}
}

void ARSHUD::DrawScoreboard(const ARSCharacter* Player)
{
	if (!Player->bScoreboardOpen)
	{
		return;
	}

	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	UFont* Font = RSFontMain();
	UFont* Med = RSFontBig();

	const float W = 780.f;
	const float X = Canvas->SizeX * 0.5f - W * 0.5f;
	const float Top = Canvas->SizeY * 0.15f;

	DrawRect(ColBgSolid, X - 20.f, Top - 20.f, W + 40.f, 440.f);
	DrawBoxOutline(X - 20.f, Top - 20.f, W + 40.f, 440.f, ColBorder, 1.5f);

	if (State)
	{
		const FString Title = FString::Printf(TEXT("CT  %d : %d  T     РАУНД %d / %d"),
			State->ScoreCT, State->ScoreT, State->RoundNumber, State->RoundsTotal);
		float TW = 0.f, TH = 0.f;
		GetTextSizeScaled(Title, TW, TH, Med, 1.5f);
		DrawTextScaled(Title, ColGold, Canvas->SizeX * 0.5f - TW * 0.5f, Top - 10.f, Med, 1.5f);
	}

	struct FRow { FString Name; int32 Kills; int32 Deaths; bool bAlive; bool bYou; int32 Money; };
	TArray<FRow> Columns[2];

	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		ARSCharacter* C = *It;
		if (!IsValid(C)) { continue; }
		const int32 Side = (C->Team == ERSTeam::CT) ? 0 : 1;
		const FString Name = C->Nick.IsEmpty() ? FString(TEXT("Игрок")) : C->Nick;
		Columns[Side].Add({ Name, C->Kills, C->Deaths, C->bAlive, C == Player, C->Money });
	}

	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		ARSBot* B = *It;
		if (!IsValid(B)) { continue; }
		const int32 Side = (B->Team == ERSTeam::CT) ? 0 : 1;
		Columns[Side].Add({ B->Nick.IsEmpty() ? FString::Printf(TEXT("Бот %d"), B->BotNumber) : B->Nick,
			B->Kills, B->Deaths, B->Health > 0.f, false, -1 });
	}

	const TCHAR* Titles[2] = { TEXT("КОНТР-ТЕРРОРИСТЫ"), TEXT("ТЕРРОРИСТЫ") };
	const FLinearColor TeamColors[2] = { ColCT, ColT };

	for (int32 Side = 0; Side < 2; Side++)
	{
		const float ColX = X + Side * (W * 0.5f + 10.f);
		float Y = Top + 30.f;

		DrawTextScaled(Titles[Side], TeamColors[Side], ColX, Y, Font, 1.4f);
		Y += 26.f;
		DrawTextScaled(TEXT("ИМЯ            У    С    $"), FLinearColor(1.f, 1.f, 1.f, 0.45f), ColX, Y, Font, 1.1f);
		Y += 22.f;

		for (const FRow& Row : Columns[Side])
		{
			const FLinearColor Color = !Row.bAlive
				? FLinearColor(0.45f, 0.45f, 0.45f)
				: (Row.bYou ? ColGold : ColWhite);

			DrawTextScaled(Row.Name, Color, ColX, Y, Font, 1.25f);
			DrawTextScaled(FString::Printf(TEXT("%d"), Row.Kills), Color, ColX + 150.f, Y, Font, 1.25f);
			DrawTextScaled(FString::Printf(TEXT("%d"), Row.Deaths), Color, ColX + 195.f, Y, Font, 1.25f);
			if (Row.Money >= 0)
			{
				DrawTextScaled(FString::Printf(TEXT("%d"), Row.Money), Color, ColX + 240.f, Y, Font, 1.25f);
			}
			Y += 24.f;
		}
	}
}

// 5-КОЛОНОЧНОЕ МЕНЮ ЗАКУПКИ CS2 (Скриншот 4 из примера)
void ARSHUD::DrawBuyMenu(const ARSCharacter* Player)
{
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State || !Player->bBuyMenuOpen)
	{
		return;
	}

	UFont* Font = RSFontMain();
	UFont* Med = RSFontBig();

	const bool bBuyTime = State->IsBuyTime();

	const int32 NumCols = RSWeapons::BuyCategories;
	const float ColW = 160.f;
	const float ColGap = 12.f;
	const float TotalW = NumCols * ColW + (NumCols - 1) * ColGap;
	const float StartX = Canvas->SizeX * 0.5f - TotalW * 0.5f;
	const float StartY = Canvas->SizeY * 0.16f;

	DrawRect(ColBgSolid, StartX - 24.f, StartY - 45.f, TotalW + 48.f, 500.f);
	DrawBoxOutline(StartX - 24.f, StartY - 45.f, TotalW + 48.f, 500.f, ColBorder, 1.5f);

	const int32 Left = FMath::CeilToInt(State->GetTimeLeft());
	DrawTextScaled(FString::Printf(TEXT("До конца закупки  %02d:%02d"), Left / 60, Left % 60), ColDim, StartX, StartY - 35.f, Font, 1.3f);
	DrawTextScaled(FString::Printf(TEXT("$ %d"), Player->Money), ColCSGreen, StartX + TotalW - 100.f, StartY - 35.f, Med, 1.5f);

	// с клавиатуры покупка в два шага (категория, потом предмет),
	// мышью — сразу по карточке; зоны для кликов собираем здесь же
	const int32 Chosen = Player->BuyCategory;
	BuyHotspots.Reset();

	FVector2D Mouse;
	const bool bHasMouse = GetMouseOnCanvas(Mouse);

	for (int32 c = 0; c < NumCols; c++)
	{
		const float CX = StartX + c * (ColW + ColGap);
		float CY = StartY;

		const bool bActive = (Chosen == c);

		FRSBuyHotspot Header;
		Header.Min = FVector2D(CX, CY);
		Header.Max = FVector2D(CX + ColW, CY + 30.f);
		Header.Category = c;
		Header.Kind = -2;
		BuyHotspots.Add(Header);

		const bool bHoverHeader = bHasMouse && Header.Contains(Mouse);
		DrawRect((bActive || bHoverHeader) ? FLinearColor(0.20f, 0.16f, 0.03f, 0.95f)
			: FLinearColor(0.04f, 0.05f, 0.07f, 0.92f), CX, CY, ColW, 30.f);
		if (bActive || bHoverHeader)
		{
			DrawBoxOutline(CX, CY, ColW, 30.f, ColGold, 1.5f);
		}
		DrawTextScaled(FString::Printf(TEXT("%d  %s"), c + 1, RSWeapons::BuyCategoryName(c)),
			ColWhite, CX + 8.f, CY + 5.f, Font, 1.2f);
		CY += 38.f;

		TArray<ERSWeapon> Items;
		if (c == RSWeapons::EquipmentCategory)
		{
			const bool bArmorFull = Player->Armor >= 100.f;
			struct FEItem { const TCHAR* Name; int32 Price; bool bOwned; };
			const FEItem EItems[] = {
				{ TEXT("Кевлар"), ARSCharacter::PriceKevlar, bArmorFull },
				{ TEXT("Кевлар + шлем"), ARSCharacter::PriceKevlarHelmet, bArmorFull && Player->bHasHelmet }
			};
			for (int32 i = 0; i < 2; i++)
			{
				const bool bCan = bBuyTime && !EItems[i].bOwned && Player->Money >= EItems[i].Price;

				FRSBuyHotspot Spot;
				Spot.Min = FVector2D(CX, CY);
				Spot.Max = FVector2D(CX + ColW, CY + 62.f);
				Spot.Category = c;
				Spot.Kind = (int8)i; // 0 кевлар, 1 кевлар со шлемом
				BuyHotspots.Add(Spot);

				const bool bHover = bHasMouse && Spot.Contains(Mouse);
				FLinearColor CardBg = EItems[i].bOwned ? FLinearColor(0.1f, 0.35f, 0.15f, 0.65f) : FLinearColor(0.06f, 0.08f, 0.10f, 0.80f);
				if (bHover && bCan)
				{
					CardBg = FLinearColor(0.12f, 0.22f, 0.14f, 0.95f);
				}

				DrawRect(CardBg, CX, CY, ColW, 62.f);
				DrawBoxOutline(CX, CY, ColW, 62.f, bCan ? ColCSGreen : ColBorder, bHover ? 2.f : 1.f);

				DrawTextScaled(FString::Printf(TEXT("%d"), i + 1), ColDim, CX + 6.f, CY + 4.f, Font, 1.0f);
				DrawTextScaled(EItems[i].Name, bCan ? ColWhite : ColDim, CX + 20.f, CY + 4.f, Font, 1.1f);
				DrawEquipIcon(Canvas, CX + ColW - 46.f, CY + 18.f, 38.f, 38.f,
					(i == 0) ? RSIcons::Kevlar() : RSIcons::Helmet(),
					bCan ? ColWhite : FLinearColor(0.65f, 0.65f, 0.65f, 0.75f));

				if (EItems[i].bOwned)
				{
					DrawTextScaled(TEXT("КУПЛЕНО"), ColCSGreen, CX + 8.f, CY + 38.f, Font, 1.05f);
				}
				else
				{
					DrawTextScaled(FString::Printf(TEXT("$%d"), EItems[i].Price), bCan ? ColCSGreen : ColDim, CX + ColW - 56.f, CY + 38.f, Font, 1.1f);
				}
				CY += 70.f;
			}
			continue;
		}
		else
		{
			Items = RSWeapons::BuyCategory(c, Player->Team);
		}

		for (int32 i = 0; i < Items.Num() && i < 6; i++)
		{
			const FRSWeaponDef& Def = RSWeapons::Get(Items[i]);
			const bool bOwned =
				(Def.Slot == ERSSlot::Primary && Player->bHasPrimary && Player->PrimaryType == Items[i])
				|| (Def.Slot == ERSSlot::Secondary && Player->bHasSecondary && Player->SecondaryType == Items[i])
				|| (Def.Slot == ERSSlot::Grenade && Player->Grenades[RSWeapons::GrenadeIndex(Items[i])] > 0);
			const bool bSlotBusy = Def.Slot == ERSSlot::Primary && Player->bHasPrimary;
			const bool bCan = bBuyTime && !bOwned && !bSlotBusy && Player->Money >= Def.Price;

			FRSBuyHotspot Spot;
			Spot.Min = FVector2D(CX, CY);
			Spot.Max = FVector2D(CX + ColW, CY + 62.f);
			Spot.Category = c;
			Spot.Weapon = Items[i];
			Spot.Kind = -1;
			BuyHotspots.Add(Spot);

			const bool bHover = bHasMouse && Spot.Contains(Mouse);
			FLinearColor CardBg = bOwned ? FLinearColor(0.1f, 0.35f, 0.15f, 0.65f) : FLinearColor(0.06f, 0.08f, 0.10f, 0.80f);
			if (bHover && bCan)
			{
				CardBg = FLinearColor(0.12f, 0.22f, 0.14f, 0.95f);
			}
			DrawRect(CardBg, CX, CY, ColW, 62.f);
			DrawBoxOutline(CX, CY, ColW, 62.f, bCan ? ColCSGreen : ColBorder, bHover ? 2.f : 1.f);

			DrawTextScaled(FString::Printf(TEXT("%d"), i + 1), ColDim, CX + 6.f, CY + 4.f, Font, 1.0f);
			DrawTextScaled(Def.Name, bCan ? ColWhite : ColDim, CX + 20.f, CY + 4.f, Font, 1.1f);

			DrawWeaponIcon(Canvas, CX + 8.f, CY + 20.f, ColW - 16.f, 24.f, Items[i],
				bCan ? ColWhite : FLinearColor(0.65f, 0.65f, 0.65f, 0.75f));

			if (bOwned)
			{
				DrawTextScaled(TEXT("КУПЛЕНО"), ColCSGreen, CX + 8.f, CY + 40.f, Font, 1.0f);
			}
			else
			{
				DrawTextScaled(FString::Printf(TEXT("$%d"), Def.Price), bCan ? ColCSGreen : ColDim, CX + ColW - 56.f, CY + 40.f, Font, 1.05f);
			}
			CY += 70.f;
		}
	}

	// подсказка меняется по шагу покупки, чтобы цифры не путались
	const FString Hint = (Chosen < 0)
		? FString(TEXT("ЛКМ — купить      цифра 1-6 — категория      отпусти B — закрыть"))
		: FString::Printf(TEXT("ЛКМ — купить      «%s»: цифра — купить      [0] назад      отпусти B — закрыть"),
			RSWeapons::BuyCategoryName(Chosen));
	DrawTextScaled(Hint, ColDim, StartX, StartY + 440.f, Font, 1.15f);

	if (!bBuyTime)
	{
		DrawTextScaled(TEXT("Покупать можно только между раундами"), ColRed,
			StartX, StartY + 418.f, Font, 1.15f);
	}
}
