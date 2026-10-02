[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet(
        "runtime.process-api",
        "scripting.gdscript-helper",
        "media.gif-support",
        "media.spout",
        "animation.spine-runtime",
        "ui.dynamic-color-theme",
        "branding.windows-icons",
        "ui.texture-button-hit-test-api",
        "security.encrypted-key-reversal",
        "scripting.script-documentation-api",
        "media.gif-streaming-exporter",
        "branding.main-icon",
        "build.profiles.fortune-wheel",
        "ui.color-role-transform",
        "ui.button-state-layers",
        "ui.stylebox-elevation",
        "ui.texture-button-state-text",
        "ui.theme-string-items",
        "ui.button-text-icon",
        "ui.custom-icon-font",
        "ui.checkbox-state-text",
        "runtime.texture-streaming-init-order",
        "editor.headless-export-teardown",
        "editor.legacy-main-screen-dock-cleanup",
        "rendering.material-post-light",
        "rendering.unjittered-projection",
        "rendering.viewport-parent-order",
        "rendering.ssr-reprojection-boundary",
        "geometry.triangle-mesh-build",
        "geometry.triangle-mesh-refit",
        "geometry.surface-decode"
    )]
    [string]$Topic,
    [string]$EnginePath
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($EnginePath)) {
    $EnginePath = Join-Path $PSScriptRoot "..\..\engine"
}
$engine = [System.IO.Path]::GetFullPath($EnginePath)

function Assert-TopicFile {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $path = Join-Path $engine $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required topic file is missing: $RelativePath"
    }
    return $path
}

function Assert-TopicText {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Text
    )

    $path = Assert-TopicFile -RelativePath $RelativePath
    $content = Get-Content -LiteralPath $path -Raw -Encoding UTF8
    if ($content.IndexOf($Text, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Required topic text '$Text' is missing from: $RelativePath"
    }
}

function Assert-TopicTextAbsent {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Text
    )

    $path = Assert-TopicFile -RelativePath $RelativePath
    $content = Get-Content -LiteralPath $path -Raw -Encoding UTF8
    if ($content.IndexOf($Text, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Unexpected topic text '$Text' remains in: $RelativePath"
    }
}

switch ($Topic) {
    "runtime.process-api" {
        Assert-TopicText "core\io\process.h" "class Process"
        Assert-TopicText "core\io\process.cpp" "Process::create"
        Assert-TopicText "doc\classes\Process.xml" '<class name="Process"'
        Assert-TopicText "core\io\file_access.h" "close_write"
        Assert-TopicText "platform\windows\os_windows.cpp" "p_working_directory"
        Assert-TopicText "core\io\process.cpp" "Child process creation returned incomplete pipe handles."
        Assert-TopicText "core\io\process.cpp" "Child process creation returned invalid pipe handles."
        Assert-TopicText "core\io\process.cpp" "Process::~Process()"
        Assert-TopicText "core\os\os.h" "release_process"
        Assert-TopicText "platform\windows\os_windows.cpp" "_close_process_handles"
        Assert-TopicText "tests\core\io\test_process.cpp" "Releases Windows child process handles"
        Assert-TopicText "tests\core\io\test_process.cpp" "Terminates a live Windows child on last reference"
    }
    "scripting.gdscript-helper" {
        Assert-TopicText "modules\gdscript\gdscript_helper.h" "class GDScriptHelper"
        Assert-TopicText "modules\gdscript\gdscript_helper.cpp" "set_validate_code"
        Assert-TopicText "modules\gdscript\register_types.cpp" "GDREGISTER_CLASS(GDScriptHelper)"
    }
    "media.gif-support" {
        Assert-TopicText "modules\gif\image_frames.h" "class ImageFrames"
        Assert-TopicText "modules\gif\image_frames_loader_gif.cpp" "load_image_frames"
        Assert-TopicFile "modules\gif\editor\import\resource_importer_sprite_frames.cpp" | Out-Null
        Assert-TopicText "modules\gif\thirdparty\README.md" "Version: 5.2.2"
        Assert-TopicText "modules\gif\thirdparty\README.md" "44241952659c5db27da3d9db85d910c2b6904216"
        Assert-TopicText "modules\gif\thirdparty\giflib\gif_lib.h" "#define GIFLIB_RELEASE 2"
    }
    "media.spout" {
        Assert-TopicText "modules\spout\spout_gd.h" "class Spout"
        Assert-TopicFile "modules\spout\register_types.cpp" | Out-Null
        Assert-TopicText "modules\spout\spout_gd.cpp" "GetSDKversion(&version)"
        Assert-TopicText "thirdparty\spout-library\README.md" "Version: SDK 2.007.017"
        $spoutHashes = @{
            "thirdparty\spout-library\Binaries\x64\SpoutLibrary.h" = "7E72293213F7366450AEB97E38333A80F971CEDE07C50919A810D339F6DB3B3B"
            "thirdparty\spout-library\Binaries\x64\SpoutLibrary.lib" = "6C8CEBA65294FB76C5CCAD23C059551B92DDEF7D6D6665B39F6B2EC553EEE091"
            "thirdparty\spout-library\Binaries\x64\SpoutLibrary.dll" = "31F12268169397BACD9048903D9AD711FAE58B788E90A860598B21B1DAAE62EB"
        }
        foreach ($relative in $spoutHashes.Keys) {
            $path = Assert-TopicFile -RelativePath $relative
            $actualSha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
            if ($actualSha256 -ne $spoutHashes[$relative]) {
                throw "Spout binary-set hash mismatch for $relative`: expected $($spoutHashes[$relative]), got $actualSha256"
            }
        }
        Assert-TopicFile "thirdparty\spout-library\COPYING" | Out-Null
    }
    "animation.spine-runtime" {
        Assert-TopicText "modules\spine_godot\SpineSprite.h" "class SpineSprite"
        Assert-TopicText "modules\spine_godot\SpineSprite.cpp" "RenderingServerTypes::SurfaceData"
        Assert-TopicText "modules\spine_godot\SpineSprite.cpp" "Spine mesh UV count must match its vertex count"
        Assert-TopicText "modules\spine_godot\SpineAtlasResource.cpp" "if (prefix_pos < 0)"
        Assert-TopicText "modules\spine_godot\SpineAtlasResource.cpp" "validate_atlas"
        Assert-TopicText "modules\spine_godot\SpineAtlasResource.cpp" 'emit_signal(SNAME("skeleton_atlas_changed"))'
        Assert-TopicText "modules\spine_godot\SpineSkeletonFileResource.cpp" "length < 8"
        Assert-TopicText "modules\spine_godot\register_types.cpp" "GDREGISTER_CLASS(SpineSprite)"
        Assert-TopicText "modules\spine_godot\config.py" '"SpineConstant"'
        Assert-TopicText "modules\spine_godot\docs\SpineConstant.xml" '<class name="SpineConstant"'
        Assert-TopicFile "modules\spine_godot\spine-cpp\LICENSE" | Out-Null
        Assert-TopicFile "modules\spine_godot\spine-cpp\include\spine\Skeleton.h" | Out-Null
        Assert-TopicText "modules\spine_godot\spine-cpp\UPSTREAM.md" 'Upstream release: `4.1.56`'
        Assert-TopicText "modules\spine_godot\spine-cpp\UPSTREAM.md" "77a5db0ec6d16331f5efbaa7662bba9355bd3424"
        Assert-TopicText "modules\spine_godot\SpineSprite.h" "mesh.is_valid() && RS::get_singleton()"
        Assert-TopicText "modules\spine_godot\SpineSprite.cpp" "const int slot_count = MIN"
        Assert-TopicText "modules\spine_godot\SpineSprite.cpp" "invalid_indices = indices->size() % 3 != 0"
    }
    "ui.dynamic-color-theme" {
        Assert-TopicText "modules\color_scheme\color_scheme.h" "class ColorScheme"
        Assert-TopicText "modules\color_scheme\color_role.h" "enum class ColorRole"
        Assert-TopicFile "thirdparty\material-color-utilities\LICENSE" | Out-Null
        Assert-TopicText "thirdparty\material-color-utilities\UPSTREAM.md" "5b3618b16fdc3825e21d5679bafd144662088ea1"
        Assert-TopicText "tests\modules\color_scheme\test_color_scheme.cpp" "Material content colors match the vendored runtime"
        Assert-TopicText "scene\resources\theme.h" "DATA_TYPE_COLOR_SCHEME"
        Assert-TopicText "scene\gui\control.h" "get_theme_color_role"
        Assert-TopicText "editor\scene\gui\theme_editor_plugin.cpp" "DATA_TYPE_COLOR_ROLE"
    }
    "branding.windows-icons" {
        $expectedSha256 = "E45360FC42BC8513CCB4520E3C74A9E74BBD43B31748045CC7745D7F9A2559D4"
        foreach ($relative in @(
            "platform\windows\godot.ico",
            "platform\windows\godot_console.ico"
        )) {
            $path = Assert-TopicFile -RelativePath $relative
            $actualSha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
            if ($actualSha256 -ne $expectedSha256) {
                throw "Icon hash mismatch for $relative`: expected $expectedSha256, got $actualSha256"
            }
        }
    }
    "ui.texture-button-hit-test-api" {
        Assert-TopicText "scene\gui\texture_button.cpp" 'D_METHOD("has_point", "point")'
        Assert-TopicText "doc\classes\TextureButton.xml" '<method name="has_point" qualifiers="const">'
        Assert-TopicText "tests\scene\test_texture_button.cpp" "has_point is exposed to scripts"
    }
    "security.encrypted-key-reversal" {
        Assert-TopicText "core\io\file_access_encrypted.cpp" "Vector<uint8_t> key_bytes = p_raw_key;"
        Assert-TopicText "core\io\file_access_encrypted.cpp" "key_bytes.reverse();"
        Assert-TopicText "doc\classes\FileAccess.xml" "This custom engine reverses the 32 key bytes"
        Assert-TopicText "tests\core\io\test_file_access.cpp" "Encrypted files reverse raw key bytes"
    }
    "scripting.script-documentation-api" {
        Assert-TopicText "core\object\script_language.h" "_get_script_documentation_list"
        Assert-TopicText "core\object\script_language.cpp" 'D_METHOD("get_script_documentation_list")'
        Assert-TopicText "core\object\script_language.cpp" "DocData::ClassDoc"
        Assert-TopicText "doc\classes\Script.xml" '<method name="get_script_documentation_list">'
        Assert-TopicText "tests\modules\gdscript\test_script_documentation.cpp" "Structured documentation is exposed to scripts"
    }
    "media.gif-streaming-exporter" {
        Assert-TopicText "modules\gif\gif_exporter.h" "class GifExporter"
        Assert-TopicText "modules\gif\gif_exporter.cpp" "GifExporter::_prepare_frame"
        Assert-TopicText "modules\gif\gif_exporter.cpp" "image_frames->save_gif(output_path, max_color_count, loop_count)"
        Assert-TopicText "modules\gif\register_types.cpp" "GDREGISTER_CLASS(GifExporter)"
        Assert-TopicText "modules\gif\doc_classes\GifExporter.xml" '<class name="GifExporter"'
        Assert-TopicText "modules\gif\doc_classes\ImageFrames.xml" '<param index="2" name="loop_count" type="int" default="0" />'
        Assert-TopicText "tests\modules\gif\test_gif_exporter.cpp" "GifExporter writes compatible animated files"
    }
    "branding.main-icon" {
        $expectedHashes = @{
            "misc\logo\icon.png" = "CCC4BF24FCE0A2C532A40DBEBF6FC9D8DB9A57508CFEA656CA411C9C78357BCE"
            "main\app_icon.png" = "10C437C8270B5A703D9F7916BACBB5A86D5E8E0F6433484980058D7477320C8D"
        }
        foreach ($relative in $expectedHashes.Keys) {
            $path = Assert-TopicFile -RelativePath $relative
            $actualSha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
            $expectedSha256 = $expectedHashes[$relative]
            if ($actualSha256 -ne $expectedSha256) {
                throw "Icon hash mismatch for $relative`: expected $expectedSha256, got $actualSha256"
            }
        }
        Assert-TopicText "tests\core\io\test_pck_packer.cpp" "maximum_expected_size"
        Assert-TopicText "tests\core\io\test_pck_packer.cpp" 'FileAccess::get_size(base_dir.path_join("../misc/logo/icon.png"))'
        Assert-TopicText "tests\core\io\test_pck_packer.cpp" 'logo.png")) + 16384'
    }
    "build.profiles.fortune-wheel" {
        foreach ($relative in @(
            "fortune_wheel_web.gdbuild",
            "fortune_wheel_windows.gdbuild"
        )) {
            $path = Assert-TopicFile -RelativePath $relative
            try {
                $profile = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
            } catch {
                throw "Build profile is not valid JSON: $relative`: $($_.Exception.Message)"
            }
            if ($profile.type -ne "build_profile") {
                throw "Build profile has an unexpected type: $relative"
            }
            if (@($profile.disabled_classes).Count -ne 319) {
                throw "Build profile must contain the audited 319 disabled classes: $relative"
            }
            foreach ($requiredClass in @("CollisionPolygon2D", "CollisionShape2D")) {
                if (@($profile.disabled_classes) -contains $requiredClass) {
                    throw "Required Fortune Wheel class must remain enabled: $requiredClass in $relative"
                }
            }
            if ($profile.disabled_build_options.PSObject.Properties.Name -contains "module_text_server_fb_enabled") {
                throw "Fallback text server must remain available: $relative"
            }
        }
    }
    "ui.color-role-transform" {
        Assert-TopicText "modules\color_scheme\color_role_transform.h" "class ColorRoleTransform"
        Assert-TopicText "modules\color_scheme\color_role_transform.cpp" "ColorRoleTransform::resolve"
        Assert-TopicText "modules\color_scheme\doc_classes\ColorRoleTransform.xml" '<class name="ColorRoleTransform"'
        Assert-TopicText "tests\modules\color_scheme\test_color_role_transform.cpp" "ColorRoleTransform resolves transformation pipeline"
    }
    "ui.button-state-layers" {
        Assert-TopicText "scene\gui\button.cpp" "_get_current_state_layer_stylebox"
        Assert-TopicText "scene\gui\button.cpp" "state_hover_pressed_layer"
        Assert-TopicText "doc\classes\Button.xml" '<theme_item name="state_focus_layer"'
        Assert-TopicText "tests\scene\test_button.cpp" "resolves optional Material state layers"
    }
    "ui.stylebox-elevation" {
        Assert-TopicText "scene\resources\style_box_flat.h" "enum ElevationLevel"
        Assert-TopicText "scene\resources\style_box_flat.cpp" "set_elevation_level"
        Assert-TopicText "doc\classes\StyleBoxFlat.xml" '<member name="dynamic_shadow"'
        Assert-TopicText "tests\scene\test_style_box_flat.cpp" "Material elevation presets"
    }
    "ui.texture-button-state-text" {
        Assert-TopicText "scene\gui\texture_button.cpp" "set_text_normal"
        Assert-TopicText "scene\gui\texture_button.cpp" "text_normal_color_role"
        Assert-TopicText "doc\classes\TextureButton.xml" '<member name="text_normal"'
        Assert-TopicText "tests\scene\test_texture_button.cpp" "State text setters and fallback"
        Assert-TopicText "tests\scene\test_texture_button.cpp" "State text contributes to minimum size"
    }
    "ui.theme-string-items" {
        Assert-TopicText "scene\resources\theme.h" "DATA_TYPE_STRING"
        Assert-TopicText "scene\resources\theme.cpp" "Theme::set_string"
        Assert-TopicText "scene\gui\control.cpp" "add_theme_string_override"
        Assert-TopicText "scene\main\window.cpp" "add_theme_string_override"
        Assert-TopicText "editor\scene\gui\theme_editor_plugin.cpp" "_string_item_changed"
        Assert-TopicText "tests\scene\test_theme.cpp" "String theme items"
        Assert-TopicText "tests\scene\test_control.cpp" "String theme overrides"
        Assert-TopicText "tests\scene\test_window.cpp" "String theme overrides"
    }
    "ui.button-text-icon" {
        Assert-TopicText "scene\gui\button.cpp" "set_text_icon"
        Assert-TopicText "scene\gui\button.cpp" "BIND_THEME_ITEM(Theme::DATA_TYPE_STRING, Button, text_icon)"
        Assert-TopicText "doc\classes\Button.xml" '<member name="text_icon"'
        Assert-TopicText "tests\scene\test_button.cpp" "uses text glyphs as fallback icons"
    }
    "ui.custom-icon-font" {
        Assert-TopicText "scene\theme\theme_db.cpp" 'GLOBAL_DEF_RST_BASIC(PropertyInfo(Variant::STRING, "gui/theme/custom_icon_font"'
        Assert-TopicText "scene\theme\theme_db.cpp" "set_fallback_icon_font(project_icon_font)"
        Assert-TopicText "scene\theme\default_theme.cpp" 'set_font("text_icon_font", "Button", default_icon_font)'
        Assert-TopicText "doc\classes\ThemeDB.xml" '<member name="fallback_icon_font"'
        Assert-TopicText "tests\scene\test_theme.cpp" "Fallback icon font"
    }
    "ui.checkbox-state-text" {
        Assert-TopicText "scene\gui\check_box.cpp" "CheckBox::_get_state_text"
        Assert-TopicText "scene\gui\check_box.cpp" "text_radio_checked_disabled_color_role"
        Assert-TopicText "scene\gui\check_button.cpp" "CheckButton::_get_state_text"
        Assert-TopicText "scene\gui\check_button.cpp" "text_checked_disabled_mirrored_color_role"
        Assert-TopicText "doc\classes\CheckBox.xml" '<theme_item name="text_radio_checked"'
        Assert-TopicText "doc\classes\CheckButton.xml" '<theme_item name="text_checked_mirrored"'
        Assert-TopicText "tests\scene\test_button.cpp" "resolves state text glyphs"
        Assert-TopicText "tests\scene\test_button.cpp" "resolves mirrored state text glyphs"
    }
    "runtime.texture-streaming-init-order" {
        Assert-TopicText "servers\rendering\rendering_server.cpp" '#include "modules/modules_enabled.gen.h"'
        Assert-TopicText "servers\rendering\rendering_server.cpp" "Forward renderers read these settings before scene-level modules are initialized."
        Assert-TopicText "servers\rendering\rendering_server.cpp" 'GLOBAL_DEF_RST("rendering/textures/streaming/enabled", false);'
        Assert-TopicText "modules\texture_streaming\texture_streaming.cpp" 'setting_streaming_is_enabled = GLOBAL_GET("rendering/textures/streaming/enabled");'
        Assert-TopicTextAbsent "modules\texture_streaming\texture_streaming.cpp" 'GLOBAL_DEF_RST("rendering/textures/streaming/enabled", false);'
    }
    "editor.headless-export-teardown" {
        Assert-TopicText "editor\editor_node.cpp" "bool EditorNode::is_cmdline_mode()"
        Assert-TopicText "editor\editor_node.cpp" "return singleton == nullptr || singleton->cmdline_mode;"
        Assert-TopicText "editor\file_system\editor_file_system.cpp" "EditorNode::is_cmdline_mode()"
    }
    "editor.legacy-main-screen-dock-cleanup" {
        Assert-TopicText "editor\editor_main_screen.cpp" "EditorDockManager::get_singleton()->remove_dock(dock);"
        Assert-TopicText "editor\editor_main_screen.cpp" "dock->queue_free();"
        Assert-TopicText "editor\editor_main_screen.cpp" 'p_editor->remove_meta("_dock");'
    }
}

if ($Topic -eq "rendering.unjittered-projection") {
    Assert-TopicText "servers\rendering\shader_preprocessor.cpp" 'insert_builtin_define("HAS_UNJITTERED_PROJECTION_MATRIX", _MKSTR(1), pp_state);'
    Assert-TopicText "servers\rendering\shader_types.cpp" 'functions["vertex"].built_ins["UNJITTERED_PROJECTION_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);'
    Assert-TopicText "servers\rendering\shader_types.cpp" 'functions["fragment"].built_ins["UNJITTERED_PROJECTION_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);'
    Assert-TopicText "servers\rendering\renderer_rd\storage_rd\render_scene_data_rd.h" 'float unjittered_projection_matrix[16];'
    Assert-TopicText "servers\rendering\renderer_rd\storage_rd\render_scene_data_rd.cpp" 'unjittered_correction * cam_projection'
    Assert-TopicText "servers\rendering\renderer_rd\storage_rd\render_scene_data_rd.cpp" 'prev_unjittered_correction * prev_view_projection[v]'
    Assert-TopicText "servers\rendering\renderer_rd\shaders\scene_data_inc.glsl" 'mat4 unjittered_projection_matrix_view[MAX_VIEWS];'
    Assert-TopicText "servers\rendering\renderer_rd\shaders\forward_clustered\scene_forward_clustered_inc.glsl" 'scene_data.unjittered_projection_matrix_view[ViewIndex]'
    Assert-TopicText "servers\rendering\renderer_rd\forward_clustered\scene_shader_forward_clustered.cpp" 'actions.renames["UNJITTERED_PROJECTION_MATRIX"] = "npr_unjittered_projection";'
}

if ($Topic -eq "rendering.material-post-light") {
    Assert-TopicText "servers\rendering\shader_language.h" 'StringName post_light;'
    Assert-TopicText "servers\rendering\shader_language.cpp" "Match ShaderCompiler's varying-location allocation."
    Assert-TopicText "servers\rendering\shader_preprocessor.cpp" 'insert_builtin_define("HAS_MATERIAL_POST_LIGHT", _MKSTR(1), pp_state);'
    Assert-TopicText "servers\rendering\shader_types.cpp" 'functions["post_light"].built_ins["MATERIAL_LIGHT_DATA"] = constt(ShaderLanguage::TYPE_VEC4);'
    Assert-TopicText "servers\rendering\shader_compiler.cpp" 'post_light() is not supported by this renderer; refusing to omit the material HDR stage.'
    Assert-TopicText "servers\rendering\renderer_rd\forward_clustered\scene_shader_forward_clustered.cpp" 'actions.entry_point_stages["post_light"] = ShaderCompiler::STAGE_FRAGMENT;'
    Assert-TopicText "servers\rendering\renderer_rd\shaders\forward_clustered\scene_forward_clustered.glsl" '#CODE : POST_LIGHT'
    Assert-TopicText "servers\rendering\renderer_rd\shaders\forward_clustered\scene_forward_clustered.glsl" 'material_light_data = vec4(0.0);'
    Assert-TopicText "servers\rendering\renderer_rd\shaders\forward_clustered\scene_forward_clustered.glsl" 'vec3 post_light_direct = vec3(diffuse_light + direct_specular_light);'
}

if ($Topic -eq "geometry.triangle-mesh-build") {
    Assert-TopicText "core\math\triangle_mesh.cpp" 'bvh.resize(fc * 2 - 1);'
    Assert-TopicText "core\math\triangle_mesh.cpp" 'db.reserve(fc);'
    Assert-TopicText "core\math\triangle_mesh.cpp" 'v[j].snappedf(0.0001)'
    Assert-TopicTextAbsent "core\math\triangle_mesh.cpp" 'bvh.resize(fc * 3);'
}

if ($Topic -eq "geometry.triangle-mesh-refit") {
    Assert-TopicText "core\math\triangle_mesh.h" 'bool refit_from_faces(const Vector<Vector3> &p_faces);'
    Assert-TopicText "core\math\triangle_mesh.cpp" 'p_faces.size() != triangles.size() * 3'
    Assert-TopicText "core\math\triangle_mesh.cpp" 'if (!r[i].is_finite())'
    Assert-TopicText "core\math\triangle_mesh.cpp" 'vw[index] = r[index].snappedf(0.0001);'
    Assert-TopicText "core\math\triangle_mesh.cpp" 'for (int i = fc; i < bvh.size(); i++)'
    Assert-TopicText "core\math\triangle_mesh.cpp" 'ClassDB::bind_method(D_METHOD("refit_from_faces", "faces"), &TriangleMesh::refit_from_faces);'
    Assert-TopicText "doc\classes\TriangleMesh.xml" '<method name="refit_from_faces">'
    Assert-TopicText "core\math\triangle_mesh.cpp" 'bool TriangleMesh::update_from_indexed_surfaces('
    Assert-TopicText "doc\classes\TriangleMesh.xml" '<method name="update_from_indexed_surfaces">'
}

if ($Topic -eq "geometry.surface-decode") {
    Assert-TopicText "servers\rendering\rendering_server.h" 'bool p_geometry_only = false'
    Assert-TopicText "servers\rendering\rendering_server.cpp" '(p_geometry_only && !(geometry_format & (1ULL << i)))'
    Assert-TopicText "servers\rendering\rendering_server.cpp" 'if (p_geometry_only || !(p_format & RSE::ARRAY_FORMAT_NORMAL))'
    Assert-TopicText "servers\rendering\rendering_server.cpp" 'D_METHOD("mesh_surface_get_geometry_arrays", "mesh", "surface")'
    Assert-TopicText "servers\rendering\rendering_server.cpp" 'D_METHOD("mesh_surface_get_geometry_blend_shape_arrays", "mesh", "surface")'
    Assert-TopicText "doc\classes\RenderingServer.xml" '<method name="mesh_surface_get_geometry_arrays" qualifiers="const">'
    Assert-TopicText "doc\classes\RenderingServer.xml" '<method name="mesh_surface_get_geometry_blend_shape_arrays" qualifiers="const">'
}

if ($Topic -eq "rendering.viewport-parent-order") {
    $path = Assert-TopicFile "servers\rendering\renderer_viewport.cpp"
    $content = [IO.File]::ReadAllText($path)
    $setter = [regex]::Match($content, '(?s)void RendererViewport::viewport_set_parent_viewport\([^\n]+\) \{.*?\n\}')
    if (-not $setter.Success -or $setter.Value -notmatch 'viewport->parent = p_parent_viewport;\s+sorted_active_viewports_dirty = true;') {
        throw "Viewport parent assignment must invalidate cached draw order."
    }
}

if ($Topic -eq "rendering.ssr-reprojection-boundary") {
    $path = Assert-TopicFile "servers\rendering\renderer_rd\shaders\effects\screen_space_reflection.glsl"
    $content = [IO.File]::ReadAllText($path)
    if ($content -notmatch 'reprojected_pos\.xy = reprojected_pos\.w > 0\.0 \? reprojected_pos\.xy / reprojected_pos\.w \* 0\.5 \+ 0\.5 : vec2\(-1\.0\);') {
        throw "SSR reprojection must reject nonpositive previous-camera w before division."
    }
    if ($content -notmatch 'margin_grad = max\(margin_grad, vec2\(0\.0\)\);\s+margin_blend = smoothstep\(0\.0, margin\.x \* margin\.y, margin_grad\.x \* margin_grad\.y\);') {
        throw "SSR must clamp each image margin before multiplying its components."
    }
}

Write-Output "Topic verification passed: $Topic"
exit 0
