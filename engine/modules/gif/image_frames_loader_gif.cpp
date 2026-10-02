/**************************************************************************/
/*  image_frames_loader_gif.cpp                                           */
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

#include "image_frames_loader_gif.h"

#include "image_frames.h"

#include "scene/resources/animated_texture.h"
#include "scene/resources/texture.h"

#include <gif_lib.h>

static Error _load_gif(const Ref<ImageFrames> &p_image_frames, const Variant &p_source, int p_max_frames) {
	GifLoader gif;
	if (p_source.get_type() == Variant::STRING) {
		Error err;
		Ref<FileAccess> f = FileAccess::open(p_source, FileAccess::READ, &err);
		if (f.is_null()) {
			ERR_PRINT("Error opening file '" + String(p_source) + "'.");
			return err;
		}
		err = gif.load_from_file_access(p_image_frames, f, p_max_frames);
		return err;
	} else {
		return gif.load_from_buffer(p_image_frames, p_source, p_max_frames);
	}
}

Error ImageFramesLoaderGIF::load_image_frames(const Ref<ImageFrames> &p_image_frames, const Ref<FileAccess> &p_file, int p_max_frames) const {
	GifLoader gif;
	return gif.load_from_file_access(p_image_frames, p_file, p_max_frames);
}

void ImageFramesLoaderGIF::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gif");
}

ImageFramesLoaderGIF::ImageFramesLoaderGIF() {
	ImageFrames::load_gif_func = _load_gif;
}

// Custom function to read the GIF data from a file.
int readFromFile(GifFileType *gif, GifByteType *data, int length) {
	FileAccess *f = (FileAccess *)(gif->UserData); // gif->UserData is the first parameter passed to DGifOpen.
	if (!f || length <= 0) {
		return 0;
	}
	return (int)f->get_buffer(data, length);
}

struct GIFBuffer { // Used to read the GIF data from a buffer.
	const uint8_t *data;
	int size;
	int index;

	GIFBuffer(const PackedByteArray &p_data) {
		data = p_data.ptr();
		size = p_data.size();
		index = 0;
	}
	GIFBuffer(const uint8_t *p_data, int p_size) {
		data = p_data;
		size = p_size;
		index = 0;
	}
};

// Custom function to read the GIF data from a buffer.
int readFromBuffer(GifFileType *gif, GifByteType *data, int length) {
	GIFBuffer *f = (GIFBuffer *)(gif->UserData);
	if (!f || !data || length <= 0 || f->index < 0 || f->index >= f->size) {
		return 0;
	}
	if (length > f->size - f->index) {
		length = f->size - f->index;
	}

	memcpy(data, &f->data[f->index], length);
	f->index += length;
	return length;
}

Error GifLoader::parse_error(Error err, const String &message) {
	ERR_PRINT(message);
	return err;
}

Error GifLoader::gif_error(int err) {
	ERR_PRINT(GifErrorString(err));
	return FAILED;
}

Error GifLoader::_open(void *source, SourceType source_type) {
	ERR_FAIL_COND_V(gif != nullptr, FAILED);
	int err = 0;
	gif = DGifOpen(source, source_type == SourceType::FILE ? readFromFile : readFromBuffer, &err); // Loads the headers of the GIF.
	if (!gif) {
		return gif_error(err);
	}
	return OK;
}

Error GifLoader::_load_frames(const Ref<ImageFrames> &p_image_frames, int p_max_frames) {
	ERR_FAIL_COND_V(gif == nullptr, FAILED);
	ERR_FAIL_COND_V(p_image_frames.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(gif->SWidth <= 0 || gif->SWidth > Image::MAX_WIDTH || gif->SHeight <= 0 || gif->SHeight > Image::MAX_HEIGHT,
			ERR_FILE_CORRUPT, "Invalid GIF canvas dimensions.");

	const int64_t pixel_count = (int64_t)gif->SWidth * gif->SHeight;
	ERR_FAIL_COND_V_MSG(pixel_count > Image::MAX_PIXELS, ERR_OUT_OF_MEMORY, "GIF canvas is too large.");
	const int image_size = pixel_count * 4;
	PackedByteArray screen;
	screen.resize(image_size);
	memset(screen.ptrw(), 0, image_size);

	GifRecordType record_type;
	int frames_loaded = 0;

	GraphicsControlBlock gcb; // Store the additional information for the next frame.
	gcb.DisposalMode = DISPOSAL_UNSPECIFIED;
	gcb.UserInputFlag = false;
	gcb.DelayTime = 0;
	gcb.TransparentColor = NO_TRANSPARENT_COLOR;

	do {
		if (DGifGetRecordType(gif, &record_type) == GIF_ERROR) {
			return gif_error(gif->Error);
		}
		switch (record_type) {
			case EXTENSION_RECORD_TYPE: {
				int extension_function;
				GifByteType *extension_data;

				if (DGifGetExtension(gif, &extension_function, &extension_data) == GIF_ERROR) {
					return gif_error(gif->Error);
				}
				if (extension_data && extension_function == GRAPHICS_EXT_FUNC_CODE) {
					if (DGifExtensionToGCB(extension_data[0], &extension_data[1], &gcb) == GIF_ERROR) {
						return gif_error(gif->Error);
					}
				}
				while (extension_data) {
					if (DGifGetExtensionNext(gif, &extension_data) == GIF_ERROR) {
						return gif_error(gif->Error);
					}
				}
			} break;
			case IMAGE_DESC_RECORD_TYPE: {
				if (DGifGetImageHeader(gif) == GIF_ERROR) {
					return gif_error(gif->Error);
				}
				const GifImageDesc &image_desc = gif->Image;

				if (image_desc.Width <= 0 || image_desc.Height <= 0 || image_desc.Left < 0 || image_desc.Top < 0 ||
						image_desc.Left > gif->SWidth - image_desc.Width || image_desc.Top > gif->SHeight - image_desc.Height) {
					return parse_error(ERR_FILE_CORRUPT, "GIF frame is outside the canvas.");
				}
				ColorMapObject *color_map = image_desc.ColorMap ? image_desc.ColorMap : gif->SColorMap;
				if (!color_map || color_map->ColorCount <= 0 || !color_map->Colors) {
					return parse_error(ERR_FILE_CORRUPT, "GIF frame has no color map.");
				}

				const int64_t frame_pixel_count = (int64_t)image_desc.Width * image_desc.Height;
				if (frame_pixel_count > Image::MAX_PIXELS) {
					return parse_error(ERR_OUT_OF_MEMORY, "GIF frame is too large.");
				}
				PackedByteArray raster_bits;
				raster_bits.resize(frame_pixel_count);

				if (image_desc.Interlace) {
					const int interlaced_offset[] = { 0, 4, 2, 1 };
					const int interlaced_jumps[] = { 8, 8, 4, 2 };

					for (int i = 0; i < 4; i++) {
						for (int j = interlaced_offset[i]; j < image_desc.Height; j += interlaced_jumps[i]) {
							if (DGifGetLine(gif, raster_bits.ptrw() + j * image_desc.Width, image_desc.Width) == GIF_ERROR) {
								return gif_error(gif->Error);
							}
						}
					}
				} else {
					if (DGifGetLine(gif, raster_bits.ptrw(), frame_pixel_count) == GIF_ERROR) {
						return gif_error(gif->Error);
					}
				}

				PackedByteArray previous_screen;
				if (gcb.DisposalMode == DISPOSE_PREVIOUS) {
					previous_screen = screen.duplicate();
				}

				const uint8_t *raster = raster_bits.ptr();
				uint8_t *screen_write = screen.ptrw();
				for (int y = 0; y < image_desc.Height; y++) {
					for (int x = 0; x < image_desc.Width; x++) {
						const int color_map_index = raster[y * image_desc.Width + x];
						if (color_map_index == gcb.TransparentColor) {
							continue;
						}
						if (color_map_index >= color_map->ColorCount) {
							return parse_error(ERR_FILE_CORRUPT, "GIF pixel references an invalid color.");
						}
						const int write_y = y + image_desc.Top;
						const int write_x = x + image_desc.Left;
						const int write_index = (write_y * gif->SWidth + write_x) * 4;
						const GifColorType &color = color_map->Colors[color_map_index];
						screen_write[write_index] = color.Red;
						screen_write[write_index + 1] = color.Green;
						screen_write[write_index + 2] = color.Blue;
						screen_write[write_index + 3] = 255;
					}
				}

				float duration = gcb.DelayTime / 100.0;
				if (duration == 0) {
					duration = 0.05; // Default duration.
				}
				Ref<Image> image = memnew(Image(gif->SWidth, gif->SHeight, false, Image::FORMAT_RGBA8, screen.duplicate()));
				p_image_frames->add_frame(image, duration);
				frames_loaded++;

				if (gcb.DisposalMode == DISPOSE_BACKGROUND) {
					for (int y = 0; y < image_desc.Height; y++) {
						const int write_y = y + image_desc.Top;
						const int write_index = (write_y * gif->SWidth + image_desc.Left) * 4;
						memset(screen.ptrw() + write_index, 0, image_desc.Width * 4);
					}
				} else if (gcb.DisposalMode == DISPOSE_PREVIOUS) {
					screen = previous_screen;
				}

				gcb.DisposalMode = DISPOSAL_UNSPECIFIED;
				gcb.UserInputFlag = false;
				gcb.DelayTime = 0;
				gcb.TransparentColor = NO_TRANSPARENT_COLOR;
			} break;
			default: {
			}
		}

		if (p_max_frames > 0 && frames_loaded >= p_max_frames) {
			break;
		}
	} while (record_type != TERMINATE_RECORD_TYPE);

	if (frames_loaded == 0) {
		return parse_error(ERR_FILE_CORRUPT, "No frames found.");
	}

	return OK;
}

Error GifLoader::_close() {
	int err = 0;
	if (!DGifCloseFile(gif, &err)) {
		return gif_error(err);
	}
	gif = nullptr;
	return OK;
}

Error GifLoader::load_from_file_access(const Ref<ImageFrames> &p_image_frames, const Ref<FileAccess> &p_file, int p_max_frames) {
	Error err;
	err = _open(*p_file, SourceType::FILE);
	if (err != OK) {
		return ERR_FILE_CORRUPT;
	}
	err = _load_frames(p_image_frames, p_max_frames);
	if (err != OK) {
		_close();
		return ERR_FILE_CORRUPT;
	}
	err = _close();
	if (err != OK) {
		return ERR_FILE_CORRUPT;
	}
	return OK;
}

Error GifLoader::load_from_buffer(const Ref<ImageFrames> &p_image_frames, const PackedByteArray &p_data, int p_max_frames) {
	GIFBuffer f = GIFBuffer(p_data);
	Error err;
	err = _open(&f, SourceType::BUFFER);
	if (err != OK) {
		return ERR_FILE_CORRUPT;
	}
	err = _load_frames(p_image_frames, p_max_frames);
	if (err != OK) {
		_close();
		return ERR_FILE_CORRUPT;
	}
	err = _close();
	if (err != OK) {
		return ERR_FILE_CORRUPT;
	}
	return OK;
}
