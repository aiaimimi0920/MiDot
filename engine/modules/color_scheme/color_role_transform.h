/**************************************************************************/
/*  color_role_transform.h                                                */
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

#pragma once

#include "color_role.h"

#include "core/io/resource.h"
#include "core/templates/hash_set.h"

class ColorScheme;

class ColorRoleTransform : public Resource {
	GDCLASS(ColorRoleTransform, Resource);

	ColorRole color_role = ColorRole::STATIC;
	Color static_color = Color(1, 1, 1, 1);
	Color color_scale = Color(1, 1, 1, 1);
	bool inverted = false;
	float darkened_amount = 0.0f;
	float lightened_amount = 0.0f;
	float lerp_weight = 0.0f;
	bool lerp_color_role_enabled = false;
	ColorRole lerp_color_role = ColorRole::STATIC;
	Color lerp_color = Color(1, 1, 1, 1);
	Ref<ColorRoleTransform> lerp_transform;
	bool clamp = false;

	Color _resolve(const Ref<ColorScheme> &p_color_scheme, HashSet<ObjectID> &r_visited) const;
	void _on_lerp_transform_changed();

protected:
	static void _bind_methods();

public:
	void set_color_role(ColorRole p_color_role);
	ColorRole get_color_role() const;

	void set_static_color(const Color &p_color);
	Color get_static_color() const;

	void set_color_scale(const Color &p_color_scale);
	Color get_color_scale() const;

	void set_inverted(bool p_inverted);
	bool is_inverted() const;

	void set_darkened_amount(float p_amount);
	float get_darkened_amount() const;

	void set_lightened_amount(float p_amount);
	float get_lightened_amount() const;

	void set_lerp_weight(float p_weight);
	float get_lerp_weight() const;

	void set_lerp_color_role_enabled(bool p_enabled);
	bool is_lerp_color_role_enabled() const;

	void set_lerp_color_role(ColorRole p_color_role);
	ColorRole get_lerp_color_role() const;

	void set_lerp_color(const Color &p_color);
	Color get_lerp_color() const;

	void set_lerp_transform(const Ref<ColorRoleTransform> &p_transform);
	Ref<ColorRoleTransform> get_lerp_transform() const;

	void set_clamp(bool p_clamp);
	bool is_clamp_enabled() const;

	Color resolve(const Ref<ColorScheme> &p_color_scheme) const;
};
