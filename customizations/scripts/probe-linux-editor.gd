extends SceneTree


func _initialize() -> void:
	var missing := PackedStringArray()
	if not TriangleMesh.new().has_method("update_from_indexed_surfaces"):
		missing.append("TriangleMesh.update_from_indexed_surfaces")
	if not RenderingServer.has_method("mesh_surface_get_geometry_arrays"):
		missing.append("RenderingServer.mesh_surface_get_geometry_arrays")
	print(JSON.stringify({"probe": "npr_engine_api", "missing": missing, "render_verified": false}))
	quit(0 if missing.is_empty() else 1)
