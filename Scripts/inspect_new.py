import unreal

lines = []
arms = unreal.load_asset("/Game/FP_AKS74U_Animation/Demo/FirstPersonArms/Character/Mesh/SK_FP_Manny_Simple")
lines.append("arms asset: {}".format(arms))

if arms:
    b = arms.get_bounds()
    lines.append("bounds origin=({:.1f},{:.1f},{:.1f}) extent=({:.1f},{:.1f},{:.1f})".format(
        b.origin.x, b.origin.y, b.origin.z, b.box_extent.x, b.box_extent.y, b.box_extent.z))

    # имена костей через временный компонент
    world = unreal.EditorLevelLibrary.get_editor_world()
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0))
    comp = actor.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(arms)
    names = comp.get_all_socket_names()
    lines.append("сокеты компонента: {}".format(names[:40]))
    bones = []
    for i in range(comp.get_num_bones()):
        bones.append(str(comp.get_bone_name(i)))
    lines.append("костей: {}".format(len(bones)))
    lines.append("кости: {}".format(bones))
    unreal.EditorLevelLibrary.destroy_actor(actor)

with open(r"C:\Dev\RageStrike\Scripts\map_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
