"""Помечает скелеты скачанных персонажей совместимыми со скелетом манекена,
чтобы наш ABP_Unarmed играл на них без ретаргета."""

import unreal

MANNY = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
OTHERS = [
    "/Game/QuantumCharacter/Mesh/SK_Military_Character_Skeleton",
    "/Game/Insurgent_2/Mesh/SKM_BaseBody_Skeleton",
]

lines = []
manny = unreal.load_asset(MANNY)
lines.append("manny={}".format(manny))


def link(target, other, label):
    """Пробует все известные способы связать скелеты."""
    try:
        target.add_compatible_skeleton(other)
        unreal.EditorAssetLibrary.save_loaded_asset(target)
        lines.append("  {} add_compatible_skeleton OK".format(label))
        return True
    except Exception as e:
        lines.append("  {} add_compatible_skeleton failed: {}".format(label, e))

    try:
        current = list(target.get_editor_property("compatible_skeletons"))
        current.append(other)
        target.set_editor_property("compatible_skeletons", current)
        unreal.EditorAssetLibrary.save_loaded_asset(target)
        lines.append("  {} compatible_skeletons property OK".format(label))
        return True
    except Exception as e:
        lines.append("  {} compatible_skeletons property failed: {}".format(label, e))
    return False


for path in OTHERS:
    skel = unreal.load_asset(path)
    lines.append("{} -> {}".format(path, skel))
    if skel is None or manny is None:
        continue
    link(skel, manny, "other<-manny")
    link(manny, skel, "manny<-other")

with open(r"C:\Dev\RageStrike\Scripts\map_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
