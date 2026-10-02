/******************************************************************************
 * Spine Runtimes License Agreement
 * Last updated July 28, 2023. Replaces all prior versions.
 *
 * Copyright (c) 2013-2023, Esoteric Software LLC
 *
 * Integration of the Spine Runtimes into software or otherwise creating
 * derivative works of the Spine Runtimes is permitted under the terms and
 * conditions of Section 2 of the Spine Editor License Agreement:
 * http://esotericsoftware.com/spine-editor-license
 *
 * Otherwise, it is permitted to integrate the Spine Runtimes into software or
 * otherwise create derivative works of the Spine Runtimes (collectively,
 * "Products"), provided that each user of the Products must obtain their own
 * Spine Editor license and redistribution of the Products in any form must
 * include this license and copyright notice.
 *
 * THE SPINE RUNTIMES ARE PROVIDED BY ESOTERIC SOFTWARE LLC "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ESOTERIC SOFTWARE LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES,
 * BUSINESS INTERRUPTION, OR LOSS OF USE, DATA, OR PROFITS) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THE
 * SPINE RUNTIMES, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include "SpineSkeletonFileResource.h"
#if VERSION_MAJOR > 3
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#else
#include "core/error_list.h"
#include "core/error_macros.h"
#include "core/os/file_access.h"
#endif
#include <spine/Extension.h>
#include <spine/Json.h>
#include <spine/Version.h>

struct BinaryInput {
	const unsigned char *cursor;
	const unsigned char *end;
};

static bool readByte(BinaryInput *input, unsigned char &r_byte) {
	if (input->cursor >= input->end) {
		return false;
	}
	r_byte = *input->cursor++;
	return true;
}

static bool readVarint(BinaryInput *input, bool optimize_positive, int &r_value) {
	uint32_t value = 0;
	for (uint32_t shift = 0; shift < 35; shift += 7) {
		unsigned char byte;
		if (!readByte(input, byte)) {
			return false;
		}
		if (shift == 28 && (byte & 0xF0) != 0) {
			return false;
		}
		value |= uint32_t(byte & 0x7F) << shift;
		if ((byte & 0x80) == 0) {
			if (optimize_positive && value > 0x7FFFFFFF) {
				return false;
			}
			r_value = optimize_positive ? int(value) : int((value >> 1) ^ (0U - (value & 1)));
			return true;
		}
	}
	return false;
}

static bool readString(BinaryInput *input, char *&r_string) {
	r_string = nullptr;
	int length;
	if (!readVarint(input, true, length)) {
		return false;
	}
	if (length == 0) {
		return true;
	}
	if (input->end - input->cursor < length - 1) {
		return false;
	}
	r_string = spine::SpineExtension::alloc<char>(length, __FILE__, __LINE__);
	memcpy(r_string, input->cursor, length - 1);
	input->cursor += length - 1;
	r_string[length - 1] = '\0';
	return true;
}

void SpineSkeletonFileResource::_bind_methods() {
	ADD_SIGNAL(MethodInfo("skeleton_file_changed"));
}

static bool checkVersion(const char *version) {
	return version && strstr(version, SPINE_VERSION_STRING) == version;
}

static bool checkJson(const char *jsonData) {
	spine::Json json(jsonData);
	spine::Json *skeleton = spine::Json::getItem(&json, "skeleton");
	if (!skeleton) {
		return false;
	}
	const char *version = spine::Json::getString(skeleton, "spine", 0);
	if (!version) {
		return false;
	}

	return checkVersion(version);
}

static bool checkBinary(const char *binaryData, int length) {
	if (!binaryData || length < 8) {
		return false;
	}
	BinaryInput input;
	input.cursor = (const unsigned char *)binaryData;
	input.end = (const unsigned char *)binaryData + length;
	// Skip hash
	input.cursor += 8;
	char *version = nullptr;
	if (!readString(&input, version)) {
		return false;
	}
	bool result = checkVersion(version);
	spine::SpineExtension::free(version, __FILE__, __LINE__);
	return result;
}

Error SpineSkeletonFileResource::load_from_file(const String &path) {
	Error error = OK;
	if (path.ends_with(".spjson") || path.ends_with(".spine-json")) {
		json = FileAccess::get_file_as_string(path, &error);
		if (error != OK) {
			return error;
		}
		if (!checkJson(json.utf8().get_data())) {
			return ERR_INVALID_DATA;
		}
	} else {
#if VERSION_MAJOR > 3
		binary = FileAccess::get_file_as_bytes(path, &error);
#else
		binary = FileAccess::get_file_as_array(path, &error);
#endif
		if (error != OK) {
			return error;
		}
		if (!checkBinary((const char *)binary.ptr(), binary.size())) {
			return ERR_INVALID_DATA;
		}
	}
	return error;
}

Error SpineSkeletonFileResource::save_to_file(const String &path) {
	Error error;
#if VERSION_MAJOR > 3
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &error);
	if (error != OK) {
		return error;
	}
#else
	FileAccess *file = FileAccess::open(path, FileAccess::WRITE, &error);
	if (error != OK) {
		if (file) {
			file->close();
		}
		return error;
	}
#endif
	if (!is_binary()) {
		file->store_string(json);
	} else {
		file->store_buffer(binary.ptr(), binary.size());
	}
#if VERSION_MAJOR > 3
	file->flush();
#else
	file->close();
#endif
	return OK;
}

#if VERSION_MAJOR > 3
Error SpineSkeletonFileResource::copy_from(const Ref<Resource> &p_resource) {
	auto error = Resource::copy_from(p_resource);
	if (error != OK) {
		return error;
	}
	const Ref<SpineSkeletonFileResource> &spineFile = static_cast<const Ref<SpineSkeletonFileResource> &>(p_resource);
	this->json = spineFile->json;
	this->binary = spineFile->binary;
	emit_signal(SNAME("skeleton_file_changed"));
	return OK;
}
#endif

#if VERSION_MAJOR > 3
RES SpineSkeletonFileResourceFormatLoader::load(const String &path, const String &original_path, Error *error, bool use_sub_threads, float *progress, CacheMode cache_mode) {
#else
RES SpineSkeletonFileResourceFormatLoader::load(const String &path, const String &original_path, Error *error) {
#endif
	Ref<SpineSkeletonFileResource> skeleton_file = memnew(SpineSkeletonFileResource);
	const Error load_error = skeleton_file->load_from_file(path);
	if (error) {
		*error = load_error;
	}
	if (load_error != OK) {
		return RES();
	}
	return skeleton_file;
}

void SpineSkeletonFileResourceFormatLoader::get_recognized_extensions(List<String> *extensions) const {
	extensions->push_back("spjson");
	extensions->push_back("spskel");
}

String SpineSkeletonFileResourceFormatLoader::get_resource_type(const String &path) const {
	return path.ends_with(".spjson") || path.ends_with(".spskel") || path.ends_with(".spine-json") || path.ends_with(".skel") ? "SpineSkeletonFileResource" : "";
}

bool SpineSkeletonFileResourceFormatLoader::handles_type(const String &type) const {
	return type == "SpineSkeletonFileResource" || ClassDB::is_parent_class(type, "SpineSkeletonFileResource");
}

#if VERSION_MAJOR > 3
Error SpineSkeletonFileResourceFormatSaver::save(const RES &resource, const String &path, uint32_t flags) {
#else
Error SpineSkeletonFileResourceFormatSaver::save(const String &path, const RES &resource, uint32_t flags) {
#endif
	Ref<SpineSkeletonFileResource> res = resource;
	Error error = res->save_to_file(path);
	return error;
}

void SpineSkeletonFileResourceFormatSaver::get_recognized_extensions(const RES &resource, List<String> *p_extensions) const {
	if (Object::cast_to<SpineSkeletonFileResource>(*resource)) {
		p_extensions->push_back("spjson");
		p_extensions->push_back("spskel");
	}
}

bool SpineSkeletonFileResourceFormatSaver::recognize(const RES &p_resource) const {
	return Object::cast_to<SpineSkeletonFileResource>(*p_resource) != nullptr;
}
