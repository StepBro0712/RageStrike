import unreal

lines = []
for path in unreal.EditorAssetLibrary.list_assets("/Game/Maps/Dust2", recursive=True):
    obj = unreal.load_asset(path)
    if not isinstance(obj, unreal.StaticMesh):
        continue
    body = obj.get_editor_property("body_setup")
    flag = body.get_editor_property("collision_trace_flag") if body else "NO BODY"
    if body:
        body.set_editor_property("collision_trace_flag",
                                 unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    obj.set_editor_property("allow_cpu_access", True)
    unreal.EditorAssetLibrary.save_loaded_asset(obj)

    body2 = obj.get_editor_property("body_setup")
    lines.append("{} before={} after={} cpu={}".format(
        obj.get_name(), flag,
        body2.get_editor_property("collision_trace_flag") if body2 else "-",
        obj.get_editor_property("allow_cpu_access")))

with open(r"C:\Dev\RageStrike\Scripts\map_info.txt", "w") as f:
    f.write("\n".join(lines))
