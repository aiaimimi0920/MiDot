/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "gif_exporter.h"
#include "image_frames.h"
#include "image_frames_loader.h"
#include "image_frames_loader_gif.h"

#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/import/resource_importer_animated_texture.h"
#include "editor/import/resource_importer_sprite_frames.h"
#endif

static ImageFramesLoaderGIF *image_frames_loader_gif = nullptr;

#ifdef TOOLS_ENABLED
static void _editor_init() {
	Ref<ResourceImporterAnimatedTexture> import_animated_texture;
	import_animated_texture.instantiate();
	ResourceFormatImporter::get_singleton()->add_importer(import_animated_texture);

	Ref<ResourceImporterSpriteFrames> import_sprite_frames;
	import_sprite_frames.instantiate();
	ResourceFormatImporter::get_singleton()->add_importer(import_sprite_frames);
}
#endif

void initialize_gif_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		image_frames_loader_gif = memnew(ImageFramesLoaderGIF);
		ImageFramesLoader::add_image_frames_format_loader(image_frames_loader_gif);

		GDREGISTER_CLASS(ImageFrames);
		GDREGISTER_CLASS(GifExporter);
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorNode::add_init_callback(_editor_init);
	}
#endif
}

void uninitialize_gif_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	if (image_frames_loader_gif) {
		ImageFramesLoader::remove_image_frames_format_loader(image_frames_loader_gif);
		memdelete(image_frames_loader_gif);
		image_frames_loader_gif = nullptr;
		ImageFrames::load_gif_func = nullptr;
	}
}
