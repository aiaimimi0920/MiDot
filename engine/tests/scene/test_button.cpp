/**************************************************************************/
/*  test_button.cpp                                                       */
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

TEST_FORCE_LINK(test_button)

#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/check_button.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/style_box.h"
#include "tests/display_server_mock.h"

namespace TestButton {

class TestableButton : public Button {
public:
	void update_theme_cache() {
		_update_theme_item_cache();
	}

	Ref<StyleBox> get_current_state_layer_stylebox() const {
		return _get_current_state_layer_stylebox();
	}
};

class TestableCheckBox : public CheckBox {
public:
	void update_theme_cache() {
		_update_theme_item_cache();
	}

	String get_state_text(bool p_pressed) const {
		return _get_state_text(p_pressed);
	}

	Size2 get_test_icon_size() const {
		return get_icon_size();
	}
};

class TestableCheckButton : public CheckButton {
public:
	void update_theme_cache() {
		_update_theme_item_cache();
	}

	String get_state_text(bool p_pressed) const {
		return _get_state_text(p_pressed);
	}

	Size2 get_test_icon_size() const {
		return get_icon_size();
	}
};

TEST_CASE("[SceneTree][Button] is_hovered()") {
	// Create new button instance.
	Button *button = memnew(Button);
	CHECK(button != nullptr);
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(button);

	// Set up button's size and position.
	button->set_size(Size2i(50, 50));
	button->set_position(Size2i(10, 10));

	// Button should initially be not hovered.
	CHECK(button->is_hovered() == false);

	// Simulate mouse entering the button.
	SEND_GUI_MOUSE_MOTION_EVENT(Point2i(25, 25), MouseButtonMask::NONE, Key::NONE);
	CHECK(button->is_hovered() == true);

	// Simulate mouse exiting the button.
	SEND_GUI_MOUSE_MOTION_EVENT(Point2i(150, 150), MouseButtonMask::NONE, Key::NONE);
	CHECK(button->is_hovered() == false);

	memdelete(button);
}

TEST_CASE("[SceneTree][Button] resolves optional Material state layers") {
	TestableButton *button = memnew(TestableButton);
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(button);
	button->set_size(Size2i(50, 50));

	Ref<StyleBoxEmpty> focus_layer;
	focus_layer.instantiate();
	focus_layer->set_content_margin(SIDE_LEFT, 11.0f);
	Ref<StyleBoxEmpty> pressed_layer;
	pressed_layer.instantiate();
	pressed_layer->set_content_margin(SIDE_LEFT, 22.0f);
	button->add_theme_style_override(SNAME("state_focus_layer"), focus_layer);
	button->add_theme_style_override(SNAME("state_pressed_layer"), pressed_layer);
	button->update_theme_cache();
	CHECK(button->get_theme_stylebox(SNAME("state_focus_layer"))->get_margin(SIDE_LEFT) == doctest::Approx(11.0f));
	CHECK(button->get_theme_stylebox(SNAME("state_pressed_layer"))->get_margin(SIDE_LEFT) == doctest::Approx(22.0f));

	button->grab_focus();
	CHECK(button->get_current_state_layer_stylebox()->get_margin(SIDE_LEFT) == doctest::Approx(11.0f));

	button->set_toggle_mode(true);
	button->set_pressed_no_signal(true);
	CHECK(button->get_current_state_layer_stylebox()->get_margin(SIDE_LEFT) == doctest::Approx(22.0f));

	memdelete(button);
}

TEST_CASE("[SceneTree][Button] uses text glyphs as fallback icons") {
	Button *button = memnew(Button);
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(button);

	const Size2 empty_size = button->get_minimum_size();
	button->set_text_icon("A");
	CHECK(button->get_text_icon() == "A");
	CHECK(button->get_minimum_size().width > empty_size.width);
	CHECK(button->get_minimum_size().height >= empty_size.height);

	button->set_text_icon("");
	button->add_theme_string_override("text_icon", "B");
	CHECK(button->get_text_icon().is_empty());
	CHECK(button->get_theme_string("text_icon") == "B");
	CHECK(button->get_minimum_size().width > empty_size.width);

	memdelete(button);
}

TEST_CASE("[SceneTree][CheckBox] resolves state text glyphs") {
	TestableCheckBox *check_box = memnew(TestableCheckBox);
	SceneTree::get_singleton()->get_root()->add_child(check_box);

	Ref<Font> icon_font = check_box->get_theme_default_font();
	REQUIRE(icon_font.is_valid());
	check_box->add_theme_font_override("text_icon_font", icon_font);
	check_box->add_theme_font_size_override("text_icon_font_size", 48);
	check_box->add_theme_string_override("text_checked", "checked");
	check_box->add_theme_string_override("text_unchecked", "unchecked");
	check_box->add_theme_string_override("text_checked_disabled", "checked-disabled");
	check_box->add_theme_string_override("text_unchecked_disabled", "unchecked-disabled");
	check_box->add_theme_string_override("text_radio_checked", "radio-checked");
	check_box->add_theme_string_override("text_radio_unchecked", "radio-unchecked");
	check_box->add_theme_string_override("text_radio_checked_disabled", "radio-checked-disabled");
	check_box->add_theme_string_override("text_radio_unchecked_disabled", "radio-unchecked-disabled");
	check_box->update_theme_cache();

	CHECK(check_box->get_state_text(true) == "checked");
	CHECK(check_box->get_state_text(false) == "unchecked");
	CHECK(check_box->get_test_icon_size().x >= 48);

	check_box->set_disabled(true);
	CHECK(check_box->get_state_text(true) == "checked-disabled");
	CHECK(check_box->get_state_text(false) == "unchecked-disabled");

	Ref<ButtonGroup> group;
	group.instantiate();
	check_box->set_button_group(group);
	CHECK(check_box->get_state_text(true) == "radio-checked-disabled");
	CHECK(check_box->get_state_text(false) == "radio-unchecked-disabled");

	check_box->set_disabled(false);
	CHECK(check_box->get_state_text(true) == "radio-checked");
	CHECK(check_box->get_state_text(false) == "radio-unchecked");

	memdelete(check_box);
}

TEST_CASE("[SceneTree][CheckButton] resolves mirrored state text glyphs") {
	TestableCheckButton *check_button = memnew(TestableCheckButton);
	SceneTree::get_singleton()->get_root()->add_child(check_button);

	Ref<Font> icon_font = check_button->get_theme_default_font();
	REQUIRE(icon_font.is_valid());
	check_button->add_theme_font_override("text_icon_font", icon_font);
	check_button->add_theme_font_size_override("text_icon_font_size", 48);
	check_button->add_theme_string_override("text_checked", "checked");
	check_button->add_theme_string_override("text_unchecked", "unchecked");
	check_button->add_theme_string_override("text_checked_disabled", "checked-disabled");
	check_button->add_theme_string_override("text_unchecked_disabled", "unchecked-disabled");
	check_button->add_theme_string_override("text_checked_mirrored", "checked-rtl");
	check_button->add_theme_string_override("text_unchecked_mirrored", "unchecked-rtl");
	check_button->add_theme_string_override("text_checked_disabled_mirrored", "checked-disabled-rtl");
	check_button->add_theme_string_override("text_unchecked_disabled_mirrored", "unchecked-disabled-rtl");
	check_button->update_theme_cache();

	CHECK(check_button->get_state_text(true) == "checked");
	CHECK(check_button->get_state_text(false) == "unchecked");
	CHECK(check_button->get_test_icon_size().x >= 48);

	check_button->set_disabled(true);
	CHECK(check_button->get_state_text(true) == "checked-disabled");
	CHECK(check_button->get_state_text(false) == "unchecked-disabled");

	check_button->set_layout_direction(Control::LAYOUT_DIRECTION_RTL);
	CHECK(check_button->get_state_text(true) == "checked-disabled-rtl");
	CHECK(check_button->get_state_text(false) == "unchecked-disabled-rtl");

	check_button->set_disabled(false);
	CHECK(check_button->get_state_text(true) == "checked-rtl");
	CHECK(check_button->get_state_text(false) == "unchecked-rtl");

	memdelete(check_button);
}

} // namespace TestButton
