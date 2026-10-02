# Each failed smoke check exits immediately to avoid cascading diagnostics.
# gdlint: disable=max-returns
extends SceneTree


func _fail(message: String) -> void:
	push_error(message)
	quit(1)


func _initialize() -> void:
	if not ClassDB.class_exists("Spout"):
		_fail("Spout class is not registered")
		return

	var spout := Spout.new()
	if spout.get_spout_version() <= 0:
		_fail("Spout SDK version is unavailable")
		return

	var sender_count := spout.get_sender_count()
	var senders := spout.get_senders()
	if sender_count < 0 or senders.size() != sender_count:
		_fail("Sender array does not match sender count")
		return

	# These calls exercise deterministic validation paths without requiring a
	# running Spout peer or a GPU-sharing session.
	if spout.create_memory_buffer("", 0) != ERR_INVALID_PARAMETER:
		_fail("Invalid memory-buffer creation was accepted")
		return
	if spout.write_memory_buffer("", PackedByteArray(), 0) != ERR_INVALID_PARAMETER:
		_fail("Invalid memory-buffer write was accepted")
		return
	if not spout.read_memory_buffer("", 0).is_empty():
		_fail("Invalid memory-buffer read returned data")
		return
	if spout.sender_send_texture(0, 0) != ERR_INVALID_PARAMETER:
		_fail("Invalid sender texture pair was accepted")
		return
	if spout.receiver_receive_texture(0, 1) != ERR_INVALID_PARAMETER:
		_fail("Invalid receiver texture pair was accepted")
		return
	if spout.set_active_sender("") != ERR_INVALID_PARAMETER:
		_fail("Empty active sender name was accepted")
		return

	# No-peer receiver status calls must remain safe and callable.
	spout.receiver_is_connected()
	spout.receiver_is_updated()
	spout.receiver_is_frame_new()
	spout.receiver_get_sender_cpu()
	spout.receiver_get_sender_gldx()

	spout = null
	print("SPOUT_SMOKE_OK")
	quit(0)
