"""Создаёт пост-процесс материал M_RSChams для подсветки моделей сквозь стены.

Логика материала: там, где CustomDepth (глубина помеченных мешей) меньше
SceneDepth (глубина того, что реально видно), модель закрыта геометрией —
значит рисуем поверх неё цвет. В остальных местах отдаём картинку как есть.

Запуск: UnrealEditor-Cmd.exe <проект> -run=pythonscript -script="<этот файл>"
"""

import unreal

PKG = "/Game/UI/M_RSChams"

tools = unreal.AssetToolsHelpers.get_asset_tools()
if unreal.EditorAssetLibrary.does_asset_exist(PKG):
    unreal.EditorAssetLibrary.delete_asset(PKG)

mat = tools.create_asset("M_RSChams", "/Game/UI", unreal.Material,
                         unreal.MaterialFactoryNew())
mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)

lib = unreal.MaterialEditingLibrary


def scene_tex(tex_id, x, y):
    node = lib.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, x, y)
    node.set_editor_property("scene_texture_id", tex_id)
    return node


custom_depth = scene_tex(unreal.SceneTextureId.PPI_CUSTOM_DEPTH, -900, -200)
scene_depth = scene_tex(unreal.SceneTextureId.PPI_SCENE_DEPTH, -900, 0)
scene_color = scene_tex(unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -900, 300)

# глубины лежат в красном канале
mask_a = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -650, -200)
mask_a.set_editor_property("r", True)
mask_a.set_editor_property("g", False)
mask_a.set_editor_property("b", False)
mask_a.set_editor_property("a", False)

mask_b = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -650, 0)
mask_b.set_editor_property("r", True)
mask_b.set_editor_property("g", False)
mask_b.set_editor_property("b", False)
mask_b.set_editor_property("a", False)

lib.connect_material_expressions(custom_depth, "Color", mask_a, "")
lib.connect_material_expressions(scene_depth, "Color", mask_b, "")

# цвет подсветки — параметр, чтобы менять из кода
color = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -650, 200)
color.set_editor_property("parameter_name", "ChamsColor")
color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.15, 0.15, 1.0))

# if (CustomDepth < SceneDepth) -> цвет, иначе исходная картинка
node_if = lib.create_material_expression(mat, unreal.MaterialExpressionIf, -300, 0)
lib.connect_material_expressions(mask_a, "", node_if, "A")
lib.connect_material_expressions(mask_b, "", node_if, "B")
lib.connect_material_expressions(scene_color, "Color", node_if, "AGreaterThan")
lib.connect_material_expressions(scene_color, "Color", node_if, "AEqualsB")
lib.connect_material_expressions(color, "", node_if, "ALessThan")

lib.connect_material_property(node_if, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(PKG)
unreal.log("RS: chams material ready at %s" % PKG)
