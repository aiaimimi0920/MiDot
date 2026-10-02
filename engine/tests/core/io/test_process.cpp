/**************************************************************************/
/*  test_process.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             Godot Engine                               */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_process)

#include "core/io/process.h"
#include "core/os/os.h"

#ifdef WINDOWS_ENABLED
#include <windows.h>
#endif

namespace TestProcess {

static Ref<Process> create_test_process() {
	Vector<String> arguments;
#ifdef WINDOWS_ENABLED
	arguments.push_back("/C");
	arguments.push_back("echo process-out&1>&2 echo process-err&exit /B 7");
	return Process::create("cmd.exe", arguments);
#else
	arguments.push_back("-c");
	arguments.push_back("printf 'process-out\\n'; printf 'process-err\\n' >&2; exit 7");
	return Process::create("/bin/sh", arguments);
#endif
}

#ifdef WINDOWS_ENABLED
static Ref<Process> create_sleeping_test_process() {
	Vector<String> arguments;
	arguments.push_back("-NoLogo");
	arguments.push_back("-NoProfile");
	arguments.push_back("-NonInteractive");
	arguments.push_back("-Command");
	arguments.push_back("Start-Sleep -Seconds 30");
	return Process::create("powershell.exe", arguments);
}
#endif

static int wait_for_exit(const Ref<Process> &p_process) {
	const uint64_t deadline = OS::get_singleton()->get_ticks_usec() + 10'000'000;
	int exit_status = -1;
	while (OS::get_singleton()->get_ticks_usec() < deadline) {
		exit_status = p_process->get_exit_status();
		if (exit_status != -1) {
			break;
		}
		OS::get_singleton()->delay_usec(1'000);
	}
	return exit_status;
}

TEST_CASE("[Process] Captures output and exit status") {
	Ref<Process> process = create_test_process();
	REQUIRE(process.is_valid());
	CHECK(wait_for_exit(process) == 7);
	CHECK(process->get_available_stdout_lines() == 1);
	CHECK(process->get_stdout_line() == "process-out");
	CHECK(process->get_available_stderr_lines() == 1);
	CHECK(process->get_stderr_line() == "process-err");
	CHECK(process->get_exit_status() == 7);
}

#ifdef WINDOWS_ENABLED
TEST_CASE("[Process] Releases Windows child process handles") {
	DWORD handles_before = 0;
	REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handles_before));

	for (int i = 0; i < 32; i++) {
		Ref<Process> process = create_test_process();
		REQUIRE(process.is_valid());
		CHECK(wait_for_exit(process) == 7);
		process.unref();
	}

	DWORD handles_after = 0;
	REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handles_after));
	CHECK_MESSAGE(handles_after <= handles_before + 4, "Completed Process instances must not retain Windows handles.");
}

TEST_CASE("[Process] Terminates a live Windows child on last reference") {
	Ref<Process> process = create_sleeping_test_process();
	REQUIRE(process.is_valid());
	const ProcessID process_id = process->get_id();
	REQUIRE(OS::get_singleton()->is_process_running(process_id));

	process.unref();

	HANDLE process_handle = OpenProcess(SYNCHRONIZE, false, static_cast<DWORD>(process_id));
	if (process_handle != nullptr) {
		CHECK(WaitForSingleObject(process_handle, 5'000) == WAIT_OBJECT_0);
		CloseHandle(process_handle);
	}
	CHECK_FALSE(OS::get_singleton()->is_process_running(process_id));
}
#endif

} // namespace TestProcess
