"""Импорт скачанных моделей оружия (FBX + PBR-текстуры) в проект.

Запуск (редактор должен быть закрыт):
  UnrealEditor-Cmd.exe RageStrike.uproject -run=pythonscript -script="Scripts/import_weapons.py"

Для каждого оружия: импортирует меш как Static Mesh, импортирует текстуры,
собирает материал и назначает его мешу. В конце печатает габариты меша,
чтобы подобрать масштаб в C++.
"""

import os
import unreal

WEAPONS = [
    {
        "name": "Sniper",
        "fbx": r"C:\Dev\RageStrike\ImportSource\Sniper\Assets\SKM_SniperR700.fbx",
        "textures": r"C:\Dev\RageStrike\ImportSource\Sniper\Assets",
    },
    {
        "name": "Knife",
        "fbx": r"C:\Dev\RageStrike\ImportSource\Knife\MeleeWeaponsPack#1\Combat_Knife\Combat_Knife.fbx",
        "textures": r"C:\Dev\RageStrike\ImportSource\Knife\MeleeWeaponsPack#1\Combat_Knife",
    },
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mat_lib = unreal.MaterialEditingLibrary


def classify(filename):
    """Определяет роль текстуры по имени файла."""
    low = os.path.basename(filename).lower()
    if "normal" in low or low.endswith("_n.png"):
        return "normal"
    if "maor" in low:
        return "maor"
    if "base_color" in low or "basecolor" in low or low.endswith("_c.png") or "albedo" in low:
        return "color"
    if "roughness" in low:
        return "roughness"
    if "metallic" in low:
        return "metallic"
    return None


def import_files(files, destination, options):
    tasks = []
    for f in files:
        task = unreal.AssetImportTask()
        task.filename = f
        task.destination_path = destination
        task.automated = True
        task.replace_existing = True
        task.save = True
        if options is not None:
            task.options = options
        tasks.append(task)
    if not tasks:
        return []
    asset_tools.import_asset_tasks(tasks)
    imported = []
    for task in tasks:
        imported.extend(list(task.get_editor_property("imported_object_paths")))
    return imported


def make_mesh_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = False
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.generate_lightmap_u_vs = True
    options.static_mesh_import_data.auto_generate_collision = False
    return options


def process(weapon):
    name = weapon["name"]
    base = "/Game/Weapons/{}".format(name)

    if not os.path.isfile(weapon["fbx"]):
        unreal.log_warning("[{}] нет файла {}".format(name, weapon["fbx"]))
        return

    mesh_paths = import_files([weapon["fbx"]], base + "/Meshes", make_mesh_options())
    mesh = None
    for p in mesh_paths:
        obj = unreal.load_asset(p)
        if isinstance(obj, unreal.StaticMesh):
            mesh = obj
            break
    if mesh is None:
        unreal.log_warning("[{}] статик-меш не импортировался".format(name))
        return

    # текстуры
    tex_files = {}
    for f in os.listdir(weapon["textures"]):
        full = os.path.join(weapon["textures"], f)
        if not os.path.isfile(full) or not f.lower().endswith((".png", ".tga", ".jpg")):
            continue
        role = classify(full)
        if role and role not in tex_files:
            tex_files[role] = full

    imported_tex = import_files(list(tex_files.values()), base + "/Textures", None)
    textures = {}
    for role, src in tex_files.items():
        stem = os.path.splitext(os.path.basename(src))[0]
        for p in imported_tex:
            if p.split(".")[-1] == stem or p.endswith("/" + stem):
                textures[role] = unreal.load_asset(p)
                break

    # нормалку и данные-текстуры переводим в линейное пространство
    for role, tex in textures.items():
        if tex is None:
            continue
        if role == "normal":
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif role in ("roughness", "metallic", "maor"):
            tex.set_editor_property("srgb", False)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)

    # материал
    mat = asset_tools.create_asset("M_" + name, base + "/Materials",
                                   unreal.Material, unreal.MaterialFactoryNew())

    def add_sample(tex, x, y):
        node = mat_lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
        node.texture = tex
        return node

    if textures.get("color"):
        node = add_sample(textures["color"], -400, -300)
        mat_lib.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if textures.get("normal"):
        node = add_sample(textures["normal"], -400, 0)
        mat_lib.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if textures.get("maor"):
        # упаковка MAOR: R - metallic, G - ambient occlusion, B - roughness
        node = add_sample(textures["maor"], -400, 300)
        mat_lib.connect_material_property(node, "R", unreal.MaterialProperty.MP_METALLIC)
        mat_lib.connect_material_property(node, "G", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        mat_lib.connect_material_property(node, "B", unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        if textures.get("roughness"):
            node = add_sample(textures["roughness"], -400, 300)
            mat_lib.connect_material_property(node, "R", unreal.MaterialProperty.MP_ROUGHNESS)
        if textures.get("metallic"):
            node = add_sample(textures["metallic"], -400, 600)
            mat_lib.connect_material_property(node, "R", unreal.MaterialProperty.MP_METALLIC)

    mat_lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)

    mesh.set_material(0, mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    bounds = mesh.get_bounds().box_extent
    unreal.log("[{}] mesh={} extent=({:.1f}, {:.1f}, {:.1f}) textures={}".format(
        name, mesh.get_path_name(), bounds.x, bounds.y, bounds.z, sorted(textures.keys())))


for w in WEAPONS:
    process(w)

unreal.log("=== import_weapons.py finished ===")
