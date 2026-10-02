/**************************************************************************/
/*  process.cpp                                                           */
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

#include "process.h"

#include "core/object/class_db.h"
#include "core/os/os.h"

void Process::_bind_methods() {
	ClassDB::bind_static_method("Process", D_METHOD("create", "path", "arguments", "working_directory", "open_stdin"), &Process::create, DEFVAL(Vector<String>()), DEFVAL(String()), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("get_available_stdout_lines"), &Process::get_available_stdout_lines);
	ClassDB::bind_method(D_METHOD("get_stdout_line"), &Process::get_stdout_line);
	ClassDB::bind_method(D_METHOD("get_available_stderr_lines"), &Process::get_available_stderr_lines);
	ClassDB::bind_method(D_METHOD("get_stderr_line"), &Process::get_stderr_line);
	ClassDB::bind_method(D_METHOD("get_exit_status"), &Process::get_exit_status);
	ClassDB::bind_method(D_METHOD("kill", "force"), &Process::kill, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_id"), &Process::get_id);
	ClassDB::bind_method(D_METHOD("write", "input"), &Process::write);
	ClassDB::bind_method(D_METHOD("close_stdin"), &Process::close_stdin);
}

void Process::_read_available(const Ref<FileAccess> &p_pipe, String &r_remainder, Vector<String> &r_lines) {
	if (p_pipe.is_null() || !p_pipe->is_open()) {
		return;
	}

	const uint64_t available = p_pipe->get_length();
	if (available == 0) {
		return;
	}

	Vector<uint8_t> bytes;
	bytes.resize(available);
	const uint64_t read = p_pipe->get_buffer(bytes.ptrw(), available);
	if (read == 0) {
		return;
	}

	String chunk = String::utf8(reinterpret_cast<const char *>(bytes.ptr()), read);
	r_remainder += chunk.replace("\r\n", "\n").replace_char('\r', '\n');

	int newline = r_remainder.find_char('\n');
	while (newline >= 0) {
		r_lines.push_back(r_remainder.left(newline));
		r_remainder = r_remainder.substr(newline + 1);
		newline = r_remainder.find_char('\n');
	}
}

void Process::_pump_output() const {
	MutexLock lock(mutex);
	_read_available(stdio, stdout_remainder, stdout_lines);
	_read_available(stderr_pipe, stderr_remainder, stderr_lines);
}

void Process::_flush_remainders_if_exited() const {
	if (OS::get_singleton()->is_process_running(process_id)) {
		return;
	}

	MutexLock lock(mutex);
	_read_available(stdio, stdout_remainder, stdout_lines);
	_read_available(stderr_pipe, stderr_remainder, stderr_lines);
	if (!stdout_remainder.is_empty()) {
		stdout_lines.push_back(stdout_remainder);
		stdout_remainder.clear();
	}
	if (!stderr_remainder.is_empty()) {
		stderr_lines.push_back(stderr_remainder);
		stderr_remainder.clear();
	}
}

Process::~Process() {
	OS *os = OS::get_singleton();
	if (os == nullptr || process_id <= 0) {
		return;
	}
	bool is_running = os->is_process_running(process_id);
	if (is_running) {
		os->kill(process_id);
		is_running = os->is_process_running(process_id);
	}
	if (!is_running) {
		os->release_process(process_id);
	}
}

Ref<Process> Process::create(const String &p_path, const Vector<String> &p_arguments, const String &p_working_directory, bool p_open_stdin) {
	List<String> arguments;
	for (const String &argument : p_arguments) {
		arguments.push_back(argument);
	}

	const String working_directory = p_working_directory.is_empty() ? OS::get_singleton()->get_executable_path().get_base_dir() : p_working_directory;
	const Dictionary handles = OS::get_singleton()->execute_with_pipe(p_path, arguments, false, working_directory);
	ERR_FAIL_COND_V_MSG(handles.is_empty(), Ref<Process>(), "Unable to create child process.");
	ERR_FAIL_COND_V_MSG(!handles.has("stdio") || !handles.has("stderr") || !handles.has("pid"), Ref<Process>(), "Child process creation returned incomplete pipe handles.");

	Ref<FileAccess> stdio = handles["stdio"];
	Ref<FileAccess> stderr_pipe = handles["stderr"];
	const ProcessID process_id = handles["pid"];
	ERR_FAIL_COND_V_MSG(stdio.is_null() || stderr_pipe.is_null() || process_id <= 0, Ref<Process>(), "Child process creation returned invalid pipe handles.");

	Ref<Process> process;
	process.instantiate();
	process->stdio = stdio;
	process->stderr_pipe = stderr_pipe;
	process->process_id = process_id;
	process->stdin_open = p_open_stdin;
	if (!p_open_stdin) {
		process->stdio->close_write();
	}
	return process;
}

int Process::get_available_stdout_lines() const {
	_pump_output();
	_flush_remainders_if_exited();
	MutexLock lock(mutex);
	return stdout_lines.size();
}

int Process::get_available_stderr_lines() const {
	_pump_output();
	_flush_remainders_if_exited();
	MutexLock lock(mutex);
	return stderr_lines.size();
}

String Process::get_stdout_line() {
	_pump_output();
	_flush_remainders_if_exited();
	MutexLock lock(mutex);
	if (stdout_lines.is_empty()) {
		return String();
	}
	const String line = stdout_lines[0];
	stdout_lines.remove_at(0);
	return line;
}

String Process::get_stderr_line() {
	_pump_output();
	_flush_remainders_if_exited();
	MutexLock lock(mutex);
	if (stderr_lines.is_empty()) {
		return String();
	}
	const String line = stderr_lines[0];
	stderr_lines.remove_at(0);
	return line;
}

int Process::get_exit_status() const {
	_pump_output();
	return OS::get_singleton()->get_process_exit_code(process_id);
}

int64_t Process::get_id() const {
	return process_id;
}

void Process::kill(bool p_force) {
	(void)p_force;
	OS::get_singleton()->kill(process_id);
}

bool Process::write(const String &p_input) {
	ERR_FAIL_COND_V_MSG(!stdin_open || stdio.is_null() || !stdio->is_open(), false, "Standard input is not open.");
	const CharString input = p_input.utf8();
	return stdio->store_buffer(reinterpret_cast<const uint8_t *>(input.ptr()), input.length());
}

void Process::close_stdin() {
	if (!stdin_open || stdio.is_null()) {
		return;
	}
	stdio->close_write();
	stdin_open = false;
}
