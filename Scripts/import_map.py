"""Импорт карты из glTF/GLB в /Game/Maps/<Name>.

После импорта включает у мешей 'использовать сложную геометрию как простую',
иначе по карте нельзя ходить и стрелять — коллизии у glTF нет.
"""

import os
import unreal

MAPS = [
    {
        "name": "Dust2",
        "file": r"C:\Dev\RageStrike\ImportSource\Maps\dust2\extracted\de_dust_2_with_real_light.glb",
    },
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
lines = []

for m in MAPS:
    dest = "/Game/Maps/{}".format(m["name"])
    if not os.path.isfile(m["file"]):
        lines.append("{} MISSING {}".format(m["name"], m["file"]))
        continue

    task = unreal.AssetImportTask()
    task.filename = m["file"]
    task.destination_path = dest
    task.automated = True
    task.replace_existing = True
    task.save = True
    asset_tools.import_asset_tasks([task])

    meshes = []
    for path in unreal.EditorAssetLibrary.list_assets(dest, recursive=True):
        obj = unreal.load_asset(path)
        if isinstance(obj, unreal.StaticMesh):
            body = obj.get_editor_property("body_setup")
            if body:
                body.set_editor_property("collision_trace_flag",
                                         unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            unreal.EditorAssetLibrary.save_loaded_asset(obj)
            e = obj.get_bounds().box_extent
            meshes.append((obj.get_path_name(), e.x, e.y, e.z))

    lines.append("{}: {} meshes".format(m["name"], len(meshes)))
    for path, x, y, z in meshes[:10]:
        lines.append("   {} extent=({:.0f},{:.0f},{:.0f})".format(path, x, y, z))

with open(r"C:\Dev\RageStrike\Scripts\map_info.txt", "w") as f:
    f.write("\n".join(lines))
