extends SceneTree


func _init() -> void:
	var spout := Spout.new()
	print("SPOUT_VERSION=", spout.get_spout_version())
	quit(0)
