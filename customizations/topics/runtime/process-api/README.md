# Scriptable Process API

## Intent and problem

Provide a first-class `Process` object for starting a child process and
interacting with its standard streams without forcing scripts to reduce the
operation to a single blocking `OS.execute()` call.

## Behavior contract

- `Process.create()` starts a child process with arguments, working directory,
  and optional standard input.
- Scripts can poll and read stdout/stderr lines, write and close stdin, inspect
  the process ID and exit status, and terminate the process.
- Releasing the final `Process` reference terminates a live child and releases
  platform process tracking. Completed children retain a stable exit status
  until their wrapper is released without retaining native process handles.
- Windows and Unix backends expose the same script API.

## Legacy origin

- Original Godot line: 4.2 development history based at
  `da0b1eb128a522bbef083b9f2a5cc2da6917c3d8`.
- Personal commits: `d654681d030b458e6b63e07479611933cc610b0f`
  and the follow-up fixes in
  `5b6b9edfdf58c5878690190c3b056e040b039df9`.
- The legacy implementation used Tiny Process Library. The Godot 4.8 port reuses
  upstream OS pipe and platform backends instead of carrying the obsolete
  duplicate vendor dependency.

## Consolidated source

- `personal/main` commit:
  `df077b5fed582a0437149ee09d7ac45dab566af9`.
- Pipe-handle validation follow-up:
  `8264caab9a17707555ed647806011c741b13f98b`.
- Process resource lifecycle follow-up:
  `69903a894c4fe3607fc46d14c13ac0c1099068e5`.

## Dependencies

None.

## Important source areas

- `core/io/process.*`
- `core/os/os.h`
- Unix, Windows, and Web `execute_with_pipe()` overrides
- Unix and Windows `FileAccess` pipe implementations
- Core type registration and `Process.xml`

## Conflict guidance

Preserve the public script behavior rather than old platform implementation
details. Reconcile with modern `OS.execute_with_pipe()` instead of duplicating
new upstream primitives when they provide equivalent lifecycle guarantees.

## Verification

Run the topic verifier, compile an editor target on Windows, and exercise stdin,
stdout, stderr, exit status, and termination against a short-lived test child.
The runtime smoke must also prove that incomplete or invalid platform pipe
handles fail before a partially initialized `Process` object is returned. Run a
repeated-child stress probe and verify that native handle count, object count,
and static memory do not grow with the number of completed children.

The Windows pipe/exit/termination fixture is
[verification/smoke.gd](verification/smoke.gd), with success marker
`PROCESS_SMOKE_OK`. Run it from a local temporary project copy as described in the
[fixture guide](../../../tests/README.md); it is not a replacement for the broader
handle and allocation stress checks above.

## Upstream status

Active personal feature; not patch-equivalent to the cached upstream master.
