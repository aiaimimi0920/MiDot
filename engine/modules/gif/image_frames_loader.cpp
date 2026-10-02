/**************************************************************************/
/*  image_frames_loader.cpp                                               */
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

#include "image_frames_loader.h"

Vector<ImageFramesFormatLoader *> ImageFramesLoader::loader;

bool ImageFramesFormatLoader::recognize(const String &p_extension) const {
	List<String> extensions;
	get_recognized_extensions(&extensions);
	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		if (E->get().nocasecmp_to(p_extension) == 0) {
			return true;
		}
	}
	return false;
}

Error ImageFramesLoader::load_image_frames(const String &p_file, const Ref<ImageFrames> &p_image_frames, const Ref<FileAccess> &p_custom, int p_max_frames) {
	ERR_FAIL_COND_V_MSG(p_image_frames.is_null(), ERR_INVALID_PARAMETER, "It's not a reference to a valid ImageFrames object.");

	Ref<FileAccess> f = p_custom;
	if (f.is_null()) {
		Error err;
		f = FileAccess::open(p_file, FileAccess::READ, &err);
		if (f.is_null()) {
			ERR_PRINT("Error opening file '" + p_file + "'.");
			return err;
		}
	}
	String extension = p_file.get_extension();

	for (int i = 0; i < loader.size(); i++) {
		if (!loader[i]->recognize(extension)) {
			continue;
		}
		Error err = loader[i]->load_image_frames(p_image_frames, f, p_max_frames);
		if (err == ERR_FILE_UNRECOGNIZED) {
			continue;
		}
		if (err != OK) {
			ERR_PRINT("Error loading image: " + p_file);
			return err;
		}
		if (p_image_frames->get_frame_count() == 0) {
			ERR_PRINT("Images frames should contain at least one frame to be loaded.");
			return ERR_INVALID_DATA;
		}
		return OK;
	}
	return ERR_FILE_UNRECOGNIZED;
}

void ImageFramesLoader::get_recognized_extensions(List<String> *p_extensions) {
	for (int i = 0; i < loader.size(); i++) {
		loader[i]->get_recognized_extensions(p_extensions);
	}
}

ImageFramesFormatLoader *ImageFramesLoader::recognize(const String &p_extension) {
	for (int i = 0; i < loader.size(); i++) {
		if (loader[i]->recognize(p_extension)) {
			return loader[i];
		}
	}
	return nullptr;
}

void ImageFramesLoader::add_image_frames_format_loader(ImageFramesFormatLoader *p_loader) {
	loader.push_back(p_loader);
}

void ImageFramesLoader::remove_image_frames_format_loader(ImageFramesFormatLoader *p_loader) {
	loader.erase(p_loader);
}

void ImageFramesLoader::cleanup() {
	while (loader.size()) {
		remove_image_frames_format_loader(loader[0]);
	}
}
