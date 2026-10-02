extends Node

var _checks: Array[Dictionary] = []
var _output := ""


func _ready() -> void:
	var arguments := OS.get_cmdline_user_args()
	if arguments.size() != 1:
		get_tree().quit(2)
		return
	_output = arguments[0]
	call_deferred("_run")


func _check(passed: bool, label: String) -> void:
	_checks.append({"check": label, "pass": passed})


func _run() -> void:
	for class_name_to_check in [
		"Process",
		"ImageFrames",
		"GifExporter",
		"Spout",
		"SpineSprite",
		"SpineAtlasResource",
		"SpineSkeletonFileResource",
		"ColorScheme",
		"ColorRoleTransform"
	]:
		_check(ClassDB.class_exists(class_name_to_check), "Registered " + class_name_to_check)
	for pair in [
		["Script", "get_script_documentation_list"],
		["TextureButton", "has_point"],
		["TextureButton", "set_text_normal"],
		["StyleBoxFlat", "set_elevation_level"],
		["Button", "set_text_icon"],
		["Control", "get_theme_color_role"],
		["Control", "add_theme_string_override"],
		["Window", "add_theme_string_override"],
		["Theme", "set_string"],
		["ThemeDB", "get_fallback_icon_font"],
		["ColorRoleTransform", "resolve"]
	]:
		_check(ClassDB.class_has_method(pair[0], pair[1]), "%s.%s" % pair)
	_check(
		ProjectSettings.has_setting("rendering/textures/streaming/enabled"),
		"Texture streaming settings registered"
	)
	_check(ProjectSettings.has_setting("gui/theme/custom_icon_font"), "Custom icon font setting")
	_check_ui()
	_check_gif()
	_check_helper()
	_check_spout()
	await _check_process()
	var failures := _checks.filter(func(row: Dictionary) -> bool: return not row["pass"])
	var file := FileAccess.open(_output.path_join("checks.json"), FileAccess.WRITE)
	file.store_string(
		JSON.stringify(
			{
				"executable": OS.get_executable_path(),
				"engine": Engine.get_version_info(),
				"editor_features": OS.has_feature("editor"),
				"checks": _checks,
				"failures": failures.size()
			},
			"  "
		)
	)
	file.close()
	print("NON_NPR_CHECKS ", _checks.size(), " FAILURES ", failures.size())
	for failure in failures:
		print("CHECK_FAILED ", failure["check"])
	get_tree().quit(0 if failures.is_empty() else 1)


func _check_ui() -> void:
	var theme := Theme.new()
	theme.set_string("probe", "Control", "from_theme")
	_check(theme.get_string("probe", "Control") == "from_theme", "Theme string round trip")
	var control := Control.new()
	control.theme = theme
	add_child(control)
	control.add_theme_string_override("probe", "override")
	_check(control.get_theme_string("probe") == "override", "Control string override")
	control.free()
	var button := Button.new()
	button.set_text_icon("+")
	_check(button.get_text_icon() == "+", "Button text icon round trip")
	button.free()
	var texture_button := TextureButton.new()
	texture_button.size = Vector2(16, 16)
	texture_button.set_text_normal("OK")
	_check(texture_button.get_text_normal() == "OK", "TextureButton state text round trip")
	_check(texture_button.has_point(Vector2(1, 1)), "TextureButton accepts an inside point")
	_check(not texture_button.has_point(Vector2(-1, -1)), "TextureButton rejects outside point")
	texture_button.free()
	var style := StyleBoxFlat.new()
	style.set_elevation_level(2)
	_check(style.get_elevation_level() == 2, "StyleBox elevation property")
	var colors := ColorScheme.new()
	var light := colors.get_primary()
	colors.set_dark(true)
	_check(not light.is_equal_approx(colors.get_primary()), "Material light/dark color generation")


func _check_gif() -> void:
	var path := _output.path_join("probe.gif")
	var exporter := GifExporter.new()
	_check(exporter.begin_export(path, 8, 8, 10), "GIF export begins")
	for color in [Color.RED, Color.BLUE]:
		var frame := Image.create(8, 8, false, Image.FORMAT_RGBA8)
		frame.fill(color)
		_check(exporter.write_frame(frame, Color.BLACK, 10), "GIF frame accepted")
	_check(exporter.end_export(), "GIF export completes")
	var decoded := ImageFrames.new()
	_check(decoded.load(path) == OK, "GIF loads through ImageFrames")
	_check(decoded.get_frame_count() == 2, "GIF round trip preserves both frames")
	if decoded.get_frame_count() == 2:
		_check(decoded.get_frame_image(0).get_pixel(0, 0).r > 0.9, "GIF first frame stays red")
		_check(decoded.get_frame_image(1).get_pixel(0, 0).b > 0.9, "GIF second frame stays blue")


func _check_helper() -> void:
	var has_helper := ClassDB.class_exists("GDScriptHelper")
	_check(has_helper == OS.has_feature("editor"), "GDScriptHelper obeys editor-only scope")
	if has_helper:
		var helper: RefCounted = ClassDB.instantiate("GDScriptHelper")
		var valid: bool = helper.call(
			"set_validate_code", "extends RefCounted\nfunc answer() -> int:\n\treturn 7\n"
		)
		_check(valid, "GDScriptHelper accepts valid code")
		var invalid: bool = helper.call("set_validate_code", "extends RefCounted\nfunc !!!\n")
		_check(not invalid and helper.call("has_errors"), "GDScriptHelper reports invalid code")


func _check_spout() -> void:
	var bridge := Spout.new()
	_check(bridge.get_spout_version() > 0, "Spout SDK linked")
	var name := "CustomEngineProbe_%d" % OS.get_process_id()
	var payload := PackedByteArray([1, 7, 42, 255])
	var created := bridge.create_memory_buffer(name, 64) == OK
	_check(created, "Spout memory buffer created")
	if created:
		_check(
			bridge.write_memory_buffer(name, payload, payload.size()) == OK, "Spout buffer write"
		)
		_check(
			bridge.read_memory_buffer(name, 64).slice(0, 4) == payload, "Spout memory round trip"
		)
		_check(bridge.delete_memory_buffer() == OK, "Spout memory buffer released")


func _check_process() -> void:
	var shell := OS.get_environment("COMSPEC")
	var arguments := PackedStringArray(
		["/D", "/C", "echo ENGINE_STDOUT&echo ENGINE_STDERR 1>&2&exit /b 7"]
	)
	var child := Process.create(shell, arguments, _output, false)
	_check(child != null and child.get_id() > 0, "Process child starts")
	if child == null:
		return
	var deadline := Time.get_ticks_msec() + 10000
	while child.get_exit_status() == -1 and Time.get_ticks_msec() < deadline:
		await get_tree().create_timer(0.02).timeout
	var exit_status := child.get_exit_status()
	if exit_status == -1:
		child.kill(true)
	_check(exit_status == 7, "Process preserves exit code")
	var stdout_lines: Array[String] = []
	var stderr_lines: Array[String] = []
	while child.get_available_stdout_lines() > 0:
		stdout_lines.append(child.get_stdout_line().strip_edges())
	while child.get_available_stderr_lines() > 0:
		stderr_lines.append(child.get_stderr_line().strip_edges())
	_check("ENGINE_STDOUT" in stdout_lines, "Process stdout captured")
	_check("ENGINE_STDERR" in stderr_lines, "Process stderr captured")
