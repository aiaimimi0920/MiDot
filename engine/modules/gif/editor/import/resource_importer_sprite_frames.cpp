/**************************************************************************/
/*  resource_importer_sprite_frames.cpp                                   */
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

#include "resource_importer_sprite_frames.h"

#include "image_frames.h"
#include "image_frames_loader.h"

#include "core/io/resource_saver.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/sprite_frames.h"

String ResourceImporterSpriteFrames::get_resource_type() const {
	return "SpriteFrames";
}

String ResourceImporterSpriteFrames::get_importer_name() const {
	return "sprite_frames";
}

String ResourceImporterSpriteFrames::get_visible_name() const {
	return "SpriteFrames";
}

void ResourceImporterSpriteFrames::get_recognized_extensions(List<String> *p_extensions) const {
	ImageFramesLoader::get_recognized_extensions(p_extensions);
}

String ResourceImporterSpriteFrames::get_save_extension() const {
	return "res";
}

bool ResourceImporterSpriteFrames::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	(void)p_path;
	(void)p_option;
	(void)p_options;
	return true;
}

int ResourceImporterSpriteFrames::get_preset_count() const {
	return 0;
}

String ResourceImporterSpriteFrames::get_preset_name(int p_idx) const {
	(void)p_idx;
	return "";
}

void ResourceImporterSpriteFrames::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	(void)p_path;
	(void)p_preset;
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "max_frames", PROPERTY_HINT_RANGE, "0, 4096, 1"), 0));
}

Error ResourceImporterSpriteFrames::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
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
	if (max_frames <= 0 || max_frames > 4096) {
		max_frames = 4096;
	}
	const int frame_count = MIN(image_frames->get_frame_count(), max_frames);

	Ref<SpriteFrames> sprite_frames;
	sprite_frames.instantiate();
	sprite_frames->set_animation_speed("default", 1.0);

	for (int i = 0; i < frame_count; ++i) {
		Ref<ImageTexture> texture = ImageTexture::create_from_image(image_frames->get_frame_image(i));
		sprite_frames->add_frame("default", texture, image_frames->get_frame_duration(i));
	}
	return ResourceSaver::save(sprite_frames, p_save_path + ".res");
}
