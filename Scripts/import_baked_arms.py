"""Импорт запечённых рук — по одной модели на ствол.

Руки запечены в позу привязки каждого ствола (bake_arms_for_weapon.py),
поэтому модель у каждого оружия своя. Берём только те, у которых в проекте
есть скелетная вьюмодель: остальные стволы показываются статик-мешем, руки
им не к чему подчинять.
"""
import os
import unreal

SRC = r"C:\Dev\RageStrike\ImportSource\Characters\csgo-arms\baked"
VM_CONTENT = r"C:\Dev\RageStrike\Content\Weapons\VM"
DEST = "/Game/Weapons/VM/Arms/Baked"

packs = sorted(d for d in os.listdir(VM_CONTENT)
               if os.path.isdir(os.path.join(VM_CONTENT, d)) and d != "Arms")

tasks = []
for pack in packs:
    glb = os.path.join(SRC, "arms_%s.glb" % pack)
    if not os.path.exists(glb):
        unreal.log_warning("нет запечённых рук для {}".format(pack))
        continue
    t = unreal.AssetImportTask()
    t.filename = glb
    t.destination_path = DEST
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

meshes = [a for a in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True)
          if isinstance(unreal.load_asset(a), unreal.SkeletalMesh)]
unreal.log("=== запечённых рук импортировано: {} из {} стволов ==="
           .format(len(meshes), len(packs)))

# Материалы в файлах не лежат — раздаём общие.
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "assign_arms_materials.py"), encoding="utf-8").read())
