"""Запекает руки в позу привязки конкретного ствола.

Зачем. В игре руки подчинены скелету оружия через SetLeaderPoseComponent:
вершина переводится в локальную систему своей кости обратной матрицей
привязки рук, а затем ставится в кость оружия. Пока позы привязки у двух
моделей разные, на каждой кости получается свой перенос, и там, где
вершина поделена между соседними костями, меш растаскивает — на замерах
рвало 3.3% рёбер, местами в 36 раз.

В Source у каждой вьюмодели своя поза привязки, с уже сложенным хватом, и
у AK, USP и AWP они разные. Поэтому руки печём под каждый ствол отдельно:
ставим скелет рук ровно в позу привязки оружия, вжигаем деформацию в
вершины и объявляем эту позу новой позой привязки. После этого переносы
совпадают тождественно, и швы не рвёт вообще.

Кости, которых в скелете оружия нет, оставляем как есть.
"""
import bpy, sys, os

argv = sys.argv[sys.argv.index("--") + 1:]
GUN, ARMS, OUT = argv[0], argv[1], argv[2]

bpy.ops.wm.read_factory_settings(use_empty=True)

bpy.ops.import_scene.gltf(filepath=GUN)
gun_arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
GUNREF = {b.name: b.matrix_local.copy() for b in gun_arm.data.bones}

# Ствол дальше не нужен, только его поза привязки. Если оставить объекты в
# сцене, экспорт утащит их в файл рук: гоняли так и получили 14 МБ вместо
# полумегабайта, да ещё со вторым скелетом внутри.
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)

bpy.ops.import_scene.gltf(filepath=ARMS)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
mesh = next(o for o in bpy.data.objects if o.type == "MESH")

# Родителей выставляем раньше детей: поза кости в пространстве арматуры
# зависит от уже применённой позы родителя.
def depth(b):
    d = 0
    while b.parent:
        b, d = b.parent, d + 1
    return d

bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="POSE")

posed = 0
for pb in sorted(arm.pose.bones, key=lambda p: depth(p.bone)):
    target = GUNREF.get(pb.name)
    if target is None:
        continue
    pb.matrix = target
    bpy.context.view_layer.update()
    posed += 1

bpy.ops.object.mode_set(mode="OBJECT")

# Вжигаем деформацию в вершины копией модификатора: сам модификатор должен
# остаться, иначе меш отвяжется от скелета.
bpy.context.view_layer.objects.active = mesh
mod = next(m for m in mesh.modifiers if m.type == "ARMATURE")
before = set(m.name for m in mesh.modifiers)
bpy.ops.object.modifier_copy(modifier=mod.name)
copy_name = next(m.name for m in mesh.modifiers if m.name not in before)
bpy.ops.object.modifier_apply(modifier=copy_name)

# Теперь текущая поза объявляется позой привязки: меш уже в ней.
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.armature_apply()
bpy.ops.object.mode_set(mode="OBJECT")

# Текстуры из запечённых файлов выкидываем, оставляя пустые слоты: модель
# рук печётся под каждый ствол, и одни и те же 8 МБ картинок легли бы в
# проект 22 раза. Материалы подставляются при импорте из общей папки, слоты
# для этого достаточно сохранить по именам.
for mat in bpy.data.materials:
    if mat.use_nodes:
        for n in [n for n in mat.node_tree.nodes if n.type == "TEX_IMAGE"]:
            mat.node_tree.nodes.remove(n)
for img in list(bpy.data.images):
    bpy.data.images.remove(img)

bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLB",
                          export_animations=False, export_skins=True,
                          export_apply=False, use_selection=False)

print("RESULT %s OK костей в позе %d из %d, размер %.1fMB" % (
    os.path.splitext(os.path.basename(GUN))[0], posed, len(arm.data.bones),
    os.path.getsize(OUT) / 1024.0 / 1024.0))
