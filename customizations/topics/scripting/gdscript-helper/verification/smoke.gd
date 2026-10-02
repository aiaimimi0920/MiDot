# Each failed smoke check exits immediately to avoid cascading diagnostics.
# gdlint: disable=max-returns
extends SceneTree


func _fail(message: String, exit_code: int) -> void:
	push_error(message)
	quit(exit_code)


func _initialize() -> void:
	print("SMOKE_STAGE=initialize")
	var helper := GDScriptHelper.new()
	if helper == null:
		_fail("GDScriptHelper could not be instantiated", 1)
		return
	var valid_code := "extends RefCounted\n\nfunc alpha() -> void:\n\tpass\n"
	if not helper.set_validate_code(valid_code, "res://valid.gd"):
		_fail("valid code was rejected: %s" % [helper.get_errors()], 1)
		return
	if (
		helper.has_errors()
		or not helper.has_functions()
		or helper.get_functions().get("alpha", -1) != 2
	):
		_fail(
			(
				"unexpected valid-code analysis: errors=%s functions=%s"
				% [helper.get_errors(), helper.get_functions()]
			),
			2
		)
		return
	print("SMOKE_STAGE=valid")

	if helper.set_validate_code("func broken(\n", "res://invalid.gd") or not helper.has_errors():
		_fail("invalid code did not produce an error", 3)
		return
	var first_error: Dictionary = helper.get_errors()[0]
	if (
		first_error.get("line", -1) != 0
		or first_error.get("column", -1) < 0
		or String(first_error.get("message", "")).is_empty()
	):
		_fail("invalid error dictionary: %s" % [first_error], 4)
		return
	print("SMOKE_STAGE=invalid")

	var completion_code := "extends Node\nfunc _ready() -> void:\n\tpri" + String.chr(0xffff)
	if (
		helper.set_completion_code(completion_code, "res://completion.gd") != OK
		or not helper.has_completion_options()
	):
		_fail("completion produced no options", 5)
		return
	var found_print := false
	var print_option := {}
	for option: Dictionary in helper.get_completion_options():
		if String(option.get("display_text", "")).begins_with("print("):
			print_option = option
			if String(option.get("insert_text", "")).begins_with("print"):
				found_print = true
				break
	if not found_print:
		_fail("completion did not include print: %s" % [print_option], 6)
		return
	print("SMOKE_STAGE=completion")

	var lookup := helper.lookup_code("", "Node", "res://lookup.gd")
	if (
		lookup.get("type", -1) != GDScriptHelper.LOOKUP_RESULT_CLASS
		or lookup.get("class_name") != "Node"
	):
		_fail("unexpected lookup result: %s" % [lookup], 7)
		return
	print("SMOKE_STAGE=lookup")

	print("GDSCRIPT_HELPER_SMOKE_OK")
	quit(0)
