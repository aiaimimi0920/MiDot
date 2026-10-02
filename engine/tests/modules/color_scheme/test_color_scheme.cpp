/**************************************************************************/
/*  test_color_scheme.cpp                                                 */
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

TEST_FORCE_LINK(test_color_scheme)

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_COLOR_SCHEME_ENABLED

#include "core/object/message_queue.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/theme.h"
#include "tests/signal_watcher.h"

#include "modules/color_scheme/color_scheme.h"

#include <limits>

namespace TestColorScheme {

static Array one_empty_signal_args() {
	Array fired_signals;
	fired_signals.push_back(Array());
	return fired_signals;
}

struct ExpectedColors {
	bool dark;
	float contrast;
	uint32_t background;
	uint32_t surface;
	uint32_t on_surface;
	uint32_t outline;
	uint32_t primary;
	uint32_t on_primary;
	uint32_t primary_container;
	uint32_t error;
};

TEST_CASE("[ColorScheme] Material content colors match the vendored runtime") {
	const ExpectedColors cases[] = {
		{ false, -1.0f, 0xfff9f9fe, 0xfff9f9fe, 0xff5d5f63, 0xffadb2bc, 0xff4678ac, 0xfffdfcff, 0xffc0dbff, 0xffda342e },
		{ false, 0.0f, 0xfff9f9fe, 0xfff9f9fe, 0xff191c1f, 0xff727780, 0xff134e7f, 0xffffffff, 0xff336699, 0xffba1a1a },
		{ false, 1.0f, 0xfff9f9fe, 0xfff9f9fe, 0xff000000, 0xff272d34, 0xff002e51, 0xffffffff, 0xff0e4b7d, 0xff600004 },
		{ true, -1.0f, 0xff111417, 0xff111417, 0xffa1a3a7, 0xff4f545d, 0xff6494ca, 0xff002b4d, 0xff003c68, 0xffff5449 },
		{ true, 0.0f, 0xff111417, 0xff111417, 0xffe2e2e7, 0xff8c919a, 0xff9ecaff, 0xff003258, 0xff336699, 0xffffb4ab },
		{ true, 1.0f, 0xff111417, 0xff111417, 0xffffffff, 0xffebf0fa, 0xffe8f0ff, 0xff000000, 0xff96c6ff, 0xffffece9 },
	};

	for (const ExpectedColors &expected : cases) {
		CAPTURE(expected.dark);
		CAPTURE(expected.contrast);
		ColorScheme scheme(Color(0.2f, 0.4f, 0.6f), expected.dark, expected.contrast);
		CHECK(scheme.get_background().to_argb32() == expected.background);
		CHECK(scheme.get_surface().to_argb32() == expected.surface);
		CHECK(scheme.get_on_surface().to_argb32() == expected.on_surface);
		CHECK(scheme.get_outline().to_argb32() == expected.outline);
		CHECK(scheme.get_primary().to_argb32() == expected.primary);
		CHECK(scheme.get_on_primary().to_argb32() == expected.on_primary);
		CHECK(scheme.get_primary_container().to_argb32() == expected.primary_container);
		CHECK(scheme.get_error().to_argb32() == expected.error);
	}
}

TEST_CASE("[ColorScheme] Contrast level is clamped to the supported range") {
	ColorScheme low(Color(0.2f, 0.4f, 0.6f), false, -10.0f);
	ColorScheme high(Color(0.2f, 0.4f, 0.6f), false, 10.0f);
	CHECK(low.get_contrast_level() == -1.0f);
	CHECK(high.get_contrast_level() == 1.0f);
}

// CPU-backed texture makes palette assertions independent of the render backend.
class MutableTestTexture : public Texture2D {
	Ref<Image> image;
	mutable int image_reads = 0;

public:
	Ref<Image> get_image() const override {
		image_reads++;
		return image;
	}
	int get_image_reads() const { return image_reads; }
	void set_image(const Ref<Image> &p_image) {
		image = p_image;
		emit_changed();
	}
	void set_color(const Color &p_color) {
		image = memnew(Image(2, 2, false, Image::FORMAT_RGB8));
		image->fill(p_color);
		emit_changed();
	}
};

TEST_CASE("[ColorScheme] Texture extraction is cached across scheme parameter changes") {
	Ref<MutableTestTexture> texture = memnew(MutableTestTexture);
	texture->set_color(Color(1, 0, 0));
	ColorScheme scheme(texture);
	CHECK(texture->get_image_reads() == 1);

	scheme.set_dark(true);
	CHECK(texture->get_image_reads() == 1);
	scheme.set_contrast_level(0.5f);
	CHECK(texture->get_image_reads() == 1);

	texture->set_color(Color(0, 0, 1));
	CHECK(texture->get_image_reads() == 2);
}

TEST_CASE("[ColorScheme] Invalid inputs preserve a usable synchronous palette") {
	const float invalid_values[] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
	Ref<MutableTestTexture> texture = memnew(MutableTestTexture);
	texture->set_color(Color(1, 0, 0));
	Ref<ColorScheme> scheme = memnew(ColorScheme(texture, true, 0.5f));
	const Color original_primary = scheme->get_primary();
	SIGNAL_WATCH(scheme.ptr(), "updated_color_scheme");
	ERR_PRINT_OFF;
	for (float invalid : invalid_values) {
		scheme->set_contrast_level(invalid);
		CHECK(scheme->get_contrast_level() == 0.5f);
		for (int component = 0; component < 4; component++) {
			Color source(0.2f, 0.4f, 0.6f);
			source[component] = invalid;
			scheme->set_source_color(source);
			CHECK(scheme->get_source_texture() == texture);
			CHECK(scheme->get_primary() == original_primary);
			ColorScheme constructed(source, false, invalid);
			ColorScheme defaults;
			CHECK(constructed.get_primary() == defaults.get_primary());
			CHECK(constructed.get_contrast_level() == 0.0f);
		}
		ColorScheme constructed_texture(texture, true, invalid);
		CHECK(constructed_texture.get_contrast_level() == 0.0f);
	}
	CHECK(scheme->get_color(static_cast<ColorRole>(-1)) == scheme->get_source_color_argb());
	CHECK(scheme->get_color(static_cast<ColorRole>(1000)) == scheme->get_source_color_argb());
	ERR_PRINT_ON;
	CHECK(scheme->get_color(ColorRole::STATIC) == scheme->get_source_color_argb());
	SIGNAL_CHECK_FALSE("updated_color_scheme");
	texture->set_color(Color(0, 0, 1));
	ColorScheme blue(Color(0, 0, 1), true, 0.5f);
	CHECK(scheme->get_primary() == blue.get_primary());
	SIGNAL_CHECK("updated_color_scheme", one_empty_signal_args());
	SIGNAL_UNWATCH(scheme.ptr(), "updated_color_scheme");
}

TEST_CASE("[ColorScheme] Unreadable and transparent textures retain distinct fallback seeds") {
	Ref<MutableTestTexture> texture = memnew(MutableTestTexture);
	ERR_PRINT_OFF;
	ColorScheme scheme(texture);
	ERR_PRINT_ON;
	CHECK(scheme.get_source_color_argb() == Color(0, 0, 0));
	CHECK(texture->get_image_reads() == 1);
	scheme.set_dark(true);
	scheme.set_contrast_level(0.5f);
	CHECK(texture->get_image_reads() == 1);

	ERR_PRINT_OFF;
	texture->set_image(memnew(Image));
	ERR_PRINT_ON;
	CHECK(scheme.get_source_color_argb() == Color(0, 0, 0));
	CHECK(texture->get_image_reads() == 2);
	Ref<Image> transparent = memnew(Image(2, 2, false, Image::FORMAT_RGBA8));
	transparent->fill(Color(1, 0, 0, 0));
	texture->set_image(transparent);
	CHECK(scheme.get_source_color_argb().to_argb32() == 0xff4285f4);
	CHECK(texture->get_image_reads() == 3);
	texture->set_color(Color(1, 0, 0));
	ColorScheme red(Color(1, 0, 0), true, 0.5f);
	CHECK(scheme.get_primary() == red.get_primary());
	CHECK(texture->get_image_reads() == 4);
}

TEST_CASE("[ColorScheme] Texture palette follows content and detaches old sources") {
	Ref<MutableTestTexture> texture = memnew(MutableTestTexture);
	texture->set_color(Color(1, 0, 0));
	Ref<ColorScheme> scheme = memnew(ColorScheme(texture));
	const Color red_primary = scheme->get_primary();
	SIGNAL_WATCH(scheme.ptr(), "updated_color_scheme");

	texture->set_color(Color(0, 0, 1));
	ColorScheme blue(Color(0, 0, 1));
	CHECK(scheme->get_primary() != red_primary);
	CHECK(scheme->get_primary() == blue.get_primary());
	SIGNAL_CHECK("updated_color_scheme", one_empty_signal_args());

	scheme->set_source_texture(texture);
	SIGNAL_CHECK_FALSE("updated_color_scheme");
	const Color same_source_primary = scheme->get_primary();
	texture->set_color(Color(1, 1, 0));
	CHECK(scheme->get_primary() != same_source_primary);
	SIGNAL_CHECK("updated_color_scheme", one_empty_signal_args());

	Ref<MutableTestTexture> replacement = memnew(MutableTestTexture);
	replacement->set_color(Color(0, 1, 0));
	scheme->set_source_texture(replacement);
	SIGNAL_DISCARD("updated_color_scheme");
	texture->set_color(Color(1, 0, 0));
	SIGNAL_CHECK_FALSE("updated_color_scheme");
	replacement->set_color(Color(0, 0, 1));
	CHECK(scheme->get_primary() == blue.get_primary());
	SIGNAL_CHECK("updated_color_scheme", one_empty_signal_args());

	scheme->set_source_color(Color(1, 0, 0));
	SIGNAL_DISCARD("updated_color_scheme");
	replacement->set_color(Color(0, 1, 0));
	CHECK(scheme->get_primary() == red_primary);
	SIGNAL_CHECK_FALSE("updated_color_scheme");
	scheme->set_source_texture(replacement);
	scheme->set_source_texture(Ref<Texture2D>());
	SIGNAL_DISCARD("updated_color_scheme");
	replacement->set_color(Color(0, 0, 1));
	SIGNAL_CHECK_FALSE("updated_color_scheme");
	SIGNAL_UNWATCH(scheme.ptr(), "updated_color_scheme");
	scheme.unref();
	replacement->set_color(Color(1, 0, 0));
}

TEST_CASE("[SceneTree][ColorScheme] ImageTexture set_image propagates palette updates") {
	Ref<Image> image = memnew(Image(2, 2, false, Image::FORMAT_RGB8));
	image->fill(Color(0.9f, 0.05f, 0.05f));
	Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
	Ref<ColorScheme> scheme = memnew(ColorScheme(texture));
	SIGNAL_WATCH(scheme.ptr(), "updated_color_scheme");

	Ref<Image> updated_image = memnew(Image(2, 2, false, Image::FORMAT_RGB8));
	updated_image->fill(Color(0.05f, 0.1f, 0.9f));
	texture->set_image(updated_image);

	SIGNAL_CHECK("updated_color_scheme", one_empty_signal_args());
	SIGNAL_UNWATCH(scheme.ptr(), "updated_color_scheme");
}

TEST_CASE("[SceneTree][ColorScheme] Source changes refresh every color consumer") {
	Ref<MutableTestTexture> texture = memnew(MutableTestTexture);
	texture->set_color(Color(1, 0, 0));
	Ref<ColorScheme> color_scheme = memnew(ColorScheme(texture));

	Ref<Theme> theme = memnew(Theme);
	theme->set_color("accent", "Control", Color(1, 1, 1, 1));
	theme->set_color_role("accent_role", "Control", ColorRole::PRIMARY);
	theme->set_color_scheme("accent_scheme", "Control", color_scheme);
	theme->set_color("window_accent", "Window", Color(1, 1, 1, 1));
	theme->set_color_role("window_accent_role", "Window", ColorRole::PRIMARY);
	theme->set_color_scheme("window_accent_scheme", "Window", color_scheme);

	Control *control = memnew(Control);
	control->set_theme(theme);
	SceneTree::get_singleton()->get_root()->add_child(control);
	Window *window = memnew(Window);
	window->set_theme(theme);
	SceneTree::get_singleton()->get_root()->add_child(window);

	Ref<StyleBoxFlat> stylebox = memnew(StyleBoxFlat);
	stylebox->set_bg_color_role(ColorRole::PRIMARY);
	stylebox->set_color_scheme(color_scheme);

	const Color initial_primary = color_scheme->get_color(ColorRole::PRIMARY);
	const Color initial_control_color = control->get_theme_color("accent");
	const Color initial_window_color = window->get_theme_color("window_accent");
	const Color initial_stylebox_color = stylebox->get_resolved_bg_color();

	texture->set_color(Color(0, 0, 1));
	MessageQueue::get_singleton()->flush();

	CHECK(color_scheme->get_color(ColorRole::PRIMARY) != initial_primary);
	CHECK(control->get_theme_color("accent") != initial_control_color);
	CHECK(window->get_theme_color("window_accent") != initial_window_color);
	CHECK(stylebox->get_resolved_bg_color() != initial_stylebox_color);

	memdelete(control);
	memdelete(window);
}

} // namespace TestColorScheme

#endif // MODULE_COLOR_SCHEME_ENABLED
