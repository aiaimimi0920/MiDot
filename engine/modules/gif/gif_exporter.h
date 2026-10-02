/**************************************************************************/
/*  gif_exporter.h                                                        */
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

#pragma once

#include "image_frames.h"

#include "core/io/image.h"
#include "core/object/ref_counted.h"

class GifExporter : public RefCounted {
	GDCLASS(GifExporter, RefCounted);

	Ref<ImageFrames> image_frames;
	String output_path;
	int width = 0;
	int height = 0;
	int loop_count = 0;
	int max_color_count = 256;
	float default_frame_delay = 1.0f;
	bool exporting = false;

	Ref<Image> _prepare_frame(const Ref<Image> &p_frame, const Color &p_background_color, int p_bit_depth, bool p_dither) const;
	void _reset();

protected:
	static void _bind_methods();

public:
	bool begin_export(const String &p_file_path, int p_width, int p_height, float p_frame_delay, int p_loop_count = 0, int p_bit_depth = 8, bool p_dither = false);
	bool write_frame(const Ref<Image> &p_frame, const Color &p_background_color, float p_frame_delay, int p_bit_depth = 8, bool p_dither = false);
	bool end_export();

	~GifExporter();
};
