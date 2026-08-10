import unreal

lines = []
try:
    unreal.EditorLoadingAndSavingUtils.load_map("/Engine/Maps/Entry")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    lines.append("Entry actors: {}".format(len(actors)))
    for a in actors:
        lines.append("  {} :: {}".format(a.get_name(), a.get_class().get_name()))
except Exception as e:
    lines.append("ERROR: {}".format(e))

with open(r"C:\Dev\RageStrike\Scripts\map_info.txt", "w") as f:
    f.write("\n".join(lines))
