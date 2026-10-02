/**************************************************************************/
/*  style_box.cpp                                                         */
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

#include "style_box.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "scene/main/canvas_item.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/style_box_line.h"
#include "scene/resources/style_box_texture.h"

Size2 StyleBox::get_minimum_size() const {
	Size2 min_size = Size2(get_margin(SIDE_LEFT) + get_margin(SIDE_RIGHT), get_margin(SIDE_TOP) + get_margin(SIDE_BOTTOM));
	Size2 custom_size;
	GDVIRTUAL_CALL(_get_minimum_size, custom_size);

	if (min_size.x < custom_size.x) {
		min_size.x = custom_size.x;
	}
	if (min_size.y < custom_size.y) {
		min_size.y = custom_size.y;
	}

	return min_size;
}

void StyleBox::set_content_margin(Side p_side, float p_value) {
	ERR_FAIL_INDEX((int)p_side, 4);

	content_margin[p_side] = p_value;
	custom_emit_changed();
}

void StyleBox::set_content_margin_all(float p_value) {
	for (int i = 0; i < 4; i++) {
		content_margin[i] = p_value;
	}
	custom_emit_changed();
}

void StyleBox::set_content_margin_individual(float p_left, float p_top, float p_right, float p_bottom) {
	content_margin[SIDE_LEFT] = p_left;
	content_margin[SIDE_TOP] = p_top;
	content_margin[SIDE_RIGHT] = p_right;
	content_margin[SIDE_BOTTOM] = p_bottom;
	custom_emit_changed();
}

float StyleBox::get_content_margin(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0.0);

	return content_margin[p_side];
}

float StyleBox::get_margin(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0.0);

	if (content_margin[p_side] < 0) {
		return get_style_margin(p_side);
	} else {
		return content_margin[p_side];
	}
}

Point2 StyleBox::get_offset() const {
	return Point2(get_margin(SIDE_LEFT), get_margin(SIDE_TOP));
}

void StyleBox::draw(RID p_canvas_item, const Rect2 &p_rect) const {
	GDVIRTUAL_CALL(_draw, p_canvas_item, p_rect);
}

bool StyleBox::uses_dynamic_color_roles() const {
	// Unknown and scripted style boxes keep the conservative scheme-injection path.
	if (_get_extension() || Ref<Script>(get_script()).is_valid()) {
		return true;
	}

	const StringName class_name = get_class_name();
	if (class_name == "StyleBoxEmpty") {
		return false;
	}
	if (class_name == "StyleBoxFlat") {
		const StyleBoxFlat *style = static_cast<const StyleBoxFlat *>(this);
		return style->get_bg_color_role() != ColorRole::STATIC || style->get_border_color_role() != ColorRole::STATIC || style->get_shadow_color_role() != ColorRole::STATIC;
	}
	if (class_name == "StyleBoxLine") {
		const StyleBoxLine *style = static_cast<const StyleBoxLine *>(this);
		return style->get_color_role() != ColorRole::STATIC;
	}
	if (class_name == "StyleBoxTexture") {
		const StyleBoxTexture *style = static_cast<const StyleBoxTexture *>(this);
		return style->get_color_role() != ColorRole::STATIC;
	}

	return true;
}

Rect2 StyleBox::get_draw_rect(const Rect2 &p_rect) const {
	Rect2 ret;
	if (GDVIRTUAL_CALL(_get_draw_rect, p_rect, ret)) {
		return ret;
	}
	return p_rect;
}

void StyleBox::set_emit_changed_signal(bool p_enabled) {
	emit_changed_signal = p_enabled;
}

bool StyleBox::is_emit_changed_signal_enabled() const {
	return emit_changed_signal;
}

void StyleBox::set_color_scheme(const Ref<ColorScheme> &p_color_scheme) {
	if (color_scheme == p_color_scheme) {
		return;
	}

	if (color_scheme.is_valid()) {
		color_scheme->disconnect_changed(callable_mp(this, &StyleBox::update_color));
	}
	color_scheme = p_color_scheme;
	if (color_scheme.is_valid()) {
		color_scheme->connect_changed(callable_mp(this, &StyleBox::update_color), CONNECT_REFERENCE_COUNTED);
	}

	update_color();
}

Ref<ColorScheme> StyleBox::get_color_scheme() const {
	return color_scheme;
}

void StyleBox::set_default_color_scheme(const Ref<ColorScheme> &p_color_scheme) {
	if (default_color_scheme == p_color_scheme) {
		return;
	}

	if (default_color_scheme.is_valid()) {
		default_color_scheme->disconnect_changed(callable_mp(this, &StyleBox::update_color));
	}
	default_color_scheme = p_color_scheme;
	if (default_color_scheme.is_valid()) {
		default_color_scheme->connect_changed(callable_mp(this, &StyleBox::update_color), CONNECT_REFERENCE_COUNTED);
	}

	update_color();
}

Ref<ColorScheme> StyleBox::get_default_color_scheme() const {
	return default_color_scheme;
}

void StyleBox::update_color() {
	const bool previous_emit_changed_signal = emit_changed_signal;
	emit_changed_signal = false;
	_update_color();
	emit_changed_signal = previous_emit_changed_signal;
	custom_emit_changed();
}

void StyleBox::custom_emit_changed() {
	if (emit_changed_signal) {
		emit_changed();
	}
}

CanvasItem *StyleBox::get_current_item_drawn() const {
	return CanvasItem::get_current_item_drawn();
}

bool StyleBox::test_mask(const Point2 &p_point, const Rect2 &p_rect) const {
	bool ret = true;
	GDVIRTUAL_CALL(_test_mask, p_point, p_rect, ret);
	return ret;
}

void StyleBox::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_minimum_size"), &StyleBox::get_minimum_size);

	ClassDB::bind_method(D_METHOD("set_content_margin", "margin", "offset"), &StyleBox::set_content_margin);
	ClassDB::bind_method(D_METHOD("set_content_margin_all", "offset"), &StyleBox::set_content_margin_all);
	ClassDB::bind_method(D_METHOD("get_content_margin", "margin"), &StyleBox::get_content_margin);

	ClassDB::bind_method(D_METHOD("get_margin", "margin"), &StyleBox::get_margin);
	ClassDB::bind_method(D_METHOD("get_offset"), &StyleBox::get_offset);

	ClassDB::bind_method(D_METHOD("draw", "canvas_item", "rect"), &StyleBox::draw);
	ClassDB::bind_method(D_METHOD("get_current_item_drawn"), &StyleBox::get_current_item_drawn);

	ClassDB::bind_method(D_METHOD("test_mask", "point", "rect"), &StyleBox::test_mask);

	ClassDB::bind_method(D_METHOD("set_color_scheme", "color_scheme"), &StyleBox::set_color_scheme);
	ClassDB::bind_method(D_METHOD("get_color_scheme"), &StyleBox::get_color_scheme);
	ClassDB::bind_method(D_METHOD("set_default_color_scheme", "color_scheme"), &StyleBox::set_default_color_scheme);
	ClassDB::bind_method(D_METHOD("get_default_color_scheme"), &StyleBox::get_default_color_scheme);

	ADD_GROUP("Content Margins", "content_margin_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "content_margin_left", PROPERTY_HINT_RANGE, "-1,2048,1,suffix:px"), "set_content_margin", "get_content_margin", SIDE_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "content_margin_top", PROPERTY_HINT_RANGE, "-1,2048,1,suffix:px"), "set_content_margin", "get_content_margin", SIDE_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "content_margin_right", PROPERTY_HINT_RANGE, "-1,2048,1,suffix:px"), "set_content_margin", "get_content_margin", SIDE_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "content_margin_bottom", PROPERTY_HINT_RANGE, "-1,2048,1,suffix:px"), "set_content_margin", "get_content_margin", SIDE_BOTTOM);

	ADD_GROUP("Dynamic Colors", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "color_scheme", PROPERTY_HINT_RESOURCE_TYPE, ColorScheme::get_class_static()), "set_color_scheme", "get_color_scheme");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "default_color_scheme", PROPERTY_HINT_RESOURCE_TYPE, ColorScheme::get_class_static()), "set_default_color_scheme", "get_default_color_scheme");

	GDVIRTUAL_BIND(_draw, "to_canvas_item", "rect")
	GDVIRTUAL_BIND(_get_draw_rect, "rect")
	GDVIRTUAL_BIND(_get_minimum_size)
	GDVIRTUAL_BIND(_test_mask, "point", "rect")
}

StyleBox::StyleBox() {
	for (int i = 0; i < 4; i++) {
		content_margin[i] = -1;
	}
}
