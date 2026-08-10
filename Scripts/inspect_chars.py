import unreal

lines = []

targets = [
    "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple",
    "/Game/QuantumCharacter/Mesh/SKM_QuantumCharacter",
    "/Game/Insurgent_2/Mesh/SK_Preset3",
    "/Game/Insurgent_2/Mesh/SK_BaseBody",
]

for path in targets:
    mesh = unreal.load_asset(path)
    if mesh is None:
        lines.append("{} MISSING".format(path))
        continue
    skel = mesh.get_editor_property("skeleton")
    b = mesh.get_bounds().box_extent
    lines.append("{}\n   skeleton={}\n   extent=({:.0f},{:.0f},{:.0f})".format(
        path, skel.get_path_name() if skel else "None", b.x, b.y, b.z))

lines.append("--- assets in packs ---")
for folder in ("/Game/QuantumCharacter", "/Game/Insurgent_2"):
    for p in unreal.EditorAssetLibrary.list_assets(folder, recursive=True):
        obj = unreal.load_asset(p)
        cls = obj.get_class().get_name() if obj else "?"
        if cls in ("AnimBlueprint", "AnimSequence", "Skeleton", "SkeletalMesh", "Blueprint",
                   "IKRetargeter", "IKRigDefinition"):
            lines.append("   {} :: {}".format(p, cls))

with open(r"C:\Dev\RageStrike\Scripts\map_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
