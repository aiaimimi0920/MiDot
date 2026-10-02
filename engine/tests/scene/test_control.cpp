/**************************************************************************/
/*  test_control.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_control)

#include "core/input/input_map.h" // IWYU pragma: keep // Used by `SEND_GUI_ACTION` macro.
#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/message_queue.h"
#include "scene/2d/node_2d.h"
#include "scene/gui/button.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/style_box_line.h"
#include "scene/resources/style_box_texture.h"
#include "scene/resources/theme.h"
#include "tests/display_server_mock.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

#include "modules/color_scheme/color_scheme.h"

namespace TestControl {

static int get_property_count(Object *p_object, const StringName &p_property, uint32_t *r_usage = nullptr) {
	List<PropertyInfo> properties;
	p_object->get_property_list(&properties);
	int count = 0;
	for (const PropertyInfo &property : properties) {
		if (property.name == p_property) {
			count++;
			if (r_usage) {
				*r_usage = property.usage;
			}
		}
	}
	return count;
}

static void check_control_dynamic_theme_override_file_roundtrip(Control *p_control, const String &p_file_prefix, const Color &p_color, ColorRole p_role, const Ref<ColorScheme> &p_color_scheme) {
	const char *extensions[] = { "tscn", "scn" };
	for (const char *extension : extensions) {
		const String path = TestUtils::get_temp_path(p_file_prefix + "." + String(extension));
		Ref<PackedScene> packed_scene = memnew(PackedScene);
		CHECK(packed_scene->pack(p_control) == OK);
		Error error = ResourceSaver::save(packed_scene, path);
		CHECK(error == OK);
		if (error == OK) {
			Ref<PackedScene> loaded_scene = ResourceLoader::load(path, "PackedScene", ResourceFormatLoader::CacheMode::CACHE_MODE_IGNORE, &error);
			CHECK(error == OK);
			CHECK(loaded_scene.is_valid());
			if (loaded_scene.is_valid()) {
				Node *instance = loaded_scene->instantiate();
				CHECK(instance != nullptr);
				if (instance != nullptr) {
					SceneTree::get_singleton()->get_root()->add_child(instance);
					Control *roundtrip = Object::cast_to<Control>(instance);
					CHECK(roundtrip != nullptr);
					if (roundtrip != nullptr) {
						CHECK(roundtrip->get_theme_color("custom_scale").is_equal_approx(p_color));
						CHECK(roundtrip->get_theme_color_role("custom_role") == p_role);
						Ref<ColorScheme> custom_scheme = roundtrip->get_theme_color_scheme("custom_scheme");
						CHECK(custom_scheme.is_valid());
						if (custom_scheme.is_valid()) {
							CHECK(custom_scheme->get_color(ColorRole::PRIMARY).is_equal_approx(p_color_scheme->get_color(ColorRole::PRIMARY)));
							CHECK(custom_scheme == roundtrip->get_theme_color_scheme("default_color_scheme"));
							CHECK(custom_scheme == roundtrip->get_theme_color_scheme("custom_style_scheme"));
						}
						Ref<StyleBox> style = roundtrip->get_theme_stylebox("custom_style");
						CHECK(style.is_valid());
						StyleBoxFlat *flat_style = Object::cast_to<StyleBoxFlat>(style.ptr());
						CHECK(flat_style != nullptr);
						if (flat_style != nullptr) {
							CHECK(flat_style->get_bg_color().is_equal_approx(Color("#123456")));
						}
					}
					memdelete(instance);
				}
			}
		}
		DirAccess::remove_absolute(path);
	}
}

static void check_button_dynamic_theme_override_file_roundtrip(Button *p_button, const Color &p_color, ColorRole p_role, const Ref<ColorScheme> &p_color_scheme) {
	check_control_dynamic_theme_override_file_roundtrip(p_button, "dynamic_theme_button", p_color, p_role, p_color_scheme);
}

TEST_CASE("[SceneTree][Control] Dynamic theme colors") {
	Control *control = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(control);

	Ref<ColorScheme> color_scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<Theme> theme = memnew(Theme);
	const Color scale(0.8, 0.6, 0.4, 0.5);
	theme->set_color("accent", "Control", Color("#ff0000"));
	theme->set_color("accent_scale", "Control", scale);
	theme->set_color_role("accent_role", "Control", ColorRole::PRIMARY);
	theme->set_color_scheme("accent_scheme", "Control", color_scheme);
	Ref<StyleBoxFlat> shared_stylebox = memnew(StyleBoxFlat);
	shared_stylebox->set_bg_color_role(ColorRole::PRIMARY);
	theme->set_stylebox("panel", "Control", shared_stylebox);
	theme->set_color_scheme("panel_scheme", "Control", color_scheme);
	control->set_theme(theme);

	CHECK(control->get_theme_color("accent").is_equal_approx(color_scheme->get_color(ColorRole::PRIMARY) * scale));

	control->add_theme_color_override("accent", Color("#00ff00"));
	CHECK(control->get_theme_color("accent").is_equal_approx(Color("#00ff00")));

	control->add_theme_color_role_override("accent_role", ColorRole::SECONDARY);
	CHECK(control->get_theme_color("accent").is_equal_approx(color_scheme->get_color(ColorRole::SECONDARY) * scale));

	color_scheme->set_dark(true);
	MessageQueue::get_singleton()->flush();
	CHECK(control->get_theme_color("accent").is_equal_approx(color_scheme->get_color(ColorRole::SECONDARY) * scale));
	Ref<StyleBoxFlat> resolved_stylebox = control->get_theme_stylebox("panel");
	CHECK(resolved_stylebox.is_valid());
	CHECK(resolved_stylebox != shared_stylebox);
	CHECK(shared_stylebox->get_default_color_scheme().is_null());
	CHECK(resolved_stylebox->get_default_color_scheme() == color_scheme);
	CHECK(resolved_stylebox->get_resolved_bg_color().is_equal_approx(color_scheme->get_color(ColorRole::PRIMARY)));

	control->remove_theme_color_role_override("accent_role");
	CHECK(control->get_theme_color("accent").is_equal_approx(Color("#00ff00")));

	memdelete(control);
}

TEST_CASE("[SceneTree][Control] Dynamic theme styleboxes isolate effective schemes") {
	Control *first_control = memnew(Control);
	Control *second_control = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(first_control);
	SceneTree::get_singleton()->get_root()->add_child(second_control);

	Ref<ColorScheme> first_scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<ColorScheme> second_scheme = memnew(ColorScheme(Color("#006495")));
	Ref<ColorScheme> replacement_scheme = memnew(ColorScheme(Color("#b3261e")));
	Ref<StyleBoxFlat> shared_stylebox = memnew(StyleBoxFlat);
	shared_stylebox->set_bg_color(Color("#123456"));
	shared_stylebox->set_bg_color_role(ColorRole::PRIMARY);

	Ref<Theme> theme = memnew(Theme);
	theme->set_stylebox("panel", "Control", shared_stylebox);
	theme->set_color_scheme("panel_scheme", "Control", first_scheme);
	first_control->set_theme(theme);
	second_control->set_theme(theme);
	second_control->add_theme_color_scheme_override("panel_scheme", second_scheme);

	Ref<StyleBoxFlat> first_stylebox = first_control->get_theme_stylebox("panel");
	Ref<StyleBoxFlat> second_stylebox = second_control->get_theme_stylebox("panel");
	CHECK(first_stylebox.is_valid());
	CHECK(second_stylebox.is_valid());
	CHECK(first_stylebox != shared_stylebox);
	CHECK(second_stylebox != shared_stylebox);
	CHECK(first_stylebox != second_stylebox);
	CHECK(first_stylebox->get_default_color_scheme() == first_scheme);
	CHECK(second_stylebox->get_default_color_scheme() == second_scheme);
	CHECK(first_stylebox->get_resolved_bg_color().is_equal_approx(first_scheme->get_color(ColorRole::PRIMARY)));
	CHECK(second_stylebox->get_resolved_bg_color().is_equal_approx(second_scheme->get_color(ColorRole::PRIMARY)));
	CHECK(shared_stylebox->get_default_color_scheme().is_null());
	CHECK(shared_stylebox->get_bg_color().is_equal_approx(Color("#123456")));

	second_control->add_theme_color_scheme_override("panel_scheme", replacement_scheme);
	Ref<StyleBoxFlat> replacement_stylebox = second_control->get_theme_stylebox("panel");
	CHECK(replacement_stylebox != second_stylebox);
	CHECK(replacement_stylebox->get_default_color_scheme() == replacement_scheme);
	CHECK(first_control->get_theme_stylebox("panel") == first_stylebox);

	shared_stylebox->set_bg_color(Color("#abcdef"));
	MessageQueue::get_singleton()->flush();
	Ref<StyleBoxFlat> updated_first_stylebox = first_control->get_theme_stylebox("panel");
	Ref<StyleBoxFlat> updated_replacement_stylebox = second_control->get_theme_stylebox("panel");
	CHECK(updated_first_stylebox != first_stylebox);
	CHECK(updated_replacement_stylebox != replacement_stylebox);
	CHECK(updated_first_stylebox->get_bg_color().is_equal_approx(Color("#abcdef")));
	CHECK(updated_replacement_stylebox->get_bg_color().is_equal_approx(Color("#abcdef")));
	CHECK(updated_first_stylebox->get_resolved_bg_color().is_equal_approx(first_scheme->get_color(ColorRole::PRIMARY)));
	CHECK(updated_replacement_stylebox->get_resolved_bg_color().is_equal_approx(replacement_scheme->get_color(ColorRole::PRIMARY)));

	shared_stylebox->set_bg_color_role(ColorRole::STATIC);
	MessageQueue::get_singleton()->flush();
	CHECK(first_control->get_theme_stylebox("panel").ptr() == shared_stylebox.ptr());
	shared_stylebox->set_bg_color_role(ColorRole::PRIMARY);
	MessageQueue::get_singleton()->flush();
	CHECK(first_control->get_theme_stylebox("panel").ptr() != shared_stylebox.ptr());

	memdelete(second_control);
	memdelete(first_control);
}

TEST_CASE("[SceneTree][Control] Static built-in theme styleboxes reuse their source") {
	Control *control = memnew(Control);
	Window *window = memnew(Window);
	SceneTree::get_singleton()->get_root()->add_child(control);
	SceneTree::get_singleton()->get_root()->add_child(window);

	Ref<ColorScheme> color_scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<StyleBoxFlat> flat = memnew(StyleBoxFlat);
	Ref<StyleBoxLine> line = memnew(StyleBoxLine);
	Ref<StyleBoxTexture> texture = memnew(StyleBoxTexture);
	Ref<StyleBoxEmpty> empty = memnew(StyleBoxEmpty);
	Ref<Theme> theme = memnew(Theme);
	theme->set_default_color_scheme(color_scheme);
	const Ref<StyleBox> styleboxes[] = { flat, line, texture, empty };
	const StringName names[] = { "flat", "line", "texture", "empty" };
	for (int i = 0; i < 4; i++) {
		theme->set_stylebox(names[i], "Control", styleboxes[i]);
		theme->set_stylebox(names[i], "Window", styleboxes[i]);
	}
	control->set_theme(theme);
	window->set_theme(theme);

	for (int i = 0; i < 4; i++) {
		CHECK(control->get_theme_stylebox(names[i]).ptr() == styleboxes[i].ptr());
		CHECK(window->get_theme_stylebox(names[i]).ptr() == styleboxes[i].ptr());
		CHECK(styleboxes[i]->get_default_color_scheme().is_null());
	}

	memdelete(window);
	memdelete(control);
}

template <typename T>
static void check_dynamic_theme_color_cache() {
	Control *parent = memnew(Control);
	T *node = memnew(T);
	SceneTree::get_singleton()->get_root()->add_child(parent);
	parent->add_child(node);
	const StringName type = node->get_class_name();
	Ref<ColorScheme> scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<Theme> theme = memnew(Theme);
	const Color authored("#123456");
	Color scale(0.8, 0.6, 0.4, 0.5);
	theme->set_default_color_scheme(scheme);
	theme->set_color("cached", type, authored);
	theme->set_color("cached_scale", type, scale);
	theme->set_color_role("cached_role", type, ColorRole::PRIMARY);
	theme->set_color_role("cached_role", "Other", ColorRole::ERROR);
	theme->set_type_variation("CacheVariation", type);
	parent->set_theme(theme);
	node->add_theme_color_role_override("cached_role", ColorRole::SECONDARY);

	auto check_cached = [&](const Color &p_expected, const StringName &p_type = StringName()) {
		CHECK(node->get_theme_color("cached", p_type).is_equal_approx(p_expected));
#ifdef DEV_ENABLED
		const uint64_t queries = scheme->get_color_query_count();
#endif
		for (int i = 0; i < 32; i++) {
			CHECK(node->get_theme_color("cached", p_type).is_equal_approx(p_expected));
		}
#ifdef DEV_ENABLED
		CHECK(scheme->get_color_query_count() == queries);
#endif
	};

	MessageQueue::get_singleton()->flush();
	check_cached(scheme->get_secondary() * scale);
	check_cached(scheme->get_error(), "Other");
	node->set_theme_type_variation("CacheVariation");
	check_cached(scheme->get_secondary() * scale, "CacheVariation");
	scheme->set_dark(true);
	MessageQueue::get_singleton()->flush();
	check_cached(scheme->get_secondary() * scale);
	scheme->set_source_color(Color("#006495"));
	MessageQueue::get_singleton()->flush();
	check_cached(scheme->get_secondary() * scale);
	scheme->set_contrast_level(1.0f);
	MessageQueue::get_singleton()->flush();
	check_cached(scheme->get_secondary() * scale);
	scale = Color(0.5, 0.4, 0.3, 0.2);
	node->add_theme_color_override("cached_scale", scale);
	check_cached(scheme->get_secondary() * scale);
	node->add_theme_color_role_override("cached_scale_role", ColorRole::TERTIARY);
	check_cached(scheme->get_secondary() * scheme->get_tertiary());
	node->remove_theme_color_role_override("cached_scale_role");

	Ref<ColorScheme> replacement = memnew(ColorScheme(Color("#b3261e")));
	node->add_theme_color_scheme_override("cached_scheme", replacement);
	check_cached(replacement->get_secondary() * scale);
	node->remove_theme_color_scheme_override("cached_scheme");
	check_cached(scheme->get_secondary() * scale);
	node->add_theme_color_role_override("cached_role", ColorRole::STATIC);
	check_cached(authored);
	node->add_theme_color_override("cached", Color("#abcdef"));
	check_cached(Color("#abcdef"));
	node->remove_theme_color_role_override("cached_role");
	check_cached(Color("#abcdef"));
	node->remove_theme_color_override("cached");
	check_cached(scheme->get_primary() * scale);

	Ref<Theme> new_parent_theme = memnew(Theme);
	new_parent_theme->set_default_color_scheme(replacement);
	new_parent_theme->set_color_role("cached_role", type, ColorRole::ERROR);
	parent->set_theme(new_parent_theme);
	check_cached(replacement->get_error() * scale);
	memdelete(parent);
}

TEST_CASE("[SceneTree][Control][Window] Dynamic theme color caches invalidate without repeated evaluation") {
	check_dynamic_theme_color_cache<Control>();
	check_dynamic_theme_color_cache<Window>();
}

TEST_CASE("[SceneTree][Control] Dynamic theme color presence and STATIC role") {
	Control *control = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(control);

	Ref<ColorScheme> color_scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<Theme> theme = memnew(Theme);
	theme->set_default_color_scheme(color_scheme);
	theme->set_type_variation("TestVariation", "Control");
	const Color static_color("#ff0000");
	theme->set_color("accent", "Control", static_color);
	theme->set_color_role("accent_role", "Control", ColorRole::PRIMARY);
	theme->set_color_scheme("accent_scheme", "Control", color_scheme);
	theme->set_color_role("accent_scale_role", "Control", ColorRole::SECONDARY);
	theme->set_color_scheme("accent_scale_scheme", "Control", color_scheme);
	theme->set_color_role("role_only_role", "Control", ColorRole::ERROR);
	theme->set_color_scheme("role_only_scheme", "Control", color_scheme);
	theme->set_color_role("masked_role_only_role", "Control", ColorRole::ERROR);
	theme->set_color_scheme("masked_role_only_scheme", "Control", color_scheme);
	theme->set_color_role("default_scheme_role", "Control", ColorRole::ERROR);
	theme->set_color("static_only", "Control", static_color);
	theme->set_color_role("variation_role", "TestVariation", ColorRole::TERTIARY);
	theme->set_color_scheme("variation_scheme", "TestVariation", color_scheme);
	theme->set_color("variation", "TestVariation", static_color);
	theme->set_color_role("explicit_role", "Other", ColorRole::ERROR);
	theme->set_color_scheme("explicit_scheme", "Other", color_scheme);
	control->set_theme(theme);

	CHECK_FALSE(control->has_theme_color("missing"));
	CHECK(control->has_theme_color("static_only"));
	CHECK(control->get_theme_color("static_only").is_equal_approx(static_color));
	CHECK(control->has_theme_color("accent"));
	CHECK(control->get_theme_color("accent").is_equal_approx(color_scheme->get_color(ColorRole::PRIMARY) * color_scheme->get_color(ColorRole::SECONDARY)));
	CHECK(control->has_theme_color("accent_scale"));
	CHECK(control->get_theme_color("accent_scale").is_equal_approx(color_scheme->get_color(ColorRole::SECONDARY)));

	CHECK(control->has_theme_color("role_only"));
	CHECK(control->get_theme_color("role_only").is_equal_approx(color_scheme->get_color(ColorRole::ERROR)));
	CHECK(control->has_theme_color("default_scheme"));
	CHECK(control->get_theme_color("default_scheme").is_equal_approx(color_scheme->get_color(ColorRole::ERROR)));
	control->add_theme_color_role_override("explicit_role", ColorRole::STATIC);
	CHECK(control->has_theme_color("explicit", "Other"));
	CHECK(control->get_theme_color("explicit", "Other").is_equal_approx(color_scheme->get_color(ColorRole::ERROR)));

	control->set_theme_type_variation("TestVariation");
	CHECK(control->has_theme_color("variation"));
	control->add_theme_color_role_override("variation_role", ColorRole::STATIC);
	CHECK(control->has_theme_color("variation"));
	CHECK(control->get_theme_color("variation").is_equal_approx(static_color));
	control->remove_theme_color_role_override("variation_role");
	CHECK(control->get_theme_color("variation").is_equal_approx(color_scheme->get_color(ColorRole::TERTIARY)));

	control->add_theme_color_role_override("accent_role", ColorRole::STATIC);
	CHECK(control->get_theme_color("accent").is_equal_approx(static_color));
	control->add_theme_color_override("accent", Color("#00ff00"));
	CHECK(control->get_theme_color("accent").is_equal_approx(Color("#00ff00")));

	control->add_theme_color_role_override("masked_role_only_role", ColorRole::STATIC);
	CHECK_FALSE(control->has_theme_color("masked_role_only"));
	CHECK(control->get_theme_color("masked_role_only").is_equal_approx(Color()));

	memdelete(control);
}

TEST_CASE("[SceneTree][Control] Dynamic theme override properties serialize") {
	Button *button = memnew(Button);
	Ref<ColorScheme> color_scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<StyleBoxFlat> style = memnew(StyleBoxFlat);
	style->set_bg_color(Color("#123456"));
	button->add_theme_color_override("custom_scale", Color(0.8, 0.6, 0.4, 0.5));
	button->add_theme_color_role_override("custom_role", ColorRole::SECONDARY);
	button->add_theme_color_scheme_override("custom_scheme", color_scheme);
	button->add_theme_color_scheme_override("default_color_scheme", color_scheme);
	button->add_theme_style_override("custom_style", style);
	button->add_theme_color_scheme_override("custom_style_scheme", color_scheme);

	uint32_t property_usage = 0;
	CHECK(get_property_count(button, "theme_override_colors/font_color") == 1);
	CHECK(get_property_count(button, "theme_override_color_roles/font_color_role") == 1);
	CHECK(get_property_count(button, "theme_override_color_schemes/font_color_scheme") == 1);
	CHECK(get_property_count(button, "theme_override_colors/font_color_scale") == 1);
	CHECK(get_property_count(button, "theme_override_color_schemes/normal_scheme") == 1);
	CHECK(get_property_count(button, "theme_override_color_schemes/default_color_scheme", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(button, "theme_override_colors/custom_scale", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(button, "theme_override_color_roles/custom_role", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(button, "theme_override_color_schemes/custom_scheme", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(button, "theme_override_styles/custom_style", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(button, "theme_override_color_schemes/custom_style_scheme") == 1);

	Ref<PackedScene> packed_scene = memnew(PackedScene);
	CHECK(packed_scene->pack(button) == OK);
	Node *instance = packed_scene->instantiate();
	CHECK(instance != nullptr);
	if (instance) {
		SceneTree::get_singleton()->get_root()->add_child(instance);
		Button *roundtrip = Object::cast_to<Button>(instance);
		CHECK(roundtrip != nullptr);
		if (roundtrip) {
			CHECK(roundtrip->get_theme_color("custom_scale").is_equal_approx(Color(0.8, 0.6, 0.4, 0.5)));
			CHECK(roundtrip->get_theme_color_role("custom_role") == ColorRole::SECONDARY);
				CHECK(roundtrip->get_theme_color_scheme("custom_scheme") == color_scheme);
				CHECK(roundtrip->get_theme_color_scheme("default_color_scheme") == color_scheme);
				CHECK(roundtrip->get_theme_color_scheme("custom_style_scheme") == color_scheme);
				Ref<StyleBox> roundtrip_style = roundtrip->get_theme_stylebox("custom_style");
				StyleBoxFlat *roundtrip_flat = Object::cast_to<StyleBoxFlat>(roundtrip_style.ptr());
				CHECK(roundtrip_flat != nullptr);
				if (roundtrip_flat != nullptr) {
					CHECK(roundtrip_flat->get_bg_color().is_equal_approx(Color("#123456")));
				}
		}
		memdelete(instance);
	}
	Node *duplicate = button->duplicate();
	CHECK(duplicate != nullptr);
	if (duplicate) {
		Button *duplicate_button = Object::cast_to<Button>(duplicate);
		CHECK(duplicate_button != nullptr);
		if (duplicate_button) {
			CHECK(duplicate_button->get_theme_color("custom_scale").is_equal_approx(Color(0.8, 0.6, 0.4, 0.5)));
			CHECK(duplicate_button->get_theme_color_role("custom_role") == ColorRole::SECONDARY);
			CHECK(duplicate_button->get_theme_color_scheme("custom_scheme") == color_scheme);
			CHECK(duplicate_button->get_theme_color_scheme("default_color_scheme") == color_scheme);
			CHECK(duplicate_button->get_theme_color_scheme("custom_style_scheme") == color_scheme);
		}
		memdelete(duplicate);
	}
	check_button_dynamic_theme_override_file_roundtrip(button, Color(0.8, 0.6, 0.4, 0.5), ColorRole::SECONDARY, color_scheme);
	memdelete(button);
}

TEST_CASE("[SceneTree][Control] Dynamic theme override properties include Label") {
	Label *label = memnew(Label);
	Ref<ColorScheme> color_scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<StyleBoxFlat> style = memnew(StyleBoxFlat);
	style->set_bg_color(Color("#123456"));
	label->add_theme_color_override("custom_scale", Color(0.8, 0.6, 0.4, 0.5));
	label->add_theme_color_role_override("custom_role", ColorRole::SECONDARY);
	label->add_theme_color_scheme_override("custom_scheme", color_scheme);
	label->add_theme_color_scheme_override("default_color_scheme", color_scheme);
	label->add_theme_style_override("custom_style", style);
	label->add_theme_color_scheme_override("custom_style_scheme", color_scheme);

	uint32_t property_usage = 0;
	CHECK(get_property_count(label, "theme_override_colors/font_color") == 1);
	CHECK(get_property_count(label, "theme_override_color_roles/font_color_role") == 1);
	CHECK(get_property_count(label, "theme_override_color_schemes/font_color_scheme") == 1);
	CHECK(get_property_count(label, "theme_override_colors/font_color_scale") == 1);
	CHECK(get_property_count(label, "theme_override_color_schemes/default_color_scheme", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(label, "theme_override_colors/custom_scale", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(label, "theme_override_color_roles/custom_role", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(label, "theme_override_color_schemes/custom_scheme", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(label, "theme_override_styles/custom_style", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(label, "theme_override_color_schemes/custom_style_scheme") == 1);

	Ref<PackedScene> packed_scene = memnew(PackedScene);
	CHECK(packed_scene->pack(label) == OK);
	Node *instance = packed_scene->instantiate();
	CHECK(instance != nullptr);
	if (instance != nullptr) {
		SceneTree::get_singleton()->get_root()->add_child(instance);
		Label *roundtrip = Object::cast_to<Label>(instance);
		CHECK(roundtrip != nullptr);
		if (roundtrip != nullptr) {
			CHECK(roundtrip->get_theme_color("custom_scale").is_equal_approx(Color(0.8, 0.6, 0.4, 0.5)));
			CHECK(roundtrip->get_theme_color_role("custom_role") == ColorRole::SECONDARY);
			CHECK(roundtrip->get_theme_color_scheme("custom_scheme") == color_scheme);
			CHECK(roundtrip->get_theme_color_scheme("default_color_scheme") == color_scheme);
			CHECK(roundtrip->get_theme_color_scheme("custom_style_scheme") == color_scheme);
			Ref<StyleBox> roundtrip_style = roundtrip->get_theme_stylebox("custom_style");
			StyleBoxFlat *roundtrip_flat = Object::cast_to<StyleBoxFlat>(roundtrip_style.ptr());
			CHECK(roundtrip_flat != nullptr);
			if (roundtrip_flat != nullptr) {
				CHECK(roundtrip_flat->get_bg_color().is_equal_approx(Color("#123456")));
			}
		}
		memdelete(instance);
	}
	Node *duplicate = label->duplicate();
	CHECK(duplicate != nullptr);
	if (duplicate != nullptr) {
		Label *duplicate_label = Object::cast_to<Label>(duplicate);
		CHECK(duplicate_label != nullptr);
		if (duplicate_label != nullptr) {
			CHECK(duplicate_label->get_theme_color("custom_scale").is_equal_approx(Color(0.8, 0.6, 0.4, 0.5)));
			CHECK(duplicate_label->get_theme_color_role("custom_role") == ColorRole::SECONDARY);
			CHECK(duplicate_label->get_theme_color_scheme("custom_scheme") == color_scheme);
			CHECK(duplicate_label->get_theme_color_scheme("default_color_scheme") == color_scheme);
			CHECK(duplicate_label->get_theme_color_scheme("custom_style_scheme") == color_scheme);
			Ref<StyleBox> duplicate_style = duplicate_label->get_theme_stylebox("custom_style");
			StyleBoxFlat *duplicate_flat = Object::cast_to<StyleBoxFlat>(duplicate_style.ptr());
			CHECK(duplicate_flat != nullptr);
			if (duplicate_flat != nullptr) {
				CHECK(duplicate_flat->get_bg_color().is_equal_approx(Color("#123456")));
			}
		}
		memdelete(duplicate);
	}
	check_control_dynamic_theme_override_file_roundtrip(label, "dynamic_theme_label", Color(0.8, 0.6, 0.4, 0.5), ColorRole::SECONDARY, color_scheme);

	memdelete(label);
}

TEST_CASE("[SceneTree][Control] String theme overrides") {
	Control *control = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(control);

	Ref<Theme> theme = memnew(Theme);
	theme->set_string("label", "Control", "theme value");
	control->set_theme(theme);

	CHECK(control->get_theme_string("label") == "theme value");
	control->add_theme_string_override("label", "override value");
	CHECK(control->has_theme_string_override("label"));
	CHECK(control->get_theme_string("label") == "override value");

	control->set("theme_override_strings/label", "property value");
	CHECK(control->get("theme_override_strings/label") == "property value");
	CHECK(control->get_theme_string("label") == "property value");

	control->remove_theme_string_override("label");
	CHECK_FALSE(control->has_theme_string_override("label"));
	CHECK(control->get_theme_string("label") == "theme value");

	memdelete(control);
}

TEST_CASE("[SceneTree][Control] Transforms") {
	SUBCASE("[Control][Global Transform] Global Transform should be accessible while not in SceneTree.") { // GH-79453
		Control *test_node = memnew(Control);
		Control *test_child = memnew(Control);
		test_node->add_child(test_child);

		test_node->set_global_position(Point2(1, 1));
		CHECK_EQ(test_node->get_global_position(), Point2(1, 1));
		CHECK_EQ(test_child->get_global_position(), Point2(1, 1));
		test_node->set_global_position(Point2(2, 2));
		CHECK_EQ(test_node->get_global_position(), Point2(2, 2));
		test_node->set_scale(Vector2(4, 4));
		CHECK_EQ(test_node->get_global_transform(), Transform2D(0, Size2(4, 4), 0, Vector2(2, 2)));
		test_node->set_scale(Vector2(1, 1));
		test_node->set_rotation_degrees(90);
		CHECK_EQ(test_node->get_global_transform(), Transform2D(Math::PI / 2, Vector2(2, 2)));
		test_node->set_pivot_offset(Vector2(1, 0));
		CHECK_EQ(test_node->get_global_transform(), Transform2D(Math::PI / 2, Vector2(3, 1)));

		memdelete(test_child);
		memdelete(test_node);
	}
}

TEST_CASE("[SceneTree][Control] Focus") {
	Control *ctrl = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(ctrl);

	SUBCASE("[SceneTree][Control] Default focus") {
		CHECK_UNARY_FALSE(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] Can't grab focus with default focus mode") {
		ERR_PRINT_OFF
		ctrl->grab_focus();
		ERR_PRINT_ON

		CHECK_UNARY_FALSE(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] Can grab focus") {
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->grab_focus();

		CHECK_UNARY(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] Can release focus") {
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->grab_focus();
		CHECK_UNARY(ctrl->has_focus());

		ctrl->release_focus();
		CHECK_UNARY_FALSE(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] Only one can grab focus at the same time") {
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->grab_focus();
		CHECK_UNARY(ctrl->has_focus());

		Control *other_ctrl = memnew(Control);
		SceneTree::get_singleton()->get_root()->add_child(other_ctrl);
		other_ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		other_ctrl->grab_focus();

		CHECK_UNARY(other_ctrl->has_focus());
		CHECK_UNARY_FALSE(ctrl->has_focus());

		memdelete(other_ctrl);
	}

	SUBCASE("[SceneTree][Control] Hide control will cause the focus to be released") {
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->grab_focus();
		CHECK_UNARY(ctrl->has_focus());

		ctrl->hide();
		CHECK_UNARY_FALSE(ctrl->has_focus());

		ctrl->show();
		CHECK_UNARY_FALSE(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] The parent node is hidden causing the focus to be released") {
		Control *child_ctrl = memnew(Control);
		ctrl->add_child(child_ctrl);

		child_ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		child_ctrl->grab_focus();
		CHECK_UNARY(child_ctrl->has_focus());

		ctrl->hide();
		CHECK_UNARY_FALSE(child_ctrl->has_focus());

		ctrl->show();
		CHECK_UNARY_FALSE(child_ctrl->has_focus());

		memdelete(child_ctrl);
	}

	SUBCASE("[SceneTree][Control] Grab focus with focus behavior recursive") {
		CHECK_UNARY_FALSE(ctrl->has_focus());

		// Cannot grab focus if focus behavior is disabled.
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->set_focus_behavior_recursive(Control::FOCUS_BEHAVIOR_DISABLED);

		ERR_PRINT_OFF
		ctrl->grab_focus();
		ERR_PRINT_ON
		CHECK_UNARY_FALSE(ctrl->has_focus());

		// Cannot grab focus if focus behavior is enabled but focus mode is none.
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_NONE);
		ctrl->set_focus_behavior_recursive(Control::FOCUS_BEHAVIOR_ENABLED);

		ERR_PRINT_OFF
		ctrl->grab_focus();
		ERR_PRINT_ON
		CHECK_UNARY_FALSE(ctrl->has_focus());

		// Can grab focus if focus behavior is enabled and focus mode is all.
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->set_focus_behavior_recursive(Control::FOCUS_BEHAVIOR_ENABLED);

		ctrl->grab_focus();
		CHECK_UNARY(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] Children focus with focus behavior recursive") {
		Control *child_control = memnew(Control);
		ctrl->add_child(child_control);

		// Can grab focus on child if parent focus behavior is inherit.
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->set_focus_behavior_recursive(Control::FOCUS_BEHAVIOR_INHERITED);
		child_control->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		child_control->set_focus_behavior_recursive(Control::FOCUS_BEHAVIOR_INHERITED);

		child_control->grab_focus();
		CHECK_UNARY(child_control->has_focus());

		// Cannot grab focus on child if parent focus behavior is disabled.
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->set_focus_behavior_recursive(Control::FOCUS_BEHAVIOR_DISABLED);
		child_control->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		child_control->set_focus_behavior_recursive(Control::FOCUS_BEHAVIOR_INHERITED);

		ERR_PRINT_OFF
		child_control->grab_focus();
		ERR_PRINT_ON
		CHECK_UNARY_FALSE(child_control->has_focus());

		memdelete(child_control);
	}

	memdelete(ctrl);
}

TEST_CASE("[SceneTree][Control] Find next/prev valid focus") {
	Node *intermediate = memnew(Node);
	Control *ctrl = memnew(Control);
	intermediate->add_child(ctrl);
	SceneTree::get_singleton()->get_root()->add_child(intermediate);

	SUBCASE("[SceneTree][Control] In FOCUS_CLICK mode") {
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_CLICK);
		ctrl->grab_focus();
		REQUIRE_UNARY(ctrl->has_focus());

		SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
			SEND_GUI_ACTION("ui_focus_next");
			CHECK_UNARY(ctrl->has_focus());
		}

		SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
			SEND_GUI_ACTION("ui_focus_prev");
			CHECK_UNARY(ctrl->has_focus());
		}

		SUBCASE("[SceneTree][Control] Has a sibling control and the parent is a window") {
			Control *ctrl1 = memnew(Control);
			Control *ctrl2 = memnew(Control);
			Control *ctrl3 = memnew(Control);
			Window *win = SceneTree::get_singleton()->get_root();

			ctrl1->set_focus_mode(Control::FocusMode::FOCUS_ALL);
			ctrl2->set_focus_mode(Control::FocusMode::FOCUS_ALL);
			ctrl3->set_focus_mode(Control::FocusMode::FOCUS_ALL);

			ctrl2->add_child(ctrl3);
			win->add_child(ctrl1);
			win->add_child(ctrl2);

			SUBCASE("[SceneTree][Control] Focus Next") {
				ctrl1->grab_focus();
				CHECK_UNARY(ctrl1->has_focus());

				SEND_GUI_ACTION("ui_focus_next");
				CHECK_UNARY(ctrl2->has_focus());

				SEND_GUI_ACTION("ui_focus_next");
				CHECK_UNARY(ctrl3->has_focus());

				SEND_GUI_ACTION("ui_focus_next");
				CHECK_UNARY(ctrl1->has_focus());
			}

			SUBCASE("[SceneTree][Control] Focus Prev") {
				ctrl1->grab_focus();
				CHECK_UNARY(ctrl1->has_focus());

				SEND_GUI_ACTION("ui_focus_prev");
				CHECK_UNARY(ctrl3->has_focus());

				SEND_GUI_ACTION("ui_focus_prev");
				CHECK_UNARY(ctrl2->has_focus());

				SEND_GUI_ACTION("ui_focus_prev");
				CHECK_UNARY(ctrl1->has_focus());
			}

			memdelete(ctrl3);
			memdelete(ctrl1);
			memdelete(ctrl2);
		}

		SUBCASE("[SceneTree][Control] Has a sibling control but the parent node is not a control or window") {
			Control *other_ctrl = memnew(Control);
			intermediate->add_child(other_ctrl);

			SUBCASE("[SceneTree][Control] Has a sibling control with FOCUS_ALL") {
				other_ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
				REQUIRE_EQ(other_ctrl->get_focus_mode(), Control::FocusMode::FOCUS_ALL);

				SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
					SEND_GUI_ACTION("ui_focus_next");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
					SEND_GUI_ACTION("ui_focus_prev");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Manually specify focus next") {
					ctrl->set_focus_next(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY_FALSE(ctrl->has_focus());
						CHECK_UNARY(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Manually specified focus next is hidden") {
						other_ctrl->hide();
						REQUIRE_UNARY_FALSE(other_ctrl->is_visible());

						SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
							SEND_GUI_ACTION("ui_focus_next");
							CHECK_UNARY(ctrl->has_focus());
							CHECK_UNARY_FALSE(other_ctrl->has_focus());
						}

						SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
							SEND_GUI_ACTION("ui_focus_prev");
							CHECK_UNARY(ctrl->has_focus());
							CHECK_UNARY_FALSE(other_ctrl->has_focus());
						}
					}
				}

				SUBCASE("[SceneTree][Control] Manually specify focus prev") {
					ctrl->set_focus_previous(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY_FALSE(ctrl->has_focus());
						CHECK_UNARY(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Manually specified focus next is hidden") {
						other_ctrl->hide();
						REQUIRE_UNARY_FALSE(other_ctrl->is_visible());

						SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
							SEND_GUI_ACTION("ui_focus_next");
							CHECK_UNARY(ctrl->has_focus());
							CHECK_UNARY_FALSE(other_ctrl->has_focus());
						}

						SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
							SEND_GUI_ACTION("ui_focus_prev");
							CHECK_UNARY(ctrl->has_focus());
							CHECK_UNARY_FALSE(other_ctrl->has_focus());
						}
					}
				}
			}

			SUBCASE("[SceneTree][Control] Has a sibling control with FOCUS_CLICK") {
				other_ctrl->set_focus_mode(Control::FocusMode::FOCUS_CLICK);
				REQUIRE_EQ(other_ctrl->get_focus_mode(), Control::FocusMode::FOCUS_CLICK);

				SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
					SEND_GUI_ACTION("ui_focus_next");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
					SEND_GUI_ACTION("ui_focus_prev");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Manually specify focus next") {
					ctrl->set_focus_next(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY_FALSE(ctrl->has_focus());
						CHECK_UNARY(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}
				}

				SUBCASE("[SceneTree][Control] Manually specify focus prev") {
					ctrl->set_focus_previous(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY_FALSE(ctrl->has_focus());
						CHECK_UNARY(other_ctrl->has_focus());
					}
				}
			}

			SUBCASE("[SceneTree][Control] Has a sibling control with FOCUS_NONE") {
				other_ctrl->set_focus_mode(Control::FocusMode::FOCUS_NONE);
				REQUIRE_EQ(other_ctrl->get_focus_mode(), Control::FocusMode::FOCUS_NONE);

				SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
					SEND_GUI_ACTION("ui_focus_next");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
					SEND_GUI_ACTION("ui_focus_prev");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Manually specify focus next") {
					ctrl->set_focus_next(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}
				}

				SUBCASE("[SceneTree][Control] Manually specify focus prev") {
					ctrl->set_focus_previous(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}
				}
			}

			memdelete(other_ctrl);
		}
	}

	SUBCASE("[SceneTree][Control] In FOCUS_ALL mode") {
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		REQUIRE_EQ(ctrl->get_focus_mode(), Control::FocusMode::FOCUS_ALL);

		ctrl->grab_focus();
		REQUIRE_UNARY(ctrl->has_focus());

		SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
			SEND_GUI_ACTION("ui_focus_next");
			CHECK_UNARY(ctrl->has_focus());
		}

		SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
			SEND_GUI_ACTION("ui_focus_prev");
			CHECK_UNARY(ctrl->has_focus());
		}

		SUBCASE("[SceneTree][Control] Has a sibling control but the parent node is not a control") {
			Control *other_ctrl = memnew(Control);
			SceneTree::get_singleton()->get_root()->add_child(other_ctrl);

			SUBCASE("[SceneTree][Control] Has a sibling control with FOCUS_ALL") {
				other_ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
				REQUIRE_EQ(other_ctrl->get_focus_mode(), Control::FocusMode::FOCUS_ALL);

				SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
					SEND_GUI_ACTION("ui_focus_next");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
					SEND_GUI_ACTION("ui_focus_prev");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Manually specify focus next") {
					ctrl->set_focus_next(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY_FALSE(ctrl->has_focus());
						CHECK_UNARY(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Manually specified focus next is hidden") {
						other_ctrl->hide();
						REQUIRE_UNARY_FALSE(other_ctrl->is_visible());

						SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
							SEND_GUI_ACTION("ui_focus_next");
							CHECK_UNARY(ctrl->has_focus());
							CHECK_UNARY_FALSE(other_ctrl->has_focus());
						}

						SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
							SEND_GUI_ACTION("ui_focus_prev");
							CHECK_UNARY(ctrl->has_focus());
							CHECK_UNARY_FALSE(other_ctrl->has_focus());
						}
					}
				}

				SUBCASE("[SceneTree][Control] Manually specify focus prev") {
					ctrl->set_focus_previous(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY_FALSE(ctrl->has_focus());
						CHECK_UNARY(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Manually specified focus next is hidden") {
						other_ctrl->hide();
						REQUIRE_UNARY_FALSE(other_ctrl->is_visible());

						SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
							SEND_GUI_ACTION("ui_focus_next");
							CHECK_UNARY(ctrl->has_focus());
							CHECK_UNARY_FALSE(other_ctrl->has_focus());
						}

						SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
							SEND_GUI_ACTION("ui_focus_prev");
							CHECK_UNARY(ctrl->has_focus());
							CHECK_UNARY_FALSE(other_ctrl->has_focus());
						}
					}
				}
			}

			SUBCASE("[SceneTree][Control] Has a sibling control with FOCUS_CLICK") {
				other_ctrl->set_focus_mode(Control::FocusMode::FOCUS_CLICK);
				REQUIRE_EQ(other_ctrl->get_focus_mode(), Control::FocusMode::FOCUS_CLICK);

				SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
					SEND_GUI_ACTION("ui_focus_next");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
					SEND_GUI_ACTION("ui_focus_prev");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Manually specify focus next") {
					ctrl->set_focus_next(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY_FALSE(ctrl->has_focus());
						CHECK_UNARY(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}
				}

				SUBCASE("[SceneTree][Control] Manually specify focus prev") {
					ctrl->set_focus_previous(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY_FALSE(ctrl->has_focus());
						CHECK_UNARY(other_ctrl->has_focus());
					}
				}
			}

			SUBCASE("[SceneTree][Control] Has a sibling control with FOCUS_NONE") {
				other_ctrl->set_focus_mode(Control::FocusMode::FOCUS_NONE);
				REQUIRE_EQ(other_ctrl->get_focus_mode(), Control::FocusMode::FOCUS_NONE);

				SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
					SEND_GUI_ACTION("ui_focus_next");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
					SEND_GUI_ACTION("ui_focus_prev");
					CHECK_UNARY(ctrl->has_focus());
					CHECK_UNARY_FALSE(other_ctrl->has_focus());
				}

				SUBCASE("[SceneTree][Control] Manually specify focus next") {
					ctrl->set_focus_next(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}
				}

				SUBCASE("[SceneTree][Control] Manually specify focus prev") {
					ctrl->set_focus_previous(ctrl->get_path_to(other_ctrl));

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
						CHECK_UNARY_FALSE(other_ctrl->has_focus());
					}
				}
			}

			memdelete(other_ctrl);
		}

		SUBCASE("[SceneTree][Control] Simple control tree") {
			Control *ctrl_0 = memnew(Control);
			Control *ctrl_1 = memnew(Control);
			Node2D *node_2d_2 = memnew(Node2D);

			ctrl->add_child(ctrl_0);
			ctrl->add_child(ctrl_1);
			ctrl->add_child(node_2d_2);

			ctrl_0->set_focus_mode(Control::FocusMode::FOCUS_ALL);
			ctrl_1->set_focus_mode(Control::FocusMode::FOCUS_ALL);
			REQUIRE_EQ(ctrl_0->get_focus_mode(), Control::FocusMode::FOCUS_ALL);
			REQUIRE_EQ(ctrl_1->get_focus_mode(), Control::FocusMode::FOCUS_ALL);

			SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
				SEND_GUI_ACTION("ui_focus_next");
				CHECK_UNARY(ctrl_0->has_focus());

				SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
					SEND_GUI_ACTION("ui_focus_next");
					CHECK_UNARY(ctrl_1->has_focus());

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl->has_focus());
					}
				}
			}

			SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
				SEND_GUI_ACTION("ui_focus_prev");
				CHECK_UNARY(ctrl_1->has_focus());

				SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
					SEND_GUI_ACTION("ui_focus_prev");
					CHECK_UNARY(ctrl_0->has_focus());

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
					}
				}
			}

			SUBCASE("[SceneTree][Control] Skip next hidden control") {
				ctrl_0->hide();
				REQUIRE_UNARY_FALSE(ctrl_0->is_visible());
				SEND_GUI_ACTION("ui_focus_next");
				CHECK_UNARY_FALSE(ctrl_0->has_focus());
				CHECK_UNARY(ctrl_1->has_focus());
			}

			SUBCASE("[SceneTree][Control] Skip next control with FOCUS_NONE") {
				ctrl_0->set_focus_mode(Control::FocusMode::FOCUS_NONE);
				REQUIRE_EQ(ctrl_0->get_focus_mode(), Control::FocusMode::FOCUS_NONE);
				SEND_GUI_ACTION("ui_focus_next");
				CHECK_UNARY_FALSE(ctrl_0->has_focus());
				CHECK_UNARY(ctrl_1->has_focus());
			}

			SUBCASE("[SceneTree][Control] Skip next control with FOCUS_CLICK") {
				ctrl_0->set_focus_mode(Control::FocusMode::FOCUS_CLICK);
				REQUIRE_EQ(ctrl_0->get_focus_mode(), Control::FocusMode::FOCUS_CLICK);
				SEND_GUI_ACTION("ui_focus_next");
				CHECK_UNARY_FALSE(ctrl_0->has_focus());
				CHECK_UNARY(ctrl_1->has_focus());
			}

			SUBCASE("[SceneTree][Control] Skip next top level control") {
				ctrl_0->set_as_top_level(true);
				REQUIRE_UNARY(ctrl_0->is_set_as_top_level());
				SEND_GUI_ACTION("ui_focus_next");
				CHECK_UNARY_FALSE(ctrl_0->has_focus());
				CHECK_UNARY(ctrl_1->has_focus());
			}

			SUBCASE("[SceneTree][Control] Skip prev hidden control") {
				ctrl_1->hide();
				REQUIRE_UNARY_FALSE(ctrl_1->is_visible());
				SEND_GUI_ACTION("ui_focus_prev");
				CHECK_UNARY_FALSE(ctrl_1->has_focus());
				CHECK_UNARY(ctrl_0->has_focus());
			}

			SUBCASE("[SceneTree][Control] Skip prev control with FOCUS_NONE") {
				ctrl_1->set_focus_mode(Control::FocusMode::FOCUS_NONE);
				REQUIRE_EQ(ctrl_1->get_focus_mode(), Control::FocusMode::FOCUS_NONE);
				SEND_GUI_ACTION("ui_focus_prev");
				CHECK_UNARY_FALSE(ctrl_1->has_focus());
				CHECK_UNARY(ctrl_0->has_focus());
			}

			SUBCASE("[SceneTree][Control] Skip prev control with FOCUS_CLICK") {
				ctrl_1->set_focus_mode(Control::FocusMode::FOCUS_CLICK);
				REQUIRE_EQ(ctrl_1->get_focus_mode(), Control::FocusMode::FOCUS_CLICK);
				SEND_GUI_ACTION("ui_focus_prev");
				CHECK_UNARY_FALSE(ctrl_1->has_focus());
				CHECK_UNARY(ctrl_0->has_focus());
			}

			SUBCASE("[SceneTree][Control] Skip prev top level control") {
				ctrl_1->set_as_top_level(true);
				REQUIRE_UNARY(ctrl_1->is_set_as_top_level());
				SEND_GUI_ACTION("ui_focus_prev");
				CHECK_UNARY_FALSE(ctrl_1->has_focus());
				CHECK_UNARY(ctrl_0->has_focus());
			}

			SUBCASE("[SceneTree][Control] Add more node controls") {
				Control *ctrl_0_0 = memnew(Control);
				Control *ctrl_0_1 = memnew(Control);
				Control *ctrl_0_2 = memnew(Control);
				ctrl_0->add_child(ctrl_0_0);
				ctrl_0->add_child(ctrl_0_1);
				ctrl_0->add_child(ctrl_0_2);
				ctrl_0_0->set_focus_mode(Control::FocusMode::FOCUS_ALL);
				ctrl_0_1->set_focus_mode(Control::FocusMode::FOCUS_ALL);
				ctrl_0_2->set_focus_mode(Control::FocusMode::FOCUS_ALL);

				Control *ctrl_1_0 = memnew(Control);
				Control *ctrl_1_1 = memnew(Control);
				Control *ctrl_1_2 = memnew(Control);
				ctrl_1->add_child(ctrl_1_0);
				ctrl_1->add_child(ctrl_1_1);
				ctrl_1->add_child(ctrl_1_2);
				ctrl_1_0->set_focus_mode(Control::FocusMode::FOCUS_ALL);
				ctrl_1_1->set_focus_mode(Control::FocusMode::FOCUS_ALL);
				ctrl_1_2->set_focus_mode(Control::FocusMode::FOCUS_ALL);

				Control *ctrl_2_0 = memnew(Control);
				Control *ctrl_2_1 = memnew(Control);
				Control *ctrl_2_2 = memnew(Control);
				node_2d_2->add_child(ctrl_2_0);
				node_2d_2->add_child(ctrl_2_1);
				node_2d_2->add_child(ctrl_2_2);
				ctrl_2_0->set_focus_mode(Control::FocusMode::FOCUS_ALL);
				ctrl_2_1->set_focus_mode(Control::FocusMode::FOCUS_ALL);
				ctrl_2_2->set_focus_mode(Control::FocusMode::FOCUS_ALL);

				SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
					SEND_GUI_ACTION("ui_focus_next");
					CHECK_UNARY(ctrl_0->has_focus());

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl_0_0->has_focus());
					}

					SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
						SEND_GUI_ACTION("ui_focus_prev");
						CHECK_UNARY(ctrl->has_focus());
					}
				}

				SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
					SEND_GUI_ACTION("ui_focus_prev");
					CHECK_UNARY(ctrl_1_2->has_focus());
				}

				SUBCASE("[SceneTree][Control] Exist top level tree") {
					ctrl_0->set_as_top_level(true);
					REQUIRE_UNARY(ctrl_0->is_set_as_top_level());

					SUBCASE("[SceneTree][Control] Outside top level tree") {
						ctrl->grab_focus();
						REQUIRE_UNARY(ctrl->has_focus());
						SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
							SEND_GUI_ACTION("ui_focus_next");
							CHECK_UNARY(ctrl_1->has_focus());

							SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
								SEND_GUI_ACTION("ui_focus_prev");
								CHECK_UNARY(ctrl->has_focus());
							}
						}
					}

					SUBCASE("[SceneTree][Control] Inside top level tree") {
						ctrl_0->grab_focus();
						REQUIRE_UNARY(ctrl_0->has_focus());
						SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
							SEND_GUI_ACTION("ui_focus_next");
							CHECK_UNARY(ctrl_0_0->has_focus());

							SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
								SEND_GUI_ACTION("ui_focus_prev");
								CHECK_UNARY(ctrl_0->has_focus());
							}
						}
						SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
							SEND_GUI_ACTION("ui_focus_prev");
							CHECK_UNARY(ctrl_0_2->has_focus());

							SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
								SEND_GUI_ACTION("ui_focus_next");
								CHECK_UNARY(ctrl_0->has_focus());
							}
						}
					}

					SUBCASE("[SceneTree][Control] Manually specified focus next") {
						ctrl->set_focus_next(ctrl->get_path_to(ctrl_2_1));
						ctrl_2_1->set_focus_next(ctrl_2_1->get_path_to(ctrl_1_0));
						ctrl_1_0->set_focus_next(ctrl_1_0->get_path_to(ctrl_0));
						ctrl_0->set_focus_next(ctrl_0->get_path_to(ctrl));

						SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
							SEND_GUI_ACTION("ui_focus_next");
							CHECK_UNARY(ctrl_2_1->has_focus());

							SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
								SEND_GUI_ACTION("ui_focus_next");
								CHECK_UNARY(ctrl_1_0->has_focus());

								SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
									SEND_GUI_ACTION("ui_focus_next");
									CHECK_UNARY(ctrl_0->has_focus());

									SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
										SEND_GUI_ACTION("ui_focus_next");
										CHECK_UNARY(ctrl->has_focus());
									}

									SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
										SEND_GUI_ACTION("ui_focus_prev");
										CHECK_UNARY(ctrl_0_2->has_focus());
									}
								}

								SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
									SEND_GUI_ACTION("ui_focus_prev");
									CHECK_UNARY(ctrl_1->has_focus());
								}
							}

							SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
								SEND_GUI_ACTION("ui_focus_prev");
								CHECK_UNARY(ctrl_2_1->has_focus());
							}
						}

						SUBCASE("[SceneTree][Control] The parent node is not visible") {
							node_2d_2->hide();
							REQUIRE_UNARY(ctrl_2_1->is_visible());
							REQUIRE_UNARY_FALSE(ctrl_2_1->is_visible_in_tree());
							SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
								SEND_GUI_ACTION("ui_focus_next");
								CHECK_UNARY_FALSE(ctrl->has_focus());
								CHECK_UNARY_FALSE(ctrl_2_1->has_focus());
								CHECK_UNARY_FALSE(ctrl_0->has_focus());
								CHECK_UNARY(ctrl_1->has_focus());
							}
						}
					}

					SUBCASE("[SceneTree][Control] Manually specified focus prev") {
						ctrl->set_focus_previous(ctrl->get_path_to(ctrl_0_2));
						ctrl_0_2->set_focus_previous(ctrl_0_2->get_path_to(ctrl_1_1));
						ctrl_1_1->set_focus_previous(ctrl_1_1->get_path_to(ctrl_2_0));
						ctrl_2_0->set_focus_previous(ctrl_2_0->get_path_to(ctrl));

						SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
							SEND_GUI_ACTION("ui_focus_prev");
							CHECK_UNARY(ctrl_0_2->has_focus());

							SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
								SEND_GUI_ACTION("ui_focus_prev");
								CHECK_UNARY(ctrl_1_1->has_focus());

								SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
									SEND_GUI_ACTION("ui_focus_prev");
									CHECK_UNARY(ctrl_2_0->has_focus());

									SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
										SEND_GUI_ACTION("ui_focus_prev");
										CHECK_UNARY(ctrl->has_focus());
									}

									SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
										SEND_GUI_ACTION("ui_focus_next");
										CHECK_UNARY(ctrl_2_0->has_focus());
									}
								}

								SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
									SEND_GUI_ACTION("ui_focus_next");
									CHECK_UNARY(ctrl_1_2->has_focus());
								}
							}

							SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
								SEND_GUI_ACTION("ui_focus_next");
								CHECK_UNARY(ctrl_0->has_focus());
							}
						}

						SUBCASE("[SceneTree][Control] The parent node is not visible") {
							ctrl_0->hide();
							REQUIRE_UNARY(ctrl_0_2->is_visible());
							REQUIRE_UNARY_FALSE(ctrl_0_2->is_visible_in_tree());
							SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
								SEND_GUI_ACTION("ui_focus_prev");
								CHECK_UNARY_FALSE(ctrl->has_focus());
								CHECK_UNARY_FALSE(ctrl_0_2->has_focus());
								CHECK_UNARY(ctrl_1_2->has_focus());
							}
						}
					}
				}

				SUBCASE("[SceneTree][Control] Exist hidden control tree") {
					ctrl_0->hide();
					REQUIRE_UNARY_FALSE(ctrl_0->is_visible());

					SUBCASE("[SceneTree][Control] Simulate ui_focus_next action") {
						SEND_GUI_ACTION("ui_focus_next");
						CHECK_UNARY(ctrl_1->has_focus());

						SUBCASE("[SceneTree][Control] Simulate ui_focus_prev action") {
							SEND_GUI_ACTION("ui_focus_prev");
							CHECK_UNARY(ctrl->has_focus());
						}
					}
				}

				memdelete(ctrl_2_2);
				memdelete(ctrl_2_1);
				memdelete(ctrl_2_0);
				memdelete(ctrl_1_2);
				memdelete(ctrl_1_1);
				memdelete(ctrl_1_0);
				memdelete(ctrl_0_2);
				memdelete(ctrl_0_1);
				memdelete(ctrl_0_0);
			}

			memdelete(node_2d_2);
			memdelete(ctrl_1);
			memdelete(ctrl_0);
		}
	}

	memdelete(ctrl);
	memdelete(intermediate);
}

TEST_CASE("[SceneTree][Control] Anchoring") {
	Control *test_control = memnew(Control);
	Control *test_child = memnew(Control);
	test_control->add_child(test_child);
	test_control->set_size(Size2(2, 2));
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(test_control);

	SUBCASE("Anchoring without offsets") {
		test_child->set_anchor(SIDE_RIGHT, 0.75);
		test_child->set_anchor(SIDE_BOTTOM, 0.1);
		CHECK_MESSAGE(
				test_child->get_size().is_equal_approx(Vector2(1.5, 0.2)),
				"With no LEFT or TOP anchors, positive RIGHT and BOTTOM anchors should be proportional to the size.");
		CHECK_MESSAGE(
				test_child->get_position().is_equal_approx(Vector2(0, 0)),
				"With positive RIGHT and BOTTOM anchors set and no LEFT or TOP anchors, the position should not change.");

		test_child->set_anchor(SIDE_LEFT, 0.5);
		test_child->set_anchor(SIDE_TOP, 0.01);
		CHECK_MESSAGE(
				test_child->get_size().is_equal_approx(Vector2(0.5, 0.18)),
				"With all anchors set, the size should fit between all four anchors.");
		CHECK_MESSAGE(
				test_child->get_position().is_equal_approx(Vector2(1, 0.02)),
				"With all anchors set, the LEFT and TOP anchors should proportional to the position.");
	}

	SUBCASE("Anchoring with offsets") {
		test_child->set_offset(SIDE_RIGHT, 0.33);
		test_child->set_offset(SIDE_BOTTOM, 0.2);
		CHECK_MESSAGE(
				test_child->get_size().is_equal_approx(Vector2(0.33, 0.2)),
				"With no anchors or LEFT or TOP offsets set, the RIGHT and BOTTOM offsets should be equal to size.");
		CHECK_MESSAGE(
				test_child->get_position().is_equal_approx(Vector2(0, 0)),
				"With only positive RIGHT and BOTTOM offsets set, the position should not change.");

		test_child->set_offset(SIDE_LEFT, 0.1);
		test_child->set_offset(SIDE_TOP, 0.05);
		CHECK_MESSAGE(
				test_child->get_size().is_equal_approx(Vector2(0.23, 0.15)),
				"With no anchors set, the size should fit between all four offsets.");
		CHECK_MESSAGE(
				test_child->get_position().is_equal_approx(Vector2(0.1, 0.05)),
				"With no anchors set, the LEFT and TOP offsets should be equal to the position.");

		test_child->set_anchor(SIDE_RIGHT, 0.5);
		test_child->set_anchor(SIDE_BOTTOM, 0.3);
		test_child->set_anchor(SIDE_LEFT, 0.2);
		test_child->set_anchor(SIDE_TOP, 0.1);
		CHECK_MESSAGE(
				test_child->get_size().is_equal_approx(Vector2(0.83, 0.55)),
				"Anchors adjust size first then it is affected by offsets.");
		CHECK_MESSAGE(
				test_child->get_position().is_equal_approx(Vector2(0.5, 0.25)),
				"Anchors adjust positions first then it is affected by offsets.");

		test_child->set_offset(SIDE_RIGHT, -0.1);
		test_child->set_offset(SIDE_BOTTOM, -0.01);
		test_child->set_offset(SIDE_LEFT, -0.33);
		test_child->set_offset(SIDE_TOP, -0.16);
		CHECK_MESSAGE(
				test_child->get_size().is_equal_approx(Vector2(0.83, 0.55)),
				"Keeping offset distance equal when changing offsets, keeps size equal.");
		CHECK_MESSAGE(
				test_child->get_position().is_equal_approx(Vector2(0.07, 0.04)),
				"Negative offsets move position in top left direction.");
	}

	SUBCASE("Anchoring is preserved on parent size changed") {
		test_child->set_offset(SIDE_RIGHT, -0.05);
		test_child->set_offset(SIDE_BOTTOM, 0.1);
		test_child->set_offset(SIDE_LEFT, 0.05);
		test_child->set_offset(SIDE_TOP, 0.1);
		test_child->set_anchor(SIDE_RIGHT, 0.3);
		test_child->set_anchor(SIDE_BOTTOM, 0.85);
		test_child->set_anchor(SIDE_LEFT, 0.2);
		test_child->set_anchor(SIDE_TOP, 0.55);
		CHECK(test_child->get_rect().is_equal_approx(
				Rect2(Vector2(0.45, 1.2), Size2(0.1, 0.6))));

		test_control->set_size(Size2(4, 1));
		CHECK(test_child->get_rect().is_equal_approx(
				Rect2(Vector2(0.85, 0.65), Size2(0.3, 0.3))));
	}

	memdelete(test_child);
	memdelete(test_control);
}

TEST_CASE("[SceneTree][Control] Custom maximum size") {
	Control *test_control = memnew(Control);
	test_control->set_custom_maximum_size(Size2(4, 2));
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(test_control);
	test_control->set_size(Size2(4, 2));
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(4, 2)),
			"Size is allowed to increase to match custom maximum size.");

	test_control->set_size(Size2(3, 1));
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(3, 1)),
			"Size does not change if below custom maximum size.");

	test_control->set_size(Size2(5, 4));
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(4, 2)),
			"Size is limited to custom maximum size.");

	test_control->set_size(Size2(5, 1));
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(4, 1)),
			"Adjust only x axis if x is above custom maximum size.");

	test_control->set_size(Size2(3, 3));
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(3, 2)),
			"Adjust only y axis if y is above custom maximum size.");

	test_control->set_custom_minimum_size(Size2(5, 3));
	SceneTree::get_singleton()->process(0);
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(4, 2)),
			"Custom maximum size overrides custom minimum size.");

	Control *test_child = memnew(Control);
	test_control->add_child(test_child);

	CHECK_MESSAGE(
			test_child->get_combined_maximum_size().is_equal_approx(Vector2(-1, -1)),
			"Child combined maximum size does not factor in parent's custom maximum size if not propagating.");

	test_child->set_size(Size2(5, 3));
	CHECK_MESSAGE(
			test_child->get_size().is_equal_approx(Vector2(5, 3)),
			"Child size is not constrained by parent's custom maximum size if not propagating.");

	test_control->set_propagate_maximum_size(true);

	CHECK_MESSAGE(
			test_child->get_combined_maximum_size().is_equal_approx(Vector2(4, 2)),
			"Child combined maximum size factors in parent's custom maximum size if propagating.");

	test_child->set_size(Size2(5, 3));
	CHECK_MESSAGE(
			test_child->get_size().is_equal_approx(Vector2(4, 2)),
			"Child size is constrained by parent's custom maximum size if propagating.");

	test_child->set_size(Size2(3, 1));
	CHECK_MESSAGE(
			test_child->get_size().is_equal_approx(Vector2(3, 1)),
			"Child size can be smaller than parent's custom maximum size.");

	memdelete(test_child);
	memdelete(test_control);
}

TEST_CASE("[SceneTree][Control] Custom minimum size") {
	Control *test_control = memnew(Control);
	test_control->set_custom_minimum_size(Size2(4, 2));
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(test_control);
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(4, 2)),
			"Size increases to match custom minimum size.");

	test_control->set_size(Size2(5, 4));
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(5, 4)),
			"Size does not change if above custom minimum size.");

	test_control->set_size(Size2(1, 1));
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(4, 2)),
			"Size matches minimum size if set below custom minimum size.");

	test_control->set_size(Size2(3, 3));
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(4, 3)),
			"Adjusts only x axis size if x is below custom minimum size.");

	test_control->set_size(Size2(10, 0.1));
	CHECK_MESSAGE(
			test_control->get_size().is_equal_approx(Vector2(10, 2)),
			"Adjusts only y axis size if y is below custom minimum size.");

	memdelete(test_control);
}

TEST_CASE("[SceneTree][Control] Grow direction") {
	Control *test_control = memnew(Control);
	test_control->set_size(Size2(1, 1));
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(test_control);

	SUBCASE("Defaults") {
		CHECK(test_control->get_h_grow_direction() == Control::GROW_DIRECTION_END);
		CHECK(test_control->get_v_grow_direction() == Control::GROW_DIRECTION_END);
	}

	SIGNAL_WATCH(test_control, SNAME("minimum_size_changed"))
	Array signal_args = { {} };

	SUBCASE("Horizontal grow direction begin") {
		test_control->set_h_grow_direction(Control::GROW_DIRECTION_BEGIN);
		test_control->set_custom_minimum_size(Size2(2, 2));
		SceneTree::get_singleton()->process(0);
		SIGNAL_CHECK("minimum_size_changed", signal_args)
		CHECK_MESSAGE(
				test_control->get_rect().is_equal_approx(
						Rect2(Vector2(-1, 0), Size2(2, 2))),
				"Expand leftwards.");
	}

	SUBCASE("Vertical grow direction begin") {
		test_control->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
		test_control->set_custom_minimum_size(Size2(4, 3));
		SceneTree::get_singleton()->process(0);
		SIGNAL_CHECK("minimum_size_changed", signal_args);
		CHECK_MESSAGE(
				test_control->get_rect().is_equal_approx(
						Rect2(Vector2(0, -2), Size2(4, 3))),
				"Expand upwards.");
	}

	SUBCASE("Horizontal grow direction end") {
		test_control->set_h_grow_direction(Control::GROW_DIRECTION_END);
		test_control->set_custom_minimum_size(Size2(5, 3));
		SceneTree::get_singleton()->process(0);
		SIGNAL_CHECK("minimum_size_changed", signal_args);
		CHECK_MESSAGE(
				test_control->get_rect().is_equal_approx(
						Rect2(Vector2(0, 0), Size2(5, 3))),
				"Expand rightwards.");
	}

	SUBCASE("Vertical grow direction end") {
		test_control->set_v_grow_direction(Control::GROW_DIRECTION_END);
		test_control->set_custom_minimum_size(Size2(4, 4));
		SceneTree::get_singleton()->process(0);
		SIGNAL_CHECK("minimum_size_changed", signal_args);
		CHECK_MESSAGE(
				test_control->get_rect().is_equal_approx(
						Rect2(Vector2(0, 0), Size2(4, 4))),
				"Expand downwards.");
		;
	}

	SUBCASE("Horizontal grow direction both") {
		test_control->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
		test_control->set_custom_minimum_size(Size2(2, 4));
		SceneTree::get_singleton()->process(0);
		SIGNAL_CHECK("minimum_size_changed", signal_args);
		CHECK_MESSAGE(
				test_control->get_rect().is_equal_approx(
						Rect2(Vector2(-0.5, 0), Size2(2, 4))),
				"Expand equally leftwards and rightwards.");
	}

	SUBCASE("Vertical grow direction both") {
		test_control->set_v_grow_direction(Control::GROW_DIRECTION_BOTH);
		test_control->set_custom_minimum_size(Size2(6, 3));
		SceneTree::get_singleton()->process(0);
		SIGNAL_CHECK("minimum_size_changed", signal_args);
		CHECK_MESSAGE(
				test_control->get_rect().is_equal_approx(
						Rect2(Vector2(0, -1), Size2(6, 3))),
				"Expand equally upwards and downwards.");
	}

	memdelete(test_control);
}

} // namespace TestControl
