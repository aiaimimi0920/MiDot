/**************************************************************************/
/*  test_style_box_flat.cpp                                               */
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

TEST_FORCE_LINK(test_style_box_flat)

#include "scene/resources/style_box_flat.h"

namespace TestStyleBoxFlat {

TEST_CASE("[StyleBoxFlat] Material elevation presets") {
	Ref<StyleBoxFlat> style_box = memnew(StyleBoxFlat);
	const Rect2 source_rect(10, 10, 100, 50);

	CHECK_FALSE(style_box->is_dynamic_shadow_enabled());
	CHECK(style_box->get_elevation_level() == StyleBoxFlat::ELEVATION_LEVEL_0);
	CHECK(style_box->get_draw_rect(source_rect) == source_rect);

	style_box->set_dynamic_shadow(true);
	style_box->set_elevation_level(StyleBoxFlat::ELEVATION_LEVEL_5);
	CHECK(style_box->is_dynamic_shadow_enabled());
	CHECK(style_box->get_elevation_level() == StyleBoxFlat::ELEVATION_LEVEL_5);
	CHECK(style_box->get_draw_rect(source_rect) == Rect2(-12, -7, 144, 96));

	style_box->set_dynamic_shadow(false);
	style_box->set_shadow_size(4);
	style_box->set_shadow_offset(Point2(2, 3));
	CHECK(style_box->get_draw_rect(source_rect) == Rect2(8, 9, 108, 58));
}

} // namespace TestStyleBoxFlat
