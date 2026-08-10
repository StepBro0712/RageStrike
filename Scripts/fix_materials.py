import unreal

mesh = unreal.load_asset("/Game/Weapons/Sniper/Meshes/SKM_SniperR700")
mat = unreal.load_asset("/Game/Weapons/Sniper/Materials/M_Sniper")

if mesh and mat:
    for i in range(len(mesh.static_materials)):
        mesh.set_material(i, mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

result = [m.material_interface.get_name() if m.material_interface else "None"
          for m in mesh.static_materials] if mesh else ["NO MESH"]

with open(r"C:\Dev\RageStrike\Scripts\mesh_info.txt", "w") as f:
    f.write("sniper slots: {}".format(result))
