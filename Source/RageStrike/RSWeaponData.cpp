#include "RSWeaponData.h"

namespace RSWeapons
{
	// Цифры примерно как в CS2: урон за пулю, скорострельность, цены,
	// скорость бега (юниты CS × 1.84), награда за килл.
	const FRSWeaponDef& Get(ERSWeapon W)
	{
		//                    имя            слот                 меш                    замок             цена  урон  гол  интерв  авто  дробь скор  перез  маг  запас награда прицел fov  кик   рост  йо    разброс
		static const FRSWeaponDef Defs[Count] = {
		/* Knife      */ { TEXT("KNIFE"),    ERSSlot::Knife,     ERSMeshKind::Knife,    ERSTeamLock::Any,    0,  55.f, 1.6f, 0.50f, true,  1, 460.f, 0.0f,   0,   0, 1500, false, 90.f, 0.f,   0.f,  0.f,  0.f  },
		/* Glock      */ { TEXT("GLOCK-18"), ERSSlot::Secondary, ERSMeshKind::Pistol,   ERSTeamLock::T,    200,  28.f, 4.f,  0.15f, false, 1, 442.f, 2.2f,  20, 120,  300, false, 90.f, 0.45f, 0.03f, 0.f, 0.30f },
		/* USP        */ { TEXT("USP-S"),    ERSSlot::Secondary, ERSMeshKind::Pistol,   ERSTeamLock::CT,   200,  33.f, 4.f,  0.17f, false, 1, 442.f, 2.2f,  12,  24,  300, false, 90.f, 0.45f, 0.03f, 0.f, 0.25f },
		/* P250       */ { TEXT("P250"),     ERSSlot::Secondary, ERSMeshKind::Pistol,   ERSTeamLock::Any,  300,  36.f, 4.f,  0.16f, false, 1, 442.f, 2.2f,  13,  26,  300, false, 90.f, 0.50f, 0.03f, 0.f, 0.30f },
		/* Deagle     */ { TEXT("DEAGLE"),   ERSSlot::Secondary, ERSMeshKind::Pistol,   ERSTeamLock::Any,  700,  53.f, 4.f,  0.25f, false, 1, 424.f, 2.2f,   7,  35,  300, false, 90.f, 0.95f, 0.05f, 0.f, 0.40f },
		/* Tec9       */ { TEXT("TEC-9"),    ERSSlot::Secondary, ERSMeshKind::Pistol,   ERSTeamLock::T,    500,  33.f, 4.f,  0.12f, false, 1, 442.f, 2.2f,  18,  90,  300, false, 90.f, 0.55f, 0.04f, 0.f, 0.40f },
		/* FiveSeven  */ { TEXT("FIVE-SEVEN"),ERSSlot::Secondary,ERSMeshKind::Pistol,   ERSTeamLock::CT,   500,  32.f, 4.f,  0.14f, false, 1, 442.f, 2.2f,  20, 100,  300, false, 90.f, 0.50f, 0.03f, 0.f, 0.35f },
		/* MP9        */ { TEXT("MP9"),      ERSSlot::Primary,   ERSMeshKind::RifleM4,  ERSTeamLock::CT,  1250,  26.f, 4.f,  0.070f, true, 1, 441.f, 2.1f,  30, 120,  600, false, 90.f, 0.18f, 0.05f, 0.35f, 0.30f },
		/* MAC10      */ { TEXT("MAC-10"),   ERSSlot::Primary,   ERSMeshKind::RifleM4,  ERSTeamLock::T,   1050,  29.f, 4.f,  0.075f, true, 1, 442.f, 2.1f,  30, 100,  600, false, 90.f, 0.20f, 0.05f, 0.40f, 0.35f },
		/* UMP45      */ { TEXT("UMP-45"),   ERSSlot::Primary,   ERSMeshKind::RifleM4,  ERSTeamLock::Any, 1200,  35.f, 4.f,  0.090f, true, 1, 420.f, 2.3f,  25, 100,  600, false, 90.f, 0.22f, 0.06f, 0.35f, 0.30f },
		/* P90        */ { TEXT("P90"),      ERSSlot::Primary,   ERSMeshKind::RifleM4,  ERSTeamLock::Any, 2350,  26.f, 4.f,  0.070f, true, 1, 424.f, 3.3f,  50, 100,  300, false, 90.f, 0.16f, 0.04f, 0.30f, 0.35f },
		/* Nova       */ { TEXT("NOVA"),     ERSSlot::Primary,   ERSMeshKind::RifleM4,  ERSTeamLock::Any, 1050,  24.f, 2.f,  0.90f, false, 8, 405.f, 3.0f,   8,  32,  900, false, 90.f, 1.40f, 0.f,   0.f,  2.50f },
		/* XM1014     */ { TEXT("XM1014"),   ERSSlot::Primary,   ERSMeshKind::RifleM4,  ERSTeamLock::Any, 2000,  20.f, 2.f,  0.35f, true,  6, 398.f, 3.0f,   7,  32,  900, false, 90.f, 1.10f, 0.f,   0.f,  2.50f },
		/* GalilAR    */ { TEXT("GALIL AR"), ERSSlot::Primary,   ERSMeshKind::RifleAK,  ERSTeamLock::T,   1800,  30.f, 4.f,  0.090f, true, 1, 397.f, 3.0f,  35,  90,  300, false, 90.f, 0.26f, 0.08f, 0.55f, 0.20f },
		/* FAMAS      */ { TEXT("FAMAS"),    ERSSlot::Primary,   ERSMeshKind::RifleM4,  ERSTeamLock::CT,  2050,  30.f, 4.f,  0.090f, true, 1, 400.f, 3.3f,  25,  90,  300, false, 90.f, 0.25f, 0.08f, 0.50f, 0.20f },
		/* AK47       */ { TEXT("AK-47"),    ERSSlot::Primary,   ERSMeshKind::RifleAK,  ERSTeamLock::T,   2700,  36.f, 4.f,  0.100f, true, 1, 397.f, 2.5f,  30,  90,  300, false, 90.f, 0.28f, 0.09f, 0.55f, 0.15f },
		/* M4A4       */ { TEXT("M4A4"),     ERSSlot::Primary,   ERSMeshKind::RifleM4,  ERSTeamLock::CT,  3100,  33.f, 4.f,  0.090f, true, 1, 407.f, 3.0f,  30,  90,  300, false, 90.f, 0.25f, 0.08f, 0.50f, 0.15f },
		/* AUG        */ { TEXT("AUG"),      ERSSlot::Primary,   ERSMeshKind::RifleM4,  ERSTeamLock::CT,  3300,  28.f, 4.f,  0.090f, true, 1, 402.f, 3.8f,  30,  90,  300, true,  55.f, 0.24f, 0.07f, 0.45f, 0.12f },
		/* SG553      */ { TEXT("SG 553"),   ERSSlot::Primary,   ERSMeshKind::RifleAK,  ERSTeamLock::T,   3000,  30.f, 4.f,  0.090f, true, 1, 397.f, 2.8f,  30,  90,  300, true,  55.f, 0.26f, 0.08f, 0.50f, 0.12f },
		/* SSG08      */ { TEXT("SSG 08"),   ERSSlot::Primary,   ERSMeshKind::Sniper,   ERSTeamLock::Any, 1700,  74.f, 4.f,  1.25f, false, 1, 425.f, 2.7f,  10,  90,  300, true,  40.f, 1.20f, 0.f,   0.f,  0.03f },
		/* AWP        */ { TEXT("AWP"),      ERSSlot::Primary,   ERSMeshKind::Sniper,   ERSTeamLock::Any, 4750, 115.f, 4.f,  1.45f, false, 1, 375.f, 3.6f,   5,  30,  100, true,  25.f, 1.80f, 0.f,   0.f,  0.03f },
		/* HEGrenade  */ { TEXT("HE"),       ERSSlot::Grenade,   ERSMeshKind::Grenade,  ERSTeamLock::Any,  300,  98.f, 1.f,  1.00f, false, 1, 452.f, 0.0f,   0,   0,  300, false, 90.f, 0.f,   0.f,  0.f,  0.f  },
		/* Flashbang  */ { TEXT("FLASH"),    ERSSlot::Grenade,   ERSMeshKind::Grenade,  ERSTeamLock::Any,  200,   1.f, 1.f,  1.00f, false, 1, 452.f, 0.0f,   0,   0,  300, false, 90.f, 0.f,   0.f,  0.f,  0.f  },
		/* Smoke      */ { TEXT("SMOKE"),    ERSSlot::Grenade,   ERSMeshKind::Grenade,  ERSTeamLock::Any,  300,   0.f, 1.f,  1.00f, false, 1, 452.f, 0.0f,   0,   0,  300, false, 90.f, 0.f,   0.f,  0.f,  0.f  },
		/* Molotov    */ { TEXT("MOLOTOV"),  ERSSlot::Grenade,   ERSMeshKind::Grenade,  ERSTeamLock::T,    400,   7.f, 1.f,  1.00f, false, 1, 452.f, 0.0f,   0,   0,  300, false, 90.f, 0.f,   0.f,  0.f,  0.f  },
		/* Incendiary */ { TEXT("INCENDIARY"),ERSSlot::Grenade,  ERSMeshKind::Grenade,  ERSTeamLock::CT,   500,   7.f, 1.f,  1.00f, false, 1, 452.f, 0.0f,   0,   0,  300, false, 90.f, 0.f,   0.f,  0.f,  0.f  },
		};
		return Defs[FMath::Clamp((int32)W, 0, Count - 1)];
	}

	TArray<ERSWeapon> BuyCategory(int32 Category, ERSTeam Team)
	{
		TArray<ERSWeapon> Out;
		auto Add = [&](std::initializer_list<ERSWeapon> List)
		{
			for (ERSWeapon W : List)
			{
				if (AllowedFor(W, Team))
				{
					Out.Add(W);
				}
			}
		};
		switch (Category)
		{
		case 0: Add({ ERSWeapon::Glock, ERSWeapon::USP, ERSWeapon::P250, ERSWeapon::Deagle,
		              ERSWeapon::Tec9, ERSWeapon::FiveSeven }); break;
		case 1: Add({ ERSWeapon::MP9, ERSWeapon::MAC10, ERSWeapon::UMP45, ERSWeapon::P90,
		              ERSWeapon::Nova, ERSWeapon::XM1014 }); break;
		case 2: Add({ ERSWeapon::GalilAR, ERSWeapon::FAMAS, ERSWeapon::AK47, ERSWeapon::M4A4,
		              ERSWeapon::AUG, ERSWeapon::SG553 }); break;
		case 3: Add({ ERSWeapon::SSG08, ERSWeapon::AWP }); break;
		case 4: Add({ ERSWeapon::HEGrenade, ERSWeapon::Flashbang, ERSWeapon::SmokeGrenade,
		              ERSWeapon::Molotov, ERSWeapon::Incendiary }); break;
		default: break;
		}
		return Out;
	}

	const TCHAR* BuyCategoryName(int32 Category)
	{
		switch (Category)
		{
		case 0: return TEXT("Пистолеты");
		case 1: return TEXT("ПП и дробовики");
		case 2: return TEXT("Винтовки");
		case 3: return TEXT("Снайперки");
		case 4: return TEXT("Гранаты");
		default: return TEXT("Броня");
		}
	}
}
