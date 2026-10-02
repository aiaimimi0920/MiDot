extends SceneTree


func _initialize() -> void:
	var child = Process.create(
		"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
		PackedStringArray(
			[
				"-NoLogo",
				"-NoProfile",
				"-NonInteractive",
				"-Command",
				"$line=[Console]::In.ReadLine(); [Console]::Out.WriteLine('OUT:' + $line); [Console]::Error.WriteLine('ERR:' + $line); exit 7",
			]
		),
		"",
		true
	)
	if child == null or child.get_id() <= 0:
		_fail("Process.create did not return a valid child process.")
		return
	if not child.write("hello\r\n"):
		_fail("Process.write rejected open standard input.")
		return
	child.close_stdin()

	var deadline = Time.get_ticks_msec() + 5000
	while child.get_exit_status() == -1 and Time.get_ticks_msec() < deadline:
		OS.delay_msec(10)

	var exit_status = child.get_exit_status()
	var stdout_lines: Array[String] = []
	var stderr_lines: Array[String] = []
	while child.get_available_stdout_lines() > 0:
		stdout_lines.append(child.get_stdout_line())
	while child.get_available_stderr_lines() > 0:
		stderr_lines.append(child.get_stderr_line())

	if exit_status != 7:
		_fail("Unexpected child exit status: %s" % [exit_status])
		return
	if "OUT:hello" not in stdout_lines:
		_fail("Missing redirected stdout line: %s" % [stdout_lines])
		return
	if "ERR:hello" not in stderr_lines:
		_fail("Missing redirected stderr line: %s" % [stderr_lines])
		return

	print("PROCESS_API_SMOKE_OK")
	quit(0)


func _fail(message: String) -> void:
	push_error("PROCESS_API_SMOKE_FAILED::%s" % [message])
	quit(1)
