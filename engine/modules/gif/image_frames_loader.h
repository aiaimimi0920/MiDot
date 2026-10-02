/**************************************************************************/
/*  image_frames_loader.h                                                 */
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

#include "image_frames.h"

#include "core/io/file_access.h"

class ImageFramesFormatLoader {
	friend class ImageFramesLoader;

public:
	virtual Error load_image_frames(const Ref<ImageFrames> &p_image_frames, const Ref<FileAccess> &p_file, int p_max_frames = 0) const = 0;
	virtual void get_recognized_extensions(List<String> *p_extensions) const = 0;
	bool recognize(const String &p_extension) const;

	virtual ~ImageFramesFormatLoader() = default;
};

class ImageFramesLoader {
	static Vector<ImageFramesFormatLoader *> loader;

public:
	static Error load_image_frames(const String &p_file, const Ref<ImageFrames> &p_image_frames, const Ref<FileAccess> &p_custom = Ref<FileAccess>(), int p_max_frames = 0);
	static void get_recognized_extensions(List<String> *p_extensions);
	static ImageFramesFormatLoader *recognize(const String &p_extension);

	static void add_image_frames_format_loader(ImageFramesFormatLoader *p_loader);
	static void remove_image_frames_format_loader(ImageFramesFormatLoader *p_loader);

	static void cleanup();
};
