"""Собирает руки CS:GO из моделей CS:SO в один GLB для Unreal.

Руки в CS:GO разложены на две модели: рукав (v_sleeve_*) — плечо и
предплечье, и перчатка (v_glove_*) — кисть с пальцами. В игре они
отрисовываются вместе на общем скелете, поэтому здесь их надо склеить.

Ни переименования костей, ни масштабирования: скелет у этих моделей
v_weapon.Bip01_* — тот же самый, что внутри наших вьюмоделей оружия,
кость в кость и с той же позой привязки. Поэтому в игре руки подчиняются
скелету оружия через SetLeaderPoseComponent как есть.

Анимации не экспортируем: их даёт оружие.
"""
import bpy, sys, os

argv = sys.argv[sys.argv.index("--") + 1:]
OUT = argv[-1]
MDLS = argv[:-1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.preferences.addon_enable(module="SourceIO-master")

for mdl in MDLS:
    bpy.ops.sourceio.mdl(
        directory=os.path.dirname(mdl) + os.sep,
        files=[{"name": os.path.basename(mdl)}],
        import_animations=False,
        import_textures=True,
        write_qc=False,
    )
    print("IMPORTED %s" % os.path.basename(mdl))

arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
print("после импорта: арматур %d, мешей %d" % (len(arms), len(meshes)))
if not arms or not meshes:
    print("RESULT FAILED пусто")
    sys.exit(0)

# Каждый .mdl приехал со своей копией скелета. Оставляем одну и переводим
# на неё все меши: имена костей совпадают, поэтому группы вершин находят
# свои кости сами, веса при этом не трогаются.
#
# Берём самую полную: у рукава в скелете только плечо с предплечьем (18
# костей), у перчатки — весь набор с пальцами (48). Оставь мы рукав,
# пальцам не к чему было бы привязаться.
keep = max(arms, key=lambda a: len(a.data.bones))
for m in meshes:
    for mod in m.modifiers:
        if mod.type == "ARMATURE":
            mod.object = keep
    m.parent = keep

for a in arms[1:]:
    bpy.data.objects.remove(a, do_unlink=True)

if len(meshes) > 1:
    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()

final = [o for o in bpy.data.objects if o.type == "MESH"]
final[0].name = "csgo_arms"

# Материалы SourceIO собирает из десятка нод под шейдер Source, и экспорт в
# glTF из такого дерева вытаскивает не всё: из трёх текстур доезжала одна.
# Поэтому дерево заменяем на простейшее — картинка в базовый цвет.
# Карту нормалей не тянем: в Unreal материалы всё равно пересобираются.
for mat in bpy.data.materials:
    if not mat.use_nodes:
        continue
    base = None
    for n in mat.node_tree.nodes:
        if n.type == "TEX_IMAGE" and n.image and "normal" not in n.image.name.lower():
            base = n.image
            break
    if base is None:
        continue
    mat.node_tree.nodes.clear()
    out = mat.node_tree.nodes.new("ShaderNodeOutputMaterial")
    bsdf = mat.node_tree.nodes.new("ShaderNodeBsdfPrincipled")
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = base
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    mat.node_tree.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    print("MATERIAL %s <- %s" % (mat.name, base.name))

bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLB",
                          export_animations=False, export_skins=True,
                          export_apply=False, use_selection=False)

print("RESULT OK bones=%d mesh=%d verts=%d size=%.1fMB" % (
    len(keep.data.bones), len(final), len(final[0].data.vertices),
    os.path.getsize(OUT) / 1024.0 / 1024.0))
