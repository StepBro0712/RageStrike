"""Создаёт эмиссивный материал M_RSSun для солнца в лобби.

Обычный BasicShapeMaterial освещается сценой и остаётся серым, поэтому
светило собирается отдельным материалом: unlit, цвет уходит прямо в эмиссию
и умножается на яркость. Оба значения — параметры, чтобы менять из кода.

Запуск: UnrealEditor-Cmd.exe <проект> -run=pythonscript -script="<этот файл>"
"""

import unreal

PKG = "/Game/UI/M_RSSun"

tools = unreal.AssetToolsHelpers.get_asset_tools()
if unreal.EditorAssetLibrary.does_asset_exist(PKG):
    unreal.EditorAssetLibrary.delete_asset(PKG)

mat = tools.create_asset("M_RSSun", "/Game/UI", unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

lib = unreal.MaterialEditingLibrary

color = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, -100)
color.set_editor_property("parameter_name", "SunColor")
color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.86, 0.55, 1.0))

power = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 120)
power.set_editor_property("parameter_name", "Intensity")
power.set_editor_property("default_value", 30.0)

mul = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 0)
lib.connect_material_expressions(color, "", mul, "A")
lib.connect_material_expressions(power, "", mul, "B")
lib.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(PKG)
unreal.log("RS: sun material ready at %s" % PKG)
