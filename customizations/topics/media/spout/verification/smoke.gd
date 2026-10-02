extends SceneTree

const INSTANCE_COUNT := 250


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	if not ClassDB.class_exists("Spout"):
		_fail("Spout class is not registered")
		return

	var buffer_name := "GodotSpoutSmoke_%d" % OS.get_process_id()
	var payload := PackedByteArray([0x47, 0x4F, 0x44, 0x4F, 0x54])
	var bridge := Spout.new()
	if bridge.get_spout_version() <= 0:
		_fail("Spout SDK version is unavailable")
		return
	var create_error := bridge.create_memory_buffer(buffer_name, 64)
	if create_error != OK:
		_fail("Could not create Spout memory buffer: %d" % create_error)
		return
	var write_error := bridge.write_memory_buffer(buffer_name, payload, payload.size())
	if write_error != OK:
		bridge.delete_memory_buffer()
		_fail("Could not write Spout memory buffer: %d" % write_error)
		return
	var received := bridge.read_memory_buffer(buffer_name, 64)
	if received.slice(0, payload.size()) != payload:
		bridge.delete_memory_buffer()
		_fail("Spout memory buffer round-trip mismatch")
		return
	if bridge.delete_memory_buffer() != OK:
		_fail("Could not delete Spout memory buffer")
		return
	bridge = null

	for index in range(INSTANCE_COUNT):
		var instance := Spout.new()
		instance.sender_set_sender_name("GodotSpoutSmoke_%d" % index)
		instance.sender_release_sender()
		instance.receiver_release_receiver()
		instance = null

	for _frame in range(10):
		await process_frame
	print("SPOUT_LIFECYCLE_OK instances=%d" % INSTANCE_COUNT)
	quit(0)


func _fail(message: String) -> void:
	push_error(message)
	print("SPOUT_LIFECYCLE_FAILED: %s" % message)
	quit(1)
