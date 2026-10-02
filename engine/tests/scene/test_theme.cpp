/**************************************************************************/
/*  test_theme.cpp                                                        */
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

TEST_FORCE_LINK(test_theme)

#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/style_box_line.h"
#include "scene/resources/style_box_texture.h"
#include "scene/resources/theme.h"
#include "scene/theme/theme_db.h"
#include "tests/signal_watcher.h"
#include "tests/test_tools.h"

#include "modules/color_scheme/color_scheme.h"

namespace TestTheme {

class Fixture {
public:
	struct DataEntry {
		Theme::DataType type;
		Variant value;
	} const valid_data[Theme::DATA_TYPE_MAX] = {
		{ Theme::DATA_TYPE_COLOR, Color() },
		{ Theme::DATA_TYPE_CONSTANT, 42 },
		{ Theme::DATA_TYPE_FONT, Ref<FontFile>(memnew(FontFile)) },
		{ Theme::DATA_TYPE_FONT_SIZE, 42 },
		{ Theme::DATA_TYPE_ICON, Ref<Texture>(memnew(ImageTexture)) },
		{ Theme::DATA_TYPE_STYLEBOX, Ref<StyleBox>(memnew(StyleBoxFlat)) },
		{ Theme::DATA_TYPE_COLOR_ROLE, static_cast<int64_t>(ColorRole::PRIMARY) },
		{ Theme::DATA_TYPE_COLOR_SCHEME, Ref<ColorScheme>(memnew(ColorScheme)) },
		{ Theme::DATA_TYPE_STRING, String("theme string") },
	};

	const StringName valid_item_name = "valid_item_name";
	const StringName valid_type_name = "ValidTypeName";
};

TEST_CASE("[Theme] Color role and color scheme theme items") {
	Ref<Theme> theme = memnew(Theme);
	Ref<ColorScheme> color_scheme = memnew(ColorScheme);

	theme->set_theme_item(Theme::DATA_TYPE_COLOR_ROLE, "font_color_role", "Button", static_cast<int64_t>(ColorRole::ON_PRIMARY));
	theme->set_theme_item(Theme::DATA_TYPE_COLOR_SCHEME, "font_color_scheme", "Button", color_scheme);

	CHECK(theme->has_theme_item(Theme::DATA_TYPE_COLOR_ROLE, "font_color_role", "Button"));
	CHECK(theme->has_theme_item(Theme::DATA_TYPE_COLOR_SCHEME, "font_color_scheme", "Button"));
	CHECK(static_cast<ColorRole>((int64_t)theme->get_theme_item(Theme::DATA_TYPE_COLOR_ROLE, "font_color_role", "Button")) == ColorRole::ON_PRIMARY);
	CHECK(Ref<ColorScheme>(theme->get_theme_item(Theme::DATA_TYPE_COLOR_SCHEME, "font_color_scheme", "Button")) == color_scheme);

	List<StringName> color_role_items;
	List<StringName> color_scheme_items;
	theme->get_theme_item_list(Theme::DATA_TYPE_COLOR_ROLE, "Button", &color_role_items);
	theme->get_theme_item_list(Theme::DATA_TYPE_COLOR_SCHEME, "Button", &color_scheme_items);
	CHECK(color_role_items.find("font_color_role"));
	CHECK(color_scheme_items.find("font_color_scheme"));

	theme->clear_theme_item(Theme::DATA_TYPE_COLOR_ROLE, "font_color_role", "Button");
	theme->clear_theme_item(Theme::DATA_TYPE_COLOR_SCHEME, "font_color_scheme", "Button");
	CHECK_FALSE(theme->has_theme_item(Theme::DATA_TYPE_COLOR_ROLE, "font_color_role", "Button"));
	CHECK_FALSE(theme->has_theme_item_nocheck(Theme::DATA_TYPE_COLOR_SCHEME, "font_color_scheme", "Button"));
}

TEST_CASE("[Theme] Raw color scheme access preserves stored null values") {
	Ref<Theme> theme = memnew(Theme);
	Ref<ColorScheme> default_scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<ColorScheme> explicit_scheme = memnew(ColorScheme(Color("#006495")));
	theme->set_default_color_scheme(default_scheme);

	CHECK_FALSE(theme->has_color_scheme_nocheck("missing_scheme", "Button"));
	CHECK_FALSE(theme->get_color_scheme_nocheck("missing_scheme", "Button").is_valid());
	CHECK(theme->get_color_scheme("missing_scheme", "Button") == default_scheme);

	theme->set_theme_item(Theme::DATA_TYPE_COLOR_SCHEME, "null_scheme", "Button", Ref<ColorScheme>());
	CHECK(theme->has_color_scheme_nocheck("null_scheme", "Button"));
	CHECK_FALSE(theme->get_color_scheme_nocheck("null_scheme", "Button").is_valid());
	CHECK(theme->get_color_scheme("null_scheme", "Button") == default_scheme);
	CHECK(Ref<ColorScheme>(theme->get_theme_item(Theme::DATA_TYPE_COLOR_SCHEME, "null_scheme", "Button")) == default_scheme);

	Ref<Theme> snapshot = memnew(Theme);
	snapshot->merge_with(theme);
	CHECK(snapshot->has_color_scheme_nocheck("null_scheme", "Button"));
	CHECK_FALSE(snapshot->get_color_scheme_nocheck("null_scheme", "Button").is_valid());

	theme->set_theme_item(Theme::DATA_TYPE_COLOR_SCHEME, "explicit_scheme", "Button", explicit_scheme);
	CHECK(theme->has_color_scheme_nocheck("explicit_scheme", "Button"));
	CHECK(theme->get_color_scheme_nocheck("explicit_scheme", "Button") == explicit_scheme);
	CHECK(theme->get_color_scheme("explicit_scheme", "Button") == explicit_scheme);
}

TEST_CASE("[Theme] Dynamic type helpers publish changes and preserve shared subscriptions") {
	Ref<Theme> theme = memnew(Theme);
	Ref<ColorScheme> scheme = memnew(ColorScheme);
	Array one_signal;
	one_signal.push_back(Array());
	SIGNAL_WATCH(theme.ptr(), "changed");
	SIGNAL_WATCH(theme.ptr(), "property_list_changed");

	theme->add_color_role_type("Original");
	SIGNAL_CHECK("changed", one_signal);
	SIGNAL_CHECK("property_list_changed", one_signal);
	theme->add_color_role_type("Original");
	SIGNAL_CHECK_FALSE("changed");
	theme->rename_color_role_type("Original", "Renamed");
	SIGNAL_CHECK("changed", one_signal);
	SIGNAL_CHECK("property_list_changed", one_signal);
	theme->remove_color_role_type("Renamed");
	SIGNAL_CHECK("changed", one_signal);
	SIGNAL_CHECK("property_list_changed", one_signal);
	theme->remove_color_role_type("Renamed");
	SIGNAL_CHECK_FALSE("changed");

	theme->add_color_scheme_type("Original");
	SIGNAL_CHECK("changed", one_signal);
	SIGNAL_CHECK("property_list_changed", one_signal);
	theme->rename_color_scheme_type("Original", "Renamed");
	SIGNAL_CHECK("changed", one_signal);
	SIGNAL_CHECK("property_list_changed", one_signal);
	theme->set_color_scheme("first", "Renamed", scheme);
	theme->set_color_scheme("second", "Retained", scheme);
	SIGNAL_DISCARD("changed");
	SIGNAL_DISCARD("property_list_changed");
	theme->remove_color_scheme_type("Renamed");
	SIGNAL_CHECK("changed", one_signal);
	SIGNAL_CHECK("property_list_changed", one_signal);
	scheme->set_dark(true);
	SIGNAL_CHECK("changed", one_signal);
	theme->remove_color_scheme_type("Retained");
	SIGNAL_DISCARD("changed");
	SIGNAL_DISCARD("property_list_changed");
	scheme->set_dark(false);
	SIGNAL_CHECK_FALSE("changed");
	SIGNAL_UNWATCH(theme.ptr(), "changed");
	SIGNAL_UNWATCH(theme.ptr(), "property_list_changed");
}

TEST_CASE("[Theme] Composite type operations publish one complete update") {
	Ref<Theme> theme = memnew(Theme);
	Ref<ColorScheme> scheme = memnew(ColorScheme);
	Array one_signal;
	one_signal.push_back(Array());
	SIGNAL_WATCH(theme.ptr(), "changed");
	SIGNAL_WATCH(theme.ptr(), "property_list_changed");
	theme->add_type("Original");
	SIGNAL_CHECK("changed", one_signal);
	SIGNAL_CHECK("property_list_changed", one_signal);
	theme->set_color_scheme("first", "Original", scheme);
	theme->set_color_scheme("second", "Original", scheme);
	theme->set_stylebox("panel", "Original", memnew(StyleBoxFlat));
	theme->set_type_variation("Variant", "Original");
	SIGNAL_DISCARD("changed");
	SIGNAL_DISCARD("property_list_changed");
	theme->rename_type("Original", "Renamed");
	SIGNAL_CHECK("changed", one_signal);
	SIGNAL_CHECK("property_list_changed", one_signal);
	CHECK(theme->get_type_variation_base("Variant") == StringName("Renamed"));
	CHECK(theme->get_color_scheme_nocheck("first", "Renamed") == scheme);
	theme->remove_type("Renamed");
	SIGNAL_CHECK("changed", one_signal);
	SIGNAL_CHECK("property_list_changed", one_signal);
	CHECK(theme->get_type_variation_base("Variant") == StringName());
	CHECK_FALSE(theme->has_color_scheme_nocheck("first", "Renamed"));
	scheme->set_dark(true);
	SIGNAL_CHECK_FALSE("changed");
	SIGNAL_UNWATCH(theme.ptr(), "changed");
	SIGNAL_UNWATCH(theme.ptr(), "property_list_changed");
}

TEST_CASE("[Theme] String theme items") {
	Ref<Theme> theme = memnew(Theme);

	theme->set_string("label", "Button", "Open");
	CHECK(theme->has_string("label", "Button"));
	CHECK(theme->get_string("label", "Button") == "Open");
	CHECK(theme->get_theme_item(Theme::DATA_TYPE_STRING, "label", "Button") == String("Open"));

	theme->set_string("empty", "Button", "");
	CHECK(theme->has_string("empty", "Button"));
	CHECK(theme->get_string("empty", "Button").is_empty());

	theme->rename_string("label", "caption", "Button");
	CHECK_FALSE(theme->has_string("label", "Button"));
	CHECK(theme->get_string("caption", "Button") == "Open");

	Ref<Theme> copy = memnew(Theme);
	copy->merge_with(theme);
	CHECK(copy->get_string("caption", "Button") == "Open");
	CHECK(copy->has_string("empty", "Button"));

	copy->clear_theme_item(Theme::DATA_TYPE_STRING, "caption", "Button");
	CHECK_FALSE(copy->has_string("caption", "Button"));
}

TEST_CASE("[Theme] StyleBox dynamic colors") {
	Ref<ColorScheme> default_scheme = memnew(ColorScheme(Color("#6750a4")));
	Ref<ColorScheme> explicit_scheme = memnew(ColorScheme(Color("#006495")));
	Ref<StyleBoxFlat> stylebox = memnew(StyleBoxFlat);
	const Color scale(0.75, 0.5, 0.25, 0.8);
	const Color authored_color("#123456");

	stylebox->set_bg_color(authored_color);
	stylebox->set_bg_color_role(ColorRole::PRIMARY);
	stylebox->set_bg_color_scale(scale);
	stylebox->set_default_color_scheme(default_scheme);
	CHECK(stylebox->get_bg_color().is_equal_approx(authored_color));
	CHECK(stylebox->get_resolved_bg_color().is_equal_approx(default_scheme->get_color(ColorRole::PRIMARY) * scale));

	stylebox->set_color_scheme(explicit_scheme);
	CHECK(stylebox->get_bg_color().is_equal_approx(authored_color));
	CHECK(stylebox->get_resolved_bg_color().is_equal_approx(explicit_scheme->get_color(ColorRole::PRIMARY) * scale));

	explicit_scheme->set_dark(true);
	CHECK(stylebox->get_bg_color().is_equal_approx(authored_color));
	CHECK(stylebox->get_resolved_bg_color().is_equal_approx(explicit_scheme->get_color(ColorRole::PRIMARY) * scale));

	stylebox->set_bg_color_role(ColorRole::STATIC);
	CHECK(stylebox->get_resolved_bg_color().is_equal_approx(authored_color));
	explicit_scheme->set_source_color(Color("#ff0000"));
	CHECK(stylebox->get_bg_color().is_equal_approx(authored_color));
	CHECK(stylebox->get_resolved_bg_color().is_equal_approx(authored_color));

	stylebox->set_bg_color_role(ColorRole::PRIMARY);
	stylebox->set_color_scheme(Ref<ColorScheme>());
	stylebox->set_default_color_scheme(Ref<ColorScheme>());
	CHECK(stylebox->get_bg_color().is_equal_approx(authored_color));
	CHECK(stylebox->get_resolved_bg_color().is_equal_approx(authored_color));

	Ref<StyleBoxLine> line = memnew(StyleBoxLine);
	const Color authored_line_color("#654321");
	line->set_color(authored_line_color);
	line->set_color_role(ColorRole::ERROR);
	line->set_color_scheme(explicit_scheme);
	CHECK(line->get_color().is_equal_approx(authored_line_color));
	CHECK(line->get_resolved_color().is_equal_approx(explicit_scheme->get_color(ColorRole::ERROR)));
	line->set_color_scheme(Ref<ColorScheme>());
	CHECK(line->get_color().is_equal_approx(authored_line_color));
	CHECK(line->get_resolved_color().is_equal_approx(authored_line_color));

	Ref<StyleBoxTexture> texture = memnew(StyleBoxTexture);
	const Color authored_modulate("#abcdef");
	texture->set_modulate(authored_modulate);
	texture->set_color_role(ColorRole::TERTIARY);
	texture->set_color_scheme(explicit_scheme);
	CHECK(texture->get_modulate().is_equal_approx(authored_modulate));
	CHECK(texture->get_resolved_modulate().is_equal_approx(explicit_scheme->get_color(ColorRole::TERTIARY)));
	texture->set_color_scheme(Ref<ColorScheme>());
	CHECK(texture->get_modulate().is_equal_approx(authored_modulate));
	CHECK(texture->get_resolved_modulate().is_equal_approx(authored_modulate));
}

TEST_CASE("[Theme] Fallback color scheme changes invalidate theme caches") {
	ThemeDB *theme_db = ThemeDB::get_singleton();
	REQUIRE(theme_db != nullptr);

	Ref<ColorScheme> original_scheme = theme_db->get_fallback_color_scheme();
	Ref<ColorScheme> test_scheme = memnew(ColorScheme);
	SIGNAL_WATCH(theme_db, SNAME("fallback_changed"));

	theme_db->set_fallback_color_scheme(test_scheme);
	Array signal_args = { {} };
	SIGNAL_CHECK(SNAME("fallback_changed"), signal_args);

	test_scheme->set_dark(!test_scheme->is_dark());
	SIGNAL_CHECK(SNAME("fallback_changed"), signal_args);

	theme_db->set_fallback_color_scheme(original_scheme);
	SIGNAL_DISCARD(SNAME("fallback_changed"));
	test_scheme->set_dark(!test_scheme->is_dark());
	SIGNAL_CHECK_FALSE(SNAME("fallback_changed"));

	SIGNAL_UNWATCH(theme_db, SNAME("fallback_changed"));
}

TEST_CASE("[ThemeDB] Fallback icon font") {
	ThemeDB *theme_db = ThemeDB::get_singleton();
	REQUIRE(theme_db != nullptr);

	Ref<Font> original_icon_font = theme_db->get_fallback_icon_font();
	Ref<FontFile> test_icon_font = memnew(FontFile);
	theme_db->set_fallback_icon_font(test_icon_font);
	CHECK(theme_db->get_fallback_icon_font() == test_icon_font);

	theme_db->set_fallback_icon_font(original_icon_font);
}

TEST_CASE_FIXTURE(Fixture, "[Theme] Good theme type names") {
	StringName names[] = {
		"", // Empty name.
		"CapitalizedName",
		"snake_cased_name",
		"42",
		"_Underscore_",
	};

	SUBCASE("add_type") {
		for (const StringName &name : names) {
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->add_type(name);
			CHECK_FALSE(ed.has_error);
		}
	}

	SUBCASE("set_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->set_theme_item(entry.type, valid_item_name, name, entry.value);
				CHECK_FALSE(ed.has_error);
			}
		}
	}

	SUBCASE("add_theme_item_type") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->add_theme_item_type(entry.type, name);
				CHECK_FALSE(ed.has_error);
			}
		}
	}

	SUBCASE("set_type_variation") {
		for (const StringName &name : names) {
			if (name == StringName()) { // Skip empty here, not allowed.
				continue;
			}
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->set_type_variation(valid_type_name, name);
			CHECK_FALSE(ed.has_error);
		}
		for (const StringName &name : names) {
			if (name == StringName()) { // Skip empty here, not allowed.
				continue;
			}
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->set_type_variation(name, valid_type_name);
			CHECK_FALSE(ed.has_error);
		}
	}
}

TEST_CASE_FIXTURE(Fixture, "[Theme] Bad theme type names") {
	StringName names[] = {
		"With/Slash",
		"With Space",
		"With@various$symbols!",
		String::utf8("contains_汉字"),
	};

	ERR_PRINT_OFF; // All these rightfully print errors.

	SUBCASE("add_type") {
		for (const StringName &name : names) {
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->add_type(name);
			CHECK(ed.has_error);
		}
	}

	SUBCASE("set_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->set_theme_item(entry.type, valid_item_name, name, entry.value);
				CHECK(ed.has_error);
			}
		}
	}

	SUBCASE("add_theme_item_type") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->add_theme_item_type(entry.type, name);
				CHECK(ed.has_error);
			}
		}
	}

	SUBCASE("set_type_variation") {
		for (const StringName &name : names) {
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->set_type_variation(valid_type_name, name);
			CHECK(ed.has_error);
		}
		for (const StringName &name : names) {
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->set_type_variation(name, valid_type_name);
			CHECK(ed.has_error);
		}
	}

	ERR_PRINT_ON;
}

TEST_CASE_FIXTURE(Fixture, "[Theme] Good theme item names") {
	StringName names[] = {
		"CapitalizedName",
		"snake_cased_name",
		"42",
		"_Underscore_",
	};

	SUBCASE("set_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->set_theme_item(entry.type, name, valid_type_name, entry.value);
				CHECK_FALSE(ed.has_error);
				CHECK(theme->has_theme_item(entry.type, name, valid_type_name));
			}
		}
	}

	SUBCASE("rename_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);
				theme->set_theme_item(entry.type, valid_item_name, valid_type_name, entry.value);

				ErrorDetector ed;
				theme->rename_theme_item(entry.type, valid_item_name, name, valid_type_name);
				CHECK_FALSE(ed.has_error);
				CHECK_FALSE(theme->has_theme_item(entry.type, valid_item_name, valid_type_name));
				CHECK(theme->has_theme_item(entry.type, name, valid_type_name));
			}
		}
	}
}

TEST_CASE_FIXTURE(Fixture, "[Theme] Bad theme item names") {
	StringName names[] = {
		"", // Empty name.
		"With/Slash",
		"With Space",
		"With@various$symbols!",
		String::utf8("contains_汉字"),
	};

	ERR_PRINT_OFF; // All these rightfully print errors.

	SUBCASE("set_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->set_theme_item(entry.type, name, valid_type_name, entry.value);
				CHECK(ed.has_error);
				CHECK_FALSE(theme->has_theme_item(entry.type, name, valid_type_name));
			}
		}
	}

	SUBCASE("rename_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);
				theme->set_theme_item(entry.type, valid_item_name, valid_type_name, entry.value);

				ErrorDetector ed;
				theme->rename_theme_item(entry.type, valid_item_name, name, valid_type_name);
				CHECK(ed.has_error);
				CHECK(theme->has_theme_item(entry.type, valid_item_name, valid_type_name));
				CHECK_FALSE(theme->has_theme_item(entry.type, name, valid_type_name));
			}
		}
	}

	ERR_PRINT_ON;
}

} // namespace TestTheme
