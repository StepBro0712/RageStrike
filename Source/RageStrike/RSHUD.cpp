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
#include "EngineUtils.h"

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
	DrawCheatPanel(Player);
	DrawPerfStats();

	const float Now = GetWorld()->GetTimeSeconds();

	// подтверждение разметки спавна
	if (Now < Player->SpawnMarkUntil)
	{
		DrawText(Player->SpawnMarkMessage, ColGold,
			Canvas->SizeX * 0.5f - 200.f, Canvas->SizeY * 0.72f, GEngine->GetSmallFont(), 1.4f);
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

	// Строка управления: держим её в актуальном виде — клавиши тут те же,
	// что привязаны в ARSCharacter::SetupPlayerInputComponent
	if (!Player->bBuyMenuOpen && !Player->bScoreboardOpen)
	{
		DrawText(TEXT("1/2/3/4 — основное · пистолет · нож · гранаты   |   B — закупка   |   G — выбросить   |   R — перезарядка   |   ПКМ — прицел (с гранатой подкат)   |   Shift — тихо   |   Ctrl — присесть   |   F — осмотр   |   V — вид   |   Tab — счёт   |   Del — читы   |   Esc — меню"),
			FLinearColor(1.f, 1.f, 1.f, 0.32f), 24.f, Canvas->SizeY - 22.f, GEngine->GetSmallFont(), 1.f);
	}
}

void ARSHUD::DrawCrosshair(const ARSCharacter* Player)
{
	if (RSWeapons::Get(Player->CurrentWeapon).Mesh == ERSMeshKind::Sniper)
	{
		return;
	}

	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	const float Gap = FMath::Clamp(4.0f + Player->CurrentSpreadDeg * 8.f, 4.0f, 26.f);
	const float Len = 8.f;
	const float Thick = 2.5f;

	const FLinearColor Green(0.10f, 0.98f, 0.25f, 1.0f);
	const FLinearColor Outline(0.f, 0.f, 0.f, 0.85f);

	// Тёмный контур прицела для 100% контраста на любом фоне
	DrawLine(CX - Gap - Len - 1.f, CY, CX - Gap + 1.f, CY, Outline, Thick + 2.f);
	DrawLine(CX + Gap - 1.f, CY, CX + Gap + Len + 1.f, CY, Outline, Thick + 2.f);
	DrawLine(CX, CY - Gap - Len - 1.f, CX, CY - Gap + 1.f, Outline, Thick + 2.f);
	DrawLine(CX, CY + Gap - 1.f, CX, CY + Gap + Len + 1.f, Outline, Thick + 2.f);

	// Зеленый прицел CS2
	DrawLine(CX - Gap - Len, CY, CX - Gap, CY, Green, Thick);
	DrawLine(CX + Gap, CY, CX + Gap + Len, CY, Green, Thick);
	DrawLine(CX, CY - Gap - Len, CX, CY - Gap, Green, Thick);
	DrawLine(CX, CY + Gap, CX, CY + Gap + Len, Green, Thick);
}

void ARSHUD::DrawSniperScope(const ARSCharacter* Player)
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;
	const float R = FMath::Min(W, H) * 0.48f;

	DrawRect(FLinearColor::Black, 0.f, 0.f, CX - R, H);
	DrawRect(FLinearColor::Black, CX + R, 0.f, W - CX - R, H);
	DrawRect(FLinearColor::Black, CX - R, 0.f, 2.f * R, CY - R);
	DrawRect(FLinearColor::Black, CX - R, CY + R, 2.f * R, H - CY - R);

	const int32 Seg = 64;
	for (int32 i = 0; i < Seg; i++)
	{
		const float A0 = 2.f * PI * i / Seg;
		const float A1 = 2.f * PI * (i + 1) / Seg;
		DrawLine(CX + R * FMath::Cos(A0), CY + R * FMath::Sin(A0),
			CX + R * FMath::Cos(A1), CY + R * FMath::Sin(A1), FLinearColor::Black, 6.f);
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
		DrawBoxOutline(MinX, MinY, W, H, FLinearColor(1.f, 0.1f, 0.1f), 1.5f);

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
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), MinX - 7.f, MinY, 4.f, H);
		DrawRect(FLinearColor(0.1f, 1.f, 0.1f), MinX - 7.f, MinY + H * (1.f - HPFrac), 4.f, H * HPFrac);

		DrawLine(Canvas->SizeX * 0.5f, Canvas->SizeY, MinX + W * 0.5f, MaxY, FLinearColor(1.f, 1.f, 1.f, 0.35f), 1.f);

		if (OwnPawn)
		{
			const int32 Meters = FMath::RoundToInt(FVector::Dist(OwnPawn->GetActorLocation(), Origin) / 100.f);
			DrawText(FString::Printf(TEXT("%dm"), Meters), FLinearColor::White, MinX, MinY - 16.f, GEngine->GetSmallFont(), 1.1f);
		}
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
	UFont* Font = GEngine->GetMediumFont();
	DrawText(TEXT("A"), ColGold, X + 18.f, Y + 14.f, Font, 1.1f);
	DrawText(TEXT("B"), ColGold, X + Size - 28.f, Y + Size - 28.f, Font, 1.1f);

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

	UFont* Font = GEngine->GetSmallFont();
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
		GetTextSize(E.Killer, KW, KH, Font, 1.25f);
		GetTextSize(WeaponPart, WW, WH, Font, 1.25f);
		GetTextSize(E.Victim, VW, VH, Font, 1.25f);

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

		DrawText(E.Killer, KC, X, Y, Font, 1.25f);
		DrawText(WeaponPart, WC, X + KW, Y, Font, 1.25f);
		DrawText(E.Victim, VC, X + KW + WW, Y, Font, 1.25f);
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

	const float Delta = FMath::Max(FApp::GetDeltaTime(), KINDA_SMALL_NUMBER);
	const float Instant = 1.f / Delta;
	SmoothedFPS = (SmoothedFPS <= 0.f) ? Instant : FMath::Lerp(SmoothedFPS, Instant, 0.08f);

	UFont* Font = GEngine->GetSmallFont();
	const float X = Canvas->SizeX - 150.f;
	float Y = 8.f;

	const int32 FPS = FMath::RoundToInt(SmoothedFPS);
	// зелёный от 90, жёлтый от 45, ниже красный
	const FLinearColor FPSColor = (FPS >= 90) ? ColCSGreen
		: (FPS >= 45) ? ColGold : ColRed;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 8.f, Y - 4.f, 148.f, (Mode >= 2) ? 74.f : 22.f);
	DrawText(FString::Printf(TEXT("%d FPS   %.1f мс"), FPS, Delta * 1000.f),
		FPSColor, X, Y, Font, 1.15f);

	if (Mode >= 2)
	{
		Y += 18.f;
		const float GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
		const float RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
		// время кадра видеокарты движок держит в отдельном счётчике
		const float GPUMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());

		DrawText(FString::Printf(TEXT("ЦП игра    %.1f мс"), GameMs), ColDim, X, Y, Font, 1.05f);
		Y += 16.f;
		DrawText(FString::Printf(TEXT("ЦП отрис.  %.1f мс"), RenderMs), ColDim, X, Y, Font, 1.05f);
		Y += 16.f;
		DrawText(FString::Printf(TEXT("ГП         %.1f мс"), GPUMs), ColDim, X, Y, Font, 1.05f);
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
		{ TEXT("F1"), TEXT("Aimbot"),      TEXT("наводит в голову"),        Player->bAimbot },
		{ TEXT("F2"), TEXT("ESP / WH"),    TEXT("враги сквозь стены"),      Player->bESP },
		{ TEXT("F3"), TEXT("Triggerbot"),  TEXT("стреляет сам по цели"),    Player->bTriggerbot },
		{ TEXT("F4"), TEXT("NoRecoil"),    TEXT("без отдачи и разброса"),   Player->bNoRecoilSpread },
		{ TEXT("F5"), TEXT("Speed + BHop"),TEXT("скорость и распрыжка"),    Player->bSpeedhack },
		{ TEXT("F6"), TEXT("Silent Aim"),  TEXT("пули летят в цель"),       Player->bSilentAim },
		{ TEXT("F7"), TEXT("GodMode"),     TEXT("бессмертие"),              Player->bGodMode },
		{ TEXT("F8"), TEXT("Деньги"),      TEXT("кошелёк не пустеет"),      Player->bInfiniteMoney },
	};

	UFont* Small = GEngine->GetSmallFont();

	// когда оверлей закрыт, на экране не остаётся ничего: состояние читов
	// смотрим по Del/Insert
	if (!Player->bCheatMenuOpen)
	{
		return;
	}

	const float W = 460.f;
	const float RowH = 34.f;
	const float H = 78.f + RowH * UE_ARRAY_COUNT(Rows) + 34.f;
	const float X = Canvas->SizeX * 0.5f - W * 0.5f;
	const float Y0 = Canvas->SizeY * 0.5f - H * 0.5f;

	// затемняем игру, чтобы оверлей читался поверх любой карты
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	DrawRect(ColBgSolid, X, Y0, W, H);
	DrawBoxOutline(X, Y0, W, H, ColRed, 2.f);
	DrawRect(ColRed, X, Y0, W, 3.f);

	DrawText(TEXT("RAGE MENU"), ColRed, X + 20.f, Y0 + 16.f, GEngine->GetMediumFont(), 1.5f);
	DrawText(TEXT("Del / Insert — закрыть"), ColDim, X + W - 190.f, Y0 + 24.f, Small, 1.1f);

	float Y = Y0 + 62.f;
	for (const FCheatRow& Row : Rows)
	{
		DrawRect(Row.bOn ? FLinearColor(0.10f, 0.30f, 0.14f, 0.55f)
		                 : FLinearColor(1.f, 1.f, 1.f, 0.04f),
			X + 14.f, Y, W - 28.f, RowH - 6.f);

		DrawText(Row.Key, ColGold, X + 24.f, Y + 6.f, Small, 1.2f);
		DrawText(Row.Name, ColWhite, X + 62.f, Y + 6.f, Small, 1.25f);
		DrawText(Row.Desc, ColDim, X + 190.f, Y + 7.f, Small, 1.05f);
		DrawText(Row.bOn ? TEXT("ВКЛ") : TEXT("ВЫКЛ"),
			Row.bOn ? ColCSGreen : FLinearColor(0.5f, 0.5f, 0.5f),
			X + W - 66.f, Y + 6.f, Small, 1.2f);
		Y += RowH;
	}

	DrawText(TEXT("Клавиши F1–F8 работают и с закрытым меню"), ColDim,
		X + 20.f, Y0 + H - 26.f, Small, 1.05f);
}

// ВЫРАЗИТЕЛЬНЫЙ НИЖНИЙ CS2 HUD (Скриншот 1 из примера)
void ARSHUD::DrawMoney(const ARSCharacter* Player)
{
	// Функция включена в единый DrawHealthArmor модуль для точной подгонки
}

void ARSHUD::DrawHealthArmor(const ARSCharacter* Player)
{
	UFont* Big = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();

	const float H = Canvas->SizeY;
	const float CX = Canvas->SizeX * 0.5f;

	// 1. ДЕНЬГИ (Слева по центру $7300 с фоновой плашкой)
	const FString MoneyStr = FString::Printf(TEXT("$ %d"), Player->Money);
	float MW = 0.f, MH = 0.f;
	GetTextSize(MoneyStr, MW, MH, Big, 1.8f);
	const float MoneyX = CX - 340.f;
	const float MoneyY = H - 65.f;

	DrawRect(ColBgDark, MoneyX - 12.f, MoneyY - 6.f, MW + 24.f, 44.f);
	DrawBoxOutline(MoneyX - 12.f, MoneyY - 6.f, MW + 24.f, 44.f, ColBorder, 1.5f);
	DrawRect(ColCSGreen, MoneyX - 12.f, MoneyY - 6.f, 4.f, 44.f); // зелёная полоса CS2
	DrawText(MoneyStr, ColCSGreen, MoneyX + 4.f, MoneyY, Big, 1.8f);

	// 2. ЗДОРОВЬЕ И БРОНЯ (Центр-Слева 100 с плашкой и 6px полосой)
	const int32 HP = FMath::Max(0, FMath::RoundToInt(Player->Health));
	const FString HPStr = FString::Printf(TEXT("%d"), HP);
	float HW = 0.f, HH = 0.f;
	GetTextSize(HPStr, HW, HH, Big, 2.4f);
	const float HPX = CX - 130.f;
	const float HPY = H - 75.f;

	DrawRect(ColBgDark, HPX - 16.f, HPY - 4.f, 150.f, 54.f);
	DrawBoxOutline(HPX - 16.f, HPY - 4.f, 150.f, 54.f, ColBorder, 1.5f);

	const FLinearColor HPCol = HP > 30 ? ColWhite : ColRed;
	DrawText(HPStr, HPCol, HPX, HPY, Big, 2.4f);

	// Толстая подчёркивающая полоса здоровья CS2 (6px)
	const float HPFrac = FMath::Clamp(HP / 100.f, 0.f, 1.f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.18f), HPX - 10.f, HPY + 44.f, 138.f, 6.f);
	DrawRect(HPCol, HPX - 10.f, HPY + 44.f, 138.f * HPFrac, 6.f);

	// Индикатор брони
	const int32 AP = FMath::RoundToInt(Player->Armor);
	if (AP > 0)
	{
		const FString APStr = FString::Printf(TEXT("[%d]"), AP);
		DrawText(APStr, ColCT, HPX + HW + 6.f, HPY + 16.f, Small, 1.3f);
	}

	// 3. ПАТРОНЫ И ОРУЖИЕ (Центр-Справа 12 / 24 с фоновой плашкой)
	const float AmmoX = CX + 90.f;
	const float AmmoY = H - 75.f;

	DrawRect(ColBgDark, AmmoX - 12.f, AmmoY - 4.f, 180.f, 54.f);
	DrawBoxOutline(AmmoX - 12.f, AmmoY - 4.f, 180.f, 54.f, ColBorder, 1.5f);

	if (Player->bReloading)
	{
		DrawText(TEXT("ПЕРЕЗАРЯДКА..."), ColGold, AmmoX, AmmoY + 10.f, Big, 1.4f);
		return;
	}

	DrawWeaponIcon(Canvas, AmmoX + 96.f, AmmoY - 32.f, 76.f, 28.f, Player->CurrentWeapon, ColWhite);
	DrawText(Player->GetWeaponName().ToUpper(), ColDim, AmmoX + 110.f, AmmoY - 6.f, Small, 1.15f);

	if (RSWeapons::IsGrenade(Player->CurrentWeapon))
	{
		const int32 GI = RSWeapons::GrenadeIndex(Player->CurrentWeapon);
		DrawText(FString::Printf(TEXT("x %d"), Player->Grenades[GI]), ColWhite, AmmoX, AmmoY + 6.f, Big, 2.0f);
		return;
	}
	if (Player->GetMaxAmmo() == 0)
	{
		DrawText(TEXT("—"), ColWhite, AmmoX, AmmoY + 6.f, Big, 2.0f);
		return;
	}

	const int32 InMag = Player->GetAmmo();
	const FLinearColor MagCol = InMag > 4 ? ColWhite : ColRed;

	const FString AmmoMagStr = FString::Printf(TEXT("%d"), InMag);
	DrawText(AmmoMagStr, MagCol, AmmoX, AmmoY, Big, 2.4f);

	const FString AmmoResStr = FString::Printf(TEXT("/  %d"), Player->GetReserveAmmo());
	DrawText(AmmoResStr, ColDim, AmmoX + 60.f, AmmoY + 14.f, Small, 1.6f);
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

	UFont* Font = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();

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
			DrawText(TEXT("CT"), ColCT, CTX + 10.f, Y + 12.f, Font, 1.2f);
		}
		else
		{
			DrawText(TEXT("X"), FLinearColor(0.4f, 0.4f, 0.4f), CTX + 13.f, Y + 12.f, Font, 1.2f);
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

	DrawText(TimerStr, bUrgent ? ColRed : ColWhite, CenterX + 26.f, Y + 4.f, Med, 1.5f);

	const FString ScoreStr = FString::Printf(TEXT("%d   %d"), State->ScoreCT, State->ScoreT);
	DrawText(ScoreStr, ColWhite, CenterX + 40.f, Y + 28.f, Font, 1.3f);

	// живые по сторонам, а не подпись «5 vs 5» на все случаи жизни
	DrawText(FString::Printf(TEXT("%d — %d"), AliveCT, AliveT), ColDim, CenterX + 41.f, Y + 44.f, Font, 0.95f);

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
			DrawText(TEXT("T"), ColT, TX + 13.f, Y + 12.f, Font, 1.2f);
		}
		else
		{
			DrawText(TEXT("X"), FLinearColor(0.4f, 0.4f, 0.4f), TX + 13.f, Y + 12.f, Font, 1.2f);
		}
		TX += CardW + CardGap;
	}

	// Плашка времени закупки
	if (State->Phase == ERSPhase::Intermission && !Player->bBuyMenuOpen)
	{
		const FString BuyMsg = FString::Printf(TEXT("ВРЕМЯ ЗАКУПКИ (%d с) — Нажмите [ B ]"), Left);
		float TW = 0.f, TH = 0.f;
		GetTextSize(BuyMsg, TW, TH, Font, 1.3f);
		const float BX = CX - TW * 0.5f;
		const float BY = Y + CardH + 28.f;

		DrawRect(FLinearColor(0.04f, 0.05f, 0.07f, 0.90f), BX - 16.f, BY - 4.f, TW + 32.f, 28.f);
		DrawBoxOutline(BX - 16.f, BY - 4.f, TW + 32.f, 28.f, ColGold, 1.5f);
		DrawText(BuyMsg, ColGold, BX, BY, Font, 1.3f);
	}
}

void ARSHUD::DrawScoreboard(const ARSCharacter* Player)
{
	if (!Player->bScoreboardOpen)
	{
		return;
	}

	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	UFont* Font = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();

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
		GetTextSize(Title, TW, TH, Med, 1.5f);
		DrawText(Title, ColGold, Canvas->SizeX * 0.5f - TW * 0.5f, Top - 10.f, Med, 1.5f);
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
		Columns[Side].Add({ FString::Printf(TEXT("Бот %d"), B->BotNumber),
			B->Kills, B->Deaths, B->Health > 0.f, false, -1 });
	}

	const TCHAR* Titles[2] = { TEXT("КОНТР-ТЕРРОРИСТЫ"), TEXT("ТЕРРОРИСТЫ") };
	const FLinearColor TeamColors[2] = { ColCT, ColT };

	for (int32 Side = 0; Side < 2; Side++)
	{
		const float ColX = X + Side * (W * 0.5f + 10.f);
		float Y = Top + 30.f;

		DrawText(Titles[Side], TeamColors[Side], ColX, Y, Font, 1.4f);
		Y += 26.f;
		DrawText(TEXT("ИМЯ            У    С    $"), FLinearColor(1.f, 1.f, 1.f, 0.45f), ColX, Y, Font, 1.1f);
		Y += 22.f;

		for (const FRow& Row : Columns[Side])
		{
			const FLinearColor Color = !Row.bAlive
				? FLinearColor(0.45f, 0.45f, 0.45f)
				: (Row.bYou ? ColGold : ColWhite);

			DrawText(Row.Name, Color, ColX, Y, Font, 1.25f);
			DrawText(FString::Printf(TEXT("%d"), Row.Kills), Color, ColX + 150.f, Y, Font, 1.25f);
			DrawText(FString::Printf(TEXT("%d"), Row.Deaths), Color, ColX + 195.f, Y, Font, 1.25f);
			if (Row.Money >= 0)
			{
				DrawText(FString::Printf(TEXT("%d"), Row.Money), Color, ColX + 240.f, Y, Font, 1.25f);
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

	UFont* Font = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();

	const bool bBuyTime = State->Phase == ERSPhase::Intermission;

	const int32 NumCols = RSWeapons::BuyCategories;
	const float ColW = 160.f;
	const float ColGap = 12.f;
	const float TotalW = NumCols * ColW + (NumCols - 1) * ColGap;
	const float StartX = Canvas->SizeX * 0.5f - TotalW * 0.5f;
	const float StartY = Canvas->SizeY * 0.16f;

	DrawRect(ColBgSolid, StartX - 24.f, StartY - 45.f, TotalW + 48.f, 500.f);
	DrawBoxOutline(StartX - 24.f, StartY - 45.f, TotalW + 48.f, 500.f, ColBorder, 1.5f);

	const int32 Left = FMath::CeilToInt(State->GetTimeLeft());
	DrawText(FString::Printf(TEXT("До конца закупки  %02d:%02d"), Left / 60, Left % 60), ColDim, StartX, StartY - 35.f, Font, 1.3f);
	DrawText(FString::Printf(TEXT("$ %d"), Player->Money), ColCSGreen, StartX + TotalW - 100.f, StartY - 35.f, Med, 1.5f);

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
		DrawText(FString::Printf(TEXT("%d  %s"), c + 1, RSWeapons::BuyCategoryName(c)),
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

				DrawText(FString::Printf(TEXT("%d"), i + 1), ColDim, CX + 6.f, CY + 4.f, Font, 1.0f);
				DrawText(EItems[i].Name, bCan ? ColWhite : ColDim, CX + 20.f, CY + 4.f, Font, 1.1f);
				DrawEquipIcon(Canvas, CX + ColW - 46.f, CY + 18.f, 38.f, 38.f,
					(i == 0) ? RSIcons::Kevlar() : RSIcons::Helmet(),
					bCan ? ColWhite : FLinearColor(0.65f, 0.65f, 0.65f, 0.75f));

				if (EItems[i].bOwned)
				{
					DrawText(TEXT("КУПЛЕНО"), ColCSGreen, CX + 8.f, CY + 38.f, Font, 1.05f);
				}
				else
				{
					DrawText(FString::Printf(TEXT("$%d"), EItems[i].Price), bCan ? ColCSGreen : ColDim, CX + ColW - 56.f, CY + 38.f, Font, 1.1f);
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

			DrawText(FString::Printf(TEXT("%d"), i + 1), ColDim, CX + 6.f, CY + 4.f, Font, 1.0f);
			DrawText(Def.Name, bCan ? ColWhite : ColDim, CX + 20.f, CY + 4.f, Font, 1.1f);

			DrawWeaponIcon(Canvas, CX + 8.f, CY + 20.f, ColW - 16.f, 24.f, Items[i],
				bCan ? ColWhite : FLinearColor(0.65f, 0.65f, 0.65f, 0.75f));

			if (bOwned)
			{
				DrawText(TEXT("КУПЛЕНО"), ColCSGreen, CX + 8.f, CY + 40.f, Font, 1.0f);
			}
			else
			{
				DrawText(FString::Printf(TEXT("$%d"), Def.Price), bCan ? ColCSGreen : ColDim, CX + ColW - 56.f, CY + 40.f, Font, 1.05f);
			}
			CY += 70.f;
		}
	}

	// подсказка меняется по шагу покупки, чтобы цифры не путались
	const FString Hint = (Chosen < 0)
		? FString(TEXT("ЛКМ — купить      цифра 1-6 — категория      [B] закрыть"))
		: FString::Printf(TEXT("ЛКМ — купить      «%s»: цифра — купить      [0] назад      [B] закрыть"),
			RSWeapons::BuyCategoryName(Chosen));
	DrawText(Hint, ColDim, StartX, StartY + 440.f, Font, 1.15f);

	if (!bBuyTime)
	{
		DrawText(TEXT("Покупать можно только между раундами"), ColRed,
			StartX, StartY + 418.f, Font, 1.15f);
	}
}
