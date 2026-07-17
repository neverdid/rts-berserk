"""Builds Vowfall's source-controlled world master materials in Unreal Editor.

Run with UnrealEditor-Cmd.exe and -ExecutePythonScript. The script is intentionally
idempotent so artists can regenerate the material assets after changing parameters.
"""

from __future__ import annotations

import unreal


MATERIAL_ROOT = "/Game/Art/Materials"
SURFACE_PATH = f"{MATERIAL_ROOT}/M_VowfallSurface"
WATER_PATH = f"{MATERIAL_ROOT}/M_VowfallWater"


def _load_or_create_material(asset_name: str) -> unreal.Material:
    asset_path = f"{MATERIAL_ROOT}/{asset_name}"
    material = unreal.load_asset(asset_path)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            MATERIAL_ROOT,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"{asset_path} exists but is not a Material")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    return material


def _expression(
    material: unreal.Material,
    expression_class: type,
    x: int,
    y: int,
    **properties: object,
) -> unreal.MaterialExpression:
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )
    if expression is None:
        raise RuntimeError(f"Could not create {expression_class.__name__}")
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression


def _connect(
    source: unreal.MaterialExpression,
    target: unreal.MaterialExpression,
    target_input: str,
    source_output: str = "",
) -> None:
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source, source_output, target, target_input
    ):
        source_outputs = list(
            unreal.MaterialEditingLibrary.get_material_expression_output_names(source)
        )
        target_inputs = list(
            unreal.MaterialEditingLibrary.get_material_expression_input_names(target)
        )
        raise RuntimeError(
            f"Could not connect {source.get_name()} to "
            f"{target.get_name()}.{target_input}; "
            f"source outputs={source_outputs}, target inputs={target_inputs}"
        )


def _connect_property(
    source: unreal.MaterialExpression,
    material_property: unreal.MaterialProperty,
    source_output: str = "",
) -> None:
    if not unreal.MaterialEditingLibrary.connect_material_property(
        source, source_output, material_property
    ):
        raise RuntimeError(
            f"Could not connect {source.get_name()} to {material_property}"
        )


def _vector_parameter(
    material: unreal.Material,
    name: str,
    value: tuple[float, float, float, float],
    x: int,
    y: int,
) -> unreal.MaterialExpression:
    return _expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        x,
        y,
        parameter_name=name,
        default_value=unreal.LinearColor(*value),
    )


def _scalar_parameter(
    material: unreal.Material,
    name: str,
    value: float,
    x: int,
    y: int,
) -> unreal.MaterialExpression:
    return _expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        x,
        y,
        parameter_name=name,
        default_value=value,
    )


def build_surface_material() -> unreal.Material:
    material = _load_or_create_material("M_VowfallSurface")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", False)
    unreal.MaterialEditingLibrary.set_base_material_usage(
        material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True
    )

    world_position = _expression(
        material, unreal.MaterialExpressionWorldPosition, -1_100, -120
    )
    macro_scale = _scalar_parameter(material, "MacroScale", 360.0, -1_100, 20)
    macro_coordinates = _expression(
        material, unreal.MaterialExpressionDivide, -860, -100
    )
    _connect(world_position, macro_coordinates, "A")
    _connect(macro_scale, macro_coordinates, "B")

    macro_noise = _expression(
        material,
        unreal.MaterialExpressionNoise,
        -620,
        -100,
        levels=2,
        level_scale=2.4,
        noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX3D,
        turbulence=True,
        output_min=0.0,
        output_max=1.0,
    )
    _connect(macro_coordinates, macro_noise, "World Position")

    base_color = _vector_parameter(
        material, "BaseColor", (0.055, 0.072, 0.052, 1.0), -620, -330
    )
    secondary_color = _vector_parameter(
        material, "SecondaryColor", (0.105, 0.105, 0.075, 1.0), -620, -250
    )
    macro_lerp = _expression(
        material, unreal.MaterialExpressionLinearInterpolate, -340, -230
    )
    _connect(base_color, macro_lerp, "A")
    _connect(secondary_color, macro_lerp, "B")
    _connect(macro_noise, macro_lerp, "Alpha")

    detail_scale = _scalar_parameter(material, "DetailScale", 72.0, -1_100, 180)
    detail_coordinates = _expression(
        material, unreal.MaterialExpressionDivide, -860, 160
    )
    _connect(world_position, detail_coordinates, "A")
    _connect(detail_scale, detail_coordinates, "B")
    detail_noise = _expression(
        material,
        unreal.MaterialExpressionNoise,
        -620,
        130,
        levels=1,
        level_scale=2.8,
        noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX3D,
        turbulence=True,
        output_min=0.0,
        output_max=1.0,
    )
    _connect(detail_coordinates, detail_noise, "World Position")
    detail_strength = _scalar_parameter(
        material, "DetailStrength", 0.18, -620, 250
    )
    detail_mask = _expression(
        material, unreal.MaterialExpressionMultiply, -370, 120
    )
    _connect(detail_noise, detail_mask, "A")
    _connect(detail_strength, detail_mask, "B")
    accent_color = _vector_parameter(
        material, "AccentColor", (0.13, 0.14, 0.095, 1.0), -340, -70
    )
    final_color = _expression(
        material, unreal.MaterialExpressionLinearInterpolate, -80, -170
    )
    _connect(macro_lerp, final_color, "A")
    _connect(accent_color, final_color, "B")
    _connect(detail_mask, final_color, "Alpha")

    white_texture = unreal.load_asset(
        "/Engine/EngineResources/WhiteSquareTexture"
    )
    normal_texture = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")
    masks_texture = unreal.load_asset(
        "/Engine/EngineMaterials/DefaultDiffuse_TC_Masks"
    )
    if white_texture is None or normal_texture is None or masks_texture is None:
        raise RuntimeError("Required Engine fallback textures are unavailable")

    texture_coordinates = _expression(
        material, unreal.MaterialExpressionTextureCoordinate, -1_100, 510
    )
    texture_tiling = _scalar_parameter(
        material, "TextureTiling", 1.0, -1_100, 610
    )
    scaled_texture_coordinates = _expression(
        material, unreal.MaterialExpressionMultiply, -870, 520
    )
    _connect(texture_coordinates, scaled_texture_coordinates, "A")
    _connect(texture_tiling, scaled_texture_coordinates, "B")

    albedo_texture = _expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -620,
        480,
        parameter_name="AlbedoTexture",
        texture=white_texture,
        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    )
    _connect(scaled_texture_coordinates, albedo_texture, "UVs")
    texture_tint = _vector_parameter(
        material, "TextureTint", (1.0, 1.0, 1.0, 1.0), -620, 610
    )
    tinted_albedo = _expression(
        material, unreal.MaterialExpressionMultiply, -370, 480
    )
    _connect(albedo_texture, tinted_albedo, "A", "RGB")
    _connect(texture_tint, tinted_albedo, "B")
    texture_blend = _scalar_parameter(
        material, "TextureBlend", 0.0, -370, 610
    )
    textured_color = _expression(
        material, unreal.MaterialExpressionLinearInterpolate, 150, -130
    )
    _connect(final_color, textured_color, "A")
    _connect(tinted_albedo, textured_color, "B")
    _connect(texture_blend, textured_color, "Alpha")
    _connect_property(textured_color, unreal.MaterialProperty.MP_BASE_COLOR)

    normal_sample = _expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -120,
        440,
        parameter_name="NormalTexture",
        texture=normal_texture,
        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    )
    _connect(scaled_texture_coordinates, normal_sample, "UVs")
    flat_normal = _vector_parameter(
        material, "FlatNormal", (0.0, 0.0, 1.0, 1.0), -120, 570
    )
    normal_strength = _scalar_parameter(
        material, "NormalStrength", 0.0, 130, 570
    )
    blended_normal = _expression(
        material, unreal.MaterialExpressionLinearInterpolate, 390, 460
    )
    _connect(flat_normal, blended_normal, "A")
    _connect(normal_sample, blended_normal, "B", "RGB")
    _connect(normal_strength, blended_normal, "Alpha")
    _connect_property(blended_normal, unreal.MaterialProperty.MP_NORMAL)

    packed_texture = _expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -120,
        700,
        parameter_name="PackedTexture",
        texture=masks_texture,
        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
    )
    _connect(scaled_texture_coordinates, packed_texture, "UVs")
    packed_strength = _scalar_parameter(
        material, "PackedStrength", 0.0, 130, 800
    )

    roughness = _scalar_parameter(material, "Roughness", 0.9, -80, 40)
    specular = _scalar_parameter(material, "Specular", 0.25, -80, 130)
    ambient_occlusion = _scalar_parameter(
        material, "AmbientOcclusion", 0.92, -80, 220
    )
    textured_roughness = _expression(
        material, unreal.MaterialExpressionLinearInterpolate, 390, 690
    )
    _connect(roughness, textured_roughness, "A")
    _connect(packed_texture, textured_roughness, "B", "G")
    _connect(packed_strength, textured_roughness, "Alpha")
    textured_ao = _expression(
        material, unreal.MaterialExpressionLinearInterpolate, 390, 800
    )
    _connect(ambient_occlusion, textured_ao, "A")
    _connect(packed_texture, textured_ao, "B", "R")
    _connect(packed_strength, textured_ao, "Alpha")
    _connect_property(textured_roughness, unreal.MaterialProperty.MP_ROUGHNESS)
    _connect_property(specular, unreal.MaterialProperty.MP_SPECULAR)
    _connect_property(textured_ao, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    return material


def build_water_material() -> unreal.Material:
    material = _load_or_create_material("M_VowfallWater")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)

    shallow_color = _vector_parameter(
        material, "ShallowColor", (0.025, 0.13, 0.14, 1.0), -700, -270
    )
    deep_color = _vector_parameter(
        material, "DeepColor", (0.008, 0.045, 0.055, 1.0), -700, -170
    )
    fresnel = _expression(
        material, unreal.MaterialExpressionFresnel, -700, -50
    )
    water_color = _expression(
        material, unreal.MaterialExpressionLinearInterpolate, -390, -190
    )
    _connect(deep_color, water_color, "A")
    _connect(shallow_color, water_color, "B")
    _connect(fresnel, water_color, "Alpha")
    _connect_property(water_color, unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = _scalar_parameter(material, "Roughness", 0.08, -380, 10)
    specular = _scalar_parameter(material, "Specular", 0.72, -380, 100)
    opacity = _scalar_parameter(material, "Opacity", 0.78, -380, 190)
    _connect_property(roughness, unreal.MaterialProperty.MP_ROUGHNESS)
    _connect_property(specular, unreal.MaterialProperty.MP_SPECULAR)
    _connect_property(opacity, unreal.MaterialProperty.MP_OPACITY)

    normal_texture = unreal.load_asset(
        "/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n"
    )
    if normal_texture is not None:
        coordinates = _expression(
            material, unreal.MaterialExpressionTextureCoordinate, -930, 330
        )
        tiling = _scalar_parameter(material, "NormalTiling", 4.5, -930, 430)
        scaled_coordinates = _expression(
            material, unreal.MaterialExpressionMultiply, -700, 340
        )
        _connect(coordinates, scaled_coordinates, "A")
        _connect(tiling, scaled_coordinates, "B")
        panner = _expression(
            material,
            unreal.MaterialExpressionPanner,
            -480,
            340,
            speed_x=0.025,
            speed_y=-0.012,
        )
        _connect(scaled_coordinates, panner, "Coordinate")
        normal_sample = _expression(
            material,
            unreal.MaterialExpressionTextureSampleParameter2D,
            -250,
            330,
            parameter_name="NormalTexture",
            texture=normal_texture,
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
        )
        _connect(panner, normal_sample, "UVs")
        _connect_property(normal_sample, unreal.MaterialProperty.MP_NORMAL, "RGB")
    else:
        unreal.log_warning("Engine water normal was unavailable; building flat water")

    return material


def _compile_and_save(material: unreal.Material, asset_path: str) -> None:
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"Material compile failed for {asset_path}: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {asset_path}")
    unreal.log(f"Built {asset_path}")


def main() -> None:
    surface = build_surface_material()
    water = build_water_material()
    _compile_and_save(surface, SURFACE_PATH)
    _compile_and_save(water, WATER_PATH)
    unreal.log("Vowfall world materials built successfully")


main()
