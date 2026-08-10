import unreal

SRC = "/Game/Fab/Game_Ready_Pistol_–_Low_Poly_PBR_Weapon/sm_gun_pistol_g01/StaticMeshes/sm_gun_pistol_g01"
DST = "/Game/Weapons/Pistol/Meshes/SM_Pistol_G01"

lines = []
src = unreal.load_asset(SRC)
if src is None:
    # путь с необычным тире мог не совпасть — ищем по имени
    for path in unreal.EditorAssetLibrary.list_assets("/Game/Fab", recursive=True):
        if "sm_gun_pistol_g01" in path and unreal.EditorAssetLibrary.find_asset_data(path).asset_class_path.asset_name == "StaticMesh":
            src = unreal.load_asset(path)
            lines.append("найден по поиску: {}".format(path))
            break

if src:
    if unreal.EditorAssetLibrary.does_asset_exist(DST):
        unreal.EditorAssetLibrary.delete_asset(DST)
    ok = unreal.EditorAssetLibrary.duplicate_asset(src.get_path_name().split(".")[0], DST)
    lines.append("копия создана: {}".format(ok is not None))

    dup = unreal.load_asset(DST)
    if dup:
        b = dup.get_bounds()
        lines.append("{} origin=({:.1f},{:.1f},{:.1f}) extent=({:.1f},{:.1f},{:.1f})".format(
            DST, b.origin.x, b.origin.y, b.origin.z,
            b.box_extent.x, b.box_extent.y, b.box_extent.z))
        unreal.EditorAssetLibrary.save_asset(DST)
else:
    lines.append("исходный меш не найден")

with open(r"C:\Dev\RageStrike\Scripts\map_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
