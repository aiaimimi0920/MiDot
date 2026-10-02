extends SceneTree


func _wait_for_exit(process: Process, timeout_msec: int = 5000) -> int:
	var deadline := Time.get_ticks_msec() + timeout_msec
	while Time.get_ticks_msec() < deadline:
		var status := process.get_exit_status()
		if status != -1:
			return status
		OS.delay_msec(10)
	return -1


func _drain_lines(process: Process, stderr: bool) -> PackedStringArray:
	var lines := PackedStringArray()
	var available := (
		process.get_available_stderr_lines() if stderr else process.get_available_stdout_lines()
	)
	while available > 0:
		lines.append(process.get_stderr_line() if stderr else process.get_stdout_line())
		available -= 1
	return lines


func _initialize() -> void:
	var echo_process := (
		Process
		. create(
			"cmd.exe",
			PackedStringArray(
				[
					"/D",
					"/V:ON",
					"/S",
					"/C",
					"set /p line=& echo OUT:!line!& 1>&2 echo ERR:!line!& exit /b 7"
				]
			),
			ProjectSettings.globalize_path("res://"),
			true,
		)
	)
	if echo_process == null:
		push_error("Process.create returned null")
		quit(1)
		return
	if not echo_process.write("hello-process\n"):
		push_error("stdin write failed")
		quit(2)
		return
	echo_process.close_stdin()
	var status := _wait_for_exit(echo_process)
	var stdout := _drain_lines(echo_process, false)
	var stderr := _drain_lines(echo_process, true)
	if (
		status != 7
		or stdout != PackedStringArray(["OUT:hello-process"])
		or stderr != PackedStringArray(["ERR:hello-process"])
	):
		push_error(
			"unexpected process result: status=%d stdout=%s stderr=%s" % [status, stdout, stderr]
		)
		quit(3)
		return

	var kill_process := Process.create(
		"cmd.exe", PackedStringArray(["/D", "/C", "ping -n 30 127.0.0.1 > nul"])
	)
	if kill_process == null or kill_process.get_id() <= 0:
		push_error("kill target could not be created")
		quit(4)
		return
	var kill_pid := kill_process.get_id()
	kill_process.kill(true)
	var kill_deadline := Time.get_ticks_msec() + 5000
	while OS.is_process_running(kill_pid) and Time.get_ticks_msec() < kill_deadline:
		OS.delay_msec(10)
	if OS.is_process_running(kill_pid):
		push_error("killed process did not exit")
		quit(5)
		return

	print("PROCESS_SMOKE_OK")
	quit(0)
