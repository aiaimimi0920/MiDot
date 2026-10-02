extends SceneTree

var failures: Array[String] = []


func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
		push_error(message)


func check_duration(actual: float, expected: float, label: String) -> void:
	check(
		absf(actual - expected) < 0.011,
		"%s duration: expected %.2f, got %.3f" % [label, expected, actual]
	)


func _init() -> void:
	var fixture_path := ProjectSettings.globalize_path("res://fixture.gif")
	var frames := ImageFrames.new()
	check(frames.load(fixture_path) == OK, "file load failed")
	check(frames.get_frame_count() == 3, "file load frame count")
	if frames.get_frame_count() == 3:
		check_duration(frames.get_frame_duration(0), 0.07, "frame 0")
		check_duration(frames.get_frame_duration(1), 0.13, "frame 1")
		check_duration(frames.get_frame_duration(2), 0.21, "frame 2")
		var expected := [Color.RED, Color.GREEN, Color.BLUE]
		for index in 3:
			var image: Image = frames.get_frame_image(index)
			check(image.get_size() == Vector2i(3, 2), "frame %d dimensions" % index)
			check(image.get_pixel(0, 0).is_equal_approx(expected[index]), "frame %d color" % index)

	var limited := ImageFrames.new()
	check(limited.load(fixture_path, 2) == OK, "limited file load failed")
	check(limited.get_frame_count() == 2, "max_frames was not honored for file load")

	var bytes := FileAccess.get_file_as_bytes(fixture_path)
	var buffered := ImageFrames.new()
	check(buffered.load_gif_from_buffer(bytes, 1) == OK, "buffer load failed")
	check(buffered.get_frame_count() == 1, "max_frames was not honored for buffer load")

	var roundtrip_path := ProjectSettings.globalize_path("res://roundtrip.gif")
	check(frames.save_gif(roundtrip_path, 16) == OK, "animated GIF save failed")
	var roundtrip := ImageFrames.new()
	check(roundtrip.load(roundtrip_path) == OK, "animated GIF reload failed")
	check(roundtrip.get_frame_count() == 3, "roundtrip frame count")
	if roundtrip.get_frame_count() == 3:
		for index in 3:
			check_duration(
				roundtrip.get_frame_duration(index),
				frames.get_frame_duration(index),
				"roundtrip frame %d" % index
			)

	var transparent_image := Image.create(2, 1, false, Image.FORMAT_RGBA8)
	transparent_image.set_pixel(0, 0, Color(0, 0, 0, 0))
	transparent_image.set_pixel(1, 0, Color.YELLOW)
	var transparent_frames := ImageFrames.new()
	transparent_frames.add_frame(transparent_image, 0.05)
	var transparent_path := ProjectSettings.globalize_path("res://transparent.gif")
	check(transparent_frames.save_gif(transparent_path, 2) == OK, "transparent GIF save failed")
	var transparent_reload := ImageFrames.new()
	check(transparent_reload.load(transparent_path) == OK, "transparent GIF reload failed")
	if transparent_reload.get_frame_count() == 1:
		var reloaded_image: Image = transparent_reload.get_frame_image(0)
		check(reloaded_image.get_pixel(0, 0).a < 0.01, "transparent pixel lost")
		check(reloaded_image.get_pixel(1, 0).is_equal_approx(Color.YELLOW), "opaque pixel changed")
	else:
		check(false, "transparent roundtrip frame count")

	var gradient_image := Image.create(16, 16, false, Image.FORMAT_RGBA8)
	for y in 16:
		for x in 16:
			gradient_image.set_pixel(x, y, Color(x / 15.0, y / 15.0, (x + y) / 30.0))
	var gradient_frames := ImageFrames.new()
	gradient_frames.add_frame(gradient_image, 0.1)
	var gradient_path := ProjectSettings.globalize_path("res://quantized.gif")
	check(gradient_frames.save_gif(gradient_path, 8) == OK, "quantized GIF save failed")
	var gradient_reload := ImageFrames.new()
	check(gradient_reload.load(gradient_path) == OK, "quantized GIF reload failed")
	check(gradient_reload.get_frame_count() == 1, "quantized GIF frame count")

	if failures.is_empty():
		print("GIF_SMOKE_OK")
		quit(0)
	else:
		print("GIF_SMOKE_FAILED: %d" % failures.size())
		quit(1)
