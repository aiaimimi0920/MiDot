extends SceneTree


func _init() -> void:
	var resource := ResourceLoader.load("res://fixture.gif")
	if not resource is SpriteFrames:
		push_error("Expected SpriteFrames import, got %s" % resource)
		quit(1)
		return
	var frames := resource as SpriteFrames
	if frames.get_frame_count("default") != 3:
		push_error("SpriteFrames frame count: %d" % frames.get_frame_count("default"))
		quit(1)
		return
	for index in 3:
		var expected: float = [0.07, 0.13, 0.21][index]
		if absf(frames.get_frame_duration("default", index) - expected) >= 0.011:
			push_error("SpriteFrames duration mismatch at frame %d" % index)
			quit(1)
			return
	print("GIF_SPRITE_IMPORT_OK")
	quit(0)
