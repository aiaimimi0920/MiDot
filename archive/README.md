# 历史归档边界

本目录保存旧个人提交的来源和恢复资料，不参与当前引擎初始化、日常构建或补丁重放。当前维护栈的精确恢复输入是 `customizations/stack.json`、补丁、锁文件及 `customizations/personal-history.bundle`，不要将后者当作临时文件清理。

## 继续跟踪的历史资料

- `legacy-godot-c69dbaf9/`：旧个人提交历史的增量 bundle、原始补丁、提交清单和恢复前置条件。
- `legacy-godot-4-3-1-03e0afdd/`：另一条旧个人提交线、原始补丁和个人图标素材。
- `legacy-color-core-net.patch`、`legacy-color-module-c69dbaf9.tar`：旧颜色功能源码资料。
- `analysis/` 中的少量补丁、差异、代码片段和图标预览：迁移分析的原始依据。
- `verification/godot-patch-stack-actual-replay.txt`：历史重放验证记录，不代表当前版本的测试结果。

以上资料保留用于追溯，不应直接作为现代引擎补丁重新应用。旧 bundle 的恢复前置提交见各自 README。

## 不再跟踪的本地输出

- `analysis/color-docgen/`、`analysis/color-doc-mergecheck-20260830/`。
- `verification/spine-doctool/`、`verification/spout-doctool/`、`verification/spout-doctool-final-20260830/`。
- `export-*/`、`engine-color-dynamic-*/`：旧编译包与回退备份。

前两组是 doctool 或文档对比快照，可从相应版本重新生成，不是项目必需源码。2026-10-02 整理仅取消其 Git 跟踪，本地原文件保留；旧 Git 历史不重写。后续运行结果应写入工作区外的临时目录，长期交付放在被忽略的 `export/`。

## 测试源码迁移

| 原目录 | 当前维护位置 |
| --- | --- |
| `verification/gdscript-helper-smoke/` | `customizations/topics/scripting/gdscript-helper/verification/` |
| `verification/process-smoke/` | `customizations/topics/runtime/process-api/verification/` |
| `verification/gif-smoke/` | `customizations/topics/media/gif-support/verification/` |
| `verification/spout-smoke/smoke.gd` | `customizations/topics/media/spout/verification/api_smoke.gd` |
| `verification/spine-smoke/` | 已有的 `customizations/topics/animation/spine-runtime/verification/` |

Spine 的项目文件、两个脚本和空 skeleton 输入与正式目录逐字节相同，保留正式副本即可。Spout 的 API 边界测试与已有生命周期测试覆盖不同场景，因此两者都保留。GIF 的 `fixture.gif` 及其导入设置是输入；`roundtrip.gif`、`transparent.gif`、`quantized.gif` 及对应导入元数据是测试输出，不再提交。

这些原目录仍可在旧提交中追溯；本地旧副本保留但被忽略，不再作为维护入口。运行方法见 [测试说明](../customizations/tests/README.md)。
