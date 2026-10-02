extends SceneTree


func _init() -> void:
	var resource := ResourceLoader.load("res://empty.spskel", "SpineSkeletonFileResource")
	if resource != null:
		push_error("Invalid Spine binary unexpectedly loaded")
		quit(1)
		return
	print("SPINE_INVALID_LOAD_REJECTED")
	quit(0)
