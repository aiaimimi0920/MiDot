/**************************************************************************/
/*  color_scheme.h                                                 */
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

#pragma once

#include "color_role.h"
#include "thirdparty/material-color-utilities/scheme/scheme_content.h"
#include "thirdparty/material-color-utilities/utils/utils.h"

#include "core/io/resource.h"
#include "scene/resources/texture.h"

class ColorScheme : public Resource {
	GDCLASS(ColorScheme, Resource);

private:
	Color source_color = Color("#6750a4");
	Ref<Texture2D> source_texture;
	Color source_texture_color;
	bool source_texture_color_cached = false;
	bool dark = false;
	float contrast_level = 0.0f;
	bool dirty = false;
	material_color_utilities::SchemeContent scheme;

#ifdef DEV_ENABLED
	uint64_t color_query_count = 0;
#endif

	void _update_color_scheme();
	void _source_texture_changed();
	material_color_utilities::SchemeContent _create_scheme_content();

protected:
	static void _bind_methods();

public:
	void set_source_color(Color p_source_color);
	Color get_source_color() const;

	void set_source_texture(const Ref<Texture2D> &p_source_texture);
	Ref<Texture2D> get_source_texture() const;

	void set_dark(bool p_dark);
	bool is_dark() const;

	void set_contrast_level(float p_contrast_level);
	float get_contrast_level() const;

	Color get_color(ColorRole p_color_role);

#ifdef DEV_ENABLED
	uint64_t get_color_query_count() const { return color_query_count; }
#endif

	static Color get_color_from_argb(material_color_utilities::Argb p_argb);
	Color get_source_color_argb();

	Color get_primary_palette_key_color();
	Color get_secondary_palette_key_color();
	Color get_tertiary_palette_key_color();
	Color get_neutral_palette_key_color();
	Color get_neutral_variant_palette_key_color();
	Color get_background();
	Color get_on_background();
	Color get_surface();
	Color get_surface_dim();
	Color get_surface_bright();
	Color get_surface_container_lowest();
	Color get_surface_container_low();
	Color get_surface_container();
	Color get_surface_container_high();
	Color get_surface_container_highest();
	Color get_on_surface();
	Color get_surface_variant();
	Color get_on_surface_variant();
	Color get_inverse_surface();
	Color get_inverse_on_surface();
	Color get_outline();
	Color get_outline_variant();
	Color get_shadow();
	Color get_scrim();
	Color get_surface_tint();
	Color get_primary();
	Color get_on_primary();
	Color get_primary_container();
	Color get_on_primary_container();
	Color get_inverse_primary();
	Color get_secondary();
	Color get_on_secondary();
	Color get_secondary_container();
	Color get_on_secondary_container();
	Color get_tertiary();
	Color get_on_tertiary();
	Color get_tertiary_container();
	Color get_on_tertiary_container();
	Color get_error();
	Color get_on_error();
	Color get_error_container();
	Color get_on_error_container();
	Color get_primary_fixed();
	Color get_primary_fixed_dim();
	Color get_on_primary_fixed();
	Color get_on_primary_fixed_variant();
	Color get_secondary_fixed();
	Color get_secondary_fixed_dim();
	Color get_on_secondary_fixed();
	Color get_on_secondary_fixed_variant();
	Color get_tertiary_fixed();
	Color get_tertiary_fixed_dim();
	Color get_on_tertiary_fixed();
	Color get_on_tertiary_fixed_variant();

	ColorScheme();
	ColorScheme(Color p_source_color, bool p_dark = false, float p_contrast_level = 0.0f);
	ColorScheme(const Ref<Texture2D> &p_source_texture, bool p_dark = false, float p_contrast_level = 0.0f);
};
