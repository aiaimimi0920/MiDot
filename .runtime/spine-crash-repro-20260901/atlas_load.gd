extends SceneTree


func _init() -> void:
	var atlas := SpineAtlasResource.new()
	var error := atlas.load_from_atlas_file("res://valid.atlas")
	if error != OK:
		fail("Valid synthetic Spine atlas failed to load: %s" % error)
		return
	if atlas.get_textures().size() != 1:
		fail("Valid synthetic Spine atlas did not retain exactly one texture")
		return

	var missing_atlas := SpineAtlasResource.new()
	if missing_atlas.load_from_atlas_file("res://missing.atlas") == OK:
		fail("Spine atlas with a missing texture unexpectedly loaded")
		return

	print("SPINE_ATLAS_LOAD_OK")
	quit(0)


func fail(message: String) -> void:
	push_error(message)
	quit(1)
