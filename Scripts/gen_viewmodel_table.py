"""Печатает C++ таблицу вьюмоделей по фактически импортированным ассетам.

Имена анимаций у стволов не унифицированы (ak47_draw, awp_draw, glock_draw,
просто draw), поэтому классифицируем по ключевым словам. Таблица нужна
именно статическая: перебирать реестр ассетов в рантайме нельзя — в собранной
игре пересканирование пути стирает записи, пришедшие из кука.

Запуск: обычным python, редактор не нужен — читаем имена файлов с диска.
"""

import os

VM = r"C:\Dev\RageStrike\Content\Weapons\VM"

# ERSWeapon -> папка пака. Того, чего в паке нет (нож, Tec9, молотов),
# в таблице просто не будет — такие стволы останутся на статик-меше.
WEAPONS = [
    ("Glock",        "v_pist_glock18"),
    ("USP",          "v_pist_usp"),
    ("P250",         "v_pist_p250"),
    ("Deagle",       "v_pist_deagle"),
    ("FiveSeven",    "v_pist_fiveseven"),
    ("MP9",          "v_smg_mp9"),
    ("MAC10",        "v_smg_mac10"),
    ("UMP45",        "v_smg_ump45"),
    ("P90",          "v_smg_p90"),
    ("Nova",         "v_shot_nova"),
    ("XM1014",       "v_shot_xm1014"),
    ("GalilAR",      "v_rif_galilar"),
    ("FAMAS",        "v_rif_famas"),
    ("AK47",         "v_rif_ak47"),
    ("M4A4",         "v_rif_m4a4"),
    ("AUG",          "v_rif_aug"),
    ("SG553",        "v_rif_sg556"),
    ("SSG08",        "v_snip_ssg08"),
    ("AWP",          "v_snip_awp"),
    ("HEGrenade",    "v_eq_fraggrenade"),
    ("Flashbang",    "v_eq_flashbang"),
    ("SmokeGrenade", "v_eq_smokegrenade"),
]


def classify(pack):
    """Раскладывает ассеты папки по ролям. Возвращает dict role -> имя ассета."""
    d = os.path.join(VM, pack, pack, "SkeletalMeshes")
    if not os.path.isdir(d):
        return None

    names = [os.path.splitext(f)[0] for f in os.listdir(d) if f.endswith(".uasset")]
    names = [n for n in names if not n.endswith("_Skeleton") and not n.endswith("_PhysicsAsset")]

    out = {"mesh": None, "idle": None, "draw": None, "reload": None,
           "inspect": None, "shoot": []}

    for n in names:
        if n == pack:
            out["mesh"] = n
            continue
        low = n.lower()
        # У гранат своя терминология: вместо draw/reload/fire там deploy,
        # pullpin и throw. Без этого им доставалась одна лишь idle.
        # Порядок проверок важен: draw_silenced не должен уехать в idle,
        # а throwcharge_* — это удержание замаха, а не сам бросок.
        if "reload" in low and not out["reload"]:
            out["reload"] = n
        elif ("fire" in low or "shoot" in low
              or ("throw" in low and "charge" not in low)) and len(out["shoot"]) < 3:
            out["shoot"].append(n)
        elif ("draw" in low or "deploy" in low) and not out["draw"]:
            out["draw"] = n
        elif "idle" in low and not out["idle"]:
            out["idle"] = n
        elif "lookat" in low and not out["inspect"]:
            out["inspect"] = n

    return out


def cpp_str(pack, asset):
    if not asset:
        return "nullptr"
    return 'TEXT("/Game/Weapons/VM/{p}/{p}/SkeletalMeshes/{a}.{a}")'.format(p=pack, a=asset)


def arms_str(pack):
    """Путь к рукам, запечённым в позу привязки этого ствола.

    Модель рук у каждого оружия своя: в Source поза привязки вьюмодели
    несёт в себе хват, и у AK, USP и AWP она разная. Одни руки на все
    стволы рвало бы по швам на суставах — замеры давали растяжение до 36
    раз на 3.3% рёбер.
    """
    d = os.path.join(VM, "Arms", "Baked", "arms_%s" % pack, "SkeletalMeshes")
    if not os.path.isfile(os.path.join(d, "arms_%s.uasset" % pack)):
        return "nullptr"
    return ('TEXT("/Game/Weapons/VM/Arms/Baked/arms_{p}'
            '/SkeletalMeshes/arms_{p}.arms_{p}")').format(p=pack)


OUT_H = r"C:\Dev\RageStrike\Source\RageStrike\RSViewModelTable.h"

lines = []
lines.append("#pragma once")
lines.append("")
lines.append("// СГЕНЕРИРОВАНО Scripts/gen_viewmodel_table.py — правки затрутся.")
lines.append("// Пути статические намеренно: перебирать реестр ассетов в рантайме нельзя,")
lines.append("// в собранной игре пересканирование пути стирает записи из кука.")
lines.append("")
lines.append("static const FRSVMEntry GVMTable[] =")
lines.append("{")

missing = []
for enum, pack in WEAPONS:
    c = classify(pack)
    if not c or not c["mesh"]:
        missing.append(pack)
        continue
    sh = (c["shoot"] + [None, None, None])[:3]
    lines.append("\t{ ERSWeapon::%s," % enum)
    lines.append("\t\t%s," % cpp_str(pack, c["mesh"]))
    lines.append("\t\t%s," % cpp_str(pack, c["idle"]))
    lines.append("\t\t%s," % cpp_str(pack, c["draw"]))
    lines.append("\t\t%s," % cpp_str(pack, c["reload"]))
    lines.append("\t\t{ %s," % cpp_str(pack, sh[0]))
    lines.append("\t\t  %s," % cpp_str(pack, sh[1]))
    lines.append("\t\t  %s }," % cpp_str(pack, sh[2]))
    lines.append("\t\t%s," % cpp_str(pack, c["inspect"]))
    lines.append("\t\t%s }," % arms_str(pack))

lines.append("};")
if missing:
    lines.append("// НЕ НАЙДЕНЫ: " + ", ".join(missing))
lines.append("")

with open(OUT_H, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

print("written: %s  (%d weapons, %d lines)" % (OUT_H, len(WEAPONS) - len(missing), len(lines)))
if missing:
    print("missing packs: " + ", ".join(missing))
