"""Импорт Миража из GLB в /Game/Maps/Mirage + коллизия complex-as-simple."""

import os
import unreal

SRC = r"C:\Dev\RageStrike\Content\Maps\Mirage\source\untitled.glb"
DEST = "/Game/Maps/Mirage"

lines = []
if not os.path.isfile(SRC):
    lines.append("MISSING " + SRC)
else:
    task = unreal.AssetImportTask()
    task.filename = SRC
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    count = 0
    for path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True):
        obj = unreal.load_asset(path)
        if isinstance(obj, unreal.StaticMesh):
            body = obj.get_editor_property("body_setup")
            if body:
                body.set_editor_property(
                    "collision_trace_flag",
                    unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            unreal.EditorAssetLibrary.save_loaded_asset(obj)
            e = obj.get_bounds().box_extent
            o = obj.get_bounds().origin
            count += 1
            lines.append("{} extent=({:.0f},{:.0f},{:.0f}) origin=({:.0f},{:.0f},{:.0f})".format(
                obj.get_path_name(), e.x, e.y, e.z, o.x, o.y, o.z))
    lines.insert(0, "Mirage: {} meshes".format(count))

with open(r"C:\Dev\RageStrike\Scripts\mirage_info.txt", "w") as f:
    f.write("\n".join(lines))
