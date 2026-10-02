/**************************************************************************/
/*  image_frames.cpp                                                      */
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

#include "image_frames.h"

#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/templates/hash_map.h"

#include <gif_lib.h>

LoadImageFramesFunction ImageFrames::load_gif_func = nullptr;

Error ImageFrames::load(const String &p_path, int p_max_frames) {
	clear();
	String ext = p_path.get_extension().to_lower();
	if (ext == "gif") {
		ERR_FAIL_NULL_V_MSG(load_gif_func, ERR_UNAVAILABLE, "GIF loader is not initialized.");
		Ref<ImageFrames> image_frames = Ref<ImageFrames>(this);
		return load_gif_func(image_frames, p_path, p_max_frames);
	} else {
		ERR_PRINT("Unrecognized image: " + p_path);
		return ERR_FILE_UNRECOGNIZED;
	}
}

Error ImageFrames::load_gif_from_buffer(const PackedByteArray &p_data, int p_max_frames) {
	ERR_FAIL_COND_V_MSG(p_data.is_empty(), ERR_INVALID_DATA, "Invalid GIF data");
	clear();
	if (p_data[0] == 'G') {
		ERR_FAIL_NULL_V_MSG(load_gif_func, ERR_UNAVAILABLE, "GIF loader is not initialized.");
		Ref<ImageFrames> image_frames = Ref<ImageFrames>(this);
		return load_gif_func(image_frames, p_data, p_max_frames);
	} else {
		ERR_PRINT("Unrecognized image.");
		return ERR_FILE_UNRECOGNIZED;
	}
}

static int save_gif_func(GifFileType *gif, const GifByteType *data, int length) {
	// gif->UserData is the first parameter passed to EGifOpen.
	FileAccess *fa = (FileAccess *)(gif->UserData);
	return fa->store_buffer((const uint8_t *)data, length) ? length : 0;
}

static int _gif_palette_size(int p_color_count) {
	const int requested_colors = CLAMP(p_color_count, 2, 256);
	int palette_size = 2;
	while (palette_size < requested_colors) {
		palette_size <<= 1;
	}
	return palette_size;
}

static Error _index_gif_frame(const Ref<Image> &p_frame, int p_palette_size, PackedByteArray &r_indices, Vector<GifColorType> &r_colors, bool &r_uses_transparency) {
	ERR_FAIL_COND_V(p_frame.is_null() || p_frame->is_empty(), ERR_INVALID_DATA);
	Ref<Image> rgba = p_frame->duplicate();
	ERR_FAIL_COND_V(rgba.is_null() || rgba->is_empty(), ERR_INVALID_DATA);
	if (rgba->is_compressed()) {
		Error err = rgba->decompress();
		ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot decompress GIF frame before saving.");
	}
	rgba->convert(Image::FORMAT_RGBA8);

	const PackedByteArray data = rgba->get_data();
	const int64_t pixel_count = (int64_t)rgba->get_width() * rgba->get_height();
	ERR_FAIL_COND_V(data.size() != pixel_count * 4, ERR_INVALID_DATA);
	const uint8_t *src = data.ptr();
	r_uses_transparency = false;
	for (int64_t i = 0; i < pixel_count; i++) {
		if (src[i * 4 + 3] < 128) {
			r_uses_transparency = true;
			break;
		}
	}

	r_indices.resize(pixel_count);
	r_colors.resize(p_palette_size);
	for (int i = 0; i < p_palette_size; i++) {
		r_colors.write[i].Red = 0;
		r_colors.write[i].Green = 0;
		r_colors.write[i].Blue = 0;
	}

	const int first_opaque_index = r_uses_transparency ? 1 : 0;
	const int opaque_color_count = p_palette_size - first_opaque_index;
	HashMap<uint32_t, int> exact_colors;
	int next_color_index = first_opaque_index;
	bool exact_palette_fits = true;

	uint8_t *indices = r_indices.ptrw();
	for (int64_t i = 0; i < pixel_count; i++) {
		const uint8_t *pixel = src + i * 4;
		if (r_uses_transparency && pixel[3] < 128) {
			indices[i] = 0;
			continue;
		}

		const uint32_t color_key = (uint32_t(pixel[0]) << 16) | (uint32_t(pixel[1]) << 8) | pixel[2];
		const int *existing_index = exact_colors.getptr(color_key);
		if (existing_index) {
			indices[i] = *existing_index;
			continue;
		}
		if (next_color_index >= p_palette_size) {
			exact_palette_fits = false;
			break;
		}

		exact_colors.insert(color_key, next_color_index);
		r_colors.write[next_color_index].Red = pixel[0];
		r_colors.write[next_color_index].Green = pixel[1];
		r_colors.write[next_color_index].Blue = pixel[2];
		indices[i] = next_color_index++;
	}

	if (exact_palette_fits) {
		return OK;
	}

	// Use a deterministic uniform RGB palette when the exact colors do not fit.
	int quantized_bits = 0;
	while (quantized_bits < 8 && (1 << (quantized_bits + 1)) <= opaque_color_count) {
		quantized_bits++;
	}
	const int red_bits = (quantized_bits + 2) / 3;
	const int green_bits = (quantized_bits + 1) / 3;
	const int blue_bits = quantized_bits / 3;
	const int red_levels = 1 << red_bits;
	const int green_levels = 1 << green_bits;
	const int blue_levels = 1 << blue_bits;

	int palette_index = first_opaque_index;
	for (int red = 0; red < red_levels; red++) {
		for (int green = 0; green < green_levels; green++) {
			for (int blue = 0; blue < blue_levels; blue++) {
				GifColorType &color = r_colors.write[palette_index++];
				color.Red = red_levels == 1 ? 0 : red * 255 / (red_levels - 1);
				color.Green = green_levels == 1 ? 0 : green * 255 / (green_levels - 1);
				color.Blue = blue_levels == 1 ? 0 : blue * 255 / (blue_levels - 1);
			}
		}
	}

	for (int64_t i = 0; i < pixel_count; i++) {
		const uint8_t *pixel = src + i * 4;
		if (r_uses_transparency && pixel[3] < 128) {
			indices[i] = 0;
			continue;
		}
		const int red = red_bits == 0 ? 0 : pixel[0] >> (8 - red_bits);
		const int green = green_bits == 0 ? 0 : pixel[1] >> (8 - green_bits);
		const int blue = blue_bits == 0 ? 0 : pixel[2] >> (8 - blue_bits);
		indices[i] = first_opaque_index + (red * green_levels + green) * blue_levels + blue;
	}

	return OK;
}

static Error _close_gif(GifFileType *p_gif, Error p_result) {
	int close_error = 0;
	if (EGifCloseFile(p_gif, &close_error) == GIF_ERROR && p_result == OK) {
		ERR_PRINT(vformat("EGifCloseFile() failed: %s.", GifErrorString(close_error)));
		return ERR_CANT_CREATE;
	}
	return p_result;
}

Error ImageFrames::save_gif(const String &p_filepath, int p_color_count, int p_loop_count) {
	ERR_FAIL_COND_V_MSG(get_frame_count() == 0, ERR_CANT_CREATE, "ImageFrames must have at least one frame.");
	ERR_FAIL_COND_V_MSG(p_loop_count < 0 || p_loop_count > UINT16_MAX, ERR_INVALID_PARAMETER, "GIF loop count must be between 0 and 65535.");

	// GIF allows to add images of different sizes,
	// but we need to determine the canvas size that could contain all frames.
	const Rect2 rect = get_bounding_rect();
	ERR_FAIL_COND_V_MSG(!rect.has_area(), ERR_CANT_CREATE, "ImageFrames contain uninitialized images.");
	ERR_FAIL_COND_V(rect.size.x > Image::MAX_WIDTH || rect.size.y > Image::MAX_HEIGHT, ERR_OUT_OF_MEMORY);

	Ref<FileAccess> f = FileAccess::open(p_filepath, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, "Error opening file.");

	int error = 0;
	GifFileType *gif = EGifOpen(f.ptr(), save_gif_func, &error);
	if (!gif) {
		ERR_PRINT(vformat("EGifOpen() failed: %s.", GifErrorString(error)));
		return ERR_CANT_CREATE;
	}

	const int width = rect.size.x;
	const int height = rect.size.y;
	if (EGifPutScreenDesc(gif, width, height, 8, 0, nullptr) == GIF_ERROR) {
		return _close_gif(gif, ERR_CANT_CREATE);
	}

	if (get_frame_count() > 1) {
		static const char application_id[] = "NETSCAPE2.0";
		const uint8_t loop_data[3] = { 1, uint8_t(p_loop_count & 0xFF), uint8_t((p_loop_count >> 8) & 0xFF) };
		if (EGifPutExtensionLeader(gif, APPLICATION_EXT_FUNC_CODE) == GIF_ERROR ||
				EGifPutExtensionBlock(gif, 11, application_id) == GIF_ERROR ||
				EGifPutExtensionBlock(gif, 3, loop_data) == GIF_ERROR ||
				EGifPutExtensionTrailer(gif) == GIF_ERROR) {
			return _close_gif(gif, ERR_CANT_CREATE);
		}
	}

	const int palette_size = _gif_palette_size(p_color_count);
	for (int i = 0; i < get_frame_count(); ++i) {
		const Ref<Image> frame = get_frame_image(i);
		const float duration = get_frame_duration(i); // Seconds.
		if (!Math::is_finite(duration) || duration <= 0.0f) {
			return _close_gif(gif, ERR_INVALID_DATA);
		}

		PackedByteArray indices;
		Vector<GifColorType> colors;
		bool uses_transparency = false;
		Error index_error = _index_gif_frame(frame, palette_size, indices, colors, uses_transparency);
		if (index_error != OK) {
			return _close_gif(gif, index_error);
		}

		ColorMapObject *gif_color_map = GifMakeMapObject(palette_size, colors.ptr());
		if (!gif_color_map) {
			return _close_gif(gif, ERR_OUT_OF_MEMORY);
		}

		GraphicsControlBlock gcb;
		gcb.DisposalMode = DISPOSE_BACKGROUND;
		gcb.UserInputFlag = false;
		gcb.DelayTime = CLAMP((int)Math::round(duration * 100.0), 1, 65535);
		gcb.TransparentColor = uses_transparency ? 0 : NO_TRANSPARENT_COLOR;
		GifByteType extension[4];
		EGifGCBToExtension(&gcb, extension);
		if (EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, 4, extension) == GIF_ERROR) {
			GifFreeMapObject(gif_color_map);
			return _close_gif(gif, ERR_CANT_CREATE);
		}

		if (EGifPutImageDesc(gif, 0, 0, frame->get_width(), frame->get_height(), false, gif_color_map) == GIF_ERROR) {
			GifFreeMapObject(gif_color_map);
			return _close_gif(gif, ERR_CANT_CREATE);
		}
		GifFreeMapObject(gif_color_map);

		GifPixelType *raster = indices.ptrw();
		for (int j = 0; j < frame->get_height(); j++) {
			if (EGifPutLine(gif, raster + j * frame->get_width(), frame->get_width()) == GIF_ERROR) {
				return _close_gif(gif, ERR_CANT_CREATE);
			}
		}
	}

	return _close_gif(gif, OK);
}

void ImageFrames::add_frame(const Ref<Image> &p_image, float p_duration) {
	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND(p_image->get_width() == 0);
	ERR_FAIL_COND(p_image->get_height() == 0);
	ERR_FAIL_COND_MSG(!Math::is_finite(p_duration) || p_duration <= 0.0f, "Frame duration must be greater than zero.");

	Frame frame;
	frame.image = p_image;
	frame.duration = p_duration;
	frames.push_back(frame);
}

void ImageFrames::remove_frame(int p_idx) {
	ERR_FAIL_INDEX(p_idx, frames.size());
	frames.remove_at(p_idx);
}

void ImageFrames::set_frame_image(int p_idx, const Ref<Image> &p_image) {
	ERR_FAIL_INDEX(p_idx, frames.size());
	frames.write[p_idx].image = p_image;
}

Ref<Image> ImageFrames::get_frame_image(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, frames.size(), Ref<Image>());
	return frames[p_idx].image;
}

void ImageFrames::set_frame_duration(int p_idx, float p_duration) {
	ERR_FAIL_INDEX(p_idx, frames.size());
	ERR_FAIL_COND_MSG(!Math::is_finite(p_duration) || p_duration <= 0.0f, "Frame duration must be greater than zero.");
	frames.write[p_idx].duration = p_duration;
}

float ImageFrames::get_frame_duration(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, frames.size(), 0);
	return frames[p_idx].duration;
}

Rect2 ImageFrames::get_bounding_rect() const {
	Rect2 rect;
	for (int i = 0; i < frames.size(); ++i) {
		const Ref<Image> &image = frames[i].image;
		ERR_CONTINUE_MSG(image.is_null(), "Uninitialized or invalid image detected, skipping.");
		rect.expand_to(image->get_size());
	}
	return rect;
}

int ImageFrames::get_frame_count() const {
	return frames.size();
}

void ImageFrames::clear() {
	frames.clear();
}

void ImageFrames::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load", "path", "max_frames"), &ImageFrames::load, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("load_gif_from_buffer", "data", "max_frames"), &ImageFrames::load_gif_from_buffer, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("save_gif", "filepath", "color_count", "loop_count"), &ImageFrames::save_gif, DEFVAL(256), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("add_frame", "image", "duration"), &ImageFrames::add_frame, DEFVAL(0.05));
	ClassDB::bind_method(D_METHOD("remove_frame", "index"), &ImageFrames::remove_frame);

	ClassDB::bind_method(D_METHOD("set_frame_image", "index", "image"), &ImageFrames::set_frame_image);
	ClassDB::bind_method(D_METHOD("get_frame_image", "index"), &ImageFrames::get_frame_image);

	ClassDB::bind_method(D_METHOD("set_frame_duration", "index", "duration"), &ImageFrames::set_frame_duration);
	ClassDB::bind_method(D_METHOD("get_frame_duration", "index"), &ImageFrames::get_frame_duration);

	ClassDB::bind_method(D_METHOD("get_bounding_rect"), &ImageFrames::get_bounding_rect);
	ClassDB::bind_method(D_METHOD("get_frame_count"), &ImageFrames::get_frame_count);

	ClassDB::bind_method(D_METHOD("clear"), &ImageFrames::clear);
}
