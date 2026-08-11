"""Импорт скачанных CS2-моделей оружия в /Game/Weapons/CS2/<Имя>.

Запуск (редактор закрыт):
  UnrealEditor-Cmd.exe RageStrike.uproject -run=pythonscript -script="Scripts/import_cs2_weapons.py"

Для каждой модели: импорт меша, переименование в SM_<Имя> (чтобы путь в C++
был предсказуемым), импорт текстур из папки, сборка материала. В конце пишет
Scripts/cs2_weapons_info.txt с габаритами — по ним подбирается масштаб в C++.
"""

import os
import unreal

SRC = r"C:\Dev\RageStrike\ImportSource"

# имя в игре -> файл модели. Текстуры берём из папки самой модели,
# запасной вариант — папка textures рядом со скачанным архивом.
WEAPONS = [
    ("AK47",   r"Weapons\rifle-ak-47-weapon-model-cs2\source\AK47.glb"),
    ("USPS",   r"Weapons\pistol-usp-s-weapon-model-cs2\source\USPS.glb"),
    ("Deagle", r"Weapons\deagle-counter-strike-2\source\unpacked\Deagle\Deagle_CS2.fbx"),
    ("M4A4",   r"Weapons\m4a4-counter-strike-2\source\unpacked\M4A4\M4A4_CS2.fbx"),
    ("M4A1S",  r"Weapons\m4a1s-counter-strike-2\source\unpacked\M4A1S\M4A1S_CS2.fbx"),
    ("AUG",    r"Weapons\aug-counter-strike-2\source\unpacked\AUG\AUG_CS2.fbx"),
    ("Famas",  r"Weapons\famas-counter-strike-2\source\unpacked\Famas\Famas_CS2.fbx"),
    ("Galil",  r"Weapons\galil-ar-counter-strike-2\source\unpacked\GalilAr\GalilAr_CS2.fbx"),
    ("MP7",    r"Weapons\mp7-counter-strike-2\source\unpacked\MP7\MP7_CS2.fbx"),
    ("M249",   r"Weapons\m249-counter-strike-2\source\unpacked\M249\M249_CS2.fbx"),
    ("Negev",  r"Weapons\negev-counter-strike-2\source\unpacked\Negev\Negev_CS2.fbx"),
    ("MP9",    r"Weapons\mp9\source\unpacked\MP9.obj"),
    ("AWP",    r"Weapons\awp\source\unpacked\AWP.obj"),
    ("Bizon",  r"Weapons\bizon\source\unpacked\bizon.obj"),
    ("C4",     r"Weapons\bomb-c4-explosive-model-cs2\source\C4.glb"),
    ("Armor",  r"Characters\armor-armor-and-helmet-model-cs2\source\ARMORandHELMET.glb"),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mat_lib = unreal.MaterialEditingLibrary
lines = []


def classify(path):
    """Роль текстуры по имени файла: у разных авторов свои суффиксы."""
    low = os.path.basename(path).lower()
    if "normal" in low or "_n." in low:
        return "normal"
    if "maor" in low or "orm" in low:
        return "maor"
    if "rough" in low:
        return "roughness"
    if "metal" in low:
        return "metallic"
    if "color" in low or "albedo" in low or "diffuse" in low or "basecolor" in low or "_c." in low:
        return "color"
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
    out = []
    for task in tasks:
        out.extend(list(task.get_editor_property("imported_object_paths")))
    return out


def mesh_options():
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_materials = False
    o.import_textures = False
    o.import_as_skeletal = False
    o.static_mesh_import_data.combine_meshes = True
    o.static_mesh_import_data.generate_lightmap_u_vs = True
    o.static_mesh_import_data.auto_generate_collision = False
    return o


def gltf_options():
    """glTF из CS2 приезжает со скелетом и десятками анимаций.
    Просим Interchange собрать всё одним статик-мешем."""
    try:
        p = unreal.InterchangeGenericAssetsPipeline()
        p.common_meshes_properties.force_all_mesh_as_type = \
            unreal.InterchangeForceMeshType.IFMT_STATIC_MESH
        p.mesh_pipeline.import_static_meshes = True
        p.mesh_pipeline.import_skeletal_meshes = False
        p.mesh_pipeline.combine_static_meshes = True
        p.animation_pipeline.import_animations = False
        return p
    except Exception as ex:
        unreal.log_warning("gltf_options: {}".format(ex))
        return None


def biggest_mesh(folder):
    """Самый крупный статик-меш в папке: у glTF модель приезжает кусками."""
    best, best_vol = None, -1.0
    for p in unreal.EditorAssetLibrary.list_assets(folder, recursive=True):
        obj = unreal.load_asset(p)
        if not isinstance(obj, unreal.StaticMesh):
            continue
        e = obj.get_bounds().box_extent
        vol = max(e.x, 0.01) * max(e.y, 0.01) * max(e.z, 0.01)
        if vol > best_vol:
            best, best_vol = obj, vol
    return best


def build_material(name, base, tex_dir):
    if not tex_dir or not os.path.isdir(tex_dir):
        return None

    found = {}
    for f in sorted(os.listdir(tex_dir)):
        full = os.path.join(tex_dir, f)
        if not os.path.isfile(full) or not f.lower().endswith((".png", ".tga", ".jpg")):
            continue
        role = classify(full)
        if role and role not in found:
            found[role] = full

    # единственная текстура без внятного имени — считаем её цветом
    if "color" not in found:
        for f in sorted(os.listdir(tex_dir)):
            full = os.path.join(tex_dir, f)
            if os.path.isfile(full) and f.lower().endswith((".png", ".tga", ".jpg")) \
                    and classify(full) is None:
                found["color"] = full
                break

    if not found:
        return None

    imported = import_files(list(found.values()), base + "/Textures", None)
    textures = {}
    for role, src in found.items():
        stem = os.path.splitext(os.path.basename(src))[0]
        for p in imported:
            if p.split(".")[-1] == stem or p.endswith("/" + stem):
                textures[role] = unreal.load_asset(p)
                break

    for role, tex in textures.items():
        if tex is None:
            continue
        if role == "normal":
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif role in ("roughness", "metallic", "maor"):
            tex.set_editor_property("srgb", False)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)

    mat_path = base + "/Materials/M_" + name
    mat = unreal.load_asset(mat_path)
    if mat is None:
        mat = asset_tools.create_asset("M_" + name, base + "/Materials",
                                       unreal.Material, unreal.MaterialFactoryNew())

        def sample(tex, x, y):
            if tex is None:
                return None
            node = mat_lib.create_material_expression(
                mat, unreal.MaterialExpressionTextureSample, x, y)
            if node:
                node.texture = tex
            return node

        n = sample(textures.get("color"), -400, -300)
        if n:
            mat_lib.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        n = sample(textures.get("normal"), -400, 0)
        if n:
            mat_lib.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
        if textures.get("maor"):
            n = sample(textures["maor"], -400, 300)
            if n:
                mat_lib.connect_material_property(n, "R", unreal.MaterialProperty.MP_METALLIC)
                mat_lib.connect_material_property(n, "G", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
                mat_lib.connect_material_property(n, "B", unreal.MaterialProperty.MP_ROUGHNESS)
        else:
            n = sample(textures.get("roughness"), -400, 300)
            if n:
                mat_lib.connect_material_property(n, "R", unreal.MaterialProperty.MP_ROUGHNESS)
            n = sample(textures.get("metallic"), -400, 600)
            if n:
                mat_lib.connect_material_property(n, "R", unreal.MaterialProperty.MP_METALLIC)

        mat_lib.recompile_material(mat)
        unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def texture_dir(model_path):
    """Текстуры лежат рядом с моделью; если нет — в папке textures пакета."""
    own = os.path.dirname(model_path)
    for f in os.listdir(own):
        if f.lower().endswith((".png", ".tga", ".jpg")):
            return own
    # поднимаемся до корня скачанного пакета и смотрим textures
    probe = own
    for _ in range(4):
        probe = os.path.dirname(probe)
        alt = os.path.join(probe, "textures")
        if os.path.isdir(alt):
            return alt
    return None


def process(name, rel_model):
    model = os.path.join(SRC, rel_model)
    if not os.path.isfile(model):
        lines.append("{}: НЕТ ФАЙЛА {}".format(name, model))
        return

    base = "/Game/Weapons/CS2/{}".format(name)
    mesh_dir = base + "/Meshes"

    is_gltf = model.lower().endswith((".glb", ".gltf"))
    # прошлый заход мог оставить скелетные меши — убираем, чтобы не мешали
    if is_gltf and unreal.EditorAssetLibrary.does_directory_exist(mesh_dir):
        unreal.EditorAssetLibrary.delete_directory(mesh_dir)

    import_files([model], mesh_dir, gltf_options() if is_gltf else mesh_options())

    mesh = biggest_mesh(mesh_dir)
    if mesh is None:
        lines.append("{}: меш не импортировался".format(name))
        return

    # предсказуемое имя ассета — путь зашивается в C++
    target = "SM_" + name
    if mesh.get_name() != target:
        new_path = mesh_dir + "/" + target
        if unreal.EditorAssetLibrary.does_asset_exist(new_path):
            unreal.EditorAssetLibrary.delete_asset(new_path)
        unreal.EditorAssetLibrary.rename_asset(mesh.get_path_name(), new_path)
        mesh = unreal.load_asset(new_path)

    mat = build_material(name, base, texture_dir(model))
    if mat is not None:
        for i in range(max(1, len(mesh.static_materials))):
            mesh.set_material(i, mat)

    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    e = mesh.get_bounds().box_extent
    o = mesh.get_bounds().origin
    axes = {"X": e.x, "Y": e.y, "Z": e.z}
    longest = max(axes, key=axes.get)
    lines.append(
        "{:8s} {:44s} extent=({:7.1f},{:7.1f},{:7.1f}) origin=({:6.1f},{:6.1f},{:6.1f}) длинная ось={} mat={}".format(
            name, mesh.get_path_name(), e.x, e.y, e.z, o.x, o.y, o.z, longest, mat is not None))


for entry in WEAPONS:
    try:
        process(*entry)
    except Exception as ex:
        lines.append("{}: ОШИБКА {}".format(entry[0], ex))

with open(r"C:\Dev\RageStrike\Scripts\cs2_weapons_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

unreal.log("=== import_cs2_weapons.py finished ===")
