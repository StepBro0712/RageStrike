"""Импорт иконок оружия в /Game/UI/Icons."""

import os
import unreal

SRC = r"C:\Dev\RageStrike\ImportSource\GeneratedIcons"
DEST = "/Game/UI/Icons"

files = [os.path.join(SRC, f) for f in sorted(os.listdir(SRC))
         if f.lower().endswith(".png") and not f.startswith("_")]

tasks = []
for f in files:
    t = unreal.AssetImportTask()
    t.filename = f
    t.destination_path = DEST
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

count = 0
for path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=False):
    obj = unreal.load_asset(path)
    if not isinstance(obj, unreal.Texture2D):
        continue
    # интерфейсная текстура: без мипов и без сжатия, иначе иконка мылится
    obj.set_editor_property("compression_settings",
                            unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    obj.set_editor_property("mip_gen_settings",
                            unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    obj.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(obj)
    count += 1

unreal.log("=== import_icons.py finished: {} textures ===".format(count))
