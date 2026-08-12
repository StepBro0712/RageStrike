"""Импорт общих материалов и текстур рук — один раз на весь проект.

Геометрия рук печётся под каждый ствол отдельно, а материалы у всех одни,
поэтому текстуры лежат здесь, а не в каждой из 22 моделей.
"""
import unreal

SRC = r"C:\Dev\RageStrike\ImportSource\Characters\csgo-arms\csgo_arms.glb"
DEST = "/Game/Weapons/VM/Arms/Shared"

t = unreal.AssetImportTask()
t.filename = SRC
t.destination_path = DEST
t.automated = True
t.replace_existing = True
t.save = True

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])

for a in sorted(unreal.EditorAssetLibrary.list_assets(DEST, recursive=True)):
    unreal.log("SHARED {} {}".format(type(unreal.load_asset(a)).__name__, a))
