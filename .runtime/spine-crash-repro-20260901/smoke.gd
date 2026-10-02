extends SceneTree

const REQUIRED_CLASSES := [
	"SpineAnimation",
	"SpineAnimationMix",
	"SpineAnimationState",
	"SpineAnimationTrack",
	"SpineAtlasResource",
	"SpineBoneNode",
	"SpineConstant",
	"SpineEventData",
	"SpineMesh2D",
	"SpineObjectWrapper",
	"SpineSkeletonDataResource",
	"SpineSkeletonFileResource",
	"SpineSkinEntry",
	"SpineSlotNode",
	"SpineSprite",
]

const INSTANTIABLE_CLASSES := [
	"SpineAnimationMix",
	"SpineAnimationTrack",
	"SpineAtlasResource",
	"SpineBoneNode",
	"SpineSkeletonDataResource",
	"SpineSkeletonFileResource",
	"SpineSlotNode",
	"SpineSprite",
]


func _init() -> void:
	for type_name in REQUIRED_CLASSES:
		if not ClassDB.class_exists(type_name):
			fail("Missing registered class: %s" % type_name)
			return

	for type_name in INSTANTIABLE_CLASSES:
		if not ClassDB.can_instantiate(type_name):
			fail("Class is not instantiable: %s" % type_name)
			return
		var instance: Object = ClassDB.instantiate(type_name)
		if instance == null:
			fail("ClassDB.instantiate returned null: %s" % type_name)
			return
		if instance is Node:
			instance.free()

	var atlas := SpineAtlasResource.new()
	if not atlas.get_textures().is_empty() or not atlas.get_normal_maps().is_empty():
		fail("A new SpineAtlasResource must have no textures")
		return

	var skeleton_data := SpineSkeletonDataResource.new()
	if skeleton_data.is_skeleton_data_loaded():
		fail("A new SpineSkeletonDataResource must not be loaded")
		return

	var sprite := SpineSprite.new()
	if sprite.get_skeleton() != null or sprite.get_animation_state() != null:
		fail("A new SpineSprite must not expose native runtime objects")
		return
	sprite.free()

	print("SPINE_SMOKE_OK")
	quit(0)


func fail(message: String) -> void:
	push_error(message)
	quit(1)
