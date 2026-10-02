@tool
extends EditorPlugin

var _checks := 0
var _failures: Array[String] = []
var _theme: Theme
var _type_editor: Control
var _theme_editor: Control


func _enter_tree() -> void:
	if OS.get_cmdline_user_args().has("--theme-editor-smoke"):
		_run.call_deferred()


func _check(condition: bool, description: String) -> bool:
	_checks += 1
	if not condition:
		_failures.append(description)
		push_error(description)
	return condition


func _frames() -> void:
	for index in range(6):
		await get_tree().process_frame


func _theme_luminance_contrast(a: Color, b: Color) -> float:
	return absf(a.get_luminance() - b.get_luminance())


func _check_editor_menu_contrast() -> void:
	var base_control := EditorInterface.get_base_control()
	var menu_text := base_control.get_theme_color("font_color", "MenuBar")
	var popup_text := base_control.get_theme_color("font_color", "PopupMenu")
	var menu_style := base_control.get_theme_stylebox("normal", "MenuBar")
	var popup_style := base_control.get_theme_stylebox("panel", "PopupMenu")
	_check(menu_style != null and popup_style != null, "Editor menu styles exist")
	if menu_style is StyleBoxFlat and popup_style is StyleBoxFlat:
		_check(
			_theme_luminance_contrast(menu_text, menu_style.bg_color) >= 0.25,
			"MenuBar text has sufficient luminance contrast",
		)
		_check(
			_theme_luminance_contrast(popup_text, popup_style.bg_color) >= 0.25,
			"PopupMenu text has sufficient luminance contrast",
		)


func _run() -> void:
	await get_tree().create_timer(3.0).timeout
	_check_editor_menu_contrast()
	DirAccess.make_dir_recursive_absolute("res://output")
	_theme = Theme.new()
	_theme.default_color_scheme = ColorScheme.new()
	_theme.set_color_scheme("null_scheme", "Button", null)
	var valid_scheme := ColorScheme.new()
	valid_scheme.source_color = Color("#006495")
	_theme.set_color_scheme("valid_scheme", "Button", valid_scheme)
	_check(
		ResourceSaver.save(_theme, "res://output/editor_theme.tres") == OK, "Save editor fixture"
	)
	EditorInterface.edit_resource(_theme)
	await _frames()
	var editors := get_tree().root.find_children("*", "ThemeTypeEditor", true, false)
	var panels := get_tree().root.find_children("*", "ThemeEditor", true, false)
	if _check(not editors.is_empty() and not panels.is_empty(), "Real Theme editor exists"):
		_type_editor = editors[0]
		_theme_editor = panels[0]
		_check(_theme_editor.is_visible_in_tree(), "Theme editor dock is visible")
		_type_editor.call("select_type", "Button")
		var tabs: TabContainer = _type_editor.find_children("*", "TabContainer", true, false)[0]
		tabs.current_tab = 7
		for toggle in _type_editor.find_children("*", "CheckButton", true, false):
			if toggle.text == "Show Default":
				toggle.button_pressed = false
				toggle.emit_signal("pressed")
		await _refresh()
		await _test_picker()
		await _test_add_remove(tabs)
		await _test_import()
		await _test_resource_roundtrip()
		await _screenshot("editor-schemes")
		await _test_inspector()
	print(
		"DYNAMIC_THEME_EDITOR ",
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
	get_tree().quit(0 if _failures.is_empty() else 1)


func _refresh() -> void:
	_type_editor.call("_update_type_items")
	await _frames()


func _raw(name: String, resource: Theme = null) -> Variant:
	if resource == null:
		resource = _theme
	return resource.get("Button/color_schemes/" + name)


func _row(name: String) -> Control:
	for label in _type_editor.find_children("*", "Label", true, false):
		if label.text == name:
			return label.get_parent().get_parent()
	return null


func _picker(name: String) -> EditorResourcePicker:
	var row := _row(name)
	if row:
		var pickers := row.find_children("*", "EditorResourcePicker", true, false)
		if not pickers.is_empty():
			return pickers[0]
	return null


func _history(object: Object) -> UndoRedo:
	var manager := get_undo_redo()
	return manager.get_history_undo_redo(manager.get_object_history_id(object))


func _select_picker_action(picker: EditorResourcePicker, text: String) -> void:
	var buttons: Array[Button] = []
	for child in picker.get_children():
		if child is Button:
			buttons.append(child)
	buttons.back().emit_signal("pressed")
	await _frames()
	var found := false
	for menu in picker.find_children("*", "PopupMenu", true, false):
		for index in range(menu.item_count):
			if menu.get_item_text(index) == text:
				menu.emit_signal("id_pressed", menu.get_item_id(index))
				menu.hide()
				found = true
				break
	_check(found, "Resource picker action: " + text)
	await _refresh()


func _test_picker() -> void:
	var picker := _picker("null_scheme")
	if not _check(picker != null, "ColorScheme picker is present"):
		return
	_check(picker.edited_resource == null, "Picker displays stored null instead of fallback")
	_check(
		_theme.get_color_scheme("null_scheme", "Button") != null, "Runtime fallback remains valid"
	)
	await _select_picker_action(picker, "ColorScheme")
	var selected: ColorScheme = _raw("null_scheme")
	_check(selected != null, "Picker writes a real scheme")
	_check(_history(_theme).undo(), "Undo picker selection")
	await _refresh()
	_check(_raw("null_scheme") == null, "Selection undo restores raw null")
	_check(_theme.get_color_scheme_list("Button").has("null_scheme"), "Selection undo retains item")
	_check(_history(_theme).redo(), "Redo picker selection")
	await _refresh()
	_check(_raw("null_scheme") == selected, "Selection redo restores selected resource")
	# Keep discrete edits outside the editor's 800 ms MERGE_ENDS window.
	await get_tree().create_timer(0.9).timeout
	await _select_picker_action(_picker("null_scheme"), "Clear")
	_check(_raw("null_scheme") == null, "Picker clear stores null")
	_check(_history(_theme).undo(), "Undo picker clear")
	await _refresh()
	_check(_raw("null_scheme") == selected, "Clear undo restores selected resource")
	_check(_history(_theme).redo(), "Redo picker clear")
	await _refresh()
	_check(_raw("null_scheme") == null, "Clear redo restores null")


func _press_button(base: Node, text: String, tooltip := false) -> bool:
	for button in base.find_children("*", "Button", true, false):
		var value: String = button.tooltip_text if tooltip else button.text
		if value == text:
			button.emit_signal("pressed")
			return true
	return false


func _test_add_remove(tabs: TabContainer) -> void:
	var row := _row("null_scheme")
	_check(_press_button(row, "Remove Item", true), "Remove button dispatches actual editor action")
	_check(not _theme.get_color_scheme_list("Button").has("null_scheme"), "Removed item is absent")
	_check(_history(_theme).undo(), "Undo remove null item")
	await _refresh()
	_check(
		_theme.get_color_scheme_list("Button").has("null_scheme"),
		"Undo restores null item existence"
	)
	_check(_raw("null_scheme") == null, "Remove undo preserves raw null")
	_check(_history(_theme).redo(), "Redo remove null item")
	await _refresh()
	_check(
		not _theme.get_color_scheme_list("Button").has("null_scheme"),
		"Remove redo restores absence"
	)
	_check(_history(_theme).undo(), "Restore null item after remove redo")
	await _refresh()
	_check(_raw("null_scheme") == null, "Restored null item keeps its raw value")
	var saved_valid: ColorScheme = _raw("valid_scheme")
	_check(
		_press_button(_row("valid_scheme"), "Remove Item", true),
		"Remove valid scheme through editor"
	)
	_check(
		not _theme.get_color_scheme_list("Button").has("valid_scheme"),
		"Removed valid scheme is absent"
	)
	_check(_history(_theme).undo(), "Undo remove valid scheme")
	await _refresh()
	_check(_raw("valid_scheme") == saved_valid, "Remove undo restores exact valid resource")
	_check(_history(_theme).redo(), "Redo remove valid scheme")
	await _refresh()
	_check(
		not _theme.get_color_scheme_list("Button").has("valid_scheme"),
		"Valid remove redo restores absence"
	)
	_check(_history(_theme).undo(), "Restore valid scheme for disk reload")
	await _refresh()
	_check(_raw("valid_scheme") == saved_valid, "Restored valid resource retains identity")
	var added := false
	for field in tabs.get_tab_control(7).find_children("*", "LineEdit", true, false):
		if field.has_meta("button"):
			field.text = "added_scheme"
			field.emit_signal("text_submitted", field.text)
			added = true
			break
	_check(added, "Add scheme through editor input")
	await _refresh()
	_check(_theme.get_color_scheme_list("Button").has("added_scheme"), "Added item exists")
	_check(_raw("added_scheme") == null, "New item stores null")
	_check(_history(_theme).undo(), "Undo added item")
	_check(
		not _theme.get_color_scheme_list("Button").has("added_scheme"), "Add undo restores absence"
	)
	_check(_history(_theme).redo(), "Redo added item")
	await _refresh()
	_check(_raw("added_scheme") == null, "Add redo restores null")


func _test_import() -> void:
	var source := Theme.new()
	source.default_color_scheme = ColorScheme.new()
	source.set_color_scheme("import_null", "Button", null)
	var imported_scheme := ColorScheme.new()
	imported_scheme.source_color = Color("#b3261e")
	source.set_color_scheme("import_valid", "Button", imported_scheme)
	var path := "res://output/import_theme.tres"
	_check(ResourceSaver.save(source, path) == OK, "Save import source")
	var dialog: Window = _theme_editor.find_children("*", "ThemeItemEditorDialog", true, false)[0]
	dialog.popup_centered_ratio(0.85)
	await _frames()
	var tabs: TabContainer = dialog.find_children("*", "TabContainer", true, false)[0]
	tabs.current_tab = 1
	var import_tabs: TabContainer = tabs.get_tab_control(1)
	import_tabs.current_tab = 2
	for file_dialog in dialog.find_children("*", "EditorFileDialog", true, false):
		if file_dialog.title == "Select Another Theme Resource:":
			file_dialog.emit_signal("file_selected", path)
	await _frames()
	var importer := (
		import_tabs.get_tab_control(2).find_children("*", "ThemeItemImportTree", true, false)[0]
	)
	_check(_press_button(importer, "Select With Data"), "Select full import in actual import tree")
	_check(_press_button(importer, "Import Selected"), "Run actual Theme import action")
	await _frames()
	_check(
		_theme.get_color_scheme_list("Button").has("import_null"),
		"Full import preserves null definition"
	)
	_check(_raw("import_null") == null, "Full import preserves raw null")
	var actual: ColorScheme = _raw("import_valid")
	_check(
		actual != null and actual.source_color == Color("#b3261e"),
		"Full import preserves authored scheme"
	)
	_check(_history(_theme).undo(), "Undo full import")
	_check(
		not _theme.get_color_scheme_list("Button").has("import_null"),
		"Import undo restores absence"
	)
	_check(_history(_theme).redo(), "Redo full import")
	_check(_raw("import_null") == null, "Import redo preserves null")
	actual = _raw("import_valid")
	_check(
		actual != null and actual.source_color == Color("#b3261e"),
		"Import redo preserves authored scheme"
	)
	dialog.hide()
	await _refresh()


func _test_resource_roundtrip() -> void:
	for extension in ["tres", "res"]:
		var path: String = "res://output/editor_roundtrip." + extension
		_check(ResourceSaver.save(_theme, path) == OK, "Save editor state " + extension)
		var loaded: Theme = ResourceLoader.load(path, "Theme", ResourceLoader.CACHE_MODE_IGNORE)
		_check(loaded != _theme, "Reload bypasses memory cache")
		_check(
			loaded.get_color_scheme_list("Button").has("null_scheme"),
			"Reload preserves null existence"
		)
		_check(_raw("null_scheme", loaded) == null, "Reload preserves null value")
		var valid: ColorScheme = _raw("valid_scheme", loaded)
		_check(
			valid != null and valid.source_color == Color("#006495"),
			"Reload preserves edited scheme contents"
		)
		var imported: ColorScheme = _raw("import_valid", loaded)
		_check(
			imported != null and imported.source_color == Color("#b3261e"),
			"Reload preserves imported scheme contents"
		)
		_check(
			not loaded.get_color_scheme_list("Button").has("absent_scheme"),
			"Reload does not invent missing schemes"
		)


func _property_editor(name: String) -> EditorProperty:
	for editor in EditorInterface.get_inspector().find_children("*", "EditorProperty", true, false):
		if editor.get_edited_property() == name:
			return editor
	return null


func _has_override(node: Node, property: String) -> bool:
	for item in node.get_property_list():
		if item.name == property:
			return bool(item.usage & PROPERTY_USAGE_CHECKED)
	return false


func _test_inspector() -> void:
	var scene_root := Control.new()
	scene_root.name = "InspectorSmoke"
	scene_root.theme = Theme.new()
	scene_root.theme.default_color_scheme = ColorScheme.new()
	for child in [Button.new(), AcceptDialog.new()]:
		child.name = child.get_class()
		scene_root.add_child(child)
		child.owner = scene_root
	var scene := PackedScene.new()
	_check(scene.pack(scene_root) == OK, "Pack Inspector scene")
	_check(ResourceSaver.save(scene, "res://output/inspector.tscn") == OK, "Save Inspector fixture")
	scene_root.free()
	EditorInterface.open_scene_from_path("res://output/inspector.tscn")
	await _frames()
	# The editor may restore the same scene from the preceding smoke run.
	EditorInterface.reload_scene_from_path("res://output/inspector.tscn")
	await _frames()
	var edited_root := EditorInterface.get_edited_scene_root()
	for node_name in ["Button", "AcceptDialog"]:
		var node := edited_root.get_node(node_name)
		EditorInterface.edit_node(node)
		await _frames()
		EditorInterface.get_inspector().expand_all_folding()
		await _frames()
		var color := "font_color" if node_name == "Button" else "title_color"
		var properties := [
			"theme_override_color_roles/" + color + "_role",
			"theme_override_color_schemes/" + color + "_scheme",
			"theme_override_color_schemes/default_color_scheme",
			"theme_override_colors/" + color + "_scale",
		]
		for property in properties:
			var editor := _property_editor(property)
			if not _check(editor != null, node_name + " Inspector exposes " + property):
				continue
			_check(editor.checkable, property + " is checkable")
			_check(not _has_override(node, property), property + " starts without an override")
			editor.emit_signal("property_checked", property, true)
			await _frames()
			_check(_has_override(node, property), property + " checkbox enables override")
			_check(_history(node).undo(), "Undo Inspector checkbox")
			_check(not _has_override(node, property), "Checkbox undo removes override")
			_check(_history(node).redo(), "Redo Inspector checkbox")
			await _frames()
			_check(_has_override(node, property), "Checkbox redo restores override")
			await get_tree().create_timer(0.9).timeout
			editor = _property_editor(property)
			editor.emit_signal("property_checked", property, false)
			await _frames()
			_check(not _has_override(node, property), "Checkbox clear removes override")
			_check(_history(node).undo(), "Undo Inspector clear")
			_check(_has_override(node, property), "Clear undo restores override")
			_check(_history(node).redo(), "Redo Inspector clear")
			await _frames()
			_check(not _has_override(node, property), "Clear redo removes override")
			_check(_history(node).undo(), "Restore Inspector override for saved state")
			await _frames()
			_check(_has_override(node, property), "Restored override remains enabled")
	_check(EditorInterface.save_scene() == OK, "Save actual edited scene")
	var loaded: PackedScene = ResourceLoader.load(
		"res://output/inspector.tscn", "PackedScene", ResourceLoader.CACHE_MODE_IGNORE
	)
	var instance := loaded.instantiate()
	for node_name in ["Button", "AcceptDialog"]:
		var node := instance.get_node(node_name)
		var color := "font_color" if node_name == "Button" else "title_color"
		for property in [
			"theme_override_color_roles/" + color + "_role",
			"theme_override_color_schemes/" + color + "_scheme",
			"theme_override_color_schemes/default_color_scheme",
			"theme_override_colors/" + color + "_scale",
		]:
			_check(
				_has_override(node, property),
				node_name + " Inspector state survives disk reload: " + property
			)
	instance.free()
	await _screenshot("editor-inspector")


func _screenshot(name: String) -> void:
	await RenderingServer.frame_post_draw
	var image := EditorInterface.get_base_control().get_viewport().get_texture().get_image()
	_check(image.save_png("res://output/" + name + ".png") == OK, "Save real editor screenshot")
