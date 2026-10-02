extends SceneTree


class SampleTexture:
	extends Texture2D

	var image: Image

	func _get_width() -> int:
		return image.get_width()

	func _get_height() -> int:
		return image.get_height()

	func _get_image() -> Image:
		return image


func _initialize() -> void:
	_run.call_deferred()


func _report(probe: String, observation: Dictionary) -> void:
	print("COMPARISON_PROBE " + probe + " " + JSON.stringify(observation))


func _run() -> void:
	_probe_effective_has(Control.new())
	_probe_effective_has(Window.new())
	await _probe_public_type_mutation("rename")
	await _probe_public_type_mutation("remove")
	_probe_invalid_texture()
	quit()


func _probe_effective_has(node) -> void:
	root.add_child(node)
	var theme := Theme.new()
	var scheme := ColorScheme.new()
	scheme.source_color = Color("#ff8800")
	scheme.dark = true
	theme.set_color_scheme("default_color_scheme", node.get_class(), scheme)
	theme.set_color_role("review_only_role", node.get_class(), COLOR_ROLE_PRIMARY)
	theme.set_color_role("review_scaled_role", node.get_class(), COLOR_ROLE_PRIMARY)
	theme.set_color_role("review_scaled_scale_role", node.get_class(), COLOR_ROLE_ON_PRIMARY)
	node.theme = theme
	_report(
		"effective_has_and_scale",
		{
			"class": node.get_class(),
			"role_declared": node.has_theme_color_role("review_only_role"),
			"has_dynamic_color": node.has_theme_color("review_only"),
			"get_dynamic_matches_role":
			node.get_theme_color("review_only").is_equal_approx(scheme.get_primary()),
			"has_dynamic_scale": node.has_theme_color("review_scaled_scale"),
			"get_dynamic_scale_matches_role":
			node.get_theme_color("review_scaled_scale").is_equal_approx(scheme.get_on_primary()),
			"scale_was_applied":
			node.get_theme_color("review_scaled").is_equal_approx(
				scheme.get_primary() * scheme.get_on_primary()
			),
			"actual": node.get_theme_color("review_scaled").to_html(),
			"expected": (scheme.get_primary() * scheme.get_on_primary()).to_html(),
		}
	)
	node.free()


func _probe_public_type_mutation(operation: String) -> void:
	var theme := Theme.new()
	var scheme := ColorScheme.new()
	theme.set_color("review_mutation", "Control", Color.RED)
	theme.set_color_role("review_mutation_role", "Control", COLOR_ROLE_PRIMARY)
	theme.set_color_scheme("review_mutation_scheme", "Control", scheme)
	var node := Control.new()
	root.add_child(node)
	node.theme = theme
	var before := node.get_theme_color("review_mutation")
	var notifications := [0]
	theme.changed.connect(func(): notifications[0] += 1)
	if operation == "rename":
		theme.rename_type("Control", "ReviewOther")
	else:
		theme.remove_type("Control")
	await process_frame
	_report(
		"public_type_mutation",
		{
			"operation": operation,
			"theme_changed_count": notifications[0],
			"cached_color_changed":
			not node.get_theme_color("review_mutation").is_equal_approx(before),
			"direct_remove_role_helper_bound": theme.has_method("remove_color_role_type"),
			"direct_rename_scheme_helper_bound": theme.has_method("rename_color_scheme_type"),
		}
	)
	node.free()


func _probe_invalid_texture() -> void:
	var scheme := ColorScheme.new()
	scheme.source_color = Color.RED
	var texture := SampleTexture.new()
	texture.image = Image.new()
	scheme.source_texture = texture
	var empty_source := scheme.get_color(COLOR_ROLE_STATIC).to_html()
	var transparent := SampleTexture.new()
	transparent.image = Image.create(8, 8, false, Image.FORMAT_RGBA8)
	transparent.image.fill(Color(1, 0, 0, 0))
	scheme.source_texture = transparent
	_report(
		"invalid_texture_fallback",
		{
			"empty_source": empty_source,
			"all_transparent_source": scheme.get_color(COLOR_ROLE_STATIC).to_html(),
		}
	)
