# 测试源码与运行产物

正式测试源码放在本目录或 `customizations/topics/<主题>/verification/`，不要提交到根 `.tmp/` 或 `archive` 的运行目录。测试时优先复制项目到独立临时目录，让 `.godot`、生成图片、日志和导出文件留在临时目录中。

## 维护脚本回归

从 MiDot 根目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/tests/run.ps1 -TemporaryDirectory C:/Users/Public/nas_home/AI/GameEditor/linshi
powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/scripts/verify-stack.ps1 -RunTests
```

第一项使用独立合成 Git 仓库；第二项检查实际引擎提交身份、补丁完整性及主题源码契约，不替代原生运行测试。

## 原生测试入口

以下路径均相对 MiDot 根目录，需要包含对应定制模块的编辑器，而不是官方未定制 Godot：

| 项目或脚本 | 用途与完成标记 |
| --- | --- |
| `customizations/tests/dynamic_color_theme/` | 当前主题回归，使用同目录旁的 `verify-dynamic-color-theme.ps1` |
| `customizations/tests/dynamic_color_theme_review/` | 历史审查探针，输出 `REVIEW_PROBE` / `COMPARISON_PROBE` JSON 观察结果 |
| `customizations/topics/scripting/gdscript-helper/verification/smoke.gd` | 解析、补全与符号查询，`GDSCRIPT_HELPER_SMOKE_OK` |
| `customizations/topics/runtime/process-api/verification/smoke.gd` | Windows 子进程管道、退出状态和终止，`PROCESS_SMOKE_OK` |
| `customizations/topics/media/gif-support/verification/` | GIF 读写及两种导入器，`GIF_SMOKE_OK` / `GIF_SPRITE_IMPORT_OK` / `GIF_ANIMATED_IMPORT_OK` |
| `customizations/topics/media/spout/verification/api_smoke.gd` | 不依赖外部 Spout peer 的 API 边界，`SPOUT_SMOKE_OK` |
| `customizations/topics/media/spout/verification/smoke.gd` | 已有命名内存与生命周期测试，`SPOUT_LIFECYCLE_OK` |
| `customizations/topics/animation/spine-runtime/verification/` | 已有 Spine 注册、无效输入和 atlas 测试 |

例如，在 Windows PowerShell 下复制后运行 GDScriptHelper 测试：

```powershell
$godot = (Resolve-Path engine/bin/godot.windows.editor.x86_64.console.exe).Path
$temporaryRoot = 'C:/Users/Public/nas_home/AI/GameEditor/linshi'
$project = Join-Path $temporaryRoot ('midot-helper-' + [guid]::NewGuid().ToString('N'))
Copy-Item customizations/topics/scripting/gdscript-helper/verification -Destination $project -Recurse
& $godot --headless --path $project --script res://smoke.gd
if ($LASTEXITCODE -ne 0) { throw 'GDScriptHelper smoke failed' }
```

自动化调用还应设置超时，并在结束时检查、清理仅属于本次项目路径的 Godot 测试进程，不应终止其他编辑器会话。Windows 的 Process / Spout 测试需要本地盘或已存在的持久映射盘路径，避免 UNC 工作目录触发临时盘符。

Spout 的 `api_smoke.gd` 有意传入六组非法参数，原生校验会输出六条对应的 `ERROR: Condition ... Returning: ...`。验收应同时核对退出码 0、`SPOUT_SMOKE_OK` 和这六个已知校验分支，不能忽略其他错误，也不能把预期的拒绝日志误判为测试失败。

GIF 测试先在临时副本中运行 `smoke.gd`，再用编辑器 `--headless --editor --import --quit` 导入。随项目提交的 `fixture.gif.import` 选择 `sprite_frames`，对应 `verify_sprite_import.gd`；测试 AnimatedTexture 时，仅在临时副本中将该设置的 importer/type 改为 `animated_texture` / `AnimatedTexture`，重新导入后运行 `verify_animated_import.gd`。不要提交生成的 GIF 和 `.godot` 缓存。

## 历史探针说明

`dynamic_color_theme_review/` 来自 2026-09-22 的审查项目，本轮只调整归属，不改变它的观察逻辑。它记录 JSON 数据和计时，不是自行断言所有功能通过的测试；当前自动化回归仍以 `dynamic_color_theme/` 和引擎 C++ 测试为准。

对应历史文档的路径已更新为当前维护位置，历史日期、源码版本和当时的检查结果不变。旧 `.tmp/` 本地文件继续保留但不再跟踪。
