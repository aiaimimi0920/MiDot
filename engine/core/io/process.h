/**************************************************************************/
/*  process.h                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                          */
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

#include "core/io/file_access.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/process_id.h"

class Process : public RefCounted {
	GDCLASS(Process, RefCounted);

	mutable Mutex mutex;
	mutable Vector<String> stdout_lines;
	mutable Vector<String> stderr_lines;
	mutable String stdout_remainder;
	mutable String stderr_remainder;

	Ref<FileAccess> stdio;
	Ref<FileAccess> stderr_pipe;
	ProcessID process_id = -1;
	bool stdin_open = false;

	static void _read_available(const Ref<FileAccess> &p_pipe, String &r_remainder, Vector<String> &r_lines);
	void _pump_output() const;
	void _flush_remainders_if_exited() const;

protected:
	static void _bind_methods();

public:
	~Process();

	static Ref<Process> create(const String &p_path, const Vector<String> &p_arguments = Vector<String>(), const String &p_working_directory = String(), bool p_open_stdin = false);

	int get_available_stdout_lines() const;
	int get_available_stderr_lines() const;
	String get_stdout_line();
	String get_stderr_line();

	int get_exit_status() const;
	int64_t get_id() const;
	void kill(bool p_force = false);
	bool write(const String &p_input);
	void close_stdin();
};
