"""Создаёт материал дыма /Game/Effects/M_RSSmoke.

Зачем свой, а не из Niagara Examples: тамошние материалы дыма спрайтовые,
рассчитаны на атласы 8x8 и параметры от системы частиц — на статичную сферу
они не натягиваются. А сферы убрать нельзя, они блокируют канал камеры, и на
этом держится и зрение ботов, и определение выстрела сквозь дым в killfeed.

Материал неосвещаемый и полупрозрачный: непрозрачный BasicShapeMaterial и
делал из облака пластилиновые шары. Параметры меняет код:
  Color   — оттенок затяжки
  Opacity — прозрачность, ею же дым проявляется и тает
"""

import unreal

DEST = "/Game/Effects"
NAME = "M_RSSmoke"
PATH = "{}/{}".format(DEST, NAME)

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.EditorAssetLibrary.delete_asset(PATH)

mat = tools.create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
# Двусторонний: камера заходит внутрь облака, и без этого изнутри видно пустоту
mat.set_editor_property("two_sided", True)

col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
col.set_editor_property("parameter_name", "Color")
col.set_editor_property("default_value", unreal.LinearColor(0.62, 0.62, 0.65, 1.0))

op = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 200)
op.set_editor_property("parameter_name", "Opacity")
op.set_editor_property("default_value", 0.5)

mel.connect_material_property(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(PATH)
unreal.log("=== make_smoke_material: {} готов ===".format(PATH))
