/**************************************************************************/
/*  spout_gd.cpp                                                          */
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

#include "spout_gd.h"

#include "core/object/class_db.h"

static constexpr int SPOUT_GL_RGB = 0x1907;
static constexpr int SPOUT_GL_BGR = 0x80E0;

static int _get_spout_pixel_size(int p_gl_format) {
	switch (p_gl_format) {
		case SPOUT_GL_RGB:
		case SPOUT_GL_BGR:
			return 3;
		case GL_RGBA:
		case GL_BGRA:
			return 4;
		default:
			return 0;
	}
}

static void _swap_red_blue(PackedByteArray &r_data, int p_pixel_size) {
	uint8_t *data = r_data.ptrw();
	for (int i = 0; i < r_data.size(); i += p_pixel_size) {
		SWAP(data[i], data[i + 2]);
	}
}

bool Spout::_has_valid_send_size() const {
	return Math::is_finite(send_size.x) && Math::is_finite(send_size.y) && send_size.x >= 1.0 && send_size.y >= 1.0 &&
			send_size.x <= Image::MAX_WIDTH && send_size.y <= Image::MAX_HEIGHT;
}

void Spout::sender_set_sender_name(const String &p_sender_name) {
	ERR_FAIL_NULL(spout_lib);
	const CharString sender_name = p_sender_name.utf8();
	spout_lib->SetSenderName(sender_name.get_data());
}

void Spout::sender_release_sender(int p_msec) {
	ERR_FAIL_NULL(spout_lib);
	ERR_FAIL_COND(p_msec < 0);
	spout_lib->ReleaseSender(static_cast<DWORD>(p_msec));
}

Error Spout::sender_send_fbo(const uint32_t p_fbo_id) {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(!_has_valid_send_size(), ERR_INVALID_PARAMETER);
	return spout_lib->SendFbo(p_fbo_id, static_cast<unsigned int>(send_size.x), static_cast<unsigned int>(send_size.y)) ? Error::OK : Error::FAILED;
}

Error Spout::sender_send_texture(const uint32_t p_texture_id, const uint32_t p_texture_target, const bool p_invert, const int p_host_fbo) {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(!_has_valid_send_size() || p_texture_id == 0 || p_texture_target == 0 || p_host_fbo < 0, ERR_INVALID_PARAMETER);
	return spout_lib->SendTexture(
				   static_cast<GLuint>(p_texture_id),
				   static_cast<GLuint>(p_texture_target),
				   static_cast<unsigned int>(send_size.x),
				   static_cast<unsigned int>(send_size.y),
				   p_invert,
				   static_cast<GLuint>(p_host_fbo))
			? Error::OK
			: Error::FAILED;
}

Error Spout::sender_send_image(const Ref<Image> &p_image, const bool p_invert, const int p_gl_format) {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(p_image.is_null() || p_image->is_empty(), ERR_INVALID_PARAMETER);
	const int pixel_size = _get_spout_pixel_size(p_gl_format);
	ERR_FAIL_COND_V(pixel_size == 0, ERR_INVALID_PARAMETER);

	Ref<Image> image = p_image->duplicate();
	if (image->is_compressed()) {
		Error err = image->decompress();
		ERR_FAIL_COND_V(err != OK, err);
	}
	image->convert(pixel_size == 4 ? Image::FORMAT_RGBA8 : Image::FORMAT_RGB8);
	PackedByteArray data = image->get_data();
	if (p_gl_format == GL_BGRA || p_gl_format == SPOUT_GL_BGR) {
		_swap_red_blue(data, pixel_size);
	}

	const bool sent = spout_lib->SendImage(data.ptr(), image->get_width(), image->get_height(), p_gl_format, p_invert);
	if (sent) {
		send_size = image->get_size();
	}
	return sent ? Error::OK : Error::FAILED;
}

String Spout::sender_get_name() {
	ERR_FAIL_NULL_V(spout_lib, String());
	const char *sender_name = spout_lib->GetName();

	return sender_name ? String(sender_name) : String();
}

double_t Spout::sender_get_fps() {
	ERR_FAIL_NULL_V(spout_lib, 0.0);
	return spout_lib->GetFps();
}

double_t Spout::sender_get_frame() {
	ERR_FAIL_NULL_V(spout_lib, 0.0);
	return static_cast<double_t>(spout_lib->GetFrame());
}

bool Spout::sender_get_cpu() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->GetCPU();
}

bool Spout::sender_get_gldx() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->GetGLDX();
}

void Spout::receiver_set_receiver_name(const String &p_sender_name) {
	ERR_FAIL_NULL(spout_lib);
	const CharString sender_name = p_sender_name.utf8();
	spout_lib->SetReceiverName(p_sender_name.is_empty() ? nullptr : sender_name.get_data());
}

void Spout::receiver_release_receiver() {
	ERR_FAIL_NULL(spout_lib);
	spout_lib->ReleaseReceiver();
}

Error Spout::receiver_receive_texture(uint32_t p_texture_id, uint32_t p_texture_target, bool p_invert, int p_host_fbo) {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(p_host_fbo < 0 || (p_texture_id == 0 && p_texture_target != 0) || (p_texture_id != 0 && p_texture_target == 0), ERR_INVALID_PARAMETER);
	return spout_lib->ReceiveTexture(static_cast<GLuint>(p_texture_id), static_cast<GLuint>(p_texture_target), p_invert, static_cast<GLuint>(p_host_fbo)) ? Error::OK : Error::FAILED;
}

Error Spout::receiver_receive_image(const Ref<Image> &p_image, const bool p_invert, const int p_host_fbo, const int p_gl_format) {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(p_image.is_null() || p_host_fbo < 0, ERR_INVALID_PARAMETER);
	const int pixel_size = _get_spout_pixel_size(p_gl_format);
	ERR_FAIL_COND_V(pixel_size == 0, ERR_INVALID_PARAMETER);

	const unsigned int width = spout_lib->GetSenderWidth();
	const unsigned int height = spout_lib->GetSenderHeight();
	ERR_FAIL_COND_V(width == 0 || height == 0 || width > Image::MAX_WIDTH || height > Image::MAX_HEIGHT ||
					static_cast<uint64_t>(width) * height > Image::MAX_PIXELS,
			ERR_DOES_NOT_EXIST);

	PackedByteArray data;
	data.resize(static_cast<int64_t>(width) * height * pixel_size);
	if (!spout_lib->ReceiveImage(data.ptrw(), p_gl_format, p_invert, static_cast<GLuint>(p_host_fbo))) {
		return Error::FAILED;
	}
	if (p_gl_format == GL_BGRA || p_gl_format == SPOUT_GL_BGR) {
		_swap_red_blue(data, pixel_size);
	}
	p_image->set_data(width, height, false, pixel_size == 4 ? Image::FORMAT_RGBA8 : Image::FORMAT_RGB8, data);
	return Error::OK;
}

bool Spout::receiver_is_updated() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->IsUpdated();
}

bool Spout::receiver_is_connected() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->IsConnected();
}

bool Spout::receiver_is_frame_new() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->IsFrameNew();
}

String Spout::receiver_get_sender_name() {
	ERR_FAIL_NULL_V(spout_lib, String());
	const char *sender_name = spout_lib->GetSenderName();

	return sender_name ? String(sender_name) : String();
}

Vector2 Spout::receiver_get_sender_size() {
	ERR_FAIL_NULL_V(spout_lib, Vector2());
	return Vector2(static_cast<real_t>(spout_lib->GetSenderWidth()), static_cast<real_t>(spout_lib->GetSenderHeight()));
}

double_t Spout::receiver_get_sender_fps() {
	ERR_FAIL_NULL_V(spout_lib, 0.0);
	return spout_lib->GetSenderFps();
}

double_t Spout::receiver_get_sender_frame() {
	ERR_FAIL_NULL_V(spout_lib, 0.0);
	return static_cast<double_t>(spout_lib->GetSenderFrame());
}

bool Spout::receiver_get_sender_cpu() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->GetSenderCPU();
}

bool Spout::receiver_get_sender_gldx() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->GetSenderGLDX();
}

void Spout::set_frame_count(bool p_enabled) {
	ERR_FAIL_NULL(spout_lib);
	spout_lib->SetFrameCount(p_enabled);
}

void Spout::disable_frame_count() {
	ERR_FAIL_NULL(spout_lib);
	spout_lib->DisableFrameCount();
}

bool Spout::is_frame_count_enabled() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->IsFrameCountEnabled();
}

void Spout::hold_fps(const int p_fps) {
	ERR_FAIL_NULL(spout_lib);
	ERR_FAIL_COND(p_fps <= 0);
	spout_lib->HoldFps(p_fps);
}

void Spout::set_frame_sync(const String &p_sender_name) {
	ERR_FAIL_NULL(spout_lib);
	const CharString sender_name = p_sender_name.utf8();
	spout_lib->SetFrameSync(sender_name.get_data());
}

Error Spout::write_memory_buffer(const String &p_sender_name, const PackedByteArray &p_data, const int p_length) {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(p_sender_name.is_empty() || p_length <= 0 || p_length > p_data.size() || p_length > MAX_MEMORY_BUFFER_SIZE, ERR_INVALID_PARAMETER);
	const uint8_t *data = p_data.ptr();
	const CharString sender_name = p_sender_name.utf8();
	return spout_lib->WriteMemoryBuffer(
				   sender_name.get_data(),
				   reinterpret_cast<const char *>(data),
				   p_length)
			? Error::OK
			: Error::FAILED;
}

PackedByteArray Spout::read_memory_buffer(const String &p_sender_name, const int p_max_length) {
	ERR_FAIL_NULL_V(spout_lib, PackedByteArray());
	ERR_FAIL_COND_V(p_sender_name.is_empty() || p_max_length <= 0 || p_max_length > MAX_MEMORY_BUFFER_SIZE, PackedByteArray());

	PackedByteArray data;
	data.resize(p_max_length);
	const CharString sender_name = p_sender_name.utf8();
	const int bytes_read = spout_lib->ReadMemoryBuffer(sender_name.get_data(), reinterpret_cast<char *>(data.ptrw()), p_max_length);
	if (bytes_read <= 0) {
		return PackedByteArray();
	}
	data.resize(MIN(bytes_read, p_max_length));
	return data;
}

Error Spout::create_memory_buffer(const String &p_name, const int p_length) {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(p_name.is_empty() || p_length <= 0 || p_length > MAX_MEMORY_BUFFER_SIZE, ERR_INVALID_PARAMETER);
	const CharString name = p_name.utf8();
	return spout_lib->CreateMemoryBuffer(name.get_data(), p_length) ? Error::OK : Error::FAILED;
}

Error Spout::delete_memory_buffer() {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	return spout_lib->DeleteMemoryBuffer() ? Error::OK : Error::FAILED;
}

int Spout::get_memory_buffer_size(const String &p_name) {
	ERR_FAIL_NULL_V(spout_lib, 0);
	ERR_FAIL_COND_V(p_name.is_empty(), 0);
	const CharString name = p_name.utf8();
	return spout_lib->GetMemoryBufferSize(name.get_data());
}

void Spout::enable_spout_log() {
	ERR_FAIL_NULL(spout_lib);
	spout_lib->EnableSpoutLog();
}

void Spout::enable_spout_log_file(const String &p_filename, const bool p_append) {
	ERR_FAIL_NULL(spout_lib);
	const CharString filename = p_filename.utf8();
	spout_lib->EnableSpoutLogFile(filename.get_data(), p_append);
}

String Spout::get_spout_log() {
	ERR_FAIL_NULL_V(spout_lib, String());
	std::string log = spout_lib->GetSpoutLog();

	return String(log.c_str());
}

void Spout::show_spout_logs() {
	ERR_FAIL_NULL(spout_lib);
	spout_lib->ShowSpoutLogs();
}

void Spout::disable_spout_log() {
	ERR_FAIL_NULL(spout_lib);
	spout_lib->DisableSpoutLog();
}

void Spout::set_spout_log_level(SpoutLogLevel p_level) {
	ERR_FAIL_NULL(spout_lib);
	ERR_FAIL_INDEX(p_level, SPOUT_LOG_FATAL + 1);
	spout_lib->SetSpoutLogLevel(static_cast<SpoutLibLogLevel>(p_level));
}

void Spout::spout_log(const String &p_format) {
	ERR_FAIL_NULL(spout_lib);
	const CharString format = p_format.utf8();
	spout_lib->SpoutLog(format.get_data());
}

void Spout::spout_log_verbose(const String &p_format) {
	ERR_FAIL_NULL(spout_lib);
	const CharString format = p_format.utf8();
	spout_lib->SpoutLogVerbose(format.get_data());
}

void Spout::spout_log_notice(const String &p_format) {
	ERR_FAIL_NULL(spout_lib);
	const CharString format = p_format.utf8();
	spout_lib->SpoutLogNotice(format.get_data());
}

void Spout::spout_log_warning(const String &p_format) {
	ERR_FAIL_NULL(spout_lib);
	const CharString format = p_format.utf8();
	spout_lib->SpoutLogWarning(format.get_data());
}

void Spout::spout_log_error(const String &p_format) {
	ERR_FAIL_NULL(spout_lib);
	const CharString format = p_format.utf8();
	spout_lib->SpoutLogError(format.get_data());
}

void Spout::spout_log_fatal(const String &p_format) {
	ERR_FAIL_NULL(spout_lib);
	const CharString format = p_format.utf8();
	spout_lib->SpoutLogFatal(format.get_data());
}

bool Spout::is_initialized() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->IsInitialized();
}

bool Spout::bind_shared_texture() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->BindSharedTexture();
}

bool Spout::unbind_shared_texture() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->UnBindSharedTexture();
}

uint32_t Spout::get_shared_texture_id() {
	ERR_FAIL_NULL_V(spout_lib, 0);
	return static_cast<uint32_t>(spout_lib->GetSharedTextureID());
}

int Spout::get_sender_count() {
	ERR_FAIL_NULL_V(spout_lib, 0);
	return spout_lib->GetSenderCount();
}

String Spout::get_sender(const int p_index, const int p_max_size) {
	ERR_FAIL_NULL_V(spout_lib, String());
	ERR_FAIL_INDEX_V(p_index, get_sender_count(), String());
	const int buffer_size = CLAMP(p_max_size, 2, MAX_TEXT_BUFFER_SIZE);
	Vector<char> sender_name;
	sender_name.resize(buffer_size);
	memset(sender_name.ptrw(), 0, buffer_size);
	if (!spout_lib->GetSender(p_index, sender_name.ptrw(), buffer_size)) {
		return String();
	}
	sender_name.write[buffer_size - 1] = '\0';
	return String::utf8(sender_name.ptr());
}

bool Spout::find_sender_name(const String &p_sender_name) {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->FindSenderName(p_sender_name.utf8().get_data());
}

String Spout::get_active_sender() {
	ERR_FAIL_NULL_V(spout_lib, String());
	char sender_name[256] = {};
	return spout_lib->GetActiveSender(sender_name) ? String::utf8(sender_name) : String();
}

Error Spout::set_active_sender(const String &p_sender_name) {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(p_sender_name.is_empty(), ERR_INVALID_PARAMETER);
	return spout_lib->SetActiveSender(p_sender_name.utf8().get_data()) ? Error::OK : Error::FAILED;
}

String Spout::get_host_path(const String &p_sender_name, const int p_max_chars) {
	ERR_FAIL_NULL_V(spout_lib, String());
	ERR_FAIL_COND_V(p_sender_name.is_empty(), String());
	const int buffer_size = CLAMP(p_max_chars, 2, MAX_TEXT_BUFFER_SIZE);
	Vector<char> host_path;
	host_path.resize(buffer_size);
	memset(host_path.ptrw(), 0, buffer_size);
	const CharString sender_name = p_sender_name.utf8();
	if (!spout_lib->GetHostPath(sender_name.get_data(), host_path.ptrw(), buffer_size)) {
		return String();
	}
	host_path.write[buffer_size - 1] = '\0';
	return String::utf8(host_path.ptr());
}

int Spout::get_vertical_sync() {
	ERR_FAIL_NULL_V(spout_lib, 0);
	return spout_lib->GetVerticalSync();
}

Error Spout::set_vertical_sync(const bool p_sync) {
	ERR_FAIL_NULL_V(spout_lib, ERR_UNAVAILABLE);
	return spout_lib->SetVerticalSync(p_sync) ? Error::OK : Error::FAILED;
}

int Spout::get_spout_version() {
	ERR_FAIL_NULL_V(spout_lib, 0);
	int version = 0;
	spout_lib->GetSDKversion(&version);
	return version;
}

bool Spout::get_auto_share() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->GetAutoShare();
}

void Spout::set_auto_share(const bool p_is_auto) {
	ERR_FAIL_NULL(spout_lib);
	spout_lib->SetAutoShare(p_is_auto);
}

bool Spout::is_gldx_ready() {
	ERR_FAIL_NULL_V(spout_lib, false);
	return spout_lib->IsGLDXready();
}

int Spout::get_num_adapters() {
	ERR_FAIL_NULL_V(spout_lib, 0);
	return spout_lib->GetNumAdapters();
}

String Spout::get_adapter_name(const int p_index, const int p_max_chars) {
	ERR_FAIL_NULL_V(spout_lib, String());
	ERR_FAIL_INDEX_V(p_index, get_num_adapters(), String());
	const int buffer_size = CLAMP(p_max_chars, 2, MAX_TEXT_BUFFER_SIZE);
	Vector<char> adapter_name;
	adapter_name.resize(buffer_size);
	memset(adapter_name.ptrw(), 0, buffer_size);
	if (!spout_lib->GetAdapterName(p_index, adapter_name.ptrw(), buffer_size)) {
		return String();
	}
	adapter_name.write[buffer_size - 1] = '\0';
	return String::utf8(adapter_name.ptr());
}

String Spout::adapter_name() {
	ERR_FAIL_NULL_V(spout_lib, String());
	const char *name = spout_lib->AdapterName();
	return name ? String::utf8(name) : String();
}

int Spout::get_adapter() {
	ERR_FAIL_NULL_V(spout_lib, -1);
	return spout_lib->GetAdapter();
}

Array Spout::get_senders() {
	Array arr;
	const int count = get_sender_count();
	for (int i = 0; i < count; i++) {
		arr.append(get_sender(i));
	}

	return arr;
}

Vector2 Spout::get_send_size() {
	return send_size;
}

void Spout::set_send_size(Vector2 p_size) {
	ERR_FAIL_COND_MSG(!Math::is_finite(p_size.x) || !Math::is_finite(p_size.y) || p_size.x < 1.0 || p_size.y < 1.0 ||
					p_size.x > Image::MAX_WIDTH || p_size.y > Image::MAX_HEIGHT,
			"Spout send size must be within the supported image dimensions.");
	send_size = p_size;
}

void Spout::_bind_methods() {
	ClassDB::bind_method(D_METHOD("sender_set_sender_name", "sender_name"), &Spout::sender_set_sender_name, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("sender_release_sender", "msec"), &Spout::sender_release_sender, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("sender_send_fbo", "fbo_id"), &Spout::sender_send_fbo);
	ClassDB::bind_method(D_METHOD("sender_send_texture", "texture_id", "texture_target", "invert", "host_fbo"), &Spout::sender_send_texture, DEFVAL(true), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("sender_send_image", "image", "invert", "gl_format"), &Spout::sender_send_image, DEFVAL(false), DEFVAL(GL_RGBA));
	ClassDB::bind_method(D_METHOD("sender_get_name"), &Spout::sender_get_name);
	ClassDB::bind_method(D_METHOD("sender_get_fps"), &Spout::sender_get_fps);
	ClassDB::bind_method(D_METHOD("sender_get_frame"), &Spout::sender_get_frame);
	ClassDB::bind_method(D_METHOD("sender_get_cpu"), &Spout::sender_get_cpu);
	ClassDB::bind_method(D_METHOD("sender_get_gldx"), &Spout::sender_get_gldx);

	ClassDB::bind_method(D_METHOD("receiver_set_receiver_name", "sender_name"), &Spout::receiver_set_receiver_name, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("receiver_release_receiver"), &Spout::receiver_release_receiver);
	ClassDB::bind_method(D_METHOD("receiver_receive_texture", "texture_id", "texture_target", "invert", "host_fbo"), &Spout::receiver_receive_texture, DEFVAL(0), DEFVAL(0), DEFVAL(false), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("receiver_receive_image", "image", "invert", "host_fbo", "gl_format"), &Spout::receiver_receive_image, DEFVAL(false), DEFVAL(0), DEFVAL(GL_RGBA));
	ClassDB::bind_method(D_METHOD("receiver_is_updated"), &Spout::receiver_is_updated);
	ClassDB::bind_method(D_METHOD("receiver_is_connected"), &Spout::receiver_is_connected);
	ClassDB::bind_method(D_METHOD("receiver_is_frame_new"), &Spout::receiver_is_frame_new);
	ClassDB::bind_method(D_METHOD("receiver_get_sender_name"), &Spout::receiver_get_sender_name);
	ClassDB::bind_method(D_METHOD("receiver_get_sender_size"), &Spout::receiver_get_sender_size);
	ClassDB::bind_method(D_METHOD("receiver_get_sender_fps"), &Spout::receiver_get_sender_fps);
	ClassDB::bind_method(D_METHOD("receiver_get_sender_frame"), &Spout::receiver_get_sender_frame);
	ClassDB::bind_method(D_METHOD("receiver_get_sender_cpu"), &Spout::receiver_get_sender_cpu);
	ClassDB::bind_method(D_METHOD("receiver_get_sender_gldx"), &Spout::receiver_get_sender_gldx);

	ClassDB::bind_method(D_METHOD("set_frame_count", "enabled"), &Spout::set_frame_count);
	ClassDB::bind_method(D_METHOD("disable_frame_count"), &Spout::disable_frame_count);
	ClassDB::bind_method(D_METHOD("is_frame_count_enabled"), &Spout::is_frame_count_enabled);
	ClassDB::bind_method(D_METHOD("hold_fps", "fps"), &Spout::hold_fps);
	ClassDB::bind_method(D_METHOD("set_frame_sync", "sender_name"), &Spout::set_frame_sync);

	ClassDB::bind_method(D_METHOD("write_memory_buffer", "sender_name", "data", "length"), &Spout::write_memory_buffer);
	ClassDB::bind_method(D_METHOD("read_memory_buffer", "sender_name", "max_length"), &Spout::read_memory_buffer);
	ClassDB::bind_method(D_METHOD("create_memory_buffer", "name", "length"), &Spout::create_memory_buffer);
	ClassDB::bind_method(D_METHOD("delete_memory_buffer"), &Spout::delete_memory_buffer);
	ClassDB::bind_method(D_METHOD("get_memory_buffer_size", "name"), &Spout::get_memory_buffer_size);

	ClassDB::bind_method(D_METHOD("enable_spout_log"), &Spout::enable_spout_log);
	ClassDB::bind_method(D_METHOD("enable_spout_log_file", "filename", "append"), &Spout::enable_spout_log_file, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_spout_logs"), &Spout::get_spout_log);
	ClassDB::bind_method(D_METHOD("show_spout_logs"), &Spout::show_spout_logs);
	ClassDB::bind_method(D_METHOD("disable_spout_log"), &Spout::disable_spout_log);
	ClassDB::bind_method(D_METHOD("set_spout_log_level", "level"), &Spout::set_spout_log_level);
	ClassDB::bind_method(D_METHOD("spout_log", "format"), &Spout::spout_log);
	ClassDB::bind_method(D_METHOD("spout_log_verbose", "format"), &Spout::spout_log_verbose);
	ClassDB::bind_method(D_METHOD("spout_log_notice", "format"), &Spout::spout_log_notice);
	ClassDB::bind_method(D_METHOD("spout_log_warning", "format"), &Spout::spout_log_warning);
	ClassDB::bind_method(D_METHOD("spout_log_error", "format"), &Spout::spout_log_error);
	ClassDB::bind_method(D_METHOD("spout_log_fatal", "format"), &Spout::spout_log_fatal);

	ClassDB::bind_method(D_METHOD("is_initialized"), &Spout::is_initialized);
	ClassDB::bind_method(D_METHOD("bind_shared_texture"), &Spout::bind_shared_texture);
	ClassDB::bind_method(D_METHOD("unbind_shared_texture"), &Spout::unbind_shared_texture);
	ClassDB::bind_method(D_METHOD("get_shared_texture_id"), &Spout::get_shared_texture_id);

	ClassDB::bind_method(D_METHOD("get_sender_count"), &Spout::get_sender_count);
	ClassDB::bind_method(D_METHOD("get_sender", "index", "max_size"), &Spout::get_sender, DEFVAL(256));
	ClassDB::bind_method(D_METHOD("find_sender_name", "sender_name"), &Spout::find_sender_name);
	ClassDB::bind_method(D_METHOD("get_active_sender"), &Spout::get_active_sender);
	ClassDB::bind_method(D_METHOD("set_active_sender", "sender_name"), &Spout::set_active_sender);

	ClassDB::bind_method(D_METHOD("get_host_path", "sender_name", "max_chars"), &Spout::get_host_path, DEFVAL(256));
	ClassDB::bind_method(D_METHOD("get_vertical_sync"), &Spout::get_vertical_sync);
	ClassDB::bind_method(D_METHOD("set_vertical_sync", "sync"), &Spout::set_vertical_sync, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_spout_version"), &Spout::get_spout_version);

	ClassDB::bind_method(D_METHOD("get_auto_share"), &Spout::get_auto_share);
	ClassDB::bind_method(D_METHOD("set_auto_share", "is_auto"), &Spout::set_auto_share, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("is_gldx_ready"), &Spout::is_gldx_ready);

	ClassDB::bind_method(D_METHOD("get_num_adapters"), &Spout::get_num_adapters);
	ClassDB::bind_method(D_METHOD("get_adapter_name", "index", "max_chars"), &Spout::get_adapter_name, DEFVAL(256));
	ClassDB::bind_method(D_METHOD("adapter_name"), &Spout::adapter_name);
	ClassDB::bind_method(D_METHOD("get_adapter"), &Spout::get_adapter);

	ClassDB::bind_method(D_METHOD("get_senders"), &Spout::get_senders);
	ClassDB::bind_method(D_METHOD("get_send_size"), &Spout::get_send_size);
	ClassDB::bind_method(D_METHOD("set_send_size", "size"), &Spout::set_send_size);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "send_size"), "set_send_size", "get_send_size");

	BIND_ENUM_CONSTANT(SPOUT_LOG_SILENT);
	BIND_ENUM_CONSTANT(SPOUT_LOG_VERBOSE);
	BIND_ENUM_CONSTANT(SPOUT_LOG_NOTICE);
	BIND_ENUM_CONSTANT(SPOUT_LOG_WARNING);
	BIND_ENUM_CONSTANT(SPOUT_LOG_ERROR);
	BIND_ENUM_CONSTANT(SPOUT_LOG_FATAL);
}

Spout::Spout() {
	spout_lib = GetSpout();
	send_size = Vector2(100.0f, 100.0f);
	if (spout_lib) {
		sender_set_sender_name("Godot_Spout");
	}
}

Spout::~Spout() {
	if (spout_lib) {
		spout_lib->ReleaseSender();
		spout_lib->Release();
		spout_lib = nullptr;
	}
}
