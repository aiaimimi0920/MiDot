extends SceneTree


func _init() -> void:
	var resource := ResourceLoader.load("res://fixture.gif")
	if not resource is AnimatedTexture:
		push_error("Expected AnimatedTexture import, got %s" % resource)
		quit(1)
		return
	var texture := resource as AnimatedTexture
	if texture.get_frames() != 3:
		push_error("AnimatedTexture frame count: %d" % texture.get_frames())
		quit(1)
		return
	for index in 3:
		var expected: float = [0.07, 0.13, 0.21][index]
		if absf(texture.get_frame_duration(index) - expected) >= 0.011:
			push_error("AnimatedTexture duration mismatch at frame %d" % index)
			quit(1)
			return
	print("GIF_ANIMATED_IMPORT_OK")
	quit(0)
