"""Свой шрифт для HUD: FontFace из TTF + Font с runtime-кэшированием.

Встроенные шрифты движка растровые: на любом масштабе текст мылится.
Runtime-шрифт рендерит глифы под нужный размер, поэтому HUD становится чётким.
Берём Roboto из состава движка — он с кириллицей и под свободной лицензией.

Каждый шаг пишется в лог сразу: если редактор упадёт, будет видно, на чём.
"""

import os
import unreal

TTF = os.path.join(unreal.Paths.engine_content_dir(), "Slate", "Fonts", "Roboto-Bold.ttf")
DEST = "/Game/UI/Fonts"
FACE_NAME = "FF_RSHud"
FONT_NAME = "F_RSHud"
LOG = r"C:\Dev\RageStrike\Scripts\font_info.txt"


def note(text):
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(str(text) + "\n")


open(LOG, "w", encoding="utf-8").close()
note("ttf exists: {}".format(os.path.isfile(TTF)))

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# 1. Начертание из файла шрифта
face_path = "{}/{}".format(DEST, FACE_NAME)
face = unreal.load_asset(face_path)
if face is None:
    task = unreal.AssetImportTask()
    task.filename = TTF
    task.destination_path = DEST
    task.destination_name = FACE_NAME
    task.automated = True
    task.replace_existing = True
    task.save = True
    asset_tools.import_asset_tasks([task])
    face = unreal.load_asset(face_path)
note("face: {}".format(face))

# 2. Сам шрифт
font_path = "{}/{}".format(DEST, FONT_NAME)
font = unreal.load_asset(font_path)
if font is None:
    # без фабрики: Font — обычный UObject, создаётся напрямую
    font = asset_tools.create_asset(FONT_NAME, DEST, unreal.Font, None)
note("font: {}".format(font))

if font is not None:
    font.set_editor_property("font_cache_type", unreal.FontCacheType.RUNTIME)
    note("cache set")

    if face is not None:
        # структуры в питоне — копии, поэтому собираем и пишем обратно
        composite = font.get_editor_property("composite_font")
        note("composite: {}".format(type(composite)))

        typeface = composite.get_editor_property("default_typeface")
        note("typeface: {}".format(type(typeface)))

        data = unreal.FontData()
        for prop in ("font_face_asset", "font_face"):
            try:
                data.set_editor_property(prop, face)
                note("font data via {}".format(prop))
                break
            except Exception as ex:
                note("prop {} failed: {}".format(prop, ex))

        entry = unreal.TypefaceEntry()
        entry.set_editor_property("name", "Default")
        entry.set_editor_property("font", data)
        note("entry ready")

        typeface.set_editor_property("fonts", [entry])
        composite.set_editor_property("default_typeface", typeface)
        font.set_editor_property("composite_font", composite)
        note("composite written")

    unreal.EditorAssetLibrary.save_loaded_asset(font)
    note("saved: {}".format(font.get_path_name()))

unreal.log("=== import_font.py finished ===")
