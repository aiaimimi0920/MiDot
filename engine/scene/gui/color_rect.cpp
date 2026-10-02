/**************************************************************************/
/*  color_rect.cpp                                                        */
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

#include "color_rect.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/display/accessibility_server.h"

void ColorRect::set_color(const Color &p_color) {
	if (color == p_color) {
		return;
	}
	color = p_color;
	_update_color();
}

Color ColorRect::get_color() const {
	return color;
}

void ColorRect::set_color_role(ColorRole p_color_role) {
	if (color_role == p_color_role) {
		return;
	}

	color_role = p_color_role;
	_update_color();
}

ColorRole ColorRect::get_color_role() const {
	return color_role;
}

void ColorRect::set_color_scheme(const Ref<ColorScheme> &p_color_scheme) {
	if (color_scheme == p_color_scheme) {
		return;
	}

	if (color_scheme.is_valid()) {
		color_scheme->disconnect_changed(callable_mp(this, &ColorRect::_update_color));
	}

	color_scheme = p_color_scheme;
	if (color_scheme.is_valid()) {
		color_scheme->connect_changed(callable_mp(this, &ColorRect::_update_color));
	}

	_update_color();
}

Ref<ColorScheme> ColorRect::get_color_scheme() const {
	return color_scheme;
}

void ColorRect::_update_color() {
	Color target_color = color;
	if (color_role != ColorRole::STATIC) {
		Ref<ColorScheme> active_color_scheme = color_scheme;
		if (active_color_scheme.is_null()) {
			active_color_scheme = get_theme_color_scheme("default_color_scheme");
		}
		if (active_color_scheme.is_null()) {
			active_color_scheme = get_theme_default_color_scheme();
		}
		if (active_color_scheme.is_valid()) {
			target_color = active_color_scheme->get_color(color_role);
		}
	}

	if (resolved_color == target_color) {
		return;
	}

	resolved_color = target_color;
	queue_accessibility_update();
	queue_redraw();
}

void ColorRect::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ACCESSIBILITY_UPDATE: {
			RID ae = get_accessibility_element();
			ERR_FAIL_COND(ae.is_null());

			AccessibilityServer::get_singleton()->update_set_color_value(ae, resolved_color);
		} break;

		case NOTIFICATION_DRAW: {
			draw_rect(Rect2(Point2(), get_size()), resolved_color);
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_color();
		} break;
	}
}

void ColorRect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_color", "color"), &ColorRect::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &ColorRect::get_color);
	ClassDB::bind_method(D_METHOD("set_color_role", "color_role"), &ColorRect::set_color_role);
	ClassDB::bind_method(D_METHOD("get_color_role"), &ColorRect::get_color_role);
	ClassDB::bind_method(D_METHOD("set_color_scheme", "color_scheme"), &ColorRect::set_color_scheme);
	ClassDB::bind_method(D_METHOD("get_color_scheme"), &ColorRect::get_color_scheme);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "color_role", PROPERTY_HINT_ENUM, color_role_hint), "set_color_role", "get_color_role");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "color_scheme", PROPERTY_HINT_RESOURCE_TYPE, "ColorScheme"), "set_color_scheme", "get_color_scheme");
}
