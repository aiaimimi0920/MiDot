extends SceneTree


class CountingTexture:
	extends Texture2D

	var image: Image
	var reads := 0

	func set_sample(color: Color) -> void:
		image = Image.create(8, 8, false, Image.FORMAT_RGBA8)
		image.fill(color)
		emit_changed()

	func _get_width() -> int:
		return image.get_width()

	func _get_height() -> int:
		return image.get_height()

	func _get_image() -> Image:
		reads += 1
		return image


func _initialize() -> void:
	_run.call_deferred()


func _report(probe: String, observation: Dictionary) -> void:
	print("REVIEW_PROBE " + probe + " " + JSON.stringify(observation))


func _run() -> void:
	_probe_static_override(Control.new())
	_probe_static_override(Window.new())
	_probe_roundtrip(Button.new(), "font_color_role", "font_color_scheme", "font_color_scale")
	_probe_roundtrip(Window.new(), "title_color_role", "title_color_scheme", "title_color_scale")
	_probe_roundtrip(
		AcceptDialog.new(), "title_color_role", "title_color_scheme", "title_color_scale"
	)
	_probe_texture_changes()
	_probe_null_editor_history()
	_probe_static_style_identity()
	_probe_default_metadata()
	_probe_lookup_timing()
	quit()


func _probe_static_override(node) -> void:
	root.add_child(node)
	var theme := Theme.new()
	var scheme := ColorScheme.new()
	scheme.source_color = Color("#0088ee")
	var authored := Color("#123456")
	theme.set_color("review_accent", node.get_class(), authored)
	theme.set_color_role("review_accent_role", node.get_class(), COLOR_ROLE_PRIMARY)
	theme.set_color_scheme("review_accent_scheme", node.get_class(), scheme)
	node.theme = theme
	node.add_theme_color_role_override("review_accent_role", COLOR_ROLE_STATIC)
	_report(
		"static_override",
		{
			"class": node.get_class(),
			"reported_role_is_static":
			node.get_theme_color_role("review_accent_role") == COLOR_ROLE_STATIC,
			"resolved_equals_authored":
			node.get_theme_color("review_accent").is_equal_approx(authored),
			"resolved": node.get_theme_color("review_accent").to_html(),
			"authored": authored.to_html(),
		}
	)
	node.free()


func _probe_roundtrip(node, role_name: String, scheme_name: String, scale_name: String) -> void:
	root.add_child(node)
	var scheme := ColorScheme.new()
	scheme.source_color = Color("#ee8800")
	node.add_theme_color_role_override(role_name, COLOR_ROLE_ERROR)
	node.add_theme_color_scheme_override(scheme_name, scheme)
	node.add_theme_color_scheme_override("default_color_scheme", scheme)
	node.add_theme_color_override(scale_name, Color(0.5, 0.5, 0.5, 0.5))
	var storage_names: Array[String] = []
	for property in node.get_property_list():
		if int(property.usage) & PROPERTY_USAGE_STORAGE:
			storage_names.append(str(property.name))
	var packed := PackedScene.new()
	var error := packed.pack(node)
	var copy = packed.instantiate()
	_report(
		"override_roundtrip",
		{
			"class": node.get_class(),
			"pack_error": error,
			"role_listed": storage_names.has("theme_override_color_roles/" + role_name),
			"scheme_listed": storage_names.has("theme_override_color_schemes/" + scheme_name),
			"role_preserved": copy.has_theme_color_role_override(role_name),
			"scheme_preserved": copy.has_theme_color_scheme_override(scheme_name),
			"default_scheme_preserved":
			copy.has_theme_color_scheme_override("default_color_scheme"),
			"scale_preserved": copy.has_theme_color_override(scale_name),
		}
	)
	copy.free()
	node.free()


func _probe_texture_changes() -> void:
	var texture := CountingTexture.new()
	texture.set_sample(Color.RED)
	var scheme := ColorScheme.new()
	scheme.source_texture = texture
	var before := scheme.get_primary()
	var updates := [0]
	scheme.updated_color_scheme.connect(func(): updates[0] += 1)
	var initial_reads := texture.reads
	texture.set_sample(Color.BLUE)
	var after := scheme.get_primary()
	_report(
		"texture_changed",
		{
			"reads_before": initial_reads,
			"reads_after_changed": texture.reads,
			"scheme_updates": updates[0],
			"primary_unchanged": before.is_equal_approx(after),
		}
	)
	scheme.dark = true
	var dark_reads := texture.reads
	scheme.contrast_level = 0.25
	_report(
		"texture_reextraction",
		{
			"reads_after_dark": dark_reads,
			"reads_after_contrast": texture.reads,
		}
	)


func _probe_null_editor_history() -> void:
	var theme := Theme.new()
	theme.set_color_scheme("font_color_scheme", "Button", null)
	var property_name := "Button/color_schemes/font_color_scheme"
	var raw_before_is_null: bool = theme.get(property_name) == null
	# Replays the exact remove/undo values used by ThemeTypeEditor.
	var editor_undo_value := theme.get_color_scheme("font_color_scheme", "Button")
	var history := UndoRedo.new()
	history.create_action("Review editor remove ColorScheme item")
	history.add_do_method(theme.clear_color_scheme.bind("font_color_scheme", "Button"))
	history.add_undo_method(
		theme.set_color_scheme.bind("font_color_scheme", "Button", editor_undo_value)
	)
	history.commit_action()
	history.undo()
	_report(
		"editor_null_undo_values",
		{
			"raw_before_is_null": raw_before_is_null,
			"resolved_undo_value_is_null": editor_undo_value == null,
			"raw_after_undo_is_null": theme.get(property_name) == null,
		}
	)
	history.free()


func _probe_static_style_identity() -> void:
	var theme := Theme.new()
	var source := StyleBoxFlat.new()
	source.bg_color = Color("#123456")
	theme.set_stylebox("review_style", "Control", source)
	var a := Control.new()
	var b := Control.new()
	root.add_child(a)
	root.add_child(b)
	a.theme = theme
	b.theme = theme
	var resolved_a := a.get_theme_stylebox("review_style")
	var resolved_b := b.get_theme_stylebox("review_style")
	_report(
		"static_style_identity",
		{
			"all_roles_static": source.bg_color_role == COLOR_ROLE_STATIC,
			"a_is_source": resolved_a == source,
			"b_is_source": resolved_b == source,
			"a_is_b": resolved_a == resolved_b,
			"same_lookup_reuses_a": resolved_a == a.get_theme_stylebox("review_style"),
		}
	)
	a.free()
	b.free()


func _probe_default_metadata() -> void:
	var theme := ThemeDB.get_default_theme()
	var orphan_roles: Array[String] = []
	for type_name in theme.get_color_role_type_list():
		for role_name in theme.get_color_role_list(type_name):
			var color_name: String = role_name.trim_suffix("_role")
			if not theme.has_color(color_name, type_name):
				orphan_roles.append(type_name + "/" + role_name)
	orphan_roles.sort()
	var label := RichTextLabel.new()
	root.add_child(label)
	var local_scheme := ColorScheme.new()
	local_scheme.source_color = Color("#ee4400")
	local_scheme.dark = true
	label.add_theme_color_scheme_override("default_color_scheme", local_scheme)
	_report(
		"default_metadata",
		{
			"exact_type_orphan_roles": orphan_roles,
			"rich_text_has_default_color_role": label.has_theme_color_role("default_color_role"),
			"rich_text_has_font_default_color_role":
			label.has_theme_color_role("font_default_color_role"),
			"rich_text_uses_on_primary":
			label.get_theme_color("default_color").is_equal_approx(local_scheme.get_on_primary()),
			"rich_text_actual": label.get_theme_color("default_color").to_html(),
			"rich_text_expected": local_scheme.get_on_primary().to_html(),
		}
	)
	label.free()


func _probe_lookup_timing() -> void:
	var node := Control.new()
	root.add_child(node)
	var theme := Theme.new()
	var scheme := ColorScheme.new()
	theme.set_color("review_color", "Control", Color.WHITE)
	theme.set_color_role("review_color_role", "Control", COLOR_ROLE_PRIMARY)
	theme.set_color_scheme("review_color_scheme", "Control", scheme)
	node.theme = theme
	var observations := {}
	for mode in ["theme_cached", "local_role_override"]:
		if mode == "local_role_override":
			node.add_theme_color_role_override("review_color_role", COLOR_ROLE_PRIMARY)
		node.get_theme_color("review_color")
		var samples: Array[int] = []
		for trial in range(3):
			var start := Time.get_ticks_usec()
			for index in range(2000):
				node.get_theme_color("review_color")
			samples.append(Time.get_ticks_usec() - start)
		observations[mode + "_2000_calls_us"] = samples
	_report("lookup_timing_dev_build", observations)
	node.free()
