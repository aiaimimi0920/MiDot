# MiDot security checks

MiDot owns a Godot patch catalog and workspace scripts, not a complete Godot
checkout. This configuration adds weekly GitHub Actions update pull requests,
secret detection in tracked UTF-8 text (including patches), and an offline
Actions security audit. It does not build, sign, publish, deploy or merge anything.

## Scope and dependency ownership

`dependabot.yml` manages only `github-actions` at `/`, weekly with at most two
version-update PRs and a seven-day version-update cooldown. There are no npm, Cargo, Go or pip dependency locks in this
repository. No ignore rules, automatic merging or permission changes are added.
Dependabot does not update the custom scanner JSON lock or Godot patch revisions.

The exact main baseline for this change is
`e85a0cf0e239232f1679c29226d1263c30a22c08`. Its existing
`customizations/stack.lock.json` pins official Godot upstream
`5ec4857b340b6284a18b49b2eda462bd250f219a` and personal integration
`38b6ddee72e16d9d646057ee6ba533c122afc47c` (56 patches). Neither is changed here.

The existing [external upstream audit](../customizations/audits/external_upstreams.md)
records giflib 5.2.2 (`44241952659c5db27da3d9db85d910c2b6904216`),
Spout2 2.007.017, Spine 4.1.56 (`77a5db0ec6d16331f5efbaa7662bba9355bd3424`),
and Material Color Utilities (`5b3618b16fdc3825e21d5679bafd144662088ea1`).
That audit is dated 2026-08-31, refers to the then-current 21 topics, and is
provenance rather than a current vulnerability assessment. Spout is a recorded
release rather than an exact upstream commit. This PR does not silently refresh
any external component or declare it free of vulnerabilities.

## What runs

The `Security checks` workflow runs on PRs, main pushes, Mondays at 03:17 UTC,
and manual requests. Its token has only `contents: read`; checkout does not
persist credentials. It uses official actions pinned to full commit identities.
Gitleaks 8.30.1 and zizmor 1.30.1 are downloaded from their official GitHub
releases. `scripts/security/tool-lock.json` pins each Linux/Windows x86_64 archive
and executable SHA-256. Existing cached binaries are verified before every run;
a wrong hash fails rather than replacing or executing that file.

The runner creates a temporary snapshot of `git ls-files --cached`. It reads
the current tracked working-tree bytes, refuses missing files and symlinks,
rejects tracked `.gitleaksignore` files and non-UTF-8/binary Actions definitions,
offers all UTF-8 text as scanner input, and reports hashes plus skipped
binary/non-UTF-8 paths. The input count is not a count of files audited by every
secret rule: Gitleaks' pinned built-in rules and allowlists can omit text paths
and tokens, including SVG, GLTF and lock files. These limitations are also
recorded in the sanitized report. No broader secret-detection coverage is claimed.
It does not traverse `.git`, untracked engine checkouts, downloads or build output.
Binary bundle contents, Git history, UTF-16 files and binary libraries/assets are
not covered. Patches are scanned as text, not compiled or audited as C++ programs.

Gitleaks uses the pinned tool's built-in rules with an explicit configuration,
an empty ignore file and `--ignore-gitleaks-allow`. Because Gitleaks also discovers
root ignore files automatically, tracked `.gitleaksignore` files fail preparation
rather than suppress findings. Output is captured with
`--redact=100`; only rule, file and line survive into the sanitized report.
zizmor audits explicit tracked workflows, actions and Dependabot definitions
with `--offline --strict-collection --no-config --no-ignores --format=json-v1`. Offline mode
does not run GitHub API audits such as checking known vulnerable action versions.

Missing/empty input, tool/hash failures, missing or invalid JSON, unexpected exit
codes, malformed findings and any reported finding produce a failing check.
JSON findings are examined even when an external process returns zero. We do
not use SARIF exit status as a pass signal: zizmor's SARIF format can return zero
with findings. The workflow also runs real synthetic-secret and dangerous-workflow
negative cases, a clean positive case, and runner failure regression tests.

`summary.json` is retained as a private repository Actions artifact for 14 days.
Raw scanner output, secret values and source snippets are not uploaded. An artifact
is not a GitHub Security alert. CodeQL/SARIF uploading and private-repository
product eligibility are not configured or claimed; no paid service or trial is
enabled. No account switch or required branch protection is implied by these files.
Repository owners review findings and merge all MiDot PRs manually.

## Local verification and maintenance

Python 3.10+ and Git are required; scanners need no token. Commands download only
the pinned tools into the chosen cache and do not install system software:

```sh
export MIDOT_SECURITY_TOOLS=/absolute/path/midot-security-tools
python3 -m unittest discover -s scripts/security -p test_checks.py -v
python3 scripts/security/run_checks.py --tools-dir "$MIDOT_SECURITY_TOOLS" --output /absolute/path/report
```

On Windows set `MIDOT_SECURITY_TOOLS` to an absolute cache path and use `python`.
Review `summary.json` for exact HEAD, input hashes, scanner identities, counts,
findings and omissions. Local staged edits are represented by their file hashes;
an exact commit claim additionally requires the working tree to match that commit.

To update scanner tools, review the official release, independently verify its
published archive hash, extract only the executable, record the executable hash,
and rerun all positive/negative cases on the supported platforms. Changes to a
custom tool lock require manual review; Dependabot will not discover them.

References: [Dependabot Actions configuration](https://docs.github.com/en/code-security/reference/supply-chain-security/dependabot-options-reference#directories-or-directory),
[Gitleaks commands](https://github.com/gitleaks/gitleaks#commands),
[zizmor operating modes and exit codes](https://docs.zizmor.sh/usage/).
Loom main `8fa17909029238a941eb1ba4f93833a12dedbecc` provided the update/scan
separation reference. Its dependency ecosystems and OSV wrapper were not copied.
