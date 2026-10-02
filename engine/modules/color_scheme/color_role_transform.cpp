/**************************************************************************/
/*  color_role_transform.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                          */
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

#include "color_role_transform.h"

#include "color_scheme.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

Color ColorRoleTransform::_resolve(const Ref<ColorScheme> &p_color_scheme, HashSet<ObjectID> &r_visited) const {
	const ObjectID instance_id = get_instance_id();
	if (r_visited.has(instance_id)) {
		ERR_PRINT("ColorRoleTransform contains a cyclic lerp_transform reference.");
		return Color();
	}
	r_visited.insert(instance_id);

	Color result = static_color;
	if (color_role != ColorRole::STATIC) {
		if (p_color_scheme.is_null()) {
			ERR_PRINT("A ColorScheme is required to resolve a non-static ColorRoleTransform.");
			r_visited.erase(instance_id);
			return Color();
		}
		result = p_color_scheme->get_color(color_role);
	}

	result *= color_scale;
	if (inverted) {
		result = result.inverted();
	}
	if (!Math::is_zero_approx(darkened_amount)) {
		result = result.darkened(darkened_amount);
	}
	if (!Math::is_zero_approx(lightened_amount)) {
		result = result.lightened(lightened_amount);
	}

	if (!Math::is_zero_approx(lerp_weight)) {
		Color target = lerp_color;
		if (lerp_transform.is_valid()) {
			target = lerp_transform->_resolve(p_color_scheme, r_visited);
		} else if (lerp_color_role_enabled) {
			if (lerp_color_role == ColorRole::STATIC) {
				target = lerp_color;
			} else if (p_color_scheme.is_valid()) {
				target = p_color_scheme->get_color(lerp_color_role);
			} else {
				ERR_PRINT("A ColorScheme is required to resolve a lerp color role.");
				r_visited.erase(instance_id);
				return Color();
			}
		}
		result = result.lerp(target, lerp_weight);
	}

	if (clamp) {
		result = result.clamp();
	}

	r_visited.erase(instance_id);
	return result;
}

void ColorRoleTransform::_on_lerp_transform_changed() {
	emit_changed();
}

void ColorRoleTransform::set_color_role(ColorRole p_color_role) {
	if (color_role == p_color_role) {
		return;
	}
	color_role = p_color_role;
	emit_changed();
}

ColorRole ColorRoleTransform::get_color_role() const {
	return color_role;
}

void ColorRoleTransform::set_static_color(const Color &p_color) {
	if (static_color == p_color) {
		return;
	}
	static_color = p_color;
	emit_changed();
}

Color ColorRoleTransform::get_static_color() const {
	return static_color;
}

void ColorRoleTransform::set_color_scale(const Color &p_color_scale) {
	if (color_scale == p_color_scale) {
		return;
	}
	color_scale = p_color_scale;
	emit_changed();
}

Color ColorRoleTransform::get_color_scale() const {
	return color_scale;
}

void ColorRoleTransform::set_inverted(bool p_inverted) {
	if (inverted == p_inverted) {
		return;
	}
	inverted = p_inverted;
	emit_changed();
}

bool ColorRoleTransform::is_inverted() const {
	return inverted;
}

void ColorRoleTransform::set_darkened_amount(float p_amount) {
	p_amount = CLAMP(p_amount, 0.0f, 1.0f);
	if (Math::is_equal_approx(darkened_amount, p_amount)) {
		return;
	}
	darkened_amount = p_amount;
	emit_changed();
}

float ColorRoleTransform::get_darkened_amount() const {
	return darkened_amount;
}

void ColorRoleTransform::set_lightened_amount(float p_amount) {
	p_amount = CLAMP(p_amount, 0.0f, 1.0f);
	if (Math::is_equal_approx(lightened_amount, p_amount)) {
		return;
	}
	lightened_amount = p_amount;
	emit_changed();
}

float ColorRoleTransform::get_lightened_amount() const {
	return lightened_amount;
}

void ColorRoleTransform::set_lerp_weight(float p_weight) {
	p_weight = CLAMP(p_weight, 0.0f, 1.0f);
	if (Math::is_equal_approx(lerp_weight, p_weight)) {
		return;
	}
	lerp_weight = p_weight;
	emit_changed();
}

float ColorRoleTransform::get_lerp_weight() const {
	return lerp_weight;
}

void ColorRoleTransform::set_lerp_color_role_enabled(bool p_enabled) {
	if (lerp_color_role_enabled == p_enabled) {
		return;
	}
	lerp_color_role_enabled = p_enabled;
	emit_changed();
}

bool ColorRoleTransform::is_lerp_color_role_enabled() const {
	return lerp_color_role_enabled;
}

void ColorRoleTransform::set_lerp_color_role(ColorRole p_color_role) {
	if (lerp_color_role == p_color_role) {
		return;
	}
	lerp_color_role = p_color_role;
	emit_changed();
}

ColorRole ColorRoleTransform::get_lerp_color_role() const {
	return lerp_color_role;
}

void ColorRoleTransform::set_lerp_color(const Color &p_color) {
	if (lerp_color == p_color) {
		return;
	}
	lerp_color = p_color;
	emit_changed();
}

Color ColorRoleTransform::get_lerp_color() const {
	return lerp_color;
}

void ColorRoleTransform::set_lerp_transform(const Ref<ColorRoleTransform> &p_transform) {
	ERR_FAIL_COND_MSG(p_transform.ptr() == this, "A ColorRoleTransform cannot reference itself.");
	if (lerp_transform == p_transform) {
		return;
	}
	if (lerp_transform.is_valid()) {
		lerp_transform->disconnect_changed(callable_mp(this, &ColorRoleTransform::_on_lerp_transform_changed));
	}
	lerp_transform = p_transform;
	if (lerp_transform.is_valid()) {
		lerp_transform->connect_changed(callable_mp(this, &ColorRoleTransform::_on_lerp_transform_changed), CONNECT_REFERENCE_COUNTED);
	}
	emit_changed();
}

Ref<ColorRoleTransform> ColorRoleTransform::get_lerp_transform() const {
	return lerp_transform;
}

void ColorRoleTransform::set_clamp(bool p_clamp) {
	if (clamp == p_clamp) {
		return;
	}
	clamp = p_clamp;
	emit_changed();
}

bool ColorRoleTransform::is_clamp_enabled() const {
	return clamp;
}

Color ColorRoleTransform::resolve(const Ref<ColorScheme> &p_color_scheme) const {
	HashSet<ObjectID> visited;
	return _resolve(p_color_scheme, visited);
}

void ColorRoleTransform::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_color_role", "color_role"), &ColorRoleTransform::set_color_role);
	ClassDB::bind_method(D_METHOD("get_color_role"), &ColorRoleTransform::get_color_role);
	ClassDB::bind_method(D_METHOD("set_static_color", "color"), &ColorRoleTransform::set_static_color);
	ClassDB::bind_method(D_METHOD("get_static_color"), &ColorRoleTransform::get_static_color);
	ClassDB::bind_method(D_METHOD("set_color_scale", "scale"), &ColorRoleTransform::set_color_scale);
	ClassDB::bind_method(D_METHOD("get_color_scale"), &ColorRoleTransform::get_color_scale);
	ClassDB::bind_method(D_METHOD("set_inverted", "inverted"), &ColorRoleTransform::set_inverted);
	ClassDB::bind_method(D_METHOD("is_inverted"), &ColorRoleTransform::is_inverted);
	ClassDB::bind_method(D_METHOD("set_darkened_amount", "amount"), &ColorRoleTransform::set_darkened_amount);
	ClassDB::bind_method(D_METHOD("get_darkened_amount"), &ColorRoleTransform::get_darkened_amount);
	ClassDB::bind_method(D_METHOD("set_lightened_amount", "amount"), &ColorRoleTransform::set_lightened_amount);
	ClassDB::bind_method(D_METHOD("get_lightened_amount"), &ColorRoleTransform::get_lightened_amount);
	ClassDB::bind_method(D_METHOD("set_lerp_weight", "weight"), &ColorRoleTransform::set_lerp_weight);
	ClassDB::bind_method(D_METHOD("get_lerp_weight"), &ColorRoleTransform::get_lerp_weight);
	ClassDB::bind_method(D_METHOD("set_lerp_color_role_enabled", "enabled"), &ColorRoleTransform::set_lerp_color_role_enabled);
	ClassDB::bind_method(D_METHOD("is_lerp_color_role_enabled"), &ColorRoleTransform::is_lerp_color_role_enabled);
	ClassDB::bind_method(D_METHOD("set_lerp_color_role", "color_role"), &ColorRoleTransform::set_lerp_color_role);
	ClassDB::bind_method(D_METHOD("get_lerp_color_role"), &ColorRoleTransform::get_lerp_color_role);
	ClassDB::bind_method(D_METHOD("set_lerp_color", "color"), &ColorRoleTransform::set_lerp_color);
	ClassDB::bind_method(D_METHOD("get_lerp_color"), &ColorRoleTransform::get_lerp_color);
	ClassDB::bind_method(D_METHOD("set_lerp_transform", "transform"), &ColorRoleTransform::set_lerp_transform);
	ClassDB::bind_method(D_METHOD("get_lerp_transform"), &ColorRoleTransform::get_lerp_transform);
	ClassDB::bind_method(D_METHOD("set_clamp", "enabled"), &ColorRoleTransform::set_clamp);
	ClassDB::bind_method(D_METHOD("is_clamp_enabled"), &ColorRoleTransform::is_clamp_enabled);
	ClassDB::bind_method(D_METHOD("resolve", "color_scheme"), &ColorRoleTransform::resolve);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "color_role", PROPERTY_HINT_ENUM, color_role_hint), "set_color_role", "get_color_role");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "static_color"), "set_static_color", "get_static_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color_scale"), "set_color_scale", "get_color_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "inverted"), "set_inverted", "is_inverted");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "darkened_amount", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_darkened_amount", "get_darkened_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lightened_amount", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_lightened_amount", "get_lightened_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lerp_weight", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_lerp_weight", "get_lerp_weight");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lerp_color_role_enabled"), "set_lerp_color_role_enabled", "is_lerp_color_role_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lerp_color_role", PROPERTY_HINT_ENUM, color_role_hint), "set_lerp_color_role", "get_lerp_color_role");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "lerp_color"), "set_lerp_color", "get_lerp_color");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "lerp_transform", PROPERTY_HINT_RESOURCE_TYPE, "ColorRoleTransform"), "set_lerp_transform", "get_lerp_transform");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "clamp"), "set_clamp", "is_clamp_enabled");
}
