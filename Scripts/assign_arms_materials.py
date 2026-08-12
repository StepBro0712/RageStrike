"""Раздаёт запечённым рукам общие материалы.

Материалы у всех 22 моделей одни и те же, поэтому в самих файлах их нет:
иначе одни и те же текстуры легли бы в проект по разу на ствол. Здесь
слоты по именам связываются с общей папкой.

Массив materials отдаётся копией, и правка элемента на месте до ассета не
доходит — список надо собрать заново и присвоить целиком.
"""
import unreal

BAKED = "/Game/Weapons/VM/Arms/Baked"
SHARED = "/Game/Weapons/VM/Arms/Shared/csgo_arms/Materials"

shared = {}
for a in unreal.EditorAssetLibrary.list_assets(SHARED, recursive=False):
    obj = unreal.load_asset(a)
    if isinstance(obj, unreal.MaterialInterface):
        shared[a.split("/")[-1].split(".")[0]] = obj

if not shared:
    unreal.log_warning("ARMSMAT общих материалов не найдено в {}".format(SHARED))

done = missed = 0
for path in unreal.EditorAssetLibrary.list_assets(BAKED, recursive=True):
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.SkeletalMesh):
        continue
    slots = mesh.get_editor_property("materials")
    rebuilt = []
    changed = False
    for m in slots:
        name = str(m.get_editor_property("material_slot_name"))
        if name in shared:
            if m.get_editor_property("material_interface") != shared[name]:
                m.set_editor_property("material_interface", shared[name])
                changed = True
        else:
            missed += 1
            unreal.log_warning("ARMSMAT нет общего материала для слота " + name)
        rebuilt.append(m)
    if changed:
        mesh.set_editor_property("materials", rebuilt)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        done += 1

unreal.log("ARMSMAT обновлено мешей {}, слотов без пары {}".format(done, missed))
