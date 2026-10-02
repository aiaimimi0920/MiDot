/**************************************************************************/
/*  color_scheme.cpp                                                      */
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

#include "color_scheme.h"

#include "thirdparty/material-color-utilities/quantize/celebi.h"
#include "thirdparty/material-color-utilities/score/score.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

static bool is_finite_source_color(const Color &p_color) {
	return Math::is_finite(p_color.r) && Math::is_finite(p_color.g) && Math::is_finite(p_color.b) && Math::is_finite(p_color.a);
}

static Color validated_source_color(const Color &p_color) {
	ERR_FAIL_COND_V_MSG(!is_finite_source_color(p_color), Color("#6750a4"), "ColorScheme source color components must be finite.");
	return p_color;
}

static float validated_contrast_level(float p_contrast_level) {
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_contrast_level), 0.0f, "ColorScheme contrast level must be finite.");
	return CLAMP(p_contrast_level, -1.0f, 1.0f);
}

void ColorScheme::_update_color_scheme() {
	if (!dirty) {
		return;
	}

	scheme = _create_scheme_content();
	dirty = false;

	emit_signal(SNAME("updated_color_scheme"));
	notify_property_list_changed();
	emit_changed();
}

void ColorScheme::_source_texture_changed() {
	source_texture_color_cached = false;
	dirty = true;
	_update_color_scheme();
}

material_color_utilities::SchemeContent ColorScheme::_create_scheme_content() {
	Color cur_source_color = source_color;
	if (source_texture.is_valid()) {
		if (source_texture_color_cached) {
			cur_source_color = source_texture_color;
		} else {
			Ref<Image> cur_source_img = source_texture->get_image();
			if (cur_source_img.is_valid() && !cur_source_img->is_empty() && (!cur_source_img->is_compressed() || cur_source_img->decompress() == OK)) {
				std::vector<material_color_utilities::Argb> pixels;
				pixels.reserve(cur_source_img->get_width() * cur_source_img->get_height());
				for (int j = 0; j < cur_source_img->get_height(); j++) {
					for (int i = 0; i < cur_source_img->get_width(); i++) {
						pixels.push_back(cur_source_img->get_pixel(i, j).to_argb32());
					}
				}

				material_color_utilities::QuantizerResult result = material_color_utilities::QuantizeCelebi(pixels, 256);
				material_color_utilities::ScoreOptions cur_options;
				cur_options.desired = 4;
				std::vector<material_color_utilities::Argb> ranked = material_color_utilities::RankedSuggestions(result.color_to_count, cur_options);
				if (!ranked.empty()) {
					cur_source_color = get_color_from_argb(ranked.front());
				}
			} else {
				WARN_PRINT(vformat("ColorScheme could not read or decompress source texture '%s' (%s); using a black seed.", source_texture->get_path(), source_texture->get_class()));
			}
			source_texture_color = cur_source_color;
			source_texture_color_cached = true;
		}
	}

	material_color_utilities::Hct source_color_hct(cur_source_color.to_argb32());
	material_color_utilities::SchemeContent cur_color_scheme_content = material_color_utilities::SchemeContent(source_color_hct, dark, contrast_level);
	return cur_color_scheme_content;
}

void ColorScheme::set_source_color(Color p_source_color) {
	ERR_FAIL_COND_MSG(!is_finite_source_color(p_source_color), "ColorScheme source color components must be finite.");
	if (source_color == p_source_color && source_texture.is_null()) {
		return;
	}

	if (source_texture.is_valid()) {
		source_texture->disconnect_changed(callable_mp(this, &ColorScheme::_source_texture_changed));
	}
	source_color = p_source_color;
	source_texture = Ref<Texture2D>();
	source_texture_color_cached = false;
	dirty = true;
	_update_color_scheme();
}

Color ColorScheme::get_source_color() const {
	return source_color;
}

void ColorScheme::set_source_texture(const Ref<Texture2D> &p_source_texture) {
	if (source_texture == p_source_texture) {
		return;
	}

	if (source_texture.is_valid()) {
		source_texture->disconnect_changed(callable_mp(this, &ColorScheme::_source_texture_changed));
	}

	source_texture = p_source_texture;
	if (source_texture.is_valid()) {
		source_texture->connect_changed(callable_mp(this, &ColorScheme::_source_texture_changed), CONNECT_REFERENCE_COUNTED);
	}
	source_color = Color();
	source_texture_color_cached = false;
	dirty = true;
	_update_color_scheme();
}

Ref<Texture2D> ColorScheme::get_source_texture() const {
	return source_texture;
}

void ColorScheme::set_dark(bool p_dark) {
	if (dark == p_dark) {
		return;
	}

	dark = p_dark;
	dirty = true;
	_update_color_scheme();
}

bool ColorScheme::is_dark() const {
	return dark;
}

void ColorScheme::set_contrast_level(float p_contrast_level) {
	ERR_FAIL_COND_MSG(!Math::is_finite(p_contrast_level), "ColorScheme contrast level must be finite.");
	const float clamped_contrast = CLAMP(p_contrast_level, -1.0f, 1.0f);
	if (Math::is_equal_approx(contrast_level, clamped_contrast)) {
		return;
	}

	contrast_level = clamped_contrast;
	dirty = true;
	_update_color_scheme();
}

float ColorScheme::get_contrast_level() const {
	return contrast_level;
}

Color ColorScheme::get_color(ColorRole p_color_role) {
	ERR_FAIL_INDEX_V_MSG(static_cast<int>(p_color_role), static_cast<int>(ColorRole::ON_TERTIARY_FIXED_VARIANT) + 1, get_source_color_argb(), "Invalid ColorScheme color role.");
#ifdef DEV_ENABLED
	color_query_count++;
#endif
	switch (p_color_role) {
		case ColorRole::PRIMARY_PALETTE_KEY:
			return get_primary_palette_key_color();
		case ColorRole::SECONDARY_PALETTE_KEY:
			return get_secondary_palette_key_color();
		case ColorRole::TERTIARY_PALETTE_KEY:
			return get_tertiary_palette_key_color();
		case ColorRole::NEUTRAL_PALETTE_KEY:
			return get_neutral_palette_key_color();
		case ColorRole::NEUTRAL_VARIANT_PALETTE_KEY:
			return get_neutral_variant_palette_key_color();
		case ColorRole::BACKGROUND:
			return get_background();
		case ColorRole::ON_BACKGROUND:
			return get_on_background();
		case ColorRole::SURFACE:
			return get_surface();
		case ColorRole::SURFACE_DIM:
			return get_surface_dim();
		case ColorRole::SURFACE_BRIGHT:
			return get_surface_bright();
		case ColorRole::SURFACE_CONTAINER_LOWEST:
			return get_surface_container_lowest();
		case ColorRole::SURFACE_CONTAINER_LOW:
			return get_surface_container_low();
		case ColorRole::SURFACE_CONTAINER:
			return get_surface_container();
		case ColorRole::SURFACE_CONTAINER_HIGH:
			return get_surface_container_high();
		case ColorRole::SURFACE_CONTAINER_HIGHEST:
			return get_surface_container_highest();
		case ColorRole::ON_SURFACE:
			return get_on_surface();
		case ColorRole::SURFACE_VARIANT:
			return get_surface_variant();
		case ColorRole::ON_SURFACE_VARIANT:
			return get_on_surface_variant();
		case ColorRole::INVERSE_SURFACE:
			return get_inverse_surface();
		case ColorRole::INVERSE_ON_SURFACE:
			return get_inverse_on_surface();
		case ColorRole::OUTLINE:
			return get_outline();
		case ColorRole::OUTLINE_VARIANT:
			return get_outline_variant();
		case ColorRole::SHADOW:
			return get_shadow();
		case ColorRole::SCRIM:
			return get_scrim();
		case ColorRole::SURFACE_TINT:
			return get_surface_tint();
		case ColorRole::PRIMARY:
			return get_primary();
		case ColorRole::ON_PRIMARY:
			return get_on_primary();
		case ColorRole::PRIMARY_CONTAINER:
			return get_primary_container();
		case ColorRole::ON_PRIMARY_CONTAINER:
			return get_on_primary_container();
		case ColorRole::INVERSE_PRIMARY:
			return get_inverse_primary();
		case ColorRole::SECONDARY:
			return get_secondary();
		case ColorRole::ON_SECONDARY:
			return get_on_secondary();
		case ColorRole::SECONDARY_CONTAINER:
			return get_secondary_container();
		case ColorRole::ON_SECONDARY_CONTAINER:
			return get_on_secondary_container();
		case ColorRole::TERTIARY:
			return get_tertiary();
		case ColorRole::ON_TERTIARY:
			return get_on_tertiary();
		case ColorRole::TERTIARY_CONTAINER:
			return get_tertiary_container();
		case ColorRole::ON_TERTIARY_CONTAINER:
			return get_on_tertiary_container();
		case ColorRole::ERROR:
			return get_error();
		case ColorRole::ON_ERROR:
			return get_on_error();
		case ColorRole::ERROR_CONTAINER:
			return get_error_container();
		case ColorRole::ON_ERROR_CONTAINER:
			return get_on_error_container();
		case ColorRole::PRIMARY_FIXED:
			return get_primary_fixed();
		case ColorRole::PRIMARY_FIXED_DIM:
			return get_primary_fixed_dim();
		case ColorRole::ON_PRIMARY_FIXED:
			return get_on_primary_fixed();
		case ColorRole::ON_PRIMARY_FIXED_VARIANT:
			return get_on_primary_fixed_variant();
		case ColorRole::SECONDARY_FIXED:
			return get_secondary_fixed();
		case ColorRole::SECONDARY_FIXED_DIM:
			return get_secondary_fixed_dim();
		case ColorRole::ON_SECONDARY_FIXED:
			return get_on_secondary_fixed();
		case ColorRole::ON_SECONDARY_FIXED_VARIANT:
			return get_on_secondary_fixed_variant();
		case ColorRole::TERTIARY_FIXED:
			return get_tertiary_fixed();
		case ColorRole::TERTIARY_FIXED_DIM:
			return get_tertiary_fixed_dim();
		case ColorRole::ON_TERTIARY_FIXED:
			return get_on_tertiary_fixed();
		case ColorRole::ON_TERTIARY_FIXED_VARIANT:
			return get_on_tertiary_fixed_variant();
		default:
			return get_source_color_argb();
	}
}

Color ColorScheme::get_color_from_argb(material_color_utilities::Argb p_argb) {
	return Color(material_color_utilities::RedFromInt(p_argb) / 255.0, material_color_utilities::GreenFromInt(p_argb) / 255.0, material_color_utilities::BlueFromInt(p_argb) / 255.0, material_color_utilities::AlphaFromInt(p_argb) / 255.0);
}

Color ColorScheme::get_source_color_argb() {
	return get_color_from_argb(scheme.SourceColorArgb());
}

Color ColorScheme::get_primary_palette_key_color() {
	return get_color_from_argb(scheme.GetPrimaryPaletteKeyColor());
}

Color ColorScheme::get_secondary_palette_key_color() {
	return get_color_from_argb(scheme.GetSecondaryPaletteKeyColor());
}

Color ColorScheme::get_tertiary_palette_key_color() {
	return get_color_from_argb(scheme.GetTertiaryPaletteKeyColor());
}

Color ColorScheme::get_neutral_palette_key_color() {
	return get_color_from_argb(scheme.GetNeutralPaletteKeyColor());
}

Color ColorScheme::get_neutral_variant_palette_key_color() {
	return get_color_from_argb(scheme.GetNeutralVariantPaletteKeyColor());
}

Color ColorScheme::get_background() {
	return get_color_from_argb(scheme.GetBackground());
}

Color ColorScheme::get_on_background() {
	return get_color_from_argb(scheme.GetOnBackground());
}

Color ColorScheme::get_surface() {
	return get_color_from_argb(scheme.GetSurface());
}

Color ColorScheme::get_surface_dim() {
	return get_color_from_argb(scheme.GetSurfaceDim());
}

Color ColorScheme::get_surface_bright() {
	return get_color_from_argb(scheme.GetSurfaceBright());
}

Color ColorScheme::get_surface_container_lowest() {
	return get_color_from_argb(scheme.GetSurfaceContainerLowest());
}

Color ColorScheme::get_surface_container_low() {
	return get_color_from_argb(scheme.GetSurfaceContainerLow());
}

Color ColorScheme::get_surface_container() {
	return get_color_from_argb(scheme.GetSurfaceContainer());
}

Color ColorScheme::get_surface_container_high() {
	return get_color_from_argb(scheme.GetSurfaceContainerHigh());
}

Color ColorScheme::get_surface_container_highest() {
	return get_color_from_argb(scheme.GetSurfaceContainerHighest());
}

Color ColorScheme::get_on_surface() {
	return get_color_from_argb(scheme.GetOnSurface());
}

Color ColorScheme::get_surface_variant() {
	return get_color_from_argb(scheme.GetSurfaceVariant());
}

Color ColorScheme::get_on_surface_variant() {
	return get_color_from_argb(scheme.GetOnSurfaceVariant());
}

Color ColorScheme::get_inverse_surface() {
	return get_color_from_argb(scheme.GetInverseSurface());
}

Color ColorScheme::get_inverse_on_surface() {
	return get_color_from_argb(scheme.GetInverseOnSurface());
}

Color ColorScheme::get_outline() {
	return get_color_from_argb(scheme.GetOutline());
}

Color ColorScheme::get_outline_variant() {
	return get_color_from_argb(scheme.GetOutlineVariant());
}

Color ColorScheme::get_shadow() {
	return get_color_from_argb(scheme.GetShadow());
}

Color ColorScheme::get_scrim() {
	return get_color_from_argb(scheme.GetScrim());
}

Color ColorScheme::get_surface_tint() {
	return get_color_from_argb(scheme.GetSurfaceTint());
}

Color ColorScheme::get_primary() {
	return get_color_from_argb(scheme.GetPrimary());
}

Color ColorScheme::get_on_primary() {
	return get_color_from_argb(scheme.GetOnPrimary());
}

Color ColorScheme::get_primary_container() {
	return get_color_from_argb(scheme.GetPrimaryContainer());
}

Color ColorScheme::get_on_primary_container() {
	return get_color_from_argb(scheme.GetOnPrimaryContainer());
}

Color ColorScheme::get_inverse_primary() {
	return get_color_from_argb(scheme.GetInversePrimary());
}

Color ColorScheme::get_secondary() {
	return get_color_from_argb(scheme.GetSecondary());
}

Color ColorScheme::get_on_secondary() {
	return get_color_from_argb(scheme.GetOnSecondary());
}

Color ColorScheme::get_secondary_container() {
	return get_color_from_argb(scheme.GetSecondaryContainer());
}

Color ColorScheme::get_on_secondary_container() {
	return get_color_from_argb(scheme.GetOnSecondaryContainer());
}

Color ColorScheme::get_tertiary() {
	return get_color_from_argb(scheme.GetTertiary());
}

Color ColorScheme::get_on_tertiary() {
	return get_color_from_argb(scheme.GetOnTertiary());
}

Color ColorScheme::get_tertiary_container() {
	return get_color_from_argb(scheme.GetTertiaryContainer());
}

Color ColorScheme::get_on_tertiary_container() {
	return get_color_from_argb(scheme.GetOnTertiaryContainer());
}

Color ColorScheme::get_error() {
	return get_color_from_argb(scheme.GetError());
}

Color ColorScheme::get_on_error() {
	return get_color_from_argb(scheme.GetOnError());
}

Color ColorScheme::get_error_container() {
	return get_color_from_argb(scheme.GetErrorContainer());
}

Color ColorScheme::get_on_error_container() {
	return get_color_from_argb(scheme.GetOnErrorContainer());
}

Color ColorScheme::get_primary_fixed() {
	return get_color_from_argb(scheme.GetPrimaryFixed());
}

Color ColorScheme::get_primary_fixed_dim() {
	return get_color_from_argb(scheme.GetPrimaryFixedDim());
}

Color ColorScheme::get_on_primary_fixed() {
	return get_color_from_argb(scheme.GetOnPrimaryFixed());
}

Color ColorScheme::get_on_primary_fixed_variant() {
	return get_color_from_argb(scheme.GetOnPrimaryFixedVariant());
}

Color ColorScheme::get_secondary_fixed() {
	return get_color_from_argb(scheme.GetSecondaryFixed());
}

Color ColorScheme::get_secondary_fixed_dim() {
	return get_color_from_argb(scheme.GetSecondaryFixedDim());
}

Color ColorScheme::get_on_secondary_fixed() {
	return get_color_from_argb(scheme.GetOnSecondaryFixed());
}

Color ColorScheme::get_on_secondary_fixed_variant() {
	return get_color_from_argb(scheme.GetOnSecondaryFixedVariant());
}

Color ColorScheme::get_tertiary_fixed() {
	return get_color_from_argb(scheme.GetTertiaryFixed());
}

Color ColorScheme::get_tertiary_fixed_dim() {
	return get_color_from_argb(scheme.GetTertiaryFixedDim());
}

Color ColorScheme::get_on_tertiary_fixed() {
	return get_color_from_argb(scheme.GetOnTertiaryFixed());
}

Color ColorScheme::get_on_tertiary_fixed_variant() {
	return get_color_from_argb(scheme.GetOnTertiaryFixedVariant());
}

void ColorScheme::_bind_methods() {
	ADD_SIGNAL(MethodInfo("updated_color_scheme"));

	ClassDB::bind_method(D_METHOD("set_source_color", "source_color"), &ColorScheme::set_source_color);
	ClassDB::bind_method(D_METHOD("set_source_texture", "source_texture"), &ColorScheme::set_source_texture);
	ClassDB::bind_method(D_METHOD("set_dark", "dark"), &ColorScheme::set_dark);
	ClassDB::bind_method(D_METHOD("set_contrast_level", "contrast_level"), &ColorScheme::set_contrast_level);

	ClassDB::bind_method(D_METHOD("get_source_color"), &ColorScheme::get_source_color);
	ClassDB::bind_method(D_METHOD("get_source_texture"), &ColorScheme::get_source_texture);
	ClassDB::bind_method(D_METHOD("is_dark"), &ColorScheme::is_dark);
	ClassDB::bind_method(D_METHOD("get_contrast_level"), &ColorScheme::get_contrast_level);

	ClassDB::bind_method(D_METHOD("get_color", "color_role"), &ColorScheme::get_color);

	ClassDB::bind_method(D_METHOD("get_primary_palette_key_color"), &ColorScheme::get_primary_palette_key_color);
	ClassDB::bind_method(D_METHOD("get_secondary_palette_key_color"), &ColorScheme::get_secondary_palette_key_color);
	ClassDB::bind_method(D_METHOD("get_tertiary_palette_key_color"), &ColorScheme::get_tertiary_palette_key_color);
	ClassDB::bind_method(D_METHOD("get_neutral_palette_key_color"), &ColorScheme::get_neutral_palette_key_color);
	ClassDB::bind_method(D_METHOD("get_neutral_variant_palette_key_color"), &ColorScheme::get_neutral_variant_palette_key_color);
	ClassDB::bind_method(D_METHOD("get_background"), &ColorScheme::get_background);
	ClassDB::bind_method(D_METHOD("get_on_background"), &ColorScheme::get_on_background);
	ClassDB::bind_method(D_METHOD("get_surface"), &ColorScheme::get_surface);
	ClassDB::bind_method(D_METHOD("get_surface_dim"), &ColorScheme::get_surface_dim);
	ClassDB::bind_method(D_METHOD("get_surface_bright"), &ColorScheme::get_surface_bright);
	ClassDB::bind_method(D_METHOD("get_surface_container_lowest"), &ColorScheme::get_surface_container_lowest);
	ClassDB::bind_method(D_METHOD("get_surface_container_low"), &ColorScheme::get_surface_container_low);
	ClassDB::bind_method(D_METHOD("get_surface_container"), &ColorScheme::get_surface_container);
	ClassDB::bind_method(D_METHOD("get_surface_container_high"), &ColorScheme::get_surface_container_high);
	ClassDB::bind_method(D_METHOD("get_surface_container_highest"), &ColorScheme::get_surface_container_highest);
	ClassDB::bind_method(D_METHOD("get_on_surface"), &ColorScheme::get_on_surface);
	ClassDB::bind_method(D_METHOD("get_surface_variant"), &ColorScheme::get_surface_variant);
	ClassDB::bind_method(D_METHOD("get_on_surface_variant"), &ColorScheme::get_on_surface_variant);
	ClassDB::bind_method(D_METHOD("get_inverse_surface"), &ColorScheme::get_inverse_surface);
	ClassDB::bind_method(D_METHOD("get_inverse_on_surface"), &ColorScheme::get_inverse_on_surface);
	ClassDB::bind_method(D_METHOD("get_outline"), &ColorScheme::get_outline);
	ClassDB::bind_method(D_METHOD("get_outline_variant"), &ColorScheme::get_outline_variant);
	ClassDB::bind_method(D_METHOD("get_shadow"), &ColorScheme::get_shadow);
	ClassDB::bind_method(D_METHOD("get_scrim"), &ColorScheme::get_scrim);
	ClassDB::bind_method(D_METHOD("get_surface_tint"), &ColorScheme::get_surface_tint);
	ClassDB::bind_method(D_METHOD("get_primary"), &ColorScheme::get_primary);
	ClassDB::bind_method(D_METHOD("get_on_primary"), &ColorScheme::get_on_primary);
	ClassDB::bind_method(D_METHOD("get_primary_container"), &ColorScheme::get_primary_container);
	ClassDB::bind_method(D_METHOD("get_on_primary_container"), &ColorScheme::get_on_primary_container);
	ClassDB::bind_method(D_METHOD("get_inverse_primary"), &ColorScheme::get_inverse_primary);
	ClassDB::bind_method(D_METHOD("get_secondary"), &ColorScheme::get_secondary);
	ClassDB::bind_method(D_METHOD("get_on_secondary"), &ColorScheme::get_on_secondary);
	ClassDB::bind_method(D_METHOD("get_secondary_container"), &ColorScheme::get_secondary_container);
	ClassDB::bind_method(D_METHOD("get_on_secondary_container"), &ColorScheme::get_on_secondary_container);
	ClassDB::bind_method(D_METHOD("get_tertiary"), &ColorScheme::get_tertiary);
	ClassDB::bind_method(D_METHOD("get_on_tertiary"), &ColorScheme::get_on_tertiary);
	ClassDB::bind_method(D_METHOD("get_tertiary_container"), &ColorScheme::get_tertiary_container);
	ClassDB::bind_method(D_METHOD("get_on_tertiary_container"), &ColorScheme::get_on_tertiary_container);
	ClassDB::bind_method(D_METHOD("get_error"), &ColorScheme::get_error);
	ClassDB::bind_method(D_METHOD("get_on_error"), &ColorScheme::get_on_error);
	ClassDB::bind_method(D_METHOD("get_error_container"), &ColorScheme::get_error_container);
	ClassDB::bind_method(D_METHOD("get_on_error_container"), &ColorScheme::get_on_error_container);
	ClassDB::bind_method(D_METHOD("get_primary_fixed"), &ColorScheme::get_primary_fixed);
	ClassDB::bind_method(D_METHOD("get_primary_fixed_dim"), &ColorScheme::get_primary_fixed_dim);
	ClassDB::bind_method(D_METHOD("get_on_primary_fixed"), &ColorScheme::get_on_primary_fixed);
	ClassDB::bind_method(D_METHOD("get_on_primary_fixed_variant"), &ColorScheme::get_on_primary_fixed_variant);
	ClassDB::bind_method(D_METHOD("get_secondary_fixed"), &ColorScheme::get_secondary_fixed);
	ClassDB::bind_method(D_METHOD("get_secondary_fixed_dim"), &ColorScheme::get_secondary_fixed_dim);
	ClassDB::bind_method(D_METHOD("get_on_secondary_fixed"), &ColorScheme::get_on_secondary_fixed);
	ClassDB::bind_method(D_METHOD("get_on_secondary_fixed_variant"), &ColorScheme::get_on_secondary_fixed_variant);
	ClassDB::bind_method(D_METHOD("get_tertiary_fixed"), &ColorScheme::get_tertiary_fixed);
	ClassDB::bind_method(D_METHOD("get_tertiary_fixed_dim"), &ColorScheme::get_tertiary_fixed_dim);
	ClassDB::bind_method(D_METHOD("get_on_tertiary_fixed"), &ColorScheme::get_on_tertiary_fixed);
	ClassDB::bind_method(D_METHOD("get_on_tertiary_fixed_variant"), &ColorScheme::get_on_tertiary_fixed_variant);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "source_color"), "set_source_color", "get_source_color");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "source_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_source_texture", "get_source_texture");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dark"), "set_dark", "is_dark");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "contrast_level", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_contrast_level", "get_contrast_level");
}

ColorScheme::ColorScheme() :
		scheme(_create_scheme_content()) {
}

ColorScheme::ColorScheme(Color p_source_color, bool p_dark, float p_contrast_level) :
		source_color(validated_source_color(p_source_color)),
		dark(p_dark),
		contrast_level(validated_contrast_level(p_contrast_level)),
		scheme(_create_scheme_content()) {
}

ColorScheme::ColorScheme(const Ref<Texture2D> &p_source_texture, bool p_dark, float p_contrast_level) :
		source_color(Color()),
		source_texture(p_source_texture),
		dark(p_dark),
		contrast_level(validated_contrast_level(p_contrast_level)),
		scheme(_create_scheme_content()) {
	if (source_texture.is_valid()) {
		source_texture->connect_changed(callable_mp(this, &ColorScheme::_source_texture_changed), CONNECT_REFERENCE_COUNTED);
	}
}
