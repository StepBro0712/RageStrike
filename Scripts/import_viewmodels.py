"""Импорт вьюмоделей CS2 (руки + оружие + анимации) в /Game/Weapons/VM.

Исходники: GLB, полученные из моделей Source пакетной конвертацией
Scripts/convert_viewmodels.ps1 -> Blender + SourceIO.

Каждая модель кладётся в свою папку: внутренние имена (скелет, материалы)
у разных стволов совпадают, и в общей папке они перетёрли бы друг друга.
"""

import os
import unreal

SRC = r"C:\Dev\RageStrike\ImportSource\Weapons\CS2-Viewmodels"
DEST_ROOT = "/Game/Weapons/VM"

files = sorted(f for f in os.listdir(SRC) if f.lower().endswith(".glb"))
unreal.log("=== import_viewmodels: {} файлов ===".format(len(files)))

tasks = []
for f in files:
    name = os.path.splitext(f)[0]
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = "{}/{}".format(DEST_ROOT, name)
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

# Что получилось: скелетные меши и анимации по каждому стволу. Имена ассетов
# заранее не предсказать — Interchange раскладывает glTF по своим правилам,
# поэтому печатаем фактический результат, по нему и пишется загрузчик.
total_mesh = 0
total_anim = 0
for f in files:
    name = os.path.splitext(f)[0]
    path = "{}/{}".format(DEST_ROOT, name)
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.log_warning("{}: папка не создана".format(name))
        continue

    meshes, anims, other = [], [], []
    for a in unreal.EditorAssetLibrary.list_assets(path, recursive=True):
        obj = unreal.load_asset(a)
        if isinstance(obj, unreal.SkeletalMesh):
            meshes.append(a)
        elif isinstance(obj, unreal.AnimSequence):
            anims.append(a)
        else:
            other.append(a)

    total_mesh += len(meshes)
    total_anim += len(anims)
    unreal.log("VM {}: mesh={} anim={}".format(name, len(meshes), len(anims)))
    for m in meshes:
        unreal.log("    MESH {}".format(m))
    for a in anims[:20]:
        unreal.log("    ANIM {}".format(a))

unreal.log("=== import_viewmodels finished: {} мешей, {} анимаций ===".format(
    total_mesh, total_anim))
