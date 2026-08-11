"""Пакетная конвертация вьюмоделей Source (.mdl) в GLB для Unreal.

Запускается по одному файлу за вызов Blender: сцена гарантированно чистая,
и падение на одной модели не уносит остальные.
"""
import bpy, sys, os

argv = sys.argv[sys.argv.index("--") + 1:]
MDL, OUT = argv[0], argv[1]

# Сброс до включения аддона: read_factory_settings сбрасывает и список
# включённых аддонов вместе с ним.
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.preferences.addon_enable(module="SourceIO-master")

name = os.path.splitext(os.path.basename(MDL))[0]

try:
    bpy.ops.sourceio.mdl(
        directory=os.path.dirname(MDL) + os.sep,
        files=[{"name": os.path.basename(MDL)}],
        import_animations=True,
        import_include_animations=True,
        import_textures=True,
        write_qc=False,
    )
except Exception as e:
    print("RESULT %s FAILED import: %s" % (name, e))
    sys.exit(0)

meshes = [o for o in bpy.data.objects if o.type == "MESH"]
arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
acts = list(bpy.data.actions)

if not meshes or not arms:
    print("RESULT %s EMPTY meshes=%d arms=%d" % (name, len(meshes), len(arms)))
    sys.exit(0)

# Склеиваем меши в один. У стволов с подвижными частями (затвор, магазин,
# спуск) это отдельные объекты на общем скелете, glTF вывозит их раздельно,
# и в проект приезжало по пять скелетных мешей на ствол — а компонент
# вьюмодели в игре один. Скелет и веса при склейке сохраняются.
if len(meshes) > 1:
    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    meshes[0].name = name
    bpy.ops.object.join()
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
else:
    meshes[0].name = name

# Каждое действие кладём в NLA: без этого glTF выгрузит только активное,
# и из девяти анимаций доехала бы одна.
arm = arms[0]
if not arm.animation_data:
    arm.animation_data_create()
for a in acts:
    track = arm.animation_data.nla_tracks.new()
    track.name = a.name
    track.strips.new(a.name, int(a.frame_range[0]), a)

try:
    bpy.ops.export_scene.gltf(
        filepath=OUT,
        export_format="GLB",
        export_animations=True,
        export_nla_strips=True,
        export_skins=True,
        export_apply=False,
        use_selection=False,
    )
    size = os.path.getsize(OUT) / 1024.0 / 1024.0
    print("RESULT %s OK bones=%d anims=%d mesh=%d size=%.1fMB" % (
        name, len(arm.data.bones), len(acts), len(meshes), size))
except Exception as e:
    print("RESULT %s FAILED export: %s" % (name, e))
