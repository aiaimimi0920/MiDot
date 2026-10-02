/**************************************************************************/
/*  check_button.cpp                                                      */
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

#include "check_button.h"

#include "scene/theme/theme_db.h"
#include "servers/display/accessibility_server.h"

int CheckButton::_get_text_icon_size() const {
	int font_size = theme_cache.text_icon_font_size;
	if (font_size <= 0) {
		font_size = get_theme_default_font_size();
	}
	return MAX(0, font_size);
}

bool CheckButton::_has_state_text() const {
	return !theme_cache.text_checked.is_empty() || !theme_cache.text_unchecked.is_empty() ||
			!theme_cache.text_checked_disabled.is_empty() || !theme_cache.text_unchecked_disabled.is_empty() ||
			!theme_cache.text_checked_mirrored.is_empty() || !theme_cache.text_unchecked_mirrored.is_empty() ||
			!theme_cache.text_checked_disabled_mirrored.is_empty() || !theme_cache.text_unchecked_disabled_mirrored.is_empty();
}

String CheckButton::_get_state_text(bool p_pressed) const {
	if (is_layout_rtl()) {
		if (is_disabled()) {
			return p_pressed ? theme_cache.text_checked_disabled_mirrored : theme_cache.text_unchecked_disabled_mirrored;
		}
		return p_pressed ? theme_cache.text_checked_mirrored : theme_cache.text_unchecked_mirrored;
	}
	if (is_disabled()) {
		return p_pressed ? theme_cache.text_checked_disabled : theme_cache.text_unchecked_disabled;
	}
	return p_pressed ? theme_cache.text_checked : theme_cache.text_unchecked;
}

Color CheckButton::_get_state_text_color(bool p_pressed) const {
	if (is_layout_rtl()) {
		if (is_disabled()) {
			return p_pressed ? theme_cache.text_checked_disabled_mirrored_color : theme_cache.text_unchecked_disabled_mirrored_color;
		}
		return p_pressed ? theme_cache.text_checked_mirrored_color : theme_cache.text_unchecked_mirrored_color;
	}
	if (is_disabled()) {
		return p_pressed ? theme_cache.text_checked_disabled_color : theme_cache.text_unchecked_disabled_color;
	}
	return p_pressed ? theme_cache.text_checked_color : theme_cache.text_unchecked_color;
}

void CheckButton::_draw_state_text(const String &p_text, const Color &p_color, const Rect2 &p_rect) const {
	if (p_text.is_empty() || theme_cache.text_icon_font.is_null() || !p_rect.has_area()) {
		return;
	}

	const int font_size = MIN(_get_text_icon_size(), (int)Math::floor(MIN(p_rect.size.x, p_rect.size.y)));
	if (font_size <= 0) {
		return;
	}
	const float font_height = theme_cache.text_icon_font->get_height(font_size);
	const Point2 baseline(p_rect.position.x, p_rect.position.y + (p_rect.size.y - font_height) / 2.0f + theme_cache.text_icon_font->get_ascent(font_size));
	draw_string(theme_cache.text_icon_font, baseline, atr(p_text), HORIZONTAL_ALIGNMENT_CENTER, p_rect.size.x, font_size, p_color);
}

Size2 CheckButton::get_icon_size() const {
	Ref<Texture2D> on_tex;
	Ref<Texture2D> off_tex;

	if (is_layout_rtl()) {
		if (is_disabled()) {
			on_tex = theme_cache.checked_disabled_mirrored;
			off_tex = theme_cache.unchecked_disabled_mirrored;
		} else {
			on_tex = theme_cache.checked_mirrored;
			off_tex = theme_cache.unchecked_mirrored;
		}
	} else {
		if (is_disabled()) {
			on_tex = theme_cache.checked_disabled;
			off_tex = theme_cache.unchecked_disabled;
		} else {
			on_tex = theme_cache.checked;
			off_tex = theme_cache.unchecked;
		}
	}

	Size2 tex_size = Size2(0, 0);
	if (on_tex.is_valid()) {
		tex_size = on_tex->get_size();
	}
	if (off_tex.is_valid()) {
		tex_size = tex_size.max(off_tex->get_size());
	}
	if (theme_cache.text_icon_font.is_valid() && _has_state_text()) {
		const float text_icon_size = _get_text_icon_size();
		tex_size = tex_size.max(Size2(text_icon_size, text_icon_size));
	}

	return _fit_icon_size(tex_size);
}

Size2 CheckButton::get_minimum_size() const {
	Size2 minsize = Button::get_minimum_size();
	const Size2 tex_size = get_icon_size();
	if (tex_size.width > 0 || tex_size.height > 0) {
		const Size2 padding = _get_largest_stylebox_size();
		Size2 content_size = minsize - padding;
		if (content_size.width > 0 && tex_size.width > 0) {
			content_size.width += MAX(0, theme_cache.h_separation);
		}
		content_size.width += tex_size.width;
		content_size.height = MAX(content_size.height, tex_size.height);

		minsize = content_size + padding;
	}

	return minsize;
}

void CheckButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ACCESSIBILITY_UPDATE: {
			RID ae = get_accessibility_element();
			ERR_FAIL_COND(ae.is_null());

			AccessibilityServer::get_singleton()->update_set_role(ae, AccessibilityServerEnums::AccessibilityRole::ROLE_CHECK_BUTTON);
		} break;

		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			if (is_layout_rtl()) {
				_set_internal_margin(SIDE_LEFT, get_icon_size().width);
				_set_internal_margin(SIDE_RIGHT, 0.f);
			} else {
				_set_internal_margin(SIDE_LEFT, 0.f);
				_set_internal_margin(SIDE_RIGHT, get_icon_size().width);
			}
		} break;

		case NOTIFICATION_DRAW: {
			RID ci = get_canvas_item();
			bool rtl = is_layout_rtl();

			Ref<Texture2D> on_tex;
			Ref<Texture2D> off_tex;

			if (rtl) {
				if (is_disabled()) {
					on_tex = theme_cache.checked_disabled_mirrored;
					off_tex = theme_cache.unchecked_disabled_mirrored;
				} else {
					on_tex = theme_cache.checked_mirrored;
					off_tex = theme_cache.unchecked_mirrored;
				}
			} else {
				if (is_disabled()) {
					on_tex = theme_cache.checked_disabled;
					off_tex = theme_cache.unchecked_disabled;
				} else {
					on_tex = theme_cache.checked;
					off_tex = theme_cache.unchecked;
				}
			}

			Vector2 ofs;
			Size2 tex_size = get_icon_size();

			if (rtl) {
				ofs.x = theme_cache.normal_style->get_margin(SIDE_LEFT);
			} else {
				ofs.x = get_size().width - (tex_size.width + theme_cache.normal_style->get_margin(SIDE_RIGHT));
			}
			ofs.y = (get_size().height - tex_size.height) / 2 + theme_cache.check_v_offset;

			const bool pressed = is_pressed();
			const String state_text = _get_state_text(pressed);
			if (!state_text.is_empty() && theme_cache.text_icon_font.is_valid()) {
				_draw_state_text(state_text, _get_state_text_color(pressed), Rect2(ofs, tex_size));
			} else if (pressed && on_tex.is_valid()) {
				on_tex->draw_rect(ci, Rect2(ofs, _fit_icon_size(on_tex->get_size())), false, theme_cache.button_checked_color);
			} else if (!pressed && off_tex.is_valid()) {
				off_tex->draw_rect(ci, Rect2(ofs, _fit_icon_size(off_tex->get_size())), false, theme_cache.button_unchecked_color);
			}
		} break;
	}
}

void CheckButton::_bind_methods() {
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, CheckButton, h_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, CheckButton, check_v_offset);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, CheckButton, normal_style, "normal");

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CheckButton, checked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CheckButton, unchecked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CheckButton, checked_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CheckButton, unchecked_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CheckButton, checked_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CheckButton, unchecked_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CheckButton, checked_disabled_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CheckButton, unchecked_disabled_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, CheckButton, text_icon_font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, CheckButton, text_icon_font_size);

	BIND_THEME_ITEM(Theme::DATA_TYPE_STRING, CheckButton, text_checked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STRING, CheckButton, text_unchecked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STRING, CheckButton, text_checked_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STRING, CheckButton, text_unchecked_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STRING, CheckButton, text_checked_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STRING, CheckButton, text_unchecked_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STRING, CheckButton, text_checked_disabled_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STRING, CheckButton, text_unchecked_disabled_mirrored);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, button_checked_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, button_unchecked_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, text_checked_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, text_unchecked_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, text_checked_disabled_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, text_unchecked_disabled_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, text_checked_mirrored_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, text_unchecked_mirrored_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, text_checked_disabled_mirrored_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CheckButton, text_unchecked_disabled_mirrored_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR_ROLE, CheckButton, text_checked_color_role);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR_ROLE, CheckButton, text_unchecked_color_role);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR_ROLE, CheckButton, text_checked_disabled_color_role);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR_ROLE, CheckButton, text_unchecked_disabled_color_role);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR_ROLE, CheckButton, text_checked_mirrored_color_role);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR_ROLE, CheckButton, text_unchecked_mirrored_color_role);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR_ROLE, CheckButton, text_checked_disabled_mirrored_color_role);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR_ROLE, CheckButton, text_unchecked_disabled_mirrored_color_role);
}

CheckButton::CheckButton(const String &p_text) :
		Button(p_text) {
	set_toggle_mode(true);

	set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);

	if (is_layout_rtl()) {
		_set_internal_margin(SIDE_LEFT, get_icon_size().width);
	} else {
		_set_internal_margin(SIDE_RIGHT, get_icon_size().width);
	}
}

CheckButton::~CheckButton() {
}
