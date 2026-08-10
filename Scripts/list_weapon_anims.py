"""Что именно приехало из CS2-шных GLB: скелетные меши и анимации."""

import unreal

FOLDERS = ["/Game/Weapons/CS2/AK47", "/Game/Weapons/CS2/USPS"]
KEEP = ("idle", "draw", "reload", "shoot", "inspect", "lookat")

lines = []
for folder in FOLDERS:
    skeletals, anims = [], []
    for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=True):
        obj = unreal.load_asset(path)
        if isinstance(obj, unreal.SkeletalMesh):
            b = obj.get_bounds().box_extent
            skeletals.append("SK  {}  extent=({:.1f},{:.1f},{:.1f})".format(
                obj.get_path_name(), b.x, b.y, b.z))
        elif isinstance(obj, unreal.AnimSequence):
            low = obj.get_name().lower()
            if any(k in low for k in KEEP):
                anims.append("AN  {}  {:.2f}s".format(
                    obj.get_path_name(), obj.get_editor_property("sequence_length")))

    lines.append("=== {}  скелетов={} анимаций(отобрано)={}".format(
        folder, len(skeletals), len(anims)))
    lines += skeletals[:6]
    lines += sorted(anims)[:26]

with open(r"C:\Dev\RageStrike\Scripts\weapon_anims.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

unreal.log("=== list_weapon_anims.py finished ===")
