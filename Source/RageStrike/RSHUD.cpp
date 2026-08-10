#include "RSHUD.h"
#include "RSCharacter.h"
#include "RSBot.h"
#include "RSGameMode.h"
#include "RSGameState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"

// палитра в духе CS2
namespace
{
	const FLinearColor ColPanel(0.04f, 0.05f, 0.06f, 0.6f);
	const FLinearColor ColCT(0.42f, 0.64f, 1.f);
	const FLinearColor ColT(0.94f, 0.71f, 0.25f);
	const FLinearColor ColWhite(0.92f, 0.94f, 0.96f);
	const FLinearColor ColDim(0.92f, 0.94f, 0.96f, 0.45f);
	const FLinearColor ColMoney(0.55f, 0.9f, 0.45f);
	const FLinearColor ColRed(1.f, 0.25f, 0.2f);

	FLinearColor TeamColor(uint8 Team)
	{
		return Team == (uint8)ERSTeam::CT ? ColCT : ColT;
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
	DrawCheatPanel(Player);
	DrawKillFeed(Player);
	DrawHealthArmor(Player);
	DrawAmmo(Player);
	DrawMoney(Player);
	DrawRoundInfo(Player);
	DrawBuyMenu(Player);
	DrawScoreboard(Player);

	const float Now = GetWorld()->GetTimeSeconds();

	// хитмаркер
	if (Now - Player->LastHitMarkerTime < 0.15f)
	{
		const float CX = Canvas->SizeX * 0.5f;
		const float CY = Canvas->SizeY * 0.5f;
		const FLinearColor C = FLinearColor::White;
		DrawLine(CX - 14.f, CY - 14.f, CX - 6.f, CY - 6.f, C, 2.f);
		DrawLine(CX + 14.f, CY - 14.f, CX + 6.f, CY - 6.f, C, 2.f);
		DrawLine(CX - 14.f, CY + 14.f, CX - 6.f, CY + 6.f, C, 2.f);
		DrawLine(CX + 14.f, CY + 14.f, CX + 6.f, CY + 6.f, C, 2.f);
	}

	// красная вспышка при получении урона
	if (Now - Player->LastDamagedTime < 0.3f)
	{
		DrawRect(FLinearColor(1.f, 0.f, 0.f, 0.12f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
	}

	// флешка: белая пелена, спадающая со временем
	if (Now < Player->FlashEndTime)
	{
		const float Left = (Player->FlashEndTime - Now) / FMath::Max(0.3f, Player->FlashDuration);
		const float Alpha = FMath::Clamp(Left * Left * 1.6f, 0.f, 1.f);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, Alpha), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
	}

	// подсказка снизу
	DrawText(TEXT("1/2/3/4 - слоты (4 - гранаты) | ПКМ с гранатой - подкат | G - выбросить | B - закупка | Shift - ходьба | F - осмотр | ПКМ - оптика | V - вид | R - перезарядка"),
		FLinearColor(1.f, 1.f, 1.f, 0.35f), 30.f, Canvas->SizeY - 24.f, GEngine->GetSmallFont(), 1.f);
}

void ARSHUD::DrawCrosshair(const ARSCharacter* Player)
{
	// со снайперкой без прицела перекрестия нет, как в CS
	if (RSWeapons::Get(Player->CurrentWeapon).Mesh == ERSMeshKind::Sniper)
	{
		return;
	}

	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	// перекрестие расходится от разброса: бег и прыжки видно сразу
	const float Gap = 4.f + Player->CurrentSpreadDeg * 7.f;
	const float Len = 8.f;
	const FLinearColor C(0.2f, 1.f, 0.2f, 0.9f);

	DrawLine(CX - Gap - Len, CY, CX - Gap, CY, C, 2.f);
	DrawLine(CX + Gap, CY, CX + Gap + Len, CY, C, 2.f);
	DrawLine(CX, CY - Gap - Len, CX, CY - Gap, C, 2.f);
	DrawLine(CX, CY + Gap, CX, CY + Gap + Len, C, 2.f);
}

void ARSHUD::DrawSniperScope(const ARSCharacter* Player)
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;
	const float R = FMath::Min(W, H) * 0.48f;

	// чёрные шторки вокруг круга оптики
	DrawRect(FLinearColor::Black, 0.f, 0.f, CX - R, H);
	DrawRect(FLinearColor::Black, CX + R, 0.f, W - CX - R, H);
	DrawRect(FLinearColor::Black, CX - R, 0.f, 2.f * R, CY - R);
	DrawRect(FLinearColor::Black, CX - R, CY + R, 2.f * R, H - CY - R);

	// кольцо из сегментов
	const int32 Seg = 64;
	for (int32 i = 0; i < Seg; i++)
	{
		const float A0 = 2.f * PI * i / Seg;
		const float A1 = 2.f * PI * (i + 1) / Seg;
		DrawLine(CX + R * FMath::Cos(A0), CY + R * FMath::Sin(A0),
			CX + R * FMath::Cos(A1), CY + R * FMath::Sin(A1), FLinearColor::Black, 6.f);
	}

	// тонкие нити прицела
	DrawLine(CX - R, CY, CX + R, CY, FLinearColor::Black, 1.5f);
	DrawLine(CX, CY - R, CX, CY + R, FLinearColor::Black, 1.5f);
}

void ARSHUD::DrawBoxOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	DrawLine(X, Y, X + W, Y, Color, Thickness);
	DrawLine(X, Y + H, X + W, Y + H, Color, Thickness);
	DrawLine(X, Y, X, Y + H, Color, Thickness);
	DrawLine(X + W, Y, X + W, Y + H, Color, Thickness);
}

void ARSHUD::DrawESP(const ARSCharacter* Player)
{
	APawn* OwnPawn = GetOwningPawn();

	// собираем всех противников: и ботов, и живых игроков
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

		// проецируем 8 углов баунд-бокса
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

		// полоска HP слева от бокса
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

		// снаплайн от низа экрана
		DrawLine(Canvas->SizeX * 0.5f, Canvas->SizeY, MinX + W * 0.5f, MaxY, FLinearColor(1.f, 1.f, 1.f, 0.35f), 1.f);

		// дистанция
		if (OwnPawn)
		{
			const int32 Meters = FMath::RoundToInt(FVector::Dist(OwnPawn->GetActorLocation(), Origin) / 100.f);
			DrawText(FString::Printf(TEXT("%dm"), Meters), FLinearColor::White, MinX, MinY - 16.f, GEngine->GetSmallFont(), 1.1f);
		}
	}
}

void ARSHUD::DrawRadar(const ARSCharacter* Player)
{
	// радар как в CS2: свой всегда в центре, карта крутится вместе с игроком.
	// союзники видны всегда, противники — только с включённым ESP
	const float Size = 170.f;
	const float X = 20.f, Y = 20.f;
	const float CX = X + Size * 0.5f;
	const float CY = Y + Size * 0.5f;
	const float Range = 5000.f; // 50 м до края

	DrawRect(ColPanel, X, Y, Size, Size);
	DrawBoxOutline(X, Y, Size, Size, FLinearColor(1.f, 1.f, 1.f, 0.15f), 1.f);
	// кольца дальности
	DrawBoxOutline(X + Size * 0.25f, Y + Size * 0.25f, Size * 0.5f, Size * 0.5f,
		FLinearColor(1.f, 1.f, 1.f, 0.07f), 1.f);

	const FVector MyLoc = Player->GetActorLocation();
	const float Yaw = FMath::DegreesToRadians(Player->GetControlRotation().Yaw);
	const FVector2D Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw));
	const FVector2D Right(-FMath::Sin(Yaw), FMath::Cos(Yaw));

	auto Plot = [&](const AActor* Who, uint8 Team, bool bEnemy)
	{
		const FVector Rel3 = Who->GetActorLocation() - MyLoc;
		const FVector2D Rel(Rel3.X, Rel3.Y);
		float U = FVector2D::DotProduct(Rel, Right) / Range * (Size * 0.5f);
		float V = -FVector2D::DotProduct(Rel, Fwd) / Range * (Size * 0.5f);
		U = FMath::Clamp(U, -Size * 0.5f + 4.f, Size * 0.5f - 4.f);
		V = FMath::Clamp(V, -Size * 0.5f + 4.f, Size * 0.5f - 4.f);
		DrawRect(bEnemy ? ColRed : TeamColor(Team), CX + U - 2.5f, CY + V - 2.5f, 5.f, 5.f);
	};

	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It) || *It == Player || !It->bAlive)
		{
			continue;
		}
		const bool bEnemy = It->Team != Player->Team;
		if (!bEnemy || Player->bESP)
		{
			Plot(*It, (uint8)It->Team, bEnemy);
		}
	}
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It) || It->Health <= 0.f)
		{
			continue;
		}
		const bool bEnemy = It->Team != Player->Team;
		if (!bEnemy || Player->bESP)
		{
			Plot(*It, (uint8)It->Team, bEnemy);
		}
	}

	// свой маркер — стрелка вверх (мир повёрнут под взгляд)
	DrawLine(CX, CY - 6.f, CX - 4.f, CY + 4.f, FLinearColor::White, 2.f);
	DrawLine(CX, CY - 6.f, CX + 4.f, CY + 4.f, FLinearColor::White, 2.f);
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
	const float Right = Canvas->SizeX - 250.f; // левее раге-панели
	float Y = 40.f;

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
		GetTextSize(E.Killer, KW, KH, Font, 1.15f);
		GetTextSize(WeaponPart, WW, WH, Font, 1.15f);
		GetTextSize(E.Victim, VW, VH, Font, 1.15f);

		const float TotalW = KW + WW + VW + 16.f;
		const float X = Right - TotalW;

		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), X - 6.f, Y - 3.f, TotalW + 12.f, 22.f);

		FLinearColor KC = TeamColor(E.KillerTeam); KC.A = Alpha;
		FLinearColor VC = TeamColor(E.VictimTeam); VC.A = Alpha;
		FLinearColor WC = ColWhite; WC.A = Alpha;
		if (E.bHeadshot)
		{
			WC = FLinearColor(1.f, 0.3f, 0.2f, Alpha);
		}

		DrawText(E.Killer, KC, X, Y, Font, 1.15f);
		DrawText(WeaponPart, WC, X + KW, Y, Font, 1.15f);
		DrawText(E.Victim, VC, X + KW + WW, Y, Font, 1.15f);
		Y += 25.f;
	}
}

void ARSHUD::DrawCheatPanel(const ARSCharacter* Player)
{
	struct FCheatRow { const TCHAR* Name; bool bOn; };
	const FCheatRow Rows[] =
	{
		{ TEXT("[F1] Aimbot"),        Player->bAimbot },
		{ TEXT("[F2] ESP / WH"),      Player->bESP },
		{ TEXT("[F3] Triggerbot"),    Player->bTriggerbot },
		{ TEXT("[F4] NoRecoil"),      Player->bNoRecoilSpread },
		{ TEXT("[F5] Speed + BHop"),  Player->bSpeedhack },
		{ TEXT("[F6] Silent Aim"),    Player->bSilentAim },
		{ TEXT("[F7] GodMode"),       Player->bGodMode },
	};

	const float X = Canvas->SizeX - 230.f;
	float Y = 40.f;

	DrawRect(ColPanel, X - 12.f, Y - 30.f, 225.f, 30.f + 22.f * UE_ARRAY_COUNT(Rows) + 10.f);
	DrawText(TEXT("RAGE MENU"), FLinearColor(1.f, 0.2f, 0.2f), X, Y - 24.f, GEngine->GetSmallFont(), 1.4f);

	for (const FCheatRow& Row : Rows)
	{
		DrawText(Row.Name, ColWhite, X, Y, GEngine->GetSmallFont(), 1.15f);
		DrawText(Row.bOn ? TEXT("ON") : TEXT("OFF"),
			Row.bOn ? FLinearColor(0.1f, 1.f, 0.1f) : FLinearColor(0.6f, 0.6f, 0.6f),
			X + 155.f, Y, GEngine->GetSmallFont(), 1.15f);
		Y += 22.f;
	}
}

void ARSHUD::DrawHealthArmor(const ARSCharacter* Player)
{
	// нижний левый угол, как в CS2: здоровье с полосой, под ним броня
	UFont* Big = GEngine->GetMediumFont();
	const float H = Canvas->SizeY;
	const float X = 24.f;
	const float PanelW = 240.f;

	DrawRect(ColPanel, X - 8.f, H - 96.f, PanelW, 72.f);

	const int32 HP = FMath::Max(0, FMath::RoundToInt(Player->Health));
	const FLinearColor HPCol = HP > 30 ? ColWhite : ColRed;

	// крестик здоровья
	DrawRect(HPCol, X + 6.f, H - 84.f, 4.f, 14.f);
	DrawRect(HPCol, X + 1.f, H - 79.f, 14.f, 4.f);
	DrawText(FString::Printf(TEXT("%d"), HP), HPCol, X + 24.f, H - 90.f, Big, 1.5f);

	// полоса здоровья
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), X + 80.f, H - 80.f, 140.f, 7.f);
	DrawRect(HPCol, X + 80.f, H - 80.f, 140.f * HP / 100.f, 7.f);

	// броня: щиток + число + полоса
	const int32 AP = FMath::RoundToInt(Player->Armor);
	const FLinearColor APCol = AP > 0 ? ColCT : FLinearColor(1.f, 1.f, 1.f, 0.25f);
	DrawBoxOutline(X + 3.f, H - 58.f, 10.f, 12.f, APCol, 2.f);
	DrawText(FString::Printf(TEXT("%d%s"), AP, Player->bHasHelmet ? TEXT(" +шлем") : TEXT("")),
		APCol, X + 24.f, H - 60.f, GEngine->GetSmallFont(), 1.4f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), X + 80.f, H - 52.f, 140.f, 5.f);
	DrawRect(ColCT, X + 80.f, H - 52.f, 140.f * AP / 100.f, 5.f);
}

void ARSHUD::DrawAmmo(const ARSCharacter* Player)
{
	// нижний правый угол: крупный магазин / запас, выше — имя оружия
	UFont* Big = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float PanelW = 210.f;
	const float X = W - PanelW - 24.f;

	DrawRect(ColPanel, X - 8.f, H - 96.f, PanelW + 16.f, 72.f);
	DrawText(Player->GetWeaponName(), ColDim, X, H - 92.f, Small, 1.2f);

	// сумка гранат над панелью: HE, флешки, дым, огонь
	const TCHAR* GrenadeTags[] = { TEXT("HE"), TEXT("FL"), TEXT("SM"), TEXT("MT"), TEXT("IN") };
	float GX = X;
	for (int32 i = 0; i < 5; i++)
	{
		if (Player->Grenades[i] > 0)
		{
			DrawText(FString::Printf(TEXT("%s x%d"), GrenadeTags[i], Player->Grenades[i]),
				ColT, GX, H - 118.f, Small, 1.15f);
			GX += 52.f;
		}
	}

	if (Player->bReloading)
	{
		DrawText(TEXT("ПЕРЕЗАРЯДКА..."), ColT, X, H - 66.f, Small, 1.5f);
		return;
	}

	// у гранаты показываем количество, у ножа — прочерк
	if (RSWeapons::IsGrenade(Player->CurrentWeapon))
	{
		const int32 GI = RSWeapons::GrenadeIndex(Player->CurrentWeapon);
		DrawText(FString::Printf(TEXT("x %d"), Player->Grenades[GI]), ColWhite, X, H - 70.f, Big, 1.4f);
		return;
	}
	if (Player->GetMaxAmmo() == 0)
	{
		DrawText(TEXT("—"), ColWhite, X, H - 70.f, Big, 1.6f);
		return;
	}

	const int32 InMag = Player->GetAmmo();
	const FLinearColor MagCol = InMag > 5 ? ColWhite : ColRed;
	DrawText(FString::Printf(TEXT("%d"), InMag), MagCol, X, H - 70.f, Big, 1.7f);
	DrawText(FString::Printf(TEXT("/ %d"), Player->GetReserveAmmo()),
		ColDim, X + 70.f, H - 58.f, Small, 1.5f);
}

void ARSHUD::DrawMoney(const ARSCharacter* Player)
{
	DrawText(FString::Printf(TEXT("$ %d"), Player->Money),
		ColMoney, 24.f, Canvas->SizeY - 130.f, GEngine->GetMediumFont(), 1.f);
}

void ARSHUD::DrawRoundInfo(const ARSCharacter* Player)
{
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State)
	{
		return;
	}

	UFont* Font = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	const float CX = Canvas->SizeX * 0.5f;

	// живые по командам, как счётчики над таймером в CS2
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

	// табло: CT-счёт | таймер | T-счёт
	const int32 Left = FMath::CeilToInt(State->GetTimeLeft());
	const FString Timer = FString::Printf(TEXT("%d:%02d"), Left / 60, Left % 60);
	const bool bUrgent = State->Phase == ERSPhase::Live && Left <= 10;

	DrawRect(ColPanel, CX - 130.f, 8.f, 260.f, 58.f);
	DrawRect(FLinearColor(ColCT.R, ColCT.G, ColCT.B, 0.25f), CX - 130.f, 8.f, 78.f, 40.f);
	DrawRect(FLinearColor(ColT.R, ColT.G, ColT.B, 0.25f), CX + 52.f, 8.f, 78.f, 40.f);

	DrawText(FString::Printf(TEXT("%d"), State->ScoreCT), ColCT, CX - 100.f, 12.f, Med, 1.2f);
	DrawText(FString::Printf(TEXT("%d"), State->ScoreT), ColT, CX + 82.f, 12.f, Med, 1.2f);
	DrawText(Timer, bUrgent ? ColRed : ColWhite, CX - 24.f, 14.f, Med, 1.f);

	DrawText(FString::Printf(TEXT("%d"), AliveCT), ColCT, CX - 44.f, 46.f, Font, 1.3f);
	DrawText(TEXT("жив."), ColDim, CX - 14.f, 48.f, Font, 1.f);
	DrawText(FString::Printf(TEXT("%d"), AliveT), ColT, CX + 32.f, 46.f, Font, 1.3f);

	DrawText(FString::Printf(TEXT("Раунд %d/%d"), State->RoundNumber, ARSGameMode::RoundsTotal),
		ColDim, CX - 34.f, 70.f, Font, 1.f);

	// объявления: итог раунда, смена сторон, конец матча
	if (State->Phase != ERSPhase::Live && !State->Announcement.IsEmpty())
	{
		const bool bOver = State->Phase == ERSPhase::MatchOver;
		const float Scale = bOver ? 2.6f : 1.6f;
		const float Y = Canvas->SizeY * (bOver ? 0.4f : 0.22f);
		DrawText(State->Announcement,
			bOver ? FLinearColor(1.f, 0.85f, 0.2f) : ColWhite,
			CX - State->Announcement.Len() * 4.f * Scale, Y, Font, Scale);
	}

	if (!Player->bAlive && State->Phase == ERSPhase::Live)
	{
		DrawText(TEXT("ПОГИБ — ждём следующий раунд"), FLinearColor(1.f, 0.35f, 0.35f),
			CX - 150.f, Canvas->SizeY * 0.55f, Font, 1.6f);

		DrawText(Player->SpectateTarget
				? TEXT("Наблюдение за союзником — ЛКМ или Пробел: следующий")
				: TEXT("Союзников не осталось — камера на месте гибели"),
			FLinearColor(1.f, 1.f, 1.f, 0.6f), CX - 160.f, Canvas->SizeY * 0.6f, Font, 1.2f);
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

	const float W = 720.f;
	const float X = Canvas->SizeX * 0.5f - W * 0.5f;
	const float Top = Canvas->SizeY * 0.15f;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.82f), X - 20.f, Top - 20.f, W + 40.f, 420.f);

	if (State)
	{
		DrawText(FString::Printf(TEXT("CT  %d : %d  T      раунд %d/%d"),
			State->ScoreCT, State->ScoreT, State->RoundNumber, ARSGameMode::RoundsTotal),
			FLinearColor(1.f, 0.85f, 0.2f), X + 200.f, Top - 10.f, Font, 1.7f);
	}

	// две колонки: слева спецназ, справа террористы
	struct FRow { FString Name; int32 Kills; int32 Deaths; bool bAlive; bool bYou; int32 Money; };
	TArray<FRow> Columns[2];

	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		ARSCharacter* C = *It;
		if (!IsValid(C))
		{
			continue;
		}
		const int32 Side = (C->Team == ERSTeam::CT) ? 0 : 1;
		Columns[Side].Add({ C == Player ? TEXT("Вы") : TEXT("Игрок"),
			C->Kills, C->Deaths, C->bAlive, C == Player, C->Money });
	}

	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		ARSBot* B = *It;
		if (!IsValid(B))
		{
			continue;
		}
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

		DrawText(Titles[Side], TeamColors[Side], ColX, Y, Font, 1.5f);
		Y += 26.f;
		DrawText(TEXT("имя            У    С    $"), FLinearColor(1.f, 1.f, 1.f, 0.45f),
			ColX, Y, Font, 1.1f);
		Y += 22.f;

		for (const FRow& Row : Columns[Side])
		{
			const FLinearColor Color = !Row.bAlive
				? FLinearColor(0.5f, 0.5f, 0.5f)
				: (Row.bYou ? FLinearColor(1.f, 0.95f, 0.5f) : FLinearColor::White);

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

void ARSHUD::DrawBuyMenu(const ARSCharacter* Player)
{
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State)
	{
		return;
	}

	UFont* Font = GEngine->GetSmallFont();

	if (!Player->bBuyMenuOpen)
	{
		if (State->Phase == ERSPhase::Intermission)
		{
			DrawText(FString::Printf(TEXT("B — закупка (%d с)"),
				FMath::CeilToInt(State->GetTimeLeft())),
				FLinearColor(1.f, 0.85f, 0.2f), 24.f, Canvas->SizeY - 155.f, Font, 1.4f);
		}
		return;
	}

	const bool bBuyTime = State->Phase == ERSPhase::Intermission;

	const float W = 480.f;
	const float X = Canvas->SizeX * 0.5f - W * 0.5f;
	float Y = Canvas->SizeY * 0.24f;

	DrawRect(FLinearColor(0.04f, 0.05f, 0.06f, 0.88f), X - 20.f, Y - 40.f, W + 40.f, 360.f);
	DrawText(TEXT("ЗАКУПКА"), FLinearColor(1.f, 0.85f, 0.2f), X, Y - 32.f, Font, 1.8f);
	DrawText(FString::Printf(TEXT("$ %d"), Player->Money), ColMoney, X + W - 100.f, Y - 32.f, Font, 1.8f);

	Y += 6.f;

	if (Player->BuyCategory < 0)
	{
		// уровень категорий
		for (int32 i = 0; i < 6; i++)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.07f), X - 8.f, Y - 4.f, W + 16.f, 30.f);
			DrawText(FString::Printf(TEXT("[%d]  %s"), i + 1, RSWeapons::BuyCategoryName(i)),
				ColWhite, X, Y, Font, 1.5f);
			Y += 36.f;
		}
	}
	else if (Player->BuyCategory == 5)
	{
		// броня
		const bool bArmorFull = Player->Armor >= 100.f;
		struct FRow { const TCHAR* Name; int32 Price; bool bCan; const TCHAR* Owned; };
		const FRow Rows[] =
		{
			{ TEXT("Кевлар"), ARSCharacter::PriceKevlar,
				bBuyTime && !bArmorFull && Player->Money >= ARSCharacter::PriceKevlar,
				bArmorFull ? TEXT("есть") : nullptr },
			{ TEXT("Кевлар + шлем"), ARSCharacter::PriceKevlarHelmet,
				bBuyTime && !(bArmorFull && Player->bHasHelmet) && Player->Money >= ARSCharacter::PriceKevlarHelmet,
				(bArmorFull && Player->bHasHelmet) ? TEXT("есть") : nullptr },
		};
		for (int32 i = 0; i < 2; i++)
		{
			const FLinearColor Color = Rows[i].bCan ? ColWhite : FLinearColor(0.55f, 0.55f, 0.55f);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, Rows[i].bCan ? 0.08f : 0.03f), X - 8.f, Y - 4.f, W + 16.f, 30.f);
			DrawText(FString::Printf(TEXT("[%d]  %s"), i + 1, Rows[i].Name), Color, X, Y, Font, 1.5f);
			if (Rows[i].Owned)
			{
				DrawText(Rows[i].Owned, ColMoney, X + 260.f, Y, Font, 1.3f);
			}
			DrawText(FString::Printf(TEXT("$ %d"), Rows[i].Price), Color, X + W - 100.f, Y, Font, 1.5f);
			Y += 36.f;
		}
		DrawText(TEXT("0 — назад"), FLinearColor(1.f, 1.f, 1.f, 0.5f), X, Y + 8.f, Font, 1.2f);
	}
	else
	{
		// оружие выбранной категории
		DrawText(RSWeapons::BuyCategoryName(Player->BuyCategory), ColDim, X, Y, Font, 1.3f);
		Y += 28.f;

		const TArray<ERSWeapon> Items = RSWeapons::BuyCategory(Player->BuyCategory, Player->Team);
		for (int32 i = 0; i < Items.Num(); i++)
		{
			const FRSWeaponDef& Def = RSWeapons::Get(Items[i]);
			const bool bOwned =
				(Def.Slot == ERSSlot::Primary && Player->bHasPrimary && Player->PrimaryType == Items[i])
				|| (Def.Slot == ERSSlot::Secondary && Player->bHasSecondary && Player->SecondaryType == Items[i])
				|| (Def.Slot == ERSSlot::Grenade && Player->Grenades[RSWeapons::GrenadeIndex(Items[i])] > 0);
			const bool bSlotBusy = Def.Slot == ERSSlot::Primary && Player->bHasPrimary;
			const bool bCan = bBuyTime && !bOwned && !bSlotBusy && Player->Money >= Def.Price;

			const FLinearColor Color = bCan ? ColWhite : FLinearColor(0.55f, 0.55f, 0.55f);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, bCan ? 0.08f : 0.03f), X - 8.f, Y - 4.f, W + 16.f, 30.f);
			DrawText(FString::Printf(TEXT("[%d]  %s"), i + 1, Def.Name), Color, X, Y, Font, 1.5f);
			if (bOwned)
			{
				DrawText(TEXT("есть"), ColMoney, X + 260.f, Y, Font, 1.3f);
			}
			DrawText(FString::Printf(TEXT("$ %d"), Def.Price), Color, X + W - 100.f, Y, Font, 1.5f);
			Y += 36.f;
		}
		DrawText(TEXT("0 — назад"), FLinearColor(1.f, 1.f, 1.f, 0.5f), X, Y + 8.f, Font, 1.2f);
		Y += 26.f;
	}

	const TCHAR* Warning = nullptr;
	if (!bBuyTime)
	{
		Warning = TEXT("Покупать можно только между раундами");
	}
	else if (Player->BuyCategory >= 0 && Player->BuyCategory <= 3 && Player->bHasPrimary)
	{
		Warning = TEXT("Основное оружие уже есть — G, чтобы выбросить");
	}
	if (Warning)
	{
		DrawText(Warning, FLinearColor(1.f, 0.5f, 0.5f), X, Y + 26.f, Font, 1.2f);
	}

	DrawText(TEXT("B — закрыть"), FLinearColor(1.f, 1.f, 1.f, 0.5f), X, Y + 50.f, Font, 1.2f);
}
