class_name DynamicThemeSmoke
extends SceneTree

const CASES := {
	"Foldable": ["font_color", "title_panel", COLOR_ROLE_ON_PRIMARY],
	"Tabs": ["icon_selected_color", "tab_selected", COLOR_ROLE_ON_PRIMARY],
	"Spin": ["up_icon_modulate", "up_background", COLOR_ROLE_ON_PRIMARY],
	"Split": ["touch_dragger_color", "split_bar_background", COLOR_ROLE_ON_SURFACE],
	"VerticalSplit": ["touch_dragger_color", "split_bar_background", COLOR_ROLE_ON_SURFACE],
	"Scroll": ["scroll_hint_vertical_color", "panel", COLOR_ROLE_ON_SURFACE],
	"RichText": ["default_color", "normal", COLOR_ROLE_ON_PRIMARY],
	"Graph": ["activity", "panel", COLOR_ROLE_PRIMARY],
	"Graph/Frame": ["resizer_color", "panel", COLOR_ROLE_ON_PRIMARY],
	"HorizontalSeparator": ["", "separator", COLOR_ROLE_STATIC],
	"VerticalSeparator": ["", "separator", COLOR_ROLE_STATIC],
}


class ScriptedStyle:
	extends StyleBoxFlat


var _checks := 0
var _failures: Array[String] = []
var _panel: Control
var _palette := ColorScheme.new()
var _node_theme := Theme.new()
var _caption: Label
var _color_rect: ColorRect
var _output_path := "res://output"


func _initialize() -> void:
	if not OS.has_feature("editor"):
		_output_path = OS.get_executable_path().get_base_dir().path_join("output")
	_run.call_deferred()


func _check(condition: bool, description: String) -> void:
	_checks += 1
	if not condition:
		_failures.append(description)
		push_error(description)


func _frames() -> void:
	for index in range(3):
		await process_frame
	await RenderingServer.frame_post_draw


func _run() -> void:
	DirAccess.make_dir_recursive_absolute(_output_path)
	root.size = Vector2i(1100, 850)
	root.gui_embed_subwindows = true
	_build_scene()
	await _frames()
	await _test_interaction_states()
	await _test_consumers()
	await _test_color_rect()
	await _test_texture()
	await _test_roundtrip()
	for dark in [false, true]:
		for contrast in [-1.0, 0.0, 1.0]:
			_palette.dark = dark
			_palette.contrast_level = contrast
			_panel.get_node("Foldable").folded = contrast > 0.0
			_panel.get_node("Foldable").title_position = 1 if dark else 0
			_panel.get_node("Tabs").current_tab = 1 if dark else 0
			_panel.get_node("Tabs").grab_focus()
			_caption.text = "Dynamic theme | dark=%s | contrast=%s" % [dark, contrast]
			await _frames()
			_test_resolved_colors()
			var mode := "dark" if dark else "light"
			_capture("%s_%s" % [mode, int(contrast)])
	print(
		"DYNAMIC_THEME_RUNTIME ",
		(
			JSON
			. stringify(
				{
					"checks": _checks,
					"failures": _failures,
					"display": DisplayServer.get_name(),
					"version": Engine.get_version_info().string,
					"hash": Engine.get_version_info().hash,
				}
			)
		)
	)
	_panel.free()
	quit(0 if _failures.is_empty() else 1)


func _capture(name: String) -> void:
	var image := root.get_texture().get_image()
	_check(not image.is_empty(), "Display backend produced pixels: " + name)
	_check(image.save_png(_output_path.path_join(name + ".png")) == OK, "Save screenshot: " + name)


func _move_pointer(at: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.position = at
	event.global_position = at
	root.push_input(event, true)
	await _frames()


func _mouse_button(at: Vector2, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.position = at
	event.global_position = at
	event.button_index = MOUSE_BUTTON_LEFT
	event.button_mask = MOUSE_BUTTON_MASK_LEFT if pressed else 0
	event.pressed = pressed
	root.push_input(event, true)
	await _frames()


func _click(at: Vector2) -> void:
	await _move_pointer(at)
	await _mouse_button(at, true)
	await _mouse_button(at, false)


func _test_interaction_states() -> void:
	var fold: FoldableContainer = _panel.get_node("Foldable")
	var title_at := fold.global_position + Vector2(32, 12)
	await _move_pointer(title_at)
	_check(root.gui_get_hovered_control() == fold, "Foldable title receives real pointer input")
	fold.grab_focus()
	await _frames()
	_check(fold.has_focus(), "Foldable title receives keyboard focus")
	_capture("foldable-hover-focus")
	await _click(title_at)
	_check(fold.folded, "Click folds the hovered title")
	_capture("foldable-collapsed-hover")
	await _click(title_at)
	_check(not fold.folded, "Second click expands the title")
	fold.title_position = FoldableContainer.POSITION_BOTTOM
	await _frames()
	title_at = fold.global_position + Vector2(32, fold.size.y - 12)
	await _click(title_at)
	_check(fold.folded, "Bottom-positioned title responds to input")
	_capture("foldable-bottom-collapsed")
	await _click(fold.global_position + Vector2(32, fold.size.y - 12))
	_check(not fold.folded, "Bottom-positioned title expands")
	fold.title_position = FoldableContainer.POSITION_TOP
	var tabs: TabBar = _panel.get_node("Tabs")
	await _click(tabs.global_position + tabs.get_tab_rect(1).get_center())
	_check(tabs.current_tab == 1, "Click selects the second tab")
	await _click(tabs.global_position + tabs.get_tab_rect(2).get_center())
	_check(tabs.current_tab == 1, "Disabled tab ignores selection")
	_capture("tabs-disabled-hover")
	var spin: SpinBox = _panel.get_node("Spin")
	var up_at := spin.global_position + Vector2(spin.size.x - 8, spin.size.y * 0.25)
	var previous := spin.value
	await _move_pointer(up_at)
	await _mouse_button(up_at, true)
	_check(spin.value == previous + spin.step, "SpinBox up button increments on press")
	_capture("spinbox-pressed")
	await _mouse_button(up_at, false)
	spin.editable = false
	await _click(up_at)
	_check(spin.value == previous + spin.step, "Disabled SpinBox ignores clicks")
	_capture("spinbox-disabled")
	spin.editable = true
	spin.value = previous
	var scroll: ScrollContainer = _panel.get_node("Scroll")
	scroll.scroll_vertical = 160
	await _frames()
	_check(scroll.scroll_vertical > 0, "ScrollContainer renders a scrolled state")
	_capture("scroll-hints")
	await _move_pointer(Vector2(1090, 840))


func _place(node: Control, node_name: String, at: Vector2, dimensions: Vector2) -> Control:
	node.name = node_name
	_panel.add_child(node)
	node.owner = _panel
	node.position = at
	node.size = dimensions
	return node


func _build_scene() -> void:
	_panel = Control.new()
	_panel.name = "DynamicThemeSmoke"
	root.add_child(_panel)
	_panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_palette.source_color = Color("#6750a4")
	_node_theme.default_color_scheme = _palette
	_panel.theme = _node_theme
	var background := ColorRect.new()
	background.color_role = COLOR_ROLE_PRIMARY
	_place(background, "Background", Vector2.ZERO, Vector2(1100, 850))
	_caption = _place(Label.new(), "Caption", Vector2(20, 15), Vector2(1060, 45))
	var fold := FoldableContainer.new()
	fold.title = "FoldableContainer: focus, fold, title position"
	_place(fold, "Foldable", Vector2(20, 80), Vector2(500, 150))
	var content := Label.new()
	content.text = "Inherited dynamic theme\nAuthored resources remain shared"
	fold.add_child(content)
	content.owner = _panel
	var tabs := TabBar.new()
	_place(tabs, "Tabs", Vector2(20, 250), Vector2(500, 42))
	var icon_image := Image.create(16, 16, false, Image.FORMAT_RGBA8)
	icon_image.fill(Color.WHITE)
	var icon := ImageTexture.create_from_image(icon_image)
	for title in ["Selected", "Hovered / focus", "Disabled", "Scroll tab"]:
		tabs.add_tab(title, icon)
	tabs.set_tab_disabled(2, true)
	var spin := SpinBox.new()
	spin.value = 42
	_place(spin, "Spin", Vector2(20, 315), Vector2(240, 45))
	var button := Button.new()
	button.text = "Button"
	_place(button, "Button", Vector2(300, 315), Vector2(220, 45))
	var split := HSplitContainer.new()
	_place(split, "Split", Vector2(20, 390), Vector2(500, 130))
	for text in ["Horizontal split left", "Horizontal split right"]:
		var label := Label.new()
		label.text = text
		label.custom_minimum_size = Vector2(190, 110)
		split.add_child(label)
		label.owner = _panel
	var vertical := VSplitContainer.new()
	_place(vertical, "VerticalSplit", Vector2(855, 380), Vector2(220, 240))
	for text in ["Vertical split top", "Vertical split bottom"]:
		var label := Label.new()
		label.text = text
		label.custom_minimum_size = Vector2(210, 95)
		vertical.add_child(label)
		label.owner = _panel
	var scroll := ScrollContainer.new()
	_place(scroll, "Scroll", Vector2(550, 380), Vector2(275, 240))
	var rows := VBoxContainer.new()
	scroll.add_child(rows)
	rows.owner = _panel
	for index in range(18):
		var label := Label.new()
		label.text = "ScrollContainer row %02d" % index
		rows.add_child(label)
		label.owner = _panel
	var rich := RichTextLabel.new()
	rich.text = "RichTextLabel body follows default_color\nAuthored overrides are preserved."
	_place(rich, "RichText", Vector2(20, 560), Vector2(500, 90))
	_place(HSeparator.new(), "HorizontalSeparator", Vector2(20, 665), Vector2(500, 12))
	_place(VSeparator.new(), "VerticalSeparator", Vector2(532, 380), Vector2(12, 270))
	_color_rect = _place(ColorRect.new(), "ColorRect", Vector2(550, 700), Vector2(525, 90))
	_color_rect.color = Color("#123456")
	_color_rect.color_role = COLOR_ROLE_PRIMARY
	_build_graph()


func _build_graph() -> void:
	var graph := GraphEdit.new()
	_place(graph, "Graph", Vector2(550, 80), Vector2(525, 270))
	var frame := GraphFrame.new()
	frame.name = "Frame"
	frame.title = "GraphFrame"
	graph.add_child(frame)
	frame.owner = _panel
	frame.position_offset = Vector2(10, 40)
	frame.size = Vector2(460, 160)
	for index in range(2):
		var node := GraphNode.new()
		node.name = "Port%d" % index
		node.title = node.name
		graph.add_child(node)
		node.owner = _panel
		node.position_offset = Vector2(35 + index * 245, 75)
		var label := Label.new()
		label.text = "Active connection"
		node.add_child(label)
		label.owner = _panel
		node.set_slot(0, true, 0, Color.WHITE, true, 0, Color.WHITE)
	graph.connect_node("Port0", 0, "Port1", 0)
	graph.set_connection_activity("Port0", 0, "Port1", 0, 1.0)


func _expected_color(node: Control, key: String, role: int) -> Color:
	var expected: Color = node.get_theme_color_scheme(key + "_scheme").get_color(role)
	if node.has_theme_color(key + "_scale"):
		expected *= node.get_theme_color(key + "_scale")
	return expected


func _test_resolved_colors() -> void:
	for path in CASES:
		var node: Control = _panel.get_node(path)
		var key: String = CASES[path][0]
		if key.is_empty():
			continue
		_check(node.get_theme_color_role(key + "_role") == CASES[path][2], path + " role mapping")
		_check(
			node.get_theme_color(key).is_equal_approx(_expected_color(node, key, CASES[path][2])),
			path + " resolved color"
		)


func _test_consumers() -> void:
	_test_resolved_colors()
	var scripted := ScriptedStyle.new()
	var button: Button = _panel.get_node("Button")
	button.add_theme_stylebox_override("normal", scripted)
	_check(button.get_theme_stylebox("normal") != scripted, "Scripted style uses conservative copy")
	_check(scripted.default_color_scheme == null, "Scripted style source is not mutated")
	button.remove_theme_stylebox_override("normal")
	for path in CASES:
		var node: Control = _panel.get_node(path)
		var key: String = CASES[path][0]
		var style_key: String = CASES[path][1]
		if not key.is_empty():
			node.add_theme_color_override(key, Color("#abcdef"))
			_check(node.get_theme_color(key) == Color("#abcdef"), path + " authored override")
			node.remove_theme_color_override(key)
		var source := node.get_theme_stylebox(style_key)
		var source_scheme := source.default_color_scheme
		var authored_style := StyleBoxFlat.new()
		authored_style.bg_color = Color("#135790")
		node.add_theme_stylebox_override(style_key, authored_style)
		_check(
			node.get_theme_stylebox(style_key) == authored_style, path + " static style identity"
		)
		node.remove_theme_stylebox_override(style_key)
		_check(source.default_color_scheme == source_scheme, path + " source scheme unchanged")
		var variation := "Smoke" + node.get_class()
		_node_theme.set_type_variation(variation, node.get_class())
		node.theme_type_variation = variation
	await _frames()
	_test_resolved_colors()


func _check_rect_pixel(expected: Color, description: String) -> void:
	await _frames()
	var center := Vector2i(_color_rect.get_global_rect().get_center())
	var actual := root.get_texture().get_image().get_pixelv(center)
	_check(
		(
			absf(actual.r - expected.r) < 0.01
			and absf(actual.g - expected.g) < 0.01
			and absf(actual.b - expected.b) < 0.01
		),
		description + " expected=" + expected.to_html() + " actual=" + actual.to_html()
	)


func _test_color_rect() -> void:
	await _check_rect_pixel(_palette.get_primary(), "ColorRect parent scheme pixels")
	var local := ColorScheme.new()
	local.source_color = Color("#006495")
	_color_rect.add_theme_color_scheme_override("default_color_scheme", local)
	await _check_rect_pixel(local.get_primary(), "ColorRect local scheme pixels")
	var explicit := ColorScheme.new()
	explicit.source_color = Color("#b3261e")
	_color_rect.color_scheme = explicit
	await _check_rect_pixel(explicit.get_primary(), "ColorRect explicit scheme pixels")
	_color_rect.color_scheme = null
	_color_rect.remove_theme_color_scheme_override("default_color_scheme")
	var typed_theme := Theme.new()
	typed_theme.set_color_scheme("default_color_scheme", "ColorRect", explicit)
	_color_rect.theme = typed_theme
	await _check_rect_pixel(explicit.get_primary(), "ColorRect type scheme pixels")
	_color_rect.theme = null
	_color_rect.color_role = COLOR_ROLE_STATIC
	await _check_rect_pixel(_color_rect.color, "ColorRect STATIC pixels")
	_color_rect.color_role = COLOR_ROLE_PRIMARY
	_check(_color_rect.color == Color("#123456"), "ColorRect preserves authored color")


func _test_texture() -> void:
	var image := Image.create(8, 8, false, Image.FORMAT_RGB8)
	image.fill(Color.RED)
	var texture := ImageTexture.create_from_image(image)
	_palette.source_texture = texture
	var original := _palette.get_primary()
	image.fill(Color.BLUE)
	texture.set_image(image)
	_check(_palette.get_primary() != original, "ImageTexture updates synchronously")
	await _check_rect_pixel(_palette.get_primary(), "Texture update reaches ColorRect pixels")
	_test_resolved_colors()
	_palette.source_color = Color("#6750a4")
	image.fill(Color.GREEN)
	texture.set_image(image)
	_check(_palette.source_texture == null, "Old texture detached")


func _test_roundtrip() -> void:
	for path in CASES:
		var node: Control = _panel.get_node(path)
		node.add_theme_color_role_override("saved_role", COLOR_ROLE_ERROR)
		node.add_theme_color_scheme_override("saved_scheme", _palette)
		node.add_theme_color_override("saved_scale", Color(0.8, 0.6, 0.4, 0.5))
	for extension in ["tscn", "scn"]:
		var scene := PackedScene.new()
		_check(scene.pack(_panel) == OK, "Pack consumer scene")
		var path: String = _output_path.path_join("consumers." + extension)
		_check(ResourceSaver.save(scene, path) == OK, "Save " + extension)
		var loaded: PackedScene = ResourceLoader.load(
			path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE
		)
		var instance: Control = loaded.instantiate()
		instance.visible = false
		root.add_child(instance)
		for node_path in CASES:
			var node: Control = instance.get_node(node_path)
			_check(
				node.get_theme_color_role("saved_role") == COLOR_ROLE_ERROR,
				node_path + " saved role"
			)
			_check(
				node.get_theme_color_scheme("saved_scheme").source_color == _palette.source_color,
				node_path + " saved scheme"
			)
			_check(
				node.get_theme_color("saved_scale") == Color(0.8, 0.6, 0.4, 0.5),
				node_path + " saved scale"
			)
		instance.free()
	await _frames()
