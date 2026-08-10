import unreal

lines = []
grey = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")

for path in unreal.EditorAssetLibrary.list_assets("/Game/Maps/Dust2", recursive=True):
    obj = unreal.load_asset(path)
    if not isinstance(obj, unreal.StaticMesh):
        continue

    b = obj.get_bounds()
    names = []
    changed = False
    for i, slot in enumerate(obj.static_materials):
        mi = slot.material_interface
        name = mi.get_name() if mi else "None"
        names.append(name)
        if mi is None or name == "WorldGridMaterial":
            obj.set_material(i, grey)
            changed = True
    if changed:
        unreal.EditorAssetLibrary.save_loaded_asset(obj)

    lines.append("{} origin=({:.0f},{:.0f},{:.0f}) extent=({:.0f},{:.0f},{:.0f}) mats={} fixed={}".format(
        obj.get_name(),
        b.origin.x, b.origin.y, b.origin.z,
        b.box_extent.x, b.box_extent.y, b.box_extent.z,
        names, changed))

with open(r"C:\Dev\RageStrike\Scripts\map_info.txt", "w") as f:
    f.write("\n".join(lines))
