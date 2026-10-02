/**************************************************************************/
/*  gif_exporter.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                          */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
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

#include "gif_exporter.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"

static uint8_t _quantize_channel(float p_value, int p_levels) {
	if (p_levels <= 1) {
		return 0;
	}
	const int level = CLAMP(int(Math::round(CLAMP(p_value, 0.0f, 255.0f) * (p_levels - 1) / 255.0f)), 0, p_levels - 1);
	return uint8_t(level * 255 / (p_levels - 1));
}

Ref<Image> GifExporter::_prepare_frame(const Ref<Image> &p_frame, const Color &p_background_color, int p_bit_depth, bool p_dither) const {
	ERR_FAIL_COND_V(p_frame.is_null() || p_frame->is_empty(), Ref<Image>());
	ERR_FAIL_COND_V(p_frame->get_width() != width || p_frame->get_height() != height, Ref<Image>());

	Ref<Image> source = p_frame->duplicate();
	ERR_FAIL_COND_V(source.is_null(), Ref<Image>());
	if (source->is_compressed()) {
		ERR_FAIL_COND_V(source->decompress() != OK, Ref<Image>());
	}
	source->convert(Image::FORMAT_RGBA8);

	const PackedByteArray source_data = source->get_data();
	PackedByteArray output_data;
	output_data.resize(source_data.size());

	const int red_bits = (p_bit_depth + 2) / 3;
	const int green_bits = (p_bit_depth + 1) / 3;
	const int blue_bits = p_bit_depth / 3;
	const int red_levels = 1 << red_bits;
	const int green_levels = 1 << green_bits;
	const int blue_levels = 1 << blue_bits;

	Vector<float> red_error;
	Vector<float> green_error;
	Vector<float> blue_error;
	Vector<float> next_red_error;
	Vector<float> next_green_error;
	Vector<float> next_blue_error;
	if (p_dither) {
		const int error_width = width + 2;
		red_error.resize(error_width);
		green_error.resize(error_width);
		blue_error.resize(error_width);
		next_red_error.resize(error_width);
		next_green_error.resize(error_width);
		next_blue_error.resize(error_width);
		red_error.fill(0.0f);
		green_error.fill(0.0f);
		blue_error.fill(0.0f);
		next_red_error.fill(0.0f);
		next_green_error.fill(0.0f);
		next_blue_error.fill(0.0f);
	}

	const uint8_t *src = source_data.ptr();
	uint8_t *dst = output_data.ptrw();
	for (int y = 0; y < height; y++) {
		if (p_dither) {
			next_red_error.fill(0.0f);
			next_green_error.fill(0.0f);
			next_blue_error.fill(0.0f);
		}

		for (int x = 0; x < width; x++) {
			const int index = (y * width + x) * 4;
			const int alpha = src[index + 3];
			float red = (src[index] * alpha + p_background_color.get_r8() * (255 - alpha) + 127) / 255.0f;
			float green = (src[index + 1] * alpha + p_background_color.get_g8() * (255 - alpha) + 127) / 255.0f;
			float blue = (src[index + 2] * alpha + p_background_color.get_b8() * (255 - alpha) + 127) / 255.0f;
			if (p_dither) {
				red += red_error[x + 1];
				green += green_error[x + 1];
				blue += blue_error[x + 1];
			}

			const uint8_t quantized_red = _quantize_channel(red, red_levels);
			const uint8_t quantized_green = _quantize_channel(green, green_levels);
			const uint8_t quantized_blue = _quantize_channel(blue, blue_levels);
			dst[index] = quantized_red;
			dst[index + 1] = quantized_green;
			dst[index + 2] = quantized_blue;
			dst[index + 3] = 255;

			if (p_dither) {
				const float red_diff = red - quantized_red;
				const float green_diff = green - quantized_green;
				const float blue_diff = blue - quantized_blue;
				red_error.write[x + 2] += red_diff * 7.0f / 16.0f;
				green_error.write[x + 2] += green_diff * 7.0f / 16.0f;
				blue_error.write[x + 2] += blue_diff * 7.0f / 16.0f;
				next_red_error.write[x] += red_diff * 3.0f / 16.0f;
				next_green_error.write[x] += green_diff * 3.0f / 16.0f;
				next_blue_error.write[x] += blue_diff * 3.0f / 16.0f;
				next_red_error.write[x + 1] += red_diff * 5.0f / 16.0f;
				next_green_error.write[x + 1] += green_diff * 5.0f / 16.0f;
				next_blue_error.write[x + 1] += blue_diff * 5.0f / 16.0f;
				next_red_error.write[x + 2] += red_diff / 16.0f;
				next_green_error.write[x + 2] += green_diff / 16.0f;
				next_blue_error.write[x + 2] += blue_diff / 16.0f;
			}
		}

		if (p_dither) {
			SWAP(red_error, next_red_error);
			SWAP(green_error, next_green_error);
			SWAP(blue_error, next_blue_error);
		}
	}

	return Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, output_data);
}

void GifExporter::_reset() {
	image_frames.unref();
	output_path.clear();
	width = 0;
	height = 0;
	loop_count = 0;
	max_color_count = 256;
	default_frame_delay = 1.0f;
	exporting = false;
}

bool GifExporter::begin_export(const String &p_file_path, int p_width, int p_height, float p_frame_delay, int p_loop_count, int p_bit_depth, bool p_dither) {
	(void)p_dither; // Legacy GifBegin accepted this option but applied dithering per frame.
	ERR_FAIL_COND_V_MSG(exporting, false, "A GIF export is already active.");
	ERR_FAIL_COND_V_MSG(p_file_path.is_empty(), false, "GIF export path must not be empty.");
	ERR_FAIL_COND_V_MSG(p_width <= 0 || p_height <= 0 || p_width > Image::MAX_WIDTH || p_height > Image::MAX_HEIGHT, false, "GIF dimensions are invalid.");
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_frame_delay) || p_frame_delay < 0.0f, false, "GIF frame delay must be non-negative.");
	ERR_FAIL_COND_V_MSG(p_loop_count < 0 || p_loop_count > UINT16_MAX, false, "GIF loop count must be between 0 and 65535.");
	ERR_FAIL_COND_V_MSG(p_bit_depth < 1 || p_bit_depth > 8, false, "GIF bit depth must be between 1 and 8.");

	image_frames.instantiate();
	output_path = ProjectSettings::get_singleton()->globalize_path(p_file_path);
	width = p_width;
	height = p_height;
	loop_count = p_loop_count;
	max_color_count = 1 << p_bit_depth;
	default_frame_delay = p_frame_delay;
	exporting = true;
	return true;
}

bool GifExporter::write_frame(const Ref<Image> &p_frame, const Color &p_background_color, float p_frame_delay, int p_bit_depth, bool p_dither) {
	ERR_FAIL_COND_V_MSG(!exporting, false, "No GIF export is active.");
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_frame_delay) || p_frame_delay < 0.0f, false, "GIF frame delay must be non-negative.");
	ERR_FAIL_COND_V_MSG(p_bit_depth < 1 || p_bit_depth > 8, false, "GIF bit depth must be between 1 and 8.");

	Ref<Image> prepared_frame = _prepare_frame(p_frame, p_background_color, p_bit_depth, p_dither);
	ERR_FAIL_COND_V_MSG(prepared_frame.is_null(), false, "GIF frame must be a valid image matching the export dimensions.");

	const float delay_hundredths = p_frame_delay > 0.0f ? p_frame_delay : default_frame_delay;
	const float duration_seconds = MAX(delay_hundredths, 1.0f) / 100.0f;
	image_frames->add_frame(prepared_frame, duration_seconds);
	max_color_count = MAX(max_color_count, 1 << p_bit_depth);
	return true;
}

bool GifExporter::end_export() {
	ERR_FAIL_COND_V_MSG(!exporting, false, "No GIF export is active.");
	ERR_FAIL_COND_V_MSG(image_frames->get_frame_count() == 0, false, "Cannot finish a GIF export without frames.");

	const Error error = image_frames->save_gif(output_path, max_color_count, loop_count);
	if (error != OK) {
		return false;
	}
	_reset();
	return true;
}

void GifExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("begin_export", "file_path", "width", "height", "frame_delay", "loop_count", "bit_depth", "dither"), &GifExporter::begin_export, DEFVAL(0), DEFVAL(8), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("write_frame", "frame", "background_color", "frame_delay", "bit_depth", "dither"), &GifExporter::write_frame, DEFVAL(8), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("end_export"), &GifExporter::end_export);
}

GifExporter::~GifExporter() {
	if (exporting && image_frames.is_valid() && image_frames->get_frame_count() > 0) {
		end_export();
	}
}
