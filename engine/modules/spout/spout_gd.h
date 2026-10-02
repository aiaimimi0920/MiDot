/**************************************************************************/
/*  spout_gd.h                                                            */
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

#include "core/error/error_list.h"
#include "core/io/image.h"
#include "core/math/vector2.h"
#include "core/object/ref_counted.h"
#include "core/variant/array.h"

#include <thirdparty/spout-library/Binaries/x64/SpoutLibrary.h>

class Spout : public RefCounted {
	GDCLASS(Spout, RefCounted);

	Vector2 send_size;

private:
	static constexpr int MAX_TEXT_BUFFER_SIZE = 4096;
	static constexpr int MAX_MEMORY_BUFFER_SIZE = 16 * 1024 * 1024;

	SPOUTLIBRARY *spout_lib = nullptr;
	bool _has_valid_send_size() const;

protected:
	static void _bind_methods();

public:
	enum SpoutLogLevel {
		SPOUT_LOG_SILENT = SpoutLibLogLevel::SPOUT_LOG_SILENT,
		SPOUT_LOG_VERBOSE = SpoutLibLogLevel::SPOUT_LOG_VERBOSE,
		SPOUT_LOG_NOTICE = SpoutLibLogLevel::SPOUT_LOG_NOTICE,
		SPOUT_LOG_WARNING = SpoutLibLogLevel::SPOUT_LOG_WARNING,
		SPOUT_LOG_ERROR = SpoutLibLogLevel::SPOUT_LOG_ERROR,
		SPOUT_LOG_FATAL = SpoutLibLogLevel::SPOUT_LOG_FATAL
	};

	void sender_set_sender_name(const String &p_sender_name = String());
	void sender_release_sender(int p_msec = 0);
	Error sender_send_fbo(const uint32_t p_fbo_id);
	Error sender_send_texture(const uint32_t p_texture_id, const uint32_t p_texture_target, const bool p_invert = true, const int p_host_fbo = 0);
	Error sender_send_image(const Ref<Image> &p_image, const bool p_invert = false, const int p_gl_format = GL_RGBA);
	String sender_get_name();
	double_t sender_get_fps();
	double_t sender_get_frame();
	bool sender_get_cpu();
	bool sender_get_gldx();

	void receiver_set_receiver_name(const String &p_sender_name = String());
	void receiver_release_receiver();
	Error receiver_receive_texture(uint32_t p_texture_id = 0, uint32_t p_texture_target = 0, bool p_invert = false, int p_host_fbo = 0);
	Error receiver_receive_image(const Ref<Image> &p_image, const bool p_invert = false, const int p_host_fbo = 0, const int p_gl_format = GL_RGBA);
	bool receiver_is_updated();
	bool receiver_is_connected();
	bool receiver_is_frame_new();
	String receiver_get_sender_name();
	Vector2 receiver_get_sender_size();
	double_t receiver_get_sender_fps();
	double_t receiver_get_sender_frame();
	bool receiver_get_sender_cpu();
	bool receiver_get_sender_gldx();

	void set_frame_count(bool p_enabled);
	void disable_frame_count();
	bool is_frame_count_enabled();
	void hold_fps(const int p_fps);
	void set_frame_sync(const String &p_sender_name);
	// WaitFrameSync is intentionally not exposed because it blocks the calling thread.

	Error write_memory_buffer(const String &p_sender_name, const PackedByteArray &p_data, const int p_length);
	PackedByteArray read_memory_buffer(const String &p_sender_name, const int p_max_length);
	Error create_memory_buffer(const String &p_name, const int p_length);
	Error delete_memory_buffer();
	int get_memory_buffer_size(const String &p_name);

	void enable_spout_log();
	void enable_spout_log_file(const String &p_filename, const bool p_append = false);
	String get_spout_log();
	void show_spout_logs();
	void disable_spout_log();
	void set_spout_log_level(SpoutLogLevel p_level);
	// The library accepts varargs, but only preformatted messages are safe to expose to scripts.
	void spout_log(const String &p_format);
	void spout_log_verbose(const String &p_format);
	void spout_log_notice(const String &p_format);
	void spout_log_warning(const String &p_format);
	void spout_log_error(const String &p_format);
	void spout_log_fatal(const String &p_format);

	bool is_initialized();
	bool bind_shared_texture();
	bool unbind_shared_texture();
	uint32_t get_shared_texture_id();

	int get_sender_count();
	String get_sender(const int p_index, const int p_max_size = 256);
	bool find_sender_name(const String &p_sender_name);
	String get_active_sender();
	Error set_active_sender(const String &p_sender_name);

	String get_host_path(const String &p_sender_name, const int p_max_chars = 256);
	int get_vertical_sync();
	Error set_vertical_sync(const bool p_sync = true);
	int get_spout_version();

	bool get_auto_share();
	void set_auto_share(const bool p_is_auto = true);
	bool is_gldx_ready();

	int get_num_adapters();
	String get_adapter_name(const int p_index, const int p_max_chars = 256);
	String adapter_name();
	int get_adapter();

	Array get_senders();

	Vector2 get_send_size();
	void set_send_size(Vector2 p_size);

	Spout();
	~Spout() override;
};

VARIANT_ENUM_CAST(Spout::SpoutLogLevel);
