"""Импорт настоящих звуков CS: выстрелы по стволам, шаги, перезарядка, гранаты.

Кладём в /Game/Audio/CS под предсказуемыми именами, чтобы C++ грузил их по пути.
Из каждой папки берём первый подходящий файл — вариантов там по десятку.
"""

import os
import re
import unreal

WEAPON_SRC = r"C:\Dev\RageStrike\ImportSource\Audio\weaponsounds"
STEP_SRC = r"C:\Dev\RageStrike\ImportSource\Audio\footsteps\Footsteps"
DEST = "/Game/Audio/CS"

# имя в игре -> папка CS
GUNS = {
    "Knife": "knife", "Glock": "glock18", "USP": "usp", "P250": "p250",
    "Deagle": "deagle", "Tec9": "tec9", "FiveSeven": "fiveseven",
    "MP9": "mp9", "MAC10": "mac10", "UMP45": "ump45", "P90": "p90",
    "Nova": "nova", "XM1014": "xm1014", "GalilAR": "galilar", "FAMAS": "famas",
    "AK47": "ak47", "M4A4": "m4a1", "AUG": "aug", "SG553": "sg556",
    "SSG08": "ssg08", "AWP": "awp",
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
plan = []   # (файл, целевое имя ассета)


def pick(folder, patterns):
    """Первый файл, чьё имя подходит под шаблон."""
    if not os.path.isdir(folder):
        return None
    files = sorted(os.listdir(folder))
    for pat in patterns:
        for f in files:
            if f.lower().endswith((".wav", ".mp3")) and re.search(pat, f.lower()):
                return os.path.join(folder, f)
    return None


for name, folder in GUNS.items():
    path = os.path.join(WEAPON_SRC, folder)
    # выстрел: <ствол>_01 / <ствол>-1 / просто первый "звук стрельбы"
    shot = pick(path, [r"_0?1\.wav$", r"-1\.wav$", r"^" + folder + r"\.wav$"])
    if shot:
        plan.append((shot, "Fire_" + name))

    # перезарядка: вставка магазина, затем затвор
    clip = pick(path, [r"clipin", r"magazine", r"boltpull", r"slide"])
    if clip:
        plan.append((clip, "Reload_" + name))

# шаги по бетону: восемь вариантов, чтобы не долбило одним и тем же
steps = [f for f in sorted(os.listdir(STEP_SRC)) if f.lower().startswith("concrete_ct")][:8]
for i, f in enumerate(steps, start=1):
    plan.append((os.path.join(STEP_SRC, f), "Step_{}".format(i)))

# гранаты и прочее
extra = {
    "Nade_Throw_CS": (os.path.join(WEAPON_SRC, "foley"), [r"throw"]),
    "Explode_CS": (os.path.join(WEAPON_SRC, "hegrenade"), [r"explode", r"expl"]),
    "Flash_CS": (os.path.join(WEAPON_SRC, "flashbang"), [r"explode", r"flashbang"]),
    "Smoke_CS": (os.path.join(WEAPON_SRC, "smokegrenade"), [r"emit", r"smoke"]),
    "Burn_CS": (os.path.join(WEAPON_SRC, "molotov"), [r"loop", r"fire"]),
}
for target, (folder, pats) in extra.items():
    found = pick(folder, pats)
    if found:
        plan.append((found, target))

# импорт по одному: так проще переименовать в нужное имя
lines = []
for src, target in plan:
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    asset_tools.import_asset_tasks([task])

    imported = list(task.get_editor_property("imported_object_paths"))
    if not imported:
        lines.append("FAIL {}".format(src))
        continue

    obj = unreal.load_asset(imported[0])
    if obj is None:
        lines.append("FAIL load {}".format(src))
        continue

    if obj.get_name() != target:
        new_path = "{}/{}".format(DEST, target)
        if unreal.EditorAssetLibrary.does_asset_exist(new_path):
            unreal.EditorAssetLibrary.delete_asset(new_path)
        unreal.EditorAssetLibrary.rename_asset(obj.get_path_name(), new_path)
        obj = unreal.load_asset(new_path)

    if obj and target == "Burn_CS":
        obj.set_editor_property("looping", True)
    if obj:
        unreal.EditorAssetLibrary.save_loaded_asset(obj)
        lines.append("{}  <-  {}".format(target, os.path.basename(src)))

with open(r"C:\Dev\RageStrike\Scripts\cs_audio_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

unreal.log("=== import_cs_audio.py finished: {} ===".format(len(lines)))
