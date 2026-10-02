# Topic Catalog

Create one directory per modification idea, using the `path` declared in
`../stack.json`. Organize by intent, not merely by the Godot source directory.

Example:

```text
topics/editor/faster-project-scan/README.md
```

Each topic README must document:

1. Intent and the original problem.
2. Observable behavior contract.
3. Original Godot version or commit, when known.
4. Dependencies on other topic IDs.
5. Important source areas and symbols.
6. Conflict-resolution guidance that preserves the intended semantics.
7. Build, test, benchmark, or runtime verification.
8. Upstream status: active, upstreamed, superseded, disabled, or obsolete.

Each commit assigned to a topic must include the configured trailer, for
example:

```text
Godot-Patch-Topic: editor.faster-project-scan
```

Generated patch files live under `../patches/<topic-path>/`. Never edit them by
hand; fix the corresponding commit on `personal/main` and export again.
