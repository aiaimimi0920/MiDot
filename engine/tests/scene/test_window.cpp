/**************************************************************************/
/*  test_window.cpp                                                       */
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

TEST_FORCE_LINK(test_window)

#include "core/input/input_map.h" // IWYU pragma: keep // Used by `SEND_GUI_MOUSE_MOTION_EVENT` macro.
#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/message_queue.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/theme.h"
#include "tests/display_server_mock.h"
#include "tests/test_utils.h"

#include "modules/color_scheme/color_scheme.h"

namespace TestWindow {

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

static void check_window_dynamic_theme_override_file_roundtrip(Window *p_window, const String &p_file_prefix, const Color &p_color, ColorRole p_role, const Ref<ColorScheme> &p_color_scheme) {
	const char *extensions[] = { "tscn", "scn" };
	for (const char *extension : extensions) {
		const String path = TestUtils::get_temp_path(p_file_prefix + "." + String(extension));
		Ref<PackedScene> packed_scene = memnew(PackedScene);
		CHECK(packed_scene->pack(p_window) == OK);
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
					Window *roundtrip = Object::cast_to<Window>(instance);
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

static void check_dialog_dynamic_theme_override_file_roundtrip(AcceptDialog *p_dialog, const Color &p_color, ColorRole p_role, const Ref<ColorScheme> &p_color_scheme) {
	check_window_dynamic_theme_override_file_roundtrip(p_dialog, "dynamic_theme_dialog", p_color, p_role, p_color_scheme);
}

class NotificationControlWindow : public Control {
	GDCLASS(NotificationControlWindow, Control);

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_MOUSE_ENTER: {
				mouse_over = true;
			} break;

			case NOTIFICATION_MOUSE_EXIT: {
				mouse_over = false;
			} break;
		}
	}

public:
	bool mouse_over = false;
};

TEST_CASE("[SceneTree][Window] Dynamic theme colors") {
	Window *window = memnew(Window);
	SceneTree::get_singleton()->get_root()->add_child(window);

	Ref<ColorScheme> color_scheme = memnew(ColorScheme(Color("#006495")));
	Ref<Theme> theme = memnew(Theme);
	const Color scale(0.7, 0.5, 0.3, 0.9);
	theme->set_color("accent", "Window", Color("#ff0000"));
	theme->set_color("accent_scale", "Window", scale);
	theme->set_color_role("accent_role", "Window", ColorRole::TERTIARY);
	theme->set_color_scheme("accent_scheme", "Window", color_scheme);
	window->set_theme(theme);

	CHECK(window->get_theme_color("accent").is_equal_approx(color_scheme->get_color(ColorRole::TERTIARY) * scale));

	window->add_theme_color_override("accent", Color("#00ff00"));
	CHECK(window->get_theme_color("accent").is_equal_approx(Color("#00ff00")));

	window->add_theme_color_role_override("accent_role", ColorRole::ERROR);
	CHECK(window->get_theme_color("accent").is_equal_approx(color_scheme->get_color(ColorRole::ERROR) * scale));

	color_scheme->set_dark(true);
	MessageQueue::get_singleton()->flush();
	CHECK(window->get_theme_color("accent").is_equal_approx(color_scheme->get_color(ColorRole::ERROR) * scale));

	memdelete(window);
}

TEST_CASE("[SceneTree][Window] Dynamic theme styleboxes isolate effective schemes") {
	Window *first_window = memnew(Window);
	Window *second_window = memnew(Window);
	SceneTree::get_singleton()->get_root()->add_child(first_window);
	SceneTree::get_singleton()->get_root()->add_child(second_window);

	Ref<ColorScheme> first_scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<ColorScheme> second_scheme = memnew(ColorScheme(Color("#006495")));
	Ref<StyleBoxFlat> shared_stylebox = memnew(StyleBoxFlat);
	shared_stylebox->set_bg_color(Color("#123456"));
	shared_stylebox->set_bg_color_role(ColorRole::PRIMARY);

	Ref<Theme> theme = memnew(Theme);
	theme->set_stylebox("panel", "Window", shared_stylebox);
	theme->set_color_scheme("panel_scheme", "Window", first_scheme);
	first_window->set_theme(theme);
	second_window->set_theme(theme);
	second_window->add_theme_color_scheme_override("panel_scheme", second_scheme);

	Ref<StyleBoxFlat> first_stylebox = first_window->get_theme_stylebox("panel");
	Ref<StyleBoxFlat> second_stylebox = second_window->get_theme_stylebox("panel");
	CHECK(first_stylebox.is_valid());
	CHECK(second_stylebox.is_valid());
	CHECK(first_stylebox != shared_stylebox);
	CHECK(second_stylebox != shared_stylebox);
	CHECK(first_stylebox != second_stylebox);
	CHECK(first_stylebox->get_resolved_bg_color().is_equal_approx(first_scheme->get_color(ColorRole::PRIMARY)));
	CHECK(second_stylebox->get_resolved_bg_color().is_equal_approx(second_scheme->get_color(ColorRole::PRIMARY)));
	CHECK(shared_stylebox->get_default_color_scheme().is_null());
	CHECK(shared_stylebox->get_bg_color().is_equal_approx(Color("#123456")));

	second_window->add_theme_color_scheme_override("panel_scheme", first_scheme);
	Ref<StyleBoxFlat> replacement_stylebox = second_window->get_theme_stylebox("panel");
	CHECK(replacement_stylebox != second_stylebox);
	CHECK(replacement_stylebox->get_default_color_scheme() == first_scheme);
	CHECK(first_window->get_theme_stylebox("panel") == first_stylebox);

	memdelete(second_window);
	memdelete(first_window);
}

TEST_CASE("[SceneTree][Window] Dynamic theme color presence and STATIC role") {
	Window *window = memnew(Window);
	SceneTree::get_singleton()->get_root()->add_child(window);

	Ref<ColorScheme> color_scheme = memnew(ColorScheme(Color("#006495")));
	Ref<Theme> theme = memnew(Theme);
	theme->set_default_color_scheme(color_scheme);
	theme->set_type_variation("TestVariation", "Window");
	const Color static_color("#ff0000");
	theme->set_color("accent", "Window", static_color);
	theme->set_color_role("accent_role", "Window", ColorRole::PRIMARY);
	theme->set_color_scheme("accent_scheme", "Window", color_scheme);
	theme->set_color_role("accent_scale_role", "Window", ColorRole::SECONDARY);
	theme->set_color_scheme("accent_scale_scheme", "Window", color_scheme);
	theme->set_color_role("role_only_role", "Window", ColorRole::ERROR);
	theme->set_color_scheme("role_only_scheme", "Window", color_scheme);
	theme->set_color_role("masked_role_only_role", "Window", ColorRole::ERROR);
	theme->set_color_scheme("masked_role_only_scheme", "Window", color_scheme);
	theme->set_color_role("default_scheme_role", "Window", ColorRole::ERROR);
	theme->set_color("static_only", "Window", static_color);
	theme->set_color_role("variation_role", "TestVariation", ColorRole::TERTIARY);
	theme->set_color_scheme("variation_scheme", "TestVariation", color_scheme);
	theme->set_color("variation", "TestVariation", static_color);
	theme->set_color_role("explicit_role", "Other", ColorRole::ERROR);
	theme->set_color_scheme("explicit_scheme", "Other", color_scheme);
	window->set_theme(theme);

	CHECK_FALSE(window->has_theme_color("missing"));
	CHECK(window->has_theme_color("static_only"));
	CHECK(window->get_theme_color("static_only").is_equal_approx(static_color));
	CHECK(window->has_theme_color("accent"));
	CHECK(window->get_theme_color("accent").is_equal_approx(color_scheme->get_color(ColorRole::PRIMARY) * color_scheme->get_color(ColorRole::SECONDARY)));
	CHECK(window->has_theme_color("accent_scale"));
	CHECK(window->get_theme_color("accent_scale").is_equal_approx(color_scheme->get_color(ColorRole::SECONDARY)));

	CHECK(window->has_theme_color("role_only"));
	CHECK(window->get_theme_color("role_only").is_equal_approx(color_scheme->get_color(ColorRole::ERROR)));
	CHECK(window->has_theme_color("default_scheme"));
	CHECK(window->get_theme_color("default_scheme").is_equal_approx(color_scheme->get_color(ColorRole::ERROR)));
	window->add_theme_color_role_override("explicit_role", ColorRole::STATIC);
	CHECK(window->has_theme_color("explicit", "Other"));
	CHECK(window->get_theme_color("explicit", "Other").is_equal_approx(color_scheme->get_color(ColorRole::ERROR)));

	window->set_theme_type_variation("TestVariation");
	CHECK(window->has_theme_color("variation"));
	window->add_theme_color_role_override("variation_role", ColorRole::STATIC);
	CHECK(window->has_theme_color("variation"));
	CHECK(window->get_theme_color("variation").is_equal_approx(static_color));
	window->remove_theme_color_role_override("variation_role");
	CHECK(window->get_theme_color("variation").is_equal_approx(color_scheme->get_color(ColorRole::TERTIARY)));

	window->add_theme_color_role_override("accent_role", ColorRole::STATIC);
	CHECK(window->get_theme_color("accent").is_equal_approx(static_color));
	window->add_theme_color_override("accent", Color("#00ff00"));
	CHECK(window->get_theme_color("accent").is_equal_approx(Color("#00ff00")));

	window->add_theme_color_role_override("masked_role_only_role", ColorRole::STATIC);
	CHECK_FALSE(window->has_theme_color("masked_role_only"));
	CHECK(window->get_theme_color("masked_role_only").is_equal_approx(Color()));

	memdelete(window);
}

TEST_CASE("[SceneTree][Window] Dynamic theme override properties serialize") {
	AcceptDialog *dialog = memnew(AcceptDialog);
	Ref<ColorScheme> color_scheme = memnew(ColorScheme(Color("#006495")));
	Ref<StyleBoxFlat> style = memnew(StyleBoxFlat);
	style->set_bg_color(Color("#123456"));
	dialog->add_theme_color_override("custom_scale", Color(0.7, 0.5, 0.3, 0.9));
	dialog->add_theme_color_role_override("custom_role", ColorRole::TERTIARY);
	dialog->add_theme_color_scheme_override("custom_scheme", color_scheme);
	dialog->add_theme_color_scheme_override("default_color_scheme", color_scheme);
	dialog->add_theme_style_override("custom_style", style);
	dialog->add_theme_color_scheme_override("custom_style_scheme", color_scheme);

	uint32_t property_usage = 0;
	CHECK(get_property_count(dialog, "theme_override_colors/title_color") == 1);
	CHECK(get_property_count(dialog, "theme_override_color_roles/title_color_role") == 1);
	CHECK(get_property_count(dialog, "theme_override_color_schemes/title_color_scheme") == 1);
	CHECK(get_property_count(dialog, "theme_override_styles/embedded_border") == 1);
	CHECK(get_property_count(dialog, "theme_override_color_schemes/embedded_border_scheme") == 1);
	CHECK(get_property_count(dialog, "theme_override_styles/panel") == 1);
	CHECK(get_property_count(dialog, "theme_override_color_schemes/default_color_scheme", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(dialog, "theme_override_color_roles/custom_role", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(dialog, "theme_override_color_schemes/custom_scheme", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(dialog, "theme_override_styles/custom_style", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));

	Ref<PackedScene> packed_scene = memnew(PackedScene);
	CHECK(packed_scene->pack(dialog) == OK);
	Node *instance = packed_scene->instantiate();
	CHECK(instance != nullptr);
	if (instance) {
		SceneTree::get_singleton()->get_root()->add_child(instance);
		AcceptDialog *roundtrip = Object::cast_to<AcceptDialog>(instance);
		CHECK(roundtrip != nullptr);
		if (roundtrip) {
			CHECK(roundtrip->get_theme_color("custom_scale").is_equal_approx(Color(0.7, 0.5, 0.3, 0.9)));
			CHECK(roundtrip->get_theme_color_role("custom_role") == ColorRole::TERTIARY);
			CHECK(roundtrip->get_theme_color_scheme("custom_scheme") == color_scheme);
			CHECK(roundtrip->get_theme_color_scheme("default_color_scheme") == color_scheme);
			CHECK(roundtrip->get_theme_color_scheme("custom_style_scheme") == color_scheme);
		}
		memdelete(instance);
	}
	Node *duplicate = dialog->duplicate();
	CHECK(duplicate != nullptr);
	if (duplicate) {
		AcceptDialog *duplicate_dialog = Object::cast_to<AcceptDialog>(duplicate);
		CHECK(duplicate_dialog != nullptr);
		if (duplicate_dialog) {
			CHECK(duplicate_dialog->get_theme_color("custom_scale").is_equal_approx(Color(0.7, 0.5, 0.3, 0.9)));
			CHECK(duplicate_dialog->get_theme_color_role("custom_role") == ColorRole::TERTIARY);
			CHECK(duplicate_dialog->get_theme_color_scheme("custom_scheme") == color_scheme);
			CHECK(duplicate_dialog->get_theme_color_scheme("default_color_scheme") == color_scheme);
			CHECK(duplicate_dialog->get_theme_color_scheme("custom_style_scheme") == color_scheme);
		}
		memdelete(duplicate);
	}
	check_dialog_dynamic_theme_override_file_roundtrip(dialog, Color(0.7, 0.5, 0.3, 0.9), ColorRole::TERTIARY, color_scheme);
	memdelete(dialog);
}

TEST_CASE("[SceneTree][Window] Dynamic theme override properties include Window base items") {
	Window *window = memnew(Window);
	Ref<ColorScheme> color_scheme = memnew(ColorScheme(Color("#006495")));
	Ref<StyleBoxFlat> style = memnew(StyleBoxFlat);
	style->set_bg_color(Color("#123456"));
	window->add_theme_color_role_override("custom_role", ColorRole::TERTIARY);
	window->add_theme_color_scheme_override("custom_scheme", color_scheme);
	window->add_theme_color_override("custom_scale", Color(0.7, 0.5, 0.3, 0.9));
	window->add_theme_color_scheme_override("default_color_scheme", color_scheme);
	window->add_theme_style_override("custom_style", style);
	window->add_theme_color_scheme_override("custom_style_scheme", color_scheme);

	uint32_t property_usage = 0;
	CHECK(get_property_count(window, "theme_override_colors/title_color") == 1);
	CHECK(get_property_count(window, "theme_override_color_roles/title_color_role") == 1);
	CHECK(get_property_count(window, "theme_override_color_schemes/title_color_scheme") == 1);
	CHECK(get_property_count(window, "theme_override_colors/title_color_scale") == 1);
	CHECK(get_property_count(window, "theme_override_styles/embedded_border") == 1);
	CHECK(get_property_count(window, "theme_override_color_schemes/embedded_border_scheme") == 1);
	CHECK(get_property_count(window, "theme_override_color_schemes/default_color_scheme", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(window, "theme_override_color_roles/custom_role", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(window, "theme_override_color_schemes/custom_scheme", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(window, "theme_override_colors/custom_scale", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(window, "theme_override_styles/custom_style", &property_usage) == 1);
	CHECK((property_usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED)) == (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED));
	CHECK(get_property_count(window, "theme_override_color_schemes/custom_style_scheme") == 1);

	Ref<PackedScene> packed_scene = memnew(PackedScene);
	CHECK(packed_scene->pack(window) == OK);
	Node *instance = packed_scene->instantiate();
	CHECK(instance != nullptr);
	if (instance != nullptr) {
		SceneTree::get_singleton()->get_root()->add_child(instance);
		Window *roundtrip = Object::cast_to<Window>(instance);
		CHECK(roundtrip != nullptr);
		if (roundtrip != nullptr) {
			CHECK(roundtrip->get_theme_color("custom_scale").is_equal_approx(Color(0.7, 0.5, 0.3, 0.9)));
			CHECK(roundtrip->get_theme_color_role("custom_role") == ColorRole::TERTIARY);
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
	Node *duplicate = window->duplicate();
	CHECK(duplicate != nullptr);
	if (duplicate != nullptr) {
		Window *duplicate_window = Object::cast_to<Window>(duplicate);
		CHECK(duplicate_window != nullptr);
		if (duplicate_window != nullptr) {
			CHECK(duplicate_window->get_theme_color("custom_scale").is_equal_approx(Color(0.7, 0.5, 0.3, 0.9)));
			CHECK(duplicate_window->get_theme_color_role("custom_role") == ColorRole::TERTIARY);
			CHECK(duplicate_window->get_theme_color_scheme("custom_scheme") == color_scheme);
			CHECK(duplicate_window->get_theme_color_scheme("default_color_scheme") == color_scheme);
			CHECK(duplicate_window->get_theme_color_scheme("custom_style_scheme") == color_scheme);
			Ref<StyleBox> duplicate_style = duplicate_window->get_theme_stylebox("custom_style");
			StyleBoxFlat *duplicate_flat = Object::cast_to<StyleBoxFlat>(duplicate_style.ptr());
			CHECK(duplicate_flat != nullptr);
			if (duplicate_flat != nullptr) {
				CHECK(duplicate_flat->get_bg_color().is_equal_approx(Color("#123456")));
			}
		}
		memdelete(duplicate);
	}
	check_window_dynamic_theme_override_file_roundtrip(window, "dynamic_theme_window", Color(0.7, 0.5, 0.3, 0.9), ColorRole::TERTIARY, color_scheme);

	memdelete(window);
}

TEST_CASE("[SceneTree][Window] String theme overrides") {
	Window *window = memnew(Window);
	SceneTree::get_singleton()->get_root()->add_child(window);

	Ref<Theme> theme = memnew(Theme);
	theme->set_string("title", "Window", "theme value");
	window->set_theme(theme);

	CHECK(window->get_theme_string("title") == "theme value");
	window->add_theme_string_override("title", "override value");
	CHECK(window->has_theme_string_override("title"));
	CHECK(window->get_theme_string("title") == "override value");

	window->set("theme_override_strings/title", "property value");
	CHECK(window->get("theme_override_strings/title") == "property value");
	CHECK(window->get_theme_string("title") == "property value");

	window->remove_theme_string_override("title");
	CHECK_FALSE(window->has_theme_string_override("title"));
	CHECK(window->get_theme_string("title") == "theme value");

	memdelete(window);
}

TEST_CASE("[SceneTree][Window]") {
	Window *root = SceneTree::get_singleton()->get_root();

	SUBCASE("Control-mouse-over within Window-black bars should not happen") {
		Window *w = memnew(Window);
		root->add_child(w);
		w->set_size(Size2i(400, 200));
		w->set_position(Size2i(0, 0));
		w->set_content_scale_size(Size2i(200, 200));
		w->set_content_scale_mode(Window::CONTENT_SCALE_MODE_CANVAS_ITEMS);
		w->set_content_scale_aspect(Window::CONTENT_SCALE_ASPECT_KEEP);
		NotificationControlWindow *c = memnew(NotificationControlWindow);
		w->add_child(c);
		c->set_size(Size2i(100, 100));
		c->set_position(Size2i(-50, -50));

		CHECK_FALSE(c->mouse_over);
		SEND_GUI_MOUSE_MOTION_EVENT(Point2i(110, 10), MouseButtonMask::NONE, Key::NONE);
		CHECK(c->mouse_over);
		SEND_GUI_MOUSE_MOTION_EVENT(Point2i(90, 10), MouseButtonMask::NONE, Key::NONE);
		CHECK_FALSE(c->mouse_over); // GH-80011

		/* TODO:
		SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(90, 10), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
		SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(Point2i(90, 10), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		CHECK(Control was not pressed);
		*/

		memdelete(c);
		memdelete(w);
	}
}

} // namespace TestWindow
