/**************************************************************************/
/*  test_color_role_transform.cpp                                         */
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
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                        */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_color_role_transform)

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_COLOR_SCHEME_ENABLED

#include "core/object/class_db.h"

#include "modules/color_scheme/color_role_transform.h"
#include "modules/color_scheme/color_scheme.h"

namespace TestColorRoleTransform {

TEST_CASE("[ColorScheme] ColorRoleTransform resolves transformation pipeline") {
	CHECK(ClassDB::class_exists(SNAME("ColorRoleTransform")));

	Ref<ColorRoleTransform> transform;
	transform.instantiate();
	transform->set_static_color(Color(0.2f, 0.4f, 0.6f, 0.8f));
	CHECK(transform->resolve(Ref<ColorScheme>()).is_equal_approx(Color(0.2f, 0.4f, 0.6f, 0.8f)));

	transform->set_color_scale(Color(2.0f, 0.5f, 1.0f, 0.5f));
	CHECK(transform->resolve(Ref<ColorScheme>()).is_equal_approx(Color(0.4f, 0.2f, 0.6f, 0.4f)));

	transform->set_color_scale(Color(1, 1, 1, 1));
	transform->set_inverted(true);
	CHECK(transform->resolve(Ref<ColorScheme>()).is_equal_approx(Color(0.8f, 0.6f, 0.4f, 0.8f)));

	transform->set_inverted(false);
	transform->set_lerp_color(Color(1, 1, 1, 1));
	transform->set_lerp_weight(0.5f);
	CHECK(transform->resolve(Ref<ColorScheme>()).is_equal_approx(Color(0.6f, 0.7f, 0.8f, 0.9f)));

	transform->set_lerp_weight(0.0f);
	transform->set_color_scale(Color(10, 10, 10, 10));
	transform->set_clamp(true);
	CHECK(transform->resolve(Ref<ColorScheme>()).is_equal_approx(Color(1, 1, 1, 1)));
}

TEST_CASE("[ColorScheme] ColorRoleTransform resolves roles and nested targets") {
	Ref<ColorScheme> scheme;
	scheme.instantiate();

	Ref<ColorRoleTransform> transform;
	transform.instantiate();
	transform->set_color_role(ColorRole::PRIMARY);
	CHECK(transform->resolve(scheme).is_equal_approx(scheme->get_color(ColorRole::PRIMARY)));

	Ref<ColorRoleTransform> target;
	target.instantiate();
	target->set_static_color(Color(1, 0, 0, 1));
	transform->set_color_role(ColorRole::STATIC);
	transform->set_static_color(Color(0, 0, 1, 1));
	transform->set_lerp_transform(target);
	transform->set_lerp_weight(0.25f);
	CHECK(transform->resolve(scheme).is_equal_approx(Color(0.25f, 0, 0.75f, 1)));

	ERR_PRINT_OFF;
	transform->set_lerp_transform(transform);
	ERR_PRINT_ON;
	CHECK(transform->get_lerp_transform() == target);
}

} // namespace TestColorRoleTransform

#endif // MODULE_COLOR_SCHEME_ENABLED
