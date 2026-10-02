extends SceneTree


func _initialize() -> void:
	var validator = load("res://mod/mod_schema_validator.gd").new()
	var result = validator.parse_json('"../icon.png"', {"type": "image"}, "res://mod")
	if result.size() >= 2 and result[0] == "":
		print("MOD_IMAGE_TRAVERSAL_REPRODUCED")
		quit(0)
		return
	printerr("MOD_IMAGE_TRAVERSAL_NOT_REPRODUCED::%s" % [result])
	quit(1)
