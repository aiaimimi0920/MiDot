/**************************************************************************/
/*  resource_importer_animated_texture.cpp                                */
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

#include "resource_importer_animated_texture.h"

#include "image_frames.h"
#include "image_frames_loader.h"

#include "core/io/resource_saver.h"
#include "scene/resources/animated_texture.h"
#include "scene/resources/image_texture.h"

String ResourceImporterAnimatedTexture::get_resource_type() const {
	return "AnimatedTexture";
}

String ResourceImporterAnimatedTexture::get_importer_name() const {
	return "animated_texture";
}

String ResourceImporterAnimatedTexture::get_visible_name() const {
	return "AnimatedTexture";
}

void ResourceImporterAnimatedTexture::get_recognized_extensions(List<String> *p_extensions) const {
	ImageFramesLoader::get_recognized_extensions(p_extensions);
}

String ResourceImporterAnimatedTexture::get_save_extension() const {
	return "res";
}

bool ResourceImporterAnimatedTexture::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	(void)p_path;
	(void)p_option;
	(void)p_options;
	return true;
}

int ResourceImporterAnimatedTexture::get_preset_count() const {
	return 0;
}

String ResourceImporterAnimatedTexture::get_preset_name(int p_idx) const {
	(void)p_idx;
	return "";
}

void ResourceImporterAnimatedTexture::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	(void)p_path;
	(void)p_preset;
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "max_frames", PROPERTY_HINT_RANGE, vformat("0, %d, 1", AnimatedTexture::MAX_FRAMES)), 0));
}

Error ResourceImporterAnimatedTexture::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	(void)p_source_id;
	(void)r_platform_variants;
	(void)r_gen_files;
	(void)r_metadata;
	int max_frames = p_options["max_frames"];

	Ref<ImageFrames> image_frames;
	image_frames.instantiate();
	Error err = ImageFramesLoader::load_image_frames(p_source_file, image_frames, nullptr, max_frames);
	if (err != OK) {
		return err;
	}
	if (max_frames <= 0 || max_frames > AnimatedTexture::MAX_FRAMES) {
		max_frames = AnimatedTexture::MAX_FRAMES;
	}
	const int frame_count = MIN(image_frames->get_frame_count(), max_frames);

	Ref<AnimatedTexture> animated_texture;
	animated_texture.instantiate();
	animated_texture->set_frames(frame_count);

	for (int i = 0; i < frame_count; ++i) {
		Ref<ImageTexture> texture = ImageTexture::create_from_image(image_frames->get_frame_image(i));
		animated_texture->set_frame_texture(i, texture);
		animated_texture->set_frame_duration(i, image_frames->get_frame_duration(i));
	}
	return ResourceSaver::save(animated_texture, p_save_path + ".res");
}
