#include "RSHUD.h"
#include "RSCharacter.h"
#include "RSBot.h"
#include "RSGameMode.h"
#include "RSGameState.h"
#include "RSPlayerController.h"
#include "RSMatchSettings.h"
#include "RSIcons.h"
#include "RSFont.h"
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

	// Шрифт HUD. Движковые GetSmallFont/GetMediumFont растровые и мылятся
	// при растяжении — теперь берём векторный из RSFont (Roboto-Bold через
	// FontFace с рантайм-кешем). Если ассет не поднялся, RSFont сам вернёт
	// движковый, поэтому текст не пропадёт ни при каком раскладе.
	UFont* RSFontMain() { return RSFont::Get(); }
	UFont* RSFontBig()  { return RSFont::GetBig(); }
}

void ARSHUD::DrawTextScaled(const FString& Text, FLinearColor Color, float X, float Y,
	UFont* Font, float Scale)
{
	// Векторный шрифт растеризуется в кеш своего кегля, а масштаб канвас
	// применяет уже к готовой картинке. Компенсация из RSFont возвращает
	// текст к размеру, под который верстался HUD, и при этом гарантирует,
	// что картинку всегда ужимают, а не растягивают.
	//
	// На UIScale домножаем здесь же: иначе панели ужимаются под разрешение,
	// а кегль остаётся прежним, и в окне ниже 1080p текст вылезает из всех
	// плашек разом. Один множитель на весь HUD — все замеры и вся вёрстка
	// автоматически остаются согласованными.
	DrawText(Text, Color, X, Y, Font, Scale * RSFont::RenderScale(Font) * UIScale());
}

void ARSHUD::GetTextSizeScaled(const FString& Text, float& OutW, float& OutH,
	UFont* Font, float Scale)
{
	// Замер обязан идти с теми же множителями, иначе панели посчитают ширину
	// не по тому, что реально нарисовано.
	GetTextSize(Text, OutW, OutH, Font, Scale * RSFont::RenderScale(Font) * UIScale());
}

void ARSHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Accent)
{
	const float S = UIScale();
	DrawRect(ColBgDark, X, Y, W, H);
	DrawBoxOutline(X, Y, W, H, ColBorder, FMath::Max(1.f, 1.5f * S));
	if (Accent.A > 0.f)
	{
		// акцентная полоса слева — приём из CS2, она же кодирует состояние
		// блока цветом, не полагаясь на один только цвет цифр
		DrawRect(Accent, X, Y, FMath::Max(2.f, 4.f * S), H);
	}
}

void ARSHUD::DrawTextCentered(const FString& Text, FLinearColor Color, float CX, float CY,
	UFont* Font, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSizeScaled(Text, W, H, Font, Scale);
	DrawTextScaled(Text, Color, CX - W * 0.5f, CY - H * 0.5f, Font, Scale);
}

float ARSHUD::UIScale() const
{
	// Размеры HUD подбирались на 1080p. На 1440p и 4K те же пиксели
	// визуально сжимались вдвое, поэтому масштабируем по высоте экрана.
	// Ниже 0.8 не опускаемся: текст канваса и так растровый.
	if (!Canvas || Canvas->SizeY <= 0)
	{
		return 1.f;
	}
	return FMath::Clamp((float)Canvas->SizeY / 1080.f, 0.8f, 2.5f);
}

void ARSHUD::DrawBoxOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	DrawLine(X, Y, X + W, Y, Color, Thickness);
	DrawLine(X, Y + H, X + W, Y + H, Color, Thickness);
	DrawLine(X, Y, X, Y + H, Color, Thickness);
	DrawLine(X + W, Y, X + W, Y + H, Color, Thickness);
}

// Вписывает иконку в отведённый прямоугольник с сохранением пропорций и
// центрует её в нём. Без этого K2_DrawTexture растягивает текстуру на весь
// бокс: в закупке карточка широкая и низкая, и стволы выходили вытянутыми.
static void FitIconRect(const UTexture2D* Icon, float& X, float& Y, float& W, float& H)
{
	if (!Icon || W <= 0.f || H <= 0.f)
	{
		return;
	}
	const float TexW = (float)Icon->GetSizeX();
	const float TexH = (float)Icon->GetSizeY();
	if (TexW <= 0.f || TexH <= 0.f)
	{
		return;
	}

	const float Fit = FMath::Min(W / TexW, H / TexH);
	const float NewW = TexW * Fit;
	const float NewH = TexH * Fit;
	X += (W - NewW) * 0.5f;
	Y += (H - NewH) * 0.5f;
	W = NewW;
	H = NewH;
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
		FitIconRect(Icon, X, Y, W, H);
		Canvas->K2_DrawTexture(Icon, FVector2D(X, Y), FVector2D(W, H),
			FVector2D::ZeroVector, FVector2D::UnitVector, Color, EBlendMode::BLEND_Translucent);
	}
}

static void DrawEquipIcon(UCanvas* Canvas, float X, float Y, float W, float H,
	UTexture2D* Icon, const FLinearColor& Color)
{
	if (Canvas && Icon)
	{
		FitIconRect(Icon, X, Y, W, H);
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

	// Хитмаркер попадания. Раньше он ровно 0.15 с висел белым и пропадал
	// рывком, а на светлых стенах Mirage белые штрихи без контура сливались
	// с фоном. Теперь гаснет за 0.22 с и рисуется с тёмной обводкой.
	const float HitAge = Now - Player->LastHitMarkerTime;
	if (HitAge < 0.22f)
	{
		const float S = UIScale();
		const float CX = Canvas->SizeX * 0.5f;
		const float CY = Canvas->SizeY * 0.5f;
		const float A = FMath::Clamp(1.f - HitAge / 0.22f, 0.f, 1.f);
		const float In = 6.f * S;
		const float Out = 14.f * S;
		const float T = FMath::Max(1.f, 3.f * S);

		// два прохода: сначала контур потолще, поверх — сам штрих
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const FLinearColor PC = (Pass == 0) ? FLinearColor(0.f, 0.f, 0.f, 0.8f * A)
			                                    : FLinearColor(1.f, 1.f, 1.f, A);
			const float PT = (Pass == 0) ? T + 2.f * FMath::Max(1.f, S) : T;
			DrawLine(CX - Out, CY - Out, CX - In, CY - In, PC, PT);
			DrawLine(CX + Out, CY - Out, CX + In, CY - In, PC, PT);
			DrawLine(CX - Out, CY + Out, CX - In, CY + In, PC, PT);
			DrawLine(CX + Out, CY + Out, CX + In, CY + In, PC, PT);
		}
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

	const float S = UIScale();
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	// всё берётся из настроек игрока; динамический зазор растёт от разброса
	const float Len = RSCrosshair::GetLength() * S;
	const float Thick = FMath::Max(1.f, FMath::RoundToFloat(RSCrosshair::GetThickness() * S));
	const float BaseGap = RSCrosshair::GetGap() * S;
	const float TargetGap = RSCrosshair::GetDynamic()
		? FMath::Clamp(BaseGap + Player->CurrentSpreadDeg * 8.f * S, BaseGap, BaseGap + 22.f * S)
		: BaseGap;

	// Раньше зазор брался прямо из разброса и скакал на каждый выстрел —
	// прицел мерцал. Теперь открывается мгновенно (выстрел должен читаться
	// сразу), а закрывается плавно, примерно за четверть секунды.
	if (CrossGapShown < 0.f)
	{
		CrossGapShown = TargetGap;
	}
	CrossGapShown = (TargetGap > CrossGapShown)
		? TargetGap
		: FMath::FInterpTo(CrossGapShown, TargetGap,
			FMath::Min(FApp::GetDeltaTime(), 0.1f), 12.f);
	const float Gap = CrossGapShown;

	const FLinearColor Color = RSCrosshair::GetColor();
	const FLinearColor Outline(0.f, 0.f, 0.f, 0.85f);

	if (RSCrosshair::GetOutline())
	{
		// тёмный контур: прицел читается на любом фоне
		const float O = FMath::Max(1.f, S);
		DrawLine(CX - Gap - Len - O, CY, CX - Gap + O, CY, Outline, Thick + 2.f * O);
		DrawLine(CX + Gap - O, CY, CX + Gap + Len + O, CY, Outline, Thick + 2.f * O);
		DrawLine(CX, CY - Gap - Len - O, CX, CY - Gap + O, Outline, Thick + 2.f * O);
		DrawLine(CX, CY + Gap - O, CX, CY + Gap + Len + O, Outline, Thick + 2.f * O);
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
		const float O = FMath::Max(1.f, S);
		if (RSCrosshair::GetOutline())
		{
			DrawRect(Outline, CX - Thick * 0.5f - O, CY - Thick * 0.5f - O,
				Thick + 2.f * O, Thick + 2.f * O);
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
	const float S = UIScale();
	const float W = 190.f * S;
	const float X = 32.f * S;
	float Y = Canvas->SizeY * 0.5f - 40.f * S;

	auto Bar = [&](const FString& Label, float Frac, const FString& Value, const FLinearColor& Col)
	{
		const float BarX = X + 46.f * S;
		const float BarW = W - 100.f * S;
		DrawTextScaled(Label, ColDim, X, Y - 1.f * S, RSFontMain(), 1.0f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), BarX, Y + 2.f * S, BarW, 8.f * S);
		DrawRect(Col, BarX, Y + 2.f * S, BarW * FMath::Clamp(Frac, 0.f, 1.f), 8.f * S);
		DrawTextScaled(Value, Col, X + W - 48.f * S, Y - 1.f * S, RSFontMain(), 1.0f);
		Y += 16.f * S;
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
	const float S = UIScale();
	const float Now = GetWorld()->GetTimeSeconds();
	const float Life = 5.f;
	// якорь считается от нижнего края радара, а не жёстким числом 240:
	// радар теперь масштабируется, и лента должна ехать вместе с ним
	float Y = (30.f + 190.f + 20.f) * S;
	const float X = 32.f * S;
	// Шаг считается от реальной высоты строки, а не жёсткими 17 пикселями:
	// у векторного шрифта своя метрика, и на константе подложки соседних
	// строк наезжали друг на друга.
	float ProbeW = 0.f, ProbeH = 0.f;
	GetTextSizeScaled(TEXT("Ap"), ProbeW, ProbeH, RSFontMain(), 1.05f);
	const float Step = ProbeH + 6.f * S;

	// Не больше шести строк. Хранится десять, и в перестрелке лента
	// растягивалась почти на треть экрана поверх карты.
	TArray<const ARSCharacter::FRSCheatLog*> Lines;
	for (const ARSCharacter::FRSCheatLog& Line : Player->CheatLogLines)
	{
		if (Now - Line.Time <= Life)
		{
			Lines.Add(&Line);
		}
	}
	while (Lines.Num() > 6)
	{
		Lines.RemoveAt(0);
	}

	UFont* Font = RSFontMain();
	for (const ARSCharacter::FRSCheatLog* Ptr : Lines)
	{
		const float Age = Now - Ptr->Time;
		const float A = FMath::Clamp((Life - Age) / 1.5f, 0.f, 1.f);

		// Тёмная подложка под строкой: лента лежит поверх карты, и на
		// светлых стенах цветной текст без фона читался плохо.
		float TW = 0.f, TH = 0.f;
		GetTextSizeScaled(Ptr->Text, TW, TH, Font, 1.05f);
		DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.55f * A),
			X - 6.f * S, Y - 2.f * S, TW + 12.f * S, TH + 4.f * S);

		FLinearColor Col = Ptr->Color;
		Col.A = A;
		DrawTextScaled(Ptr->Text, Col, X, Y, Font, 1.05f);
		Y += Step;
	}
}

void ARSHUD::DrawRadar(const ARSCharacter* Player)
{
	// Круглый радар CS2
	const float S = UIScale();
	const float Size = 190.f * S;
	const float X = 30.f * S, Y = 30.f * S;
	const float CX = X + Size * 0.5f;
	const float CY = Y + Size * 0.5f;
	const float R = Size * 0.5f;
	const float Range = 4500.f;

	DrawPanel(X, Y, Size, Size, FLinearColor(0.f, 0.f, 0.f, 0.f));

	// Сетка и круги дальности
	DrawLine(CX, Y + 4.f * S, CX, Y + Size - 4.f * S, FLinearColor(1.f, 1.f, 1.f, 0.10f), FMath::Max(1.f, 1.5f * S));
	DrawLine(X + 4.f * S, CY, X + Size - 4.f * S, CY, FLinearColor(1.f, 1.f, 1.f, 0.10f), FMath::Max(1.f, 1.5f * S));
	DrawBoxOutline(X + Size * 0.25f, Y + Size * 0.25f, Size * 0.5f, Size * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.10f), 1.f);

	// Метки плентов A и B
	UFont* Font = RSFontBig();
	DrawTextScaled(TEXT("A"), ColGold, X + 18.f * S, Y + 14.f * S, Font, 1.1f);
	DrawTextScaled(TEXT("B"), ColGold, X + Size - 28.f * S, Y + Size - 28.f * S, Font, 1.1f);

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
		const float Dot = 8.f * S;
		U = FMath::Clamp(U, -R + Dot, R - Dot);
		V = FMath::Clamp(V, -R + Dot, R - Dot);

		const FLinearColor DotCol = bEnemy ? ColRed : TeamColor(Team);
		DrawRect(DotCol, CX + U - Dot * 0.5f, CY + V - Dot * 0.5f, Dot, Dot);
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
	const float AT = FMath::Max(1.5f, 2.5f * S);
	DrawLine(CX, CY - 9.f * S, CX - 6.f * S, CY + 6.f * S, FLinearColor::White, AT);
	DrawLine(CX, CY - 9.f * S, CX + 6.f * S, CY + 6.f * S, FLinearColor::White, AT);
	DrawLine(CX - 6.f * S, CY + 6.f * S, CX + 6.f * S, CY + 6.f * S, FLinearColor::White, AT * 0.8f);
}

void ARSHUD::DrawKillFeed(const ARSCharacter* Player)
{
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State)
	{
		return;
	}

	UFont* Font = RSFontMain();
	const float S = UIScale();
	const float Now = GetWorld()->GetTimeSeconds();
	// Отступ справа был жёстко 260 px: на 4K лента почти упиралась в край,
	// на 1080p съедала четверть строки. Считаем от ширины экрана.
	const float Right = Canvas->SizeX - FMath::Max(140.f, Canvas->SizeX * 0.135f);
	// Без множителя S: его теперь применяет сама DrawTextScaled. Здесь он
	// оставался с прошлой правки, и масштаб домножался дважды — текст рос
	// как квадрат, а высота строки линейно, из-за чего буквы вылезали за
	// рамку тем сильнее, чем дальше разрешение от 1080p.
	const float Scale = 1.25f;
	// высота строки — от реального текста, а не от константы
	float RowProbeW = 0.f, RowProbeH = 0.f;
	GetTextSizeScaled(TEXT("Ap"), RowProbeW, RowProbeH, Font, Scale);
	const float RowH = RowProbeH + 10.f * S;
	float Y = 45.f * S;

	// имя локального игрока, чтобы подсветить свои строки
	const FString Me = RSCombatantName(Player);

	// Показываем не больше пяти строк. Записей хранится восемь, и когда
	// раунд сливался в перестрелку, лента доезжала до радара.
	TArray<const FRSKillEntry*> Rows;
	for (const FRSKillEntry& E : State->KillFeed)
	{
		if (Now - E.Time <= 6.f)
		{
			Rows.Add(&E);
		}
	}
	while (Rows.Num() > 5)
	{
		Rows.RemoveAt(0);
	}

	for (const FRSKillEntry* Ptr : Rows)
	{
		const FRSKillEntry& E = *Ptr;
		const float Age = Now - E.Time;
		// Проявление за 0.12 с: раньше строка возникала мгновенно и дёргала
		// взгляд посреди боя. Уход прежний — последняя секунда жизни.
		const float Alpha = FMath::Min(
			FMath::Clamp(Age / 0.12f, 0.f, 1.f),
			FMath::Clamp(1.f - (Age - 5.f), 0.f, 1.f));
		if (Alpha <= 0.f)
		{
			continue;
		}

		// Свой килл и своя смерть — события разной цены, и мешать их в одну
		// подсветку нельзя: против ботов игрок делает почти все килы, и лента
		// целиком становилась выделенной, то есть не выделенной вовсе.
		const bool bMyKill = !Me.IsEmpty() && E.Killer == Me;
		const bool bMyDeath = !Me.IsEmpty() && E.Victim == Me;

		// Оружие показываем иконкой из закупки. Если тип неизвестен — убил
		// не ствол, а падение, огонь или мир, — откатываемся на прежний текст.
		const bool bIcon = (E.WeaponType < ERSWeapon::COUNT);

		// Значки обстоятельств идут после оружия. Обычное убийство не несёт
		// ни одного — так и просили: если просто, то без иконки.
		UTexture2D* Badges[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
		int32 BadgeCount = 0;
		auto AddBadge = [&](uint8 Bit, UTexture2D* Tex)
		{
			if ((E.Flags & Bit) && Tex && BadgeCount < 5)
			{
				Badges[BadgeCount++] = Tex;
			}
		};
		AddBadge(RSKill::Headshot, RSIcons::KillHeadshot());
		AddBadge(RSKill::Penetrate, RSIcons::KillPenetrate());
		AddBadge(RSKill::Noscope, RSIcons::KillNoscope());
		AddBadge(RSKill::Blind, RSIcons::KillBlind());
		AddBadge(RSKill::Smoke, RSIcons::KillSmoke());

		// Хедшот раньше показывался словом HS. Теперь для него есть значок,
		// но текст остаётся запасным: если иконка не загрузилась, признак
		// не должен пропасть совсем.
		const bool bHSText = E.bHeadshot && Badges[0] == nullptr;
		const FString WeaponPart = bIcon
			? (bHSText ? FString(TEXT(" HS ")) : FString())
			: FString::Printf(TEXT("  [%s%s]  "), *E.Weapon, E.bHeadshot ? TEXT(" HS") : TEXT(""));

		float KW, KH, WW, WH, VW, VH;
		GetTextSizeScaled(E.Killer, KW, KH, Font, Scale);
		GetTextSizeScaled(WeaponPart, WW, WH, Font, Scale);
		GetTextSizeScaled(E.Victim, VW, VH, Font, Scale);

		// иконки занимают своё место в ширине строки наравне с текстом
		const float IcoW = bIcon ? 58.f * S : 0.f;
		const float IcoH = bIcon ? 20.f * S : 0.f;
		const float IcoPad = bIcon ? 12.f * S : 0.f;
		const float BadgeSz = 20.f * S;
		const float BadgeGap = 6.f * S;
		const float BadgesW = BadgeCount > 0
			? BadgeCount * (BadgeSz + BadgeGap) : 0.f;
		WW += IcoW + IcoPad * 2.f + BadgesW;

		const float TotalW = KW + WW + VW + 20.f * S;
		const float X = Right - TotalW;
		// боковой отступ 12, а не 8: на масштабе 0.83 текст почти упирался
		// в рамку
		const float PanelX = X - 12.f * S;
		const float PanelW = TotalW + 24.f * S;

		// Подложка гаснет медленнее текста. Раньше обе шли с одним и тем же
		// коэффициентом, и к концу затухания полупрозрачная панель уже почти
		// не перекрывала карту — имена оказывались на голой светлой стене.
		const float PanelA = 0.88f * FMath::Pow(Alpha, 0.6f);

		// Свой килл отмечаем сдержанно — подложка чуть светлее и рамка ярче,
		// но обе нейтрально-серые. Тёплый фон здесь пробовали: на нём синий
		// цвет CT у имени жертвы терял насыщенность и читался почти белым.
		// Своя смерть — событие редкое, её и надо ловить взглядом: красная
		// подложка и красная рамка вдвое толще.
		FLinearColor PanelC(0.04f, 0.05f, 0.07f, PanelA);
		FLinearColor BorderC(1.f, 1.f, 1.f, 0.20f * Alpha);
		float BorderT = FMath::Max(1.f, S);
		if (bMyDeath)
		{
			PanelC = FLinearColor(0.16f, 0.03f, 0.03f, PanelA);
			BorderC = FLinearColor(0.95f, 0.25f, 0.20f, 0.9f * Alpha);
			BorderT = FMath::Max(2.f, 2.f * S);
		}
		else if (bMyKill)
		{
			PanelC = FLinearColor(0.09f, 0.10f, 0.13f, PanelA);
			BorderC = FLinearColor(1.f, 1.f, 1.f, 0.45f * Alpha);
		}

		DrawRect(PanelC, PanelX, Y, PanelW, RowH);
		DrawBoxOutline(PanelX, Y, PanelW, RowH, BorderC, BorderT);

		FLinearColor KC = TeamColor(E.KillerTeam); KC.A = Alpha;
		FLinearColor VC = TeamColor(E.VictimTeam); VC.A = Alpha;
		FLinearColor WC = ColWhite; WC.A = Alpha;
		if (E.bHeadshot)
		{
			WC = FLinearColor(1.f, 0.25f, 0.2f, Alpha);
		}

		// Текст по центру строки: раньше он рисовался от верхнего края
		// подложки и висел в её верхней трети.
		const float TextH = FMath::Max3(KH, WH, VH);
		const float TextY = Y + (RowH - TextH) * 0.5f;

		DrawTextScaled(E.Killer, KC, X, TextY, Font, Scale);

		if (bIcon)
		{
			// иконка оружия по центру строки, за ней значки обстоятельств
			DrawWeaponIcon(Canvas, X + KW + IcoPad, Y + (RowH - IcoH) * 0.5f,
				IcoW, IcoH, E.WeaponType, WC);

			float BadgeX = X + KW + IcoPad + IcoW;
			if (bHSText)
			{
				DrawTextScaled(WeaponPart, WC, BadgeX, TextY, Font, Scale);
				BadgeX += WW - IcoW - IcoPad * 2.f - BadgesW;
			}
			for (int32 b = 0; b < BadgeCount; b++)
			{
				float BX = BadgeX + BadgeGap * 0.5f;
				float BY = Y + (RowH - BadgeSz) * 0.5f;
				float BW = BadgeSz;
				float BH = BadgeSz;
				FitIconRect(Badges[b], BX, BY, BW, BH);
				Canvas->K2_DrawTexture(Badges[b], FVector2D(BX, BY), FVector2D(BW, BH),
					FVector2D::ZeroVector, FVector2D::UnitVector, WC,
					EBlendMode::BLEND_Translucent);
				BadgeX += BadgeSz + BadgeGap;
			}
		}
		else
		{
			DrawTextScaled(WeaponPart, WC, X + KW, TextY, Font, Scale);
		}

		DrawTextScaled(E.Victim, VC, X + KW + WW, TextY, Font, Scale);
		Y += RowH + 4.f * S;
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
	const float S = UIScale();
	const int32 FPS = FMath::RoundToInt(ShownFPS);
	// зелёный от 90, жёлтый от 45, ниже красный
	const FLinearColor FPSColor = (FPS >= 90) ? ColCSGreen
		: (FPS >= 45) ? ColGold : ColRed;

	const FString TopStr = FString::Printf(TEXT("%d FPS   %.1f мс"), FPS, ShownFrameMs);
	const FString CPUStr = FString::Printf(TEXT("ЦП   %3.0f %%"), ShownCPUPct);
	const FString GPUStr = FString::Printf(TEXT("ГП   %3.0f %%"), ShownGPUPct);

	// Ширину блока меряем по самой длинной строке, а не берём 148 пикселей:
	// на широком шрифте «мс» уезжало за край экрана и обрезалось.
	float BoxW = 0.f, LineH = 0.f, TW = 0.f, TH = 0.f;
	GetTextSizeScaled(TopStr, TW, LineH, Font, 1.15f);
	BoxW = TW;
	if (Mode >= 2)
	{
		GetTextSizeScaled(CPUStr, TW, TH, Font, 1.05f);
		BoxW = FMath::Max(BoxW, TW);
		GetTextSizeScaled(GPUStr, TW, TH, Font, 1.05f);
		BoxW = FMath::Max(BoxW, TW);
	}

	const float Pad = 8.f * S;
	const float X = Canvas->SizeX - BoxW - Pad * 2.f;
	float Y = 8.f * S;
	const float BoxH = (Mode >= 2) ? (LineH + 2.f * TH + 16.f * S) : (LineH + 8.f * S);

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - Pad, Y - 4.f * S, BoxW + Pad * 2.f, BoxH);
	DrawTextScaled(TopStr, FPSColor, X, Y, Font, 1.15f);

	if (Mode >= 2)
	{
		auto LoadColor = [](float Pct)
		{
			return (Pct >= 90.f) ? ColRed : (Pct >= 70.f) ? ColGold : ColDim;
		};

		Y += LineH + 2.f * S;
		DrawTextScaled(CPUStr, LoadColor(ShownCPUPct), X, Y, Font, 1.05f);
		Y += TH + 2.f * S;
		DrawTextScaled(GPUStr, LoadColor(ShownGPUPct), X, Y, Font, 1.05f);
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

	const float S = UIScale();
	const float H = Canvas->SizeY;
	const float W = Canvas->SizeX;

	// Раскладка как в CS2: слева снизу столбец «деньги над здоровьем»,
	// справа снизу патроны. Центр экрана освобождён — там раньше стояло
	// здоровье и лезло прямо под прицел.
	const float Margin = 40.f * S;
	const float Bottom = H - 26.f * S;
	const float RowH = 52.f * S;
	const float Pad = 16.f * S;

	// --- ДЕНЬГИ ---
	const FString MoneyStr = FString::Printf(TEXT("$ %d"), Player->Money);
	float MW = 0.f, MH = 0.f;
	GetTextSizeScaled(MoneyStr, MW, MH, Big, 1.5f);

	const float MoneyH = 38.f * S;
	// ширина по содержимому, но не уже минимума: иначе плашка прыгает
	// на каждую покупку
	const float MoneyW = FMath::Max(MW + Pad * 2.f + 8.f * S, 160.f * S);
	const float MoneyY = Bottom - RowH - 8.f * S - MoneyH;

	DrawPanel(Margin, MoneyY, MoneyW, MoneyH, ColCSGreen);
	DrawTextScaled(MoneyStr, ColCSGreen, Margin + Pad, MoneyY + (MoneyH - MH) * 0.5f, Big, 1.5f);

	// --- ЗДОРОВЬЕ И БРОНЯ ---
	const int32 HP = FMath::Max(0, FMath::RoundToInt(Player->Health));
	const int32 AP = FMath::RoundToInt(Player->Armor);
	const FLinearColor HPCol = HP > 30 ? ColWhite : ColRed;

	const FString HPStr = FString::Printf(TEXT("%d"), HP);
	float HW = 0.f, HH = 0.f;
	GetTextSizeScaled(HPStr, HW, HH, Big, 2.2f);

	const float HPW = 200.f * S;
	const float HPY = Bottom - RowH;
	const float BarH = 4.f * S;

	DrawPanel(Margin, HPY, HPW, RowH, HPCol);
	DrawTextScaled(HPStr, HPCol, Margin + Pad, HPY + (RowH - HH) * 0.5f - BarH, Big, 2.2f);

	if (AP > 0)
	{
		// броня прижата к правому краю плашки, а не приклеена к цифре HP:
		// раньше она ездила вместе с шириной числа (100 против 7)
		const FString APStr = FString::Printf(TEXT("%d"), AP);
		float AW = 0.f, AH = 0.f;
		GetTextSizeScaled(APStr, AW, AH, Big, 1.4f);
		DrawTextScaled(APStr, ColCT, Margin + HPW - AW - Pad,
			HPY + (RowH - AH) * 0.5f - BarH, Big, 1.4f);
	}

	// полоса здоровья по нижнему краю плашки, во всю её ширину
	const float HPFrac = FMath::Clamp(HP / 100.f, 0.f, 1.f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), Margin, HPY + RowH - BarH, HPW, BarH);
	DrawRect(HPCol, Margin, HPY + RowH - BarH, HPW * HPFrac, BarH);

	// --- ПАТРОНЫ ---
	const float AmmoW = 210.f * S;
	const float AmmoX = W - Margin - AmmoW;
	const float AmmoY = Bottom - RowH;

	const bool bGrenade = RSWeapons::IsGrenade(Player->CurrentWeapon);
	const int32 InMag = Player->GetAmmo();
	// Порог «мало патронов» считается от размера магазина, а не жёсткими
	// четырьмя штуками: у AWP с магазином на 10 четыре патрона — это ещё
	// не тревога, а у пистолета на 20 — уже да.
	const int32 MaxMag = Player->GetMaxAmmo();
	const bool bLow = !bGrenade && MaxMag > 0
		&& InMag <= FMath::Max(2, FMath::RoundToInt(MaxMag * 0.25f));
	const FLinearColor AmmoAccent = Player->bReloading ? ColGold : (bLow ? ColRed : ColWhite);

	DrawPanel(AmmoX, AmmoY, AmmoW, RowH, AmmoAccent);

	// Название над плашкой. Отступ считается от собственной высоты строки,
	// а не фиксированными 20 пикселями: на малом масштабе подпись садилась
	// на верхнюю кромку панели.
	const FString WeaponLabel = Player->GetWeaponName().ToUpper();
	float LblW = 0.f, LblH = 0.f;
	GetTextSizeScaled(WeaponLabel, LblW, LblH, Small, 1.15f);
	DrawTextScaled(WeaponLabel, ColDim, AmmoX + Pad, AmmoY - LblH - 6.f * S, Small, 1.15f);
	// иконка выравнивается по низу с подписью, а не по своему отступу
	DrawWeaponIcon(Canvas, AmmoX + AmmoW - 80.f * S, AmmoY - LblH - 22.f * S,
		72.f * S, 24.f * S, Player->CurrentWeapon, ColWhite);

	if (Player->bReloading)
	{
		DrawTextCentered(TEXT("ПЕРЕЗАРЯДКА"), ColGold,
			AmmoX + AmmoW * 0.5f, AmmoY + RowH * 0.5f, Big, 1.3f);
		return;
	}

	if (bGrenade)
	{
		const int32 GI = RSWeapons::GrenadeIndex(Player->CurrentWeapon);
		DrawTextCentered(FString::Printf(TEXT("x %d"), Player->Grenades[GI]), ColWhite,
			AmmoX + AmmoW * 0.5f, AmmoY + RowH * 0.5f, Big, 2.0f);
		return;
	}
	if (Player->GetMaxAmmo() == 0)
	{
		DrawTextCentered(TEXT("—"), ColWhite,
			AmmoX + AmmoW * 0.5f, AmmoY + RowH * 0.5f, Big, 2.0f);
		return;
	}

	// В магазине — крупно, запас — мельче и приглушённо, обе строки по
	// одной базовой линии. Раньше запас стоял на фиксированном сдвиге +60
	// и налезал на число, когда в магазине было три цифры.
	const FLinearColor MagCol = bLow ? ColRed : ColWhite;
	const FString MagStr = FString::Printf(TEXT("%d"), InMag);
	const FString ResStr = FString::Printf(TEXT("/ %d"), Player->GetReserveAmmo());

	float MagW = 0.f, MagH = 0.f, ResW = 0.f, ResH = 0.f;
	GetTextSizeScaled(MagStr, MagW, MagH, Big, 2.2f);
	GetTextSizeScaled(ResStr, ResW, ResH, Small, 1.6f);

	const float BaseY = AmmoY + (RowH - MagH) * 0.5f;
	DrawTextScaled(MagStr, MagCol, AmmoX + Pad, BaseY, Big, 2.2f);
	DrawTextScaled(ResStr, ColDim, AmmoX + Pad + MagW + 10.f * S,
		BaseY + (MagH - ResH), Small, 1.6f);
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

	const float S = UIScale();
	const float CX = Canvas->SizeX * 0.5f;
	const float Y = 12.f * S;

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
	const float CardW = 38.f * S;
	const float CardH = 42.f * S;
	const float CardGap = 4.f * S;
	const float CenterW = 132.f * S;
	// Третьей строки больше нет, поэтому плашка стала ниже: с прежней
	// высотой под счётом оставалась пустая полоса.
	const float CenterH = CardH + 8.f * S;
	const float SideGap = 10.f * S;
	// карточки отталкиваются от края центральной плашки, а не от жёстких
	// 65 пикселей: иначе при другом масштабе они на неё налезали
	float CTX = CX - CenterW * 0.5f - SideGap - (5 * (CardW + CardGap));

	for (int32 i = 0; i < 5; i++)
	{
		const bool bAlive = i < AliveCT;
		const FLinearColor CardBg = bAlive ? FLinearColor(0.12f, 0.28f, 0.52f, 0.90f) : FLinearColor(0.04f, 0.05f, 0.07f, 0.65f);
		DrawRect(CardBg, CTX, Y, CardW, CardH);
		DrawRect(ColCT, CTX, Y, CardW, 4.f * S);

		DrawTextCentered(bAlive ? TEXT("CT") : TEXT("X"),
			bAlive ? ColCT : FLinearColor(0.4f, 0.4f, 0.4f),
			CTX + CardW * 0.5f, Y + CardH * 0.5f + 2.f * S, Font, 1.2f);
		CTX += CardW + CardGap;
	}

	// Центральный таймер и счёт
	const float CenterX = CX - CenterW * 0.5f;
	DrawRect(ColBgDark, CenterX, Y, CenterW, CenterH);
	DrawBoxOutline(CenterX, Y, CenterW, CenterH, ColBorder, FMath::Max(1.f, 1.5f * S));

	const int32 Left = FMath::CeilToInt(State->GetTimeLeft());
	const FString TimerStr = FString::Printf(TEXT("%d:%02d"), Left / 60, Left % 60);
	const bool bUrgent = State->Phase == ERSPhase::Live && Left <= 10;

	// Три строки центрируются по середине плашки. Раньше каждая стояла на
	// своём сдвиге (+26, +40, +41), подобранном под конкретную ширину строки,
	// и таймер «10:05» вылезал за рамку, а нижняя строка обрезалась.
	DrawTextCentered(TimerStr, bUrgent ? ColRed : ColWhite,
		CX, Y + 15.f * S, Med, 1.5f);

	const FString ScoreStr = FString::Printf(TEXT("%d   %d"), State->ScoreCT, State->ScoreT);
	DrawTextCentered(ScoreStr, ColWhite, CX, Y + 36.f * S, Font, 1.3f);
	// Строку «живые по сторонам» убрали по просьбе: то же самое уже видно
	// по карточкам CT и T слева и справа, а третья строка только жала
	// таймер со счётом.

	// 5 Крупных карточек террористов (T) справа
	float TX = CX + CenterW * 0.5f + SideGap;
	for (int32 i = 0; i < 5; i++)
	{
		const bool bAlive = i < AliveT;
		const FLinearColor CardBg = bAlive ? FLinearColor(0.52f, 0.36f, 0.12f, 0.90f) : FLinearColor(0.04f, 0.05f, 0.07f, 0.65f);
		DrawRect(CardBg, TX, Y, CardW, CardH);
		DrawRect(ColT, TX, Y, CardW, 4.f * S);

		DrawTextCentered(bAlive ? TEXT("T") : TEXT("X"),
			bAlive ? ColT : FLinearColor(0.4f, 0.4f, 0.4f),
			TX + CardW * 0.5f, Y + CardH * 0.5f + 2.f * S, Font, 1.2f);
		TX += CardW + CardGap;
	}

	// Плашка времени закупки
	if (State->Phase == ERSPhase::Intermission && !Player->bBuyMenuOpen)
	{
		const FString BuyMsg = FString::Printf(TEXT("ВРЕМЯ ЗАКУПКИ (%d с) — Удерживай [ B ]"), Left);
		float TW = 0.f, TH = 0.f;
		GetTextSizeScaled(BuyMsg, TW, TH, Font, 1.3f);
		const float BW = TW + 32.f * S;
		const float BH = TH + 12.f * S;
		const float BX = CX - BW * 0.5f;
		const float BY = Y + CenterH + 14.f * S;

		DrawRect(FLinearColor(0.04f, 0.05f, 0.07f, 0.90f), BX, BY, BW, BH);
		DrawBoxOutline(BX, BY, BW, BH, ColGold, FMath::Max(1.f, 1.5f * S));
		DrawTextCentered(BuyMsg, ColGold, CX, BY + BH * 0.5f, Font, 1.3f);
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

	const float S = UIScale();
	const float W = 780.f * S;
	const float X = Canvas->SizeX * 0.5f - W * 0.5f;
	const float Top = Canvas->SizeY * 0.15f;
	const float RowStep = 24.f * S;
	const float HalfW = W * 0.5f - 20.f * S;
	// Числовые колонки прижаты к правому краю своей половины, имени достаётся
	// всё остальное. Раньше все три стояли на отступах 150/195/240 от начала
	// колонки: имя зажималось в 150 пикселей и обрезалось многоточием, а
	// справа пустовала треть таблицы.
	const float ColKRight = HalfW - 150.f * S;
	const float ColDRight = HalfW - 95.f * S;
	const float ColMRight = HalfW;
	const float NameW = ColKRight - 30.f * S;

	// правое выравнивание: числа в таблице должны стоять колонкой
	auto DrawRight = [&](const FString& Text, const FLinearColor& Color,
		float RightX, float TextY, float TextScale)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSizeScaled(Text, TW, TH, Font, TextScale);
		DrawTextScaled(Text, Color, RightX - TW, TextY, Font, TextScale);
	};

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

	// Высота панели по фактическому числу строк, а не константой 440
	const int32 MaxRows = FMath::Max(Columns[0].Num(), Columns[1].Num());
	const float PanelH = (30.f + 26.f + 22.f) * S + MaxRows * RowStep + 40.f * S;
	DrawRect(ColBgSolid, X - 20.f * S, Top - 20.f * S, W + 40.f * S, PanelH);
	DrawBoxOutline(X - 20.f * S, Top - 20.f * S, W + 40.f * S, PanelH, ColBorder, FMath::Max(1.f, 1.5f * S));

	if (State)
	{
		const FString Title = FString::Printf(TEXT("CT  %d : %d  T     РАУНД %d / %d"),
			State->ScoreCT, State->ScoreT, State->RoundNumber, State->RoundsTotal);
		DrawTextCentered(Title, ColGold, Canvas->SizeX * 0.5f, Top - 10.f * S + 8.f * S, Med, 1.5f);
	}

	for (int32 Side = 0; Side < 2; Side++)
	{
		const float ColX = X + Side * (W * 0.5f + 10.f * S);
		float Y = Top + 30.f * S;

		DrawTextScaled(Titles[Side], TeamColors[Side], ColX, Y, Font, 1.4f);
		Y += 26.f * S;

		// Шапка колонок раньше была одной строкой с выравниванием пробелами,
		// а данные стояли на числовых отступах. С пропорциональным шрифтом
		// это не совпадало никогда, а с широким разъехалось окончательно.
		const FLinearColor HeadCol(1.f, 1.f, 1.f, 0.45f);
		DrawTextScaled(TEXT("ИМЯ"), HeadCol, ColX, Y, Font, 1.1f);
		DrawRight(TEXT("У"), HeadCol, ColX + ColKRight, Y, 1.1f);
		DrawRight(TEXT("С"), HeadCol, ColX + ColDRight, Y, 1.1f);
		DrawRight(TEXT("$"), HeadCol, ColX + ColMRight, Y, 1.1f);
		Y += 22.f * S;

		for (const FRow& Row : Columns[Side])
		{
			const FLinearColor Color = !Row.bAlive
				? FLinearColor(0.45f, 0.45f, 0.45f)
				: (Row.bYou ? ColGold : ColWhite);

			// Длинный ник обрезаем: иначе он наезжает на колонку убийств.
			// Ник задаётся игроком и ничем не ограничен.
			FString Name = Row.Name;
			float NW = 0.f, NH = 0.f;
			GetTextSizeScaled(Name, NW, NH, Font, 1.25f);
			while (NW > NameW && Name.Len() > 3)
			{
				Name.LeftInline(Name.Len() - 1, EAllowShrinking::No);
				GetTextSizeScaled(Name + TEXT("…"), NW, NH, Font, 1.25f);
			}
			if (Name.Len() != Row.Name.Len())
			{
				Name += TEXT("…");
			}

			DrawTextScaled(Name, Color, ColX, Y, Font, 1.25f);
			DrawRight(FString::Printf(TEXT("%d"), Row.Kills), Color, ColX + ColKRight, Y, 1.25f);
			DrawRight(FString::Printf(TEXT("%d"), Row.Deaths), Color, ColX + ColDRight, Y, 1.25f);
			if (Row.Money >= 0)
			{
				DrawRight(FString::Printf(TEXT("%d"), Row.Money), Color, ColX + ColMRight, Y, 1.25f);
			}
			Y += RowStep;
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
	const float S = UIScale();
	const float ColGap = 12.f * S;
	const float HeaderH = 30.f * S;

	// Карточка делится на три зоны, и все они считаются от измеренного
	// текста: название сверху, иконка в середине, цена снизу. Раньше зоны
	// стояли на фиксированных отступах, и при широком шрифте название
	// ложилось прямо на силуэт ствола.
	float ProbeW = 0.f, NameH = 0.f, PriceH = 0.f;
	GetTextSizeScaled(TEXT("Ap"), ProbeW, NameH, Font, 1.1f);
	GetTextSizeScaled(TEXT("$0"), ProbeW, PriceH, Font, 1.05f);
	// Высота зоны иконки — единственное, что задаёт их размер: при вписывании
	// по пропорциям всё упирается именно в неё. Квадратные силуэты (гранаты,
	// броня) получают ровно IconH по обеим сторонам, поэтому на низкой зоне
	// они выглядели крошечными рядом с вытянутыми стволами.
	const float IconH = 42.f * S;
	// зазоры по 6 пикселей: при четырёх иконка касалась названия, а
	// «КУПЛЕНО» подрезалось нижней кромкой карточки
	const float Gap = 6.f * S;
	const float CardH = 4.f * S + NameH + Gap + IconH + Gap + PriceH + Gap;
	const float CardStep = CardH + 8.f * S;
	const float IconY = 4.f * S + NameH + Gap;    // от верха карточки
	const float PriceY = CardH - PriceH - Gap;

	// Ширину колонки считаем по самому длинному тексту, который в неё попадёт.
	// Раньше стояли жёсткие 160 px, подобранные под встроенный шрифт; у
	// векторного глифы шире, и «ПП и дробовики» с «Снайперские» вылезали за
	// карточку, а названия стволов наезжали на иконки.
	float ColW = 150.f * S;
	{
		auto Fit = [&](const FString& Text, float TextScale, float LeftPad)
		{
			float TW = 0.f, TH = 0.f;
			GetTextSizeScaled(Text, TW, TH, Font, TextScale);
			ColW = FMath::Max(ColW, LeftPad + TW + 10.f * S);
		};
		for (int32 c = 0; c < NumCols; c++)
		{
			Fit(FString::Printf(TEXT("%d  %s"), c + 1, RSWeapons::BuyCategoryName(c)), 1.2f, 8.f * S);
			if (c == RSWeapons::EquipmentCategory)
			{
				Fit(TEXT("Кевлар + шлем"), 1.1f, 20.f * S);
				continue;
			}
			for (ERSWeapon Wp : RSWeapons::BuyCategory(c, Player->Team))
			{
				Fit(RSWeapons::Get(Wp).Name, 1.1f, 20.f * S);
			}
		}
	}

	const float TotalW = NumCols * ColW + (NumCols - 1) * ColGap;
	const float StartX = Canvas->SizeX * 0.5f - TotalW * 0.5f;
	const float StartY = Canvas->SizeY * 0.16f;

	// Высота панели — по самой длинной колонке. При константе 500 нижняя
	// подсказка упиралась в край панели и обрезалась.
	int32 MaxItems = 2;
	for (int32 c = 0; c < NumCols; c++)
	{
		if (c == RSWeapons::EquipmentCategory)
		{
			continue;
		}
		MaxItems = FMath::Max(MaxItems,
			FMath::Min(RSWeapons::BuyCategory(c, Player->Team).Num(), 6));
	}
	const float ContentH = HeaderH + 8.f * S + MaxItems * CardStep;
	const float HintY = StartY + ContentH + 14.f * S;
	const float PanelTop = StartY - 45.f * S;
	const float PanelH = HintY + 30.f * S - PanelTop;

	DrawRect(ColBgSolid, StartX - 24.f * S, PanelTop, TotalW + 48.f * S, PanelH);
	DrawBoxOutline(StartX - 24.f * S, PanelTop, TotalW + 48.f * S, PanelH, ColBorder, FMath::Max(1.f, 1.5f * S));

	const int32 Left = FMath::CeilToInt(State->GetTimeLeft());
	DrawTextScaled(FString::Printf(TEXT("До конца закупки  %02d:%02d"), Left / 60, Left % 60),
		ColDim, StartX, StartY - 35.f * S, Font, 1.3f);

	// сумма прижата к правому краю по факту измерения, а не сдвигом на 100
	const FString MoneyStr = FString::Printf(TEXT("$ %d"), Player->Money);
	float MnW = 0.f, MnH = 0.f;
	GetTextSizeScaled(MoneyStr, MnW, MnH, Med, 1.5f);
	DrawTextScaled(MoneyStr, ColCSGreen, StartX + TotalW - MnW, StartY - 35.f * S, Med, 1.5f);

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
		Header.Max = FVector2D(CX + ColW, CY + HeaderH);
		Header.Category = c;
		Header.Kind = -2;
		BuyHotspots.Add(Header);

		const bool bHoverHeader = bHasMouse && Header.Contains(Mouse);
		DrawRect((bActive || bHoverHeader) ? FLinearColor(0.20f, 0.16f, 0.03f, 0.95f)
			: FLinearColor(0.04f, 0.05f, 0.07f, 0.92f), CX, CY, ColW, HeaderH);
		if (bActive || bHoverHeader)
		{
			DrawBoxOutline(CX, CY, ColW, HeaderH, ColGold, FMath::Max(1.f, 1.5f * S));
		}
		{
			const FString HeadStr = FString::Printf(TEXT("%d  %s"), c + 1, RSWeapons::BuyCategoryName(c));
			float HdW = 0.f, HdH = 0.f;
			GetTextSizeScaled(HeadStr, HdW, HdH, Font, 1.2f);
			DrawTextScaled(HeadStr, ColWhite, CX + 8.f * S, CY + (HeaderH - HdH) * 0.5f, Font, 1.2f);
		}
		CY += HeaderH + 8.f * S;

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
				Spot.Max = FVector2D(CX + ColW, CY + CardH);
				Spot.Category = c;
				Spot.Kind = (int8)i; // 0 кевлар, 1 кевлар со шлемом
				BuyHotspots.Add(Spot);

				const bool bHover = bHasMouse && Spot.Contains(Mouse);
				FLinearColor CardBg = EItems[i].bOwned ? FLinearColor(0.1f, 0.35f, 0.15f, 0.65f) : FLinearColor(0.06f, 0.08f, 0.10f, 0.80f);
				if (bHover && bCan)
				{
					CardBg = FLinearColor(0.12f, 0.22f, 0.14f, 0.95f);
				}

				DrawRect(CardBg, CX, CY, ColW, CardH);
				DrawBoxOutline(CX, CY, ColW, CardH, bCan ? ColCSGreen : ColBorder,
					bHover ? FMath::Max(1.5f, 2.f * S) : FMath::Max(1.f, S));

				DrawTextScaled(FString::Printf(TEXT("%d"), i + 1), ColDim, CX + 6.f * S, CY + 4.f * S, Font, 1.0f);
				DrawTextScaled(EItems[i].Name, bCan ? ColWhite : ColDim, CX + 20.f * S, CY + 4.f * S, Font, 1.1f);
				// Иконка живёт в средней зоне, как у оружия: раньше она
				// стояла справа во всю высоту и цена ложилась на неё.
				// Бокс шире квадрата — кевлар вытянут по вертикали, и в
				// квадрате его ограничивала бы высота вдвое сильнее нужного.
				DrawEquipIcon(Canvas, CX + ColW - IconH * 1.6f - 8.f * S, CY + IconY,
					IconH * 1.6f, IconH,
					(i == 0) ? RSIcons::Kevlar() : RSIcons::Helmet(),
					bCan ? ColWhite : FLinearColor(0.65f, 0.65f, 0.65f, 0.75f));

				if (EItems[i].bOwned)
				{
					DrawTextScaled(TEXT("КУПЛЕНО"), ColCSGreen, CX + 8.f * S, CY + PriceY, Font, 1.05f);
				}
				else
				{
					// цена прижата к правому краю карточки по измерению
					const FString PriceStr = FString::Printf(TEXT("$%d"), EItems[i].Price);
					float PW = 0.f, PH = 0.f;
					GetTextSizeScaled(PriceStr, PW, PH, Font, 1.05f);
					DrawTextScaled(PriceStr, bCan ? ColCSGreen : ColDim,
						CX + ColW - PW - 8.f * S, CY + PriceY, Font, 1.05f);
				}
				CY += CardStep;
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
			Spot.Max = FVector2D(CX + ColW, CY + CardH);
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
			DrawRect(CardBg, CX, CY, ColW, CardH);
			DrawBoxOutline(CX, CY, ColW, CardH, bCan ? ColCSGreen : ColBorder,
				bHover ? FMath::Max(1.5f, 2.f * S) : FMath::Max(1.f, S));

			DrawTextScaled(FString::Printf(TEXT("%d"), i + 1), ColDim, CX + 6.f * S, CY + 4.f * S, Font, 1.0f);
			DrawTextScaled(Def.Name, bCan ? ColWhite : ColDim, CX + 20.f * S, CY + 4.f * S, Font, 1.1f);

			// Иконка живёт в своей зоне между названием и ценой: при широком
			// шрифте она начиналась на той же высоте, что и текст, и силуэт
			// ствола перечёркивал название.
			DrawWeaponIcon(Canvas, CX + 8.f * S, CY + IconY, ColW - 16.f * S, IconH, Items[i],
				bCan ? ColWhite : FLinearColor(0.65f, 0.65f, 0.65f, 0.75f));

			if (bOwned)
			{
				DrawTextScaled(TEXT("КУПЛЕНО"), ColCSGreen, CX + 8.f * S, CY + PriceY, Font, 1.0f);
			}
			else
			{
				const FString PriceStr = FString::Printf(TEXT("$%d"), Def.Price);
				float PW = 0.f, PH = 0.f;
				GetTextSizeScaled(PriceStr, PW, PH, Font, 1.05f);
				DrawTextScaled(PriceStr, bCan ? ColCSGreen : ColDim,
					CX + ColW - PW - 8.f * S, CY + PriceY, Font, 1.05f);
			}
			CY += CardStep;
		}
	}

	// подсказка меняется по шагу покупки, чтобы цифры не путались
	const FString Hint = (Chosen < 0)
		? FString(TEXT("ЛКМ — купить      цифра 1-6 — категория      отпусти B — закрыть"))
		: FString::Printf(TEXT("ЛКМ — купить      «%s»: цифра — купить      [0] назад      отпусти B — закрыть"),
			RSWeapons::BuyCategoryName(Chosen));
	DrawTextScaled(Hint, ColDim, StartX, HintY, Font, 1.15f);

	if (!bBuyTime)
	{
		DrawTextScaled(TEXT("Покупать можно только между раундами"), ColRed,
			StartX, HintY - 22.f * S, Font, 1.15f);
	}
}
