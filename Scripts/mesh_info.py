import unreal

lines = []
for path in ("/Game/Weapons/Pistol/Meshes/SM_Pistol",
             "/Game/Weapons/Rifle/Meshes/SM_Rifle"):
    mesh = unreal.load_asset(path)
    if mesh is None:
        lines.append("{} MISSING".format(path))
        continue
    b = mesh.get_bounds()
    mats = [m.material_interface.get_name() if m.material_interface else "None"
            for m in mesh.static_materials]
    lines.append("{} origin=({:.1f},{:.1f},{:.1f}) extent=({:.1f},{:.1f},{:.1f}) mats={}".format(
        path, b.origin.x, b.origin.y, b.origin.z,
        b.box_extent.x, b.box_extent.y, b.box_extent.z, mats))

with open(r"C:\Dev\RageStrike\Scripts\mesh_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
