/**************************************************************************/
/*  test_gif_exporter.cpp                                                 */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_gif_exporter)

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_GIF_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "tests/test_utils.h"

#include "modules/gif/gif_exporter.h"
#include "modules/gif/image_frames.h"

namespace TestGifExporter {

TEST_CASE("[GIF] GifExporter writes compatible animated files") {
	CHECK(ClassDB::class_exists(SNAME("GifExporter")));
	const String path = TestUtils::get_temp_path("gif_exporter_compat.gif");
	if (FileAccess::exists(path)) {
		DirAccess::remove_file_or_error(path);
	}

	Ref<GifExporter> exporter;
	exporter.instantiate();
	REQUIRE(exporter->begin_export(path, 2, 1, 5.0f, 3, 8, false));

	PackedByteArray pixels;
	pixels.resize(8);
	uint8_t *data = pixels.ptrw();
	data[0] = 255;
	data[1] = 0;
	data[2] = 0;
	data[3] = 0;
	data[4] = 0;
	data[5] = 0;
	data[6] = 255;
	data[7] = 255;
	Ref<Image> frame = Image::create_from_data(2, 1, false, Image::FORMAT_RGBA8, pixels);
	REQUIRE(exporter->write_frame(frame, Color(0, 1, 0), 5.0f, 8, false));
	REQUIRE(exporter->write_frame(frame, Color(0, 1, 0), 10.0f, 4, true));
	REQUIRE(exporter->end_export());

	Ref<ImageFrames> loaded;
	loaded.instantiate();
	REQUIRE(loaded->load(path) == OK);
	REQUIRE(loaded->get_frame_count() == 2);
	CHECK(loaded->get_frame_duration(0) == doctest::Approx(0.05));
	CHECK(loaded->get_frame_duration(1) == doctest::Approx(0.10));
	CHECK(loaded->get_frame_image(0)->get_pixel(0, 0).is_equal_approx(Color(0, 1, 0, 1)));
	CHECK(loaded->get_frame_image(0)->get_pixel(1, 0).is_equal_approx(Color(0, 0, 1, 1)));

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	REQUIRE(file.is_valid());
	PackedByteArray encoded = file->get_buffer(file->get_length());
	bool loop_count_found = false;
	for (int i = 0; i + 2 < encoded.size(); i++) {
		if (encoded[i] == 0x01 && encoded[i + 1] == 0x03 && encoded[i + 2] == 0x00) {
			loop_count_found = true;
			break;
		}
	}
	CHECK(loop_count_found);
	file.unref();
	DirAccess::remove_file_or_error(path);
}

TEST_CASE("[GIF] GifExporter rejects invalid state and dimensions") {
	Ref<GifExporter> exporter;
	exporter.instantiate();
	ERR_PRINT_OFF;
	CHECK_FALSE(exporter->end_export());
	CHECK_FALSE(exporter->begin_export("res://invalid.gif", 0, 1, 5.0f));
	const bool began_export = exporter->begin_export("res://unused.gif", 2, 2, 5.0f);
	CHECK(began_export);
	if (!began_export) {
		ERR_PRINT_ON;
		return;
	}

	Ref<Image> wrong_size = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	CHECK_FALSE(exporter->write_frame(wrong_size, Color(), 5.0f));
	CHECK_FALSE(exporter->end_export());
	ERR_PRINT_ON;
}

} // namespace TestGifExporter

#endif // MODULE_GIF_ENABLED
