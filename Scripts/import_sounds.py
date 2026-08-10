"""Импорт сгенерированных WAV в /Game/Audio + петли для музыки и огня."""

import os
import unreal

SRC = r"C:\Dev\RageStrike\ImportSource\GeneratedAudio"
DEST = "/Game/Audio"
LOOPING = {"Music_Menu", "Music_Buy", "Burn"}

files = [os.path.join(SRC, f) for f in sorted(os.listdir(SRC)) if f.lower().endswith(".wav")]

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

lines = []
for path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=False):
    obj = unreal.load_asset(path)
    if not isinstance(obj, unreal.SoundWave):
        continue
    name = obj.get_name()
    if name in LOOPING:
        obj.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_loaded_asset(obj)
    lines.append("{}  loop={}".format(name, name in LOOPING))

with open(r"C:\Dev\RageStrike\Scripts\audio_info.txt", "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines))

unreal.log("=== import_sounds.py finished: {} assets ===".format(len(lines)))
