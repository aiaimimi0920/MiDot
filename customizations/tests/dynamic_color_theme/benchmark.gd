extends SceneTree

const READS := 20000
const SAMPLES := 7

var _failures := 0


class CpuTexture:
	extends Texture2D

	var image: Image
	var reads := 0

	func _get_width() -> int:
		return image.get_width()

	func _get_height() -> int:
		return image.get_height()

	func _get_image() -> Image:
		reads += 1
		# Match Texture2D readback: decompression must operate on a fresh snapshot.
		return image.duplicate()


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	print(
		"BENCH_VERSION ", Engine.get_version_info().string, " hash=", Engine.get_version_info().hash
	)
	_benchmark_colors(Control.new())
	_benchmark_colors(Window.new())
	_benchmark_styles()
	for dimension in [256, 1024, 4096]:
		for image_kind in ["opaque", "mixed_alpha", "s3tc"]:
			_benchmark_texture(dimension, image_kind)
	quit(0 if _failures == 0 else 1)


func _benchmark_colors(node) -> void:
	root.add_child(node)
	var palette := ColorScheme.new()
	var node_theme := Theme.new()
	node_theme.default_color_scheme = palette
	node_theme.set_color_role("inherited_role", node.get_class(), COLOR_ROLE_PRIMARY)
	node.theme = node_theme
	node.add_theme_color_role_override("overridden_role", COLOR_ROLE_PRIMARY)
	var results := {"class": node.get_class(), "reads": READS, "samples": SAMPLES}
	for key in ["inherited", "overridden"]:
		for warmup in range(100):
			node.get_theme_color(key)
		var times: Array[int] = []
		for sample in range(SAMPLES):
			var start := Time.get_ticks_usec()
			for iteration in range(READS):
				node.get_theme_color(key)
			times.append(Time.get_ticks_usec() - start)
		times.sort()
		results[key + "_median_us"] = times[SAMPLES / 2]
		results[key + "_samples_us"] = times
	print("BENCH_COLORS ", JSON.stringify(results))
	node.free()


func _benchmark_styles() -> void:
	var palette := ColorScheme.new()
	var node_theme := Theme.new()
	node_theme.default_color_scheme = palette
	var style := StyleBoxFlat.new()
	node_theme.set_stylebox("panel", "Control", style)
	var nodes: Array[Control] = []
	for index in range(256):
		var node := Control.new()
		node.theme = node_theme
		root.add_child(node)
		nodes.append(node)
	var memory_before := OS.get_static_memory_usage()
	var identities := {}
	var start := Time.get_ticks_usec()
	for node in nodes:
		identities[node.get_theme_stylebox("panel").get_instance_id()] = true
	print(
		"BENCH_STYLES ",
		(
			JSON
			. stringify(
				{
					"nodes": nodes.size(),
					"unique_styles": identities.size(),
					"source_reused": identities.has(style.get_instance_id()),
					"resolve_us": Time.get_ticks_usec() - start,
					"godot_static_bytes_delta": OS.get_static_memory_usage() - memory_before,
					"godot_static_bytes_peak": OS.get_static_memory_peak_usage(),
				}
			)
		)
	)
	for node in nodes:
		node.free()


func _benchmark_texture(dimension: int, image_kind: String) -> void:
	var texture := CpuTexture.new()
	texture.image = Image.create(dimension, dimension, false, Image.FORMAT_RGBA8)
	texture.image.fill(Color("#336699"))
	if image_kind == "mixed_alpha":
		texture.image.fill_rect(Rect2i(0, 0, dimension / 2, dimension), Color(0.2, 0.4, 0.6, 0.0))
	elif image_kind == "s3tc":
		var error := texture.image.compress(Image.COMPRESS_S3TC)
		if error != OK:
			_failures += 1
			push_error("Could not prepare S3TC benchmark texture: %s" % error_string(error))
			return
	var palette := ColorScheme.new()
	var start := Time.get_ticks_usec()
	palette.source_texture = texture
	var extraction_us := Time.get_ticks_usec() - start
	var source_color := palette.get_color(COLOR_ROLE_STATIC)
	start = Time.get_ticks_usec()
	for index in range(6):
		palette.dark = not palette.dark
		palette.contrast_level = -1.0 if index % 2 == 0 else 1.0
	var parameters_us := Time.get_ticks_usec() - start
	var cached_reads := texture.reads
	texture.emit_changed()
	print(
		"BENCH_TEXTURE ",
		(
			JSON
			. stringify(
				{
					"dimension": dimension,
					"image_kind": image_kind,
					"input_compressed": texture.image.is_compressed(),
					"input_format": texture.image.get_format(),
					"pixels": dimension * dimension,
					"extraction_us": extraction_us,
					"twelve_parameter_updates_us": parameters_us,
					"reads_after_parameters": cached_reads,
					"reads_after_source_change": texture.reads,
					"seed_unchanged": palette.get_color(COLOR_ROLE_STATIC) == source_color,
					"godot_static_bytes_peak": OS.get_static_memory_peak_usage(),
				}
			)
		)
	)
