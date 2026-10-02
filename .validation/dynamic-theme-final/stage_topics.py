"""Stage reviewed topic slices in the index without changing working files."""

import argparse
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / "engine"
BASE = "db1af1e99a5025bfbf8b05ef27dd4a5e739ec371"
FILES = [
    "SConstruct",
    "doc/classes/Theme.xml",
    "editor/inspector/editor_inspector.cpp",
    "editor/scene/gui/theme_editor_plugin.cpp",
    "modules/color_scheme/color_scheme.cpp",
    "modules/color_scheme/color_scheme.h",
    "modules/color_scheme/doc_classes/ColorScheme.xml",
    "scene/gui/color_rect.cpp",
    "scene/gui/control.cpp",
    "scene/main/window.cpp",
    "scene/resources/style_box.cpp",
    "scene/resources/style_box.h",
    "scene/resources/theme.cpp",
    "scene/resources/theme.h",
    "scene/theme/default_theme_dynamic_color.inc",
    "tests/modules/color_scheme/test_color_scheme.cpp",
    "tests/scene/test_control.cpp",
    "tests/scene/test_theme.cpp",
    "tests/scene/test_window.cpp",
]


def git(*args, data=None):
    result = subprocess.run(
        ["rtk", "proxy", "git", *args], cwd=ROOT, input=data,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    )
    return result.stdout


def read_tree(ref):
    raw = git("cat-file", "--batch", data="".join(f"{ref}:{p}\n" for p in FILES).encode())
    result = {}
    offset = 0
    for path in FILES:
        end = raw.index(b"\n", offset)
        size = int(raw[offset:end].split()[-1])
        result[path] = raw[end + 1:end + 1 + size].decode("utf-8").replace("\r\n", "\n")
        offset = end + size + 2
    return result


def block(text, marker, terminator="\n}\n"):
    start = text.index(marker)
    end = text.index(terminator, start) + len(terminator)
    return text[start:end]


def replace_once(text, old, new):
    assert text.count(old) == 1, (old[:100], text.count(old))
    return text.replace(old, new, 1)


def replace_function(text, source, marker):
    return replace_once(text, block(text, marker), block(source, marker))


def remove_block(text, marker, terminator="\n}\n"):
    return replace_once(text, block(text, marker, terminator) + "\n", "")


old = read_tree(BASE)
final = {p: (ROOT / p).read_text(encoding="utf-8") for p in FILES}


def snapshot(stage):
    result = dict(old)
    complete = {
        "SConstruct": 13,
        "doc/classes/Theme.xml": 3,
        "editor/inspector/editor_inspector.cpp": 1,
        "editor/scene/gui/theme_editor_plugin.cpp": 3,
        "modules/color_scheme/doc_classes/ColorScheme.xml": 11,
        "scene/gui/color_rect.cpp": 6,
        "scene/resources/style_box.cpp": 10,
        "scene/resources/style_box.h": 10,
    }
    for path, first_stage in complete.items():
        if stage >= first_stage:
            result[path] = final[path]

    for path, cls, prefix in [
        ("scene/gui/control.cpp", "Control", "data."),
        ("scene/main/window.cpp", "Window", ""),
    ]:
        text = final[path]
        for marker, first_stage in [
            (f"void {cls}::_get_property_list(", 1),
            (f"Ref<StyleBox> {cls}::get_theme_stylebox(", 10),
            (f"Color {cls}::get_theme_color(", 2),
            (f"bool {cls}::has_theme_color(", 2),
        ]:
            if stage < first_stage:
                text = replace_function(text, old[path], marker)
        if 2 <= stage < 9:
            memo = (
                f"\t\t\tif ({prefix}theme_color_cache.has(p_theme_type) && {prefix}theme_color_cache[p_theme_type].has(p_name)) {{\n"
                f"\t\t\t\treturn {prefix}theme_color_cache[p_theme_type][p_name];\n\t\t\t}}\n"
            )
            text = replace_once(text, memo, "")
            text = replace_once(text, f"\t\t\t\t{prefix}theme_color_cache[p_theme_type][p_name] = dynamic_color;\n", "")
        result[path] = text

    path = "scene/resources/theme.cpp"
    if stage >= 12:
        result[path] = final[path]
    elif stage >= 3:
        marker = "Ref<ColorScheme> Theme::get_color_scheme_nocheck("
        result[path] = replace_once(old[path], "bool Theme::has_color_scheme(", block(final[path], marker) + "\nbool Theme::has_color_scheme(")
    path = "scene/resources/theme.h"
    text = final[path]
    if stage < 12:
        text = replace_once(text, "uint32_t change_propagation_depth = 0;", "bool no_change_propagation = false;")
    if stage < 3:
        text = re.sub(r"\t// Returns only the value stored[^\n]*\n\tRef<ColorScheme> get_color_scheme_nocheck[^\n]*\n", "", text)
    result[path] = text

    path = "scene/theme/default_theme_dynamic_color.inc"
    if stage >= 7:
        result[path] = final[path]
    elif stage >= 5:
        text = old[path]
        for line in [
            '\ttheme->set_color_scheme("font_default_color_scheme", "RichTextLabel", Ref<ColorScheme>());\n',
            '\ttheme->set_color("outline_color_scale_scale", "RichTextLabel", Color(1, 1, 1, 1));\n',
        ]:
            text = replace_once(text, line, "")
        for before, after in [
            ("font_default_color_scale", "default_color_scale"),
            ("font_default_color_role", "default_color_role"),
            ("activity_color_scheme", "activity_scheme"),
            ("activity_color_role", "activity_role"),
        ]:
            text = replace_once(text, before, after)
        result[path] = text

    path = "modules/color_scheme/color_scheme.cpp"
    text = final[path]
    if stage < 4:
        text = replace_once(text, '#include "core/object/callable_mp.h"\n', "")
    if stage < 13:
        text = replace_once(text, '\n#include "thirdparty/material-color-utilities/quantize/celebi.h"\n#include "thirdparty/material-color-utilities/score/score.h"\n', "")
    if stage < 11:
        for name in ["is_finite_source_color", "validated_source_color", "validated_contrast_level"]:
            marker = re.search(r"^static [^\n]* " + name + r"\(", text, re.M).group()
            text = remove_block(text, marker)
        text = re.sub(r"^\tERR_FAIL_(?:COND|INDEX)[^\n]*\n", "", text, flags=re.M)
        text = text.replace("source_color(validated_source_color(p_source_color))", "source_color(p_source_color)")
        text = text.replace("contrast_level(validated_contrast_level(p_contrast_level))", "contrast_level(CLAMP(p_contrast_level, -1.0f, 1.0f))")
        text = re.sub(r'\t\t\t} else \{\n\t\t\t\tWARN_PRINT\(vformat\("ColorScheme[^\n]*\n\t\t\t}', "\t\t\t}", text)
    if stage < 9:
        text = replace_once(text, "#ifdef DEV_ENABLED\n\tcolor_query_count++;\n#endif\n", "")
    if stage < 8:
        text = replace_function(text, old[path], "material_color_utilities::SchemeContent ColorScheme::_create_scheme_content(")
        text = text.replace("\tsource_texture_color_cached = false;\n", "")
    if stage < 4:
        for marker in ["void ColorScheme::set_source_color(", "void ColorScheme::set_source_texture(", "ColorScheme::ColorScheme(const Ref<Texture2D> &"]:
            text = replace_function(text, old[path], marker)
        text = remove_block(text, "void ColorScheme::_source_texture_changed(")
    result[path] = text

    path = "modules/color_scheme/color_scheme.h"
    text = final[path]
    if stage < 13:
        text = old[path].split("class ColorScheme", 1)[0] + "class ColorScheme" + text.split("class ColorScheme", 1)[1]
    if stage < 9:
        text = re.sub(r"#ifdef DEV_ENABLED\n[^#]*#endif\n\n", "", text)
    if stage < 8:
        text = replace_once(text, "\tColor source_texture_color;\n\tbool source_texture_color_cached = false;\n", "")
    if stage < 4:
        text = replace_once(text, "\tvoid _source_texture_changed();\n", "")
    result[path] = text

    for path in [p for p in FILES if p.startswith("tests/")]:
        text = final[path]
        for match in list(re.finditer(r'^TEST_CASE\("([^"\n]+)"\) \{', final[path], re.M)):
            if match.group() in old[path]:
                continue
            name = match.group(1)
            if "override properties" in name:
                first_stage = 1
            elif "presence and STATIC" in name:
                first_stage = 2
            elif "Raw color scheme" in name:
                first_stage = 3
            elif any(key in name for key in ["Texture palette follows", "ImageTexture set_image", "Source changes refresh"]):
                first_stage = 4
            elif "Texture extraction is cached" in name:
                first_stage = 8
            elif "caches invalidate" in name:
                first_stage = 9
            elif "styleboxes isolate effective schemes" in name:
                first_stage = 10
            elif "Static built-in" in name:
                first_stage = 10
            elif "Invalid inputs" in name or "Unreadable and transparent" in name:
                first_stage = 11
            elif "type helpers" in name or "Composite type" in name:
                first_stage = 12
            else:
                raise AssertionError(name)
            if stage < first_stage:
                text = remove_block(text, match.group())
        helpers = [
            ("static Array one_empty_signal_args(", 12, "\n}\n"),
            ("static void check_control_dynamic_theme_override_file_roundtrip(", 1, "\n}\n"),
            ("static void check_window_dynamic_theme_override_file_roundtrip(", 1, "\n}\n"),
            ("static int get_property_count(", 1, "\n}\n"),
            ("static void check_button_dynamic_theme_override_file_roundtrip(", 1, "\n}\n"),
            ("static void check_dialog_dynamic_theme_override_file_roundtrip(", 1, "\n}\n"),
            ("template <typename T>\nstatic void check_dynamic_theme_color_cache(", 9, "\n}\n"),
            ("// CPU-backed texture makes palette assertions independent of the render backend.\nclass MutableTestTexture", 4, "\n};\n"),
        ]
        for marker, first_stage, end in helpers:
            if marker in text and marker not in old[path] and stage < first_stage:
                text = remove_block(text, marker, end)
        for line in re.findall(r"^#include [^\n]*\n", final[path], re.M):
            if line in old[path]:
                continue
            if "<limits>" in line:
                if stage < 11:
                    text = replace_once(text, "\n" + line, "")
                continue
            first_stage = 4 if "/modules/" in path else 1
            if path.endswith("test_control.cpp"):
                if "message_queue" in line:
                    first_stage = 9
                elif "style_box_line" in line or "style_box_texture" in line:
                    first_stage = 10
            if stage < first_stage:
                text = replace_once(text, line, "")
        if path.endswith("test_color_scheme.cpp") and stage < 4:
            text = re.sub(
                r'(#ifdef MODULE_COLOR_SCHEME_ENABLED)\n(?:[ \t]*\n){2,}(#include "modules/color_scheme/color_scheme.h")',
                r"\1\n\n\2",
                text,
            )
        if path.endswith("test_control.cpp") and stage < 9:
            text = replace_once(text, "\tMessageQueue::get_singleton()->flush();\n", "")
        if path.endswith("test_window.cpp") and stage < 9:
            text = replace_once(text, "\tMessageQueue::get_singleton()->flush();\n", "")
        result[path] = text
    return result


def check_recipes():
    zero, last = snapshot(0), snapshot(13)
    for path in FILES:
        for label, expected, actual in [("baseline", old[path], zero[path]), ("final", final[path], last[path])]:
            if expected != actual:
                import difflib
                print("".join(difflib.unified_diff(expected.splitlines(True), actual.splitlines(True), fromfile=path + " " + label, tofile="recipe")))
                raise AssertionError((path, label))
    print(f"Recipe coverage: {len(FILES)} files; baseline and final match exactly.")


parser = argparse.ArgumentParser()
parser.add_argument("--stage", type=int, choices=range(1, 14))
args = parser.parse_args()
check_recipes()
if args.stage:
    assert git("branch", "--show-current").decode().strip() == "personal/main"
    assert not git("diff", "--cached", "--name-only").strip(), "Existing staged changes must be reviewed first."
    previous = snapshot(args.stage - 1)
    current = read_tree("HEAD")
    assert current == previous, "HEAD does not match the preceding reviewed slice."
    target = snapshot(args.stage)
    entries = []
    for path in FILES:
        if target[path] != current[path]:
            oid = git("hash-object", "-w", "--stdin", data=target[path].encode()).decode().strip()
            entries.append(f"100644 {oid}\t{path}\n")
    assert entries, "Empty stage."
    git("update-index", "--index-info", data="".join(entries).encode())
    git("diff", "--cached", "--check")
    print(git("diff", "--cached", "--stat").decode())
