# Godot 引擎目录登记

更新：2026-09-23。本登记覆盖 Godot 工作区与本次 NPR 工作产生的交付构建目录。

## 日常入口

唯一日常发布目录是 [export](C:/Users/Public/nas_home/godot/export)，当前版本 `4.8.dev.custom_build.3a6537d06`，完整源码提交 `3a6537d06bca0cbcacfa8f70952ad9451deba199`。使用其中的 [editor](C:/Users/Public/nas_home/godot/export/godot.windows.editor.x86_64.exe) 打开项目；对应的 Debug / Release templates 和 console 启动器均在同一目录。

源码位于 [engine](C:/Users/Public/nas_home/godot/engine)，分支 `personal/main`。个人补丁目录为 [customizations](C:/Users/Public/nas_home/godot/customizations)，当前提交 `6ac2b3e392b6d44c32da1759230ad6e0c1197357`。`engine/bin` 是编译中间输出，不作为 RoleNPR 默认运行入口。

本次发布对 7 个二进制文件记录了完整 SHA-256，6 个 EXE 版本探测全部通过；构建与发布信息分别记录在 [build-manifest.json](C:/Users/Public/nas_home/godot/export/build-manifest.json) 和 [publication-manifest.json](C:/Users/Public/nas_home/godot/export/publication-manifest.json)。源码修改、发布后测试与项目入口说明见 [RoleNPR 引擎发布说明](C:/Users/Public/nas_home/RoleNPR/docs/NPR_ENGINE_PATHS.md)。

该发布保留完整定制补丁栈，包括非 NPR 功能。原有 24 个主题、28 个补丁全部保留；当前 31 个主题、51 个补丁已通过正式验证器。非 NPR 二进制验证和覆盖边界见 [完整补丁保留核验](C:/Users/Public/nas_home/RoleNPR/docs/NPR_ENGINE_PATCH_COVERAGE.md)。

## 回退与历史构建

本轮被替换的 db1af1e99 export 完整备份为 [export-db1af1e99-20260923-070725](C:/Users/Public/nas_home/godot/archive/export-db1af1e99-20260923-070725)。其 9 个文件保持原始字节，版本为 `4.8.dev.custom_build.db1af1e99`；此前 69903a894 的更早备份仍位于 [export-69903a894-20260910-133419](C:/Users/Public/nas_home/godot/archive/export-69903a894-20260910-133419)。只有明确回退时才使用这些目录；恢复旧引擎还必须配合对应的旧项目版本和校验配置。

下表各目录均位于 `C:\Users\Public\rolenpr_delivery`。它们保留历史验收证据，当前项目默认入口不会选择这些目录。有 manifest 的构建按其声明登记；这次没有重新认证每一个历史构建的运行表现。

| 历史目录 | 身份 | 用途 |
| --- | --- | --- |
| `engine-postlight-3090ee48` | `3090ee48efce30f8936e7721cc8421e851f6a1a4` | post_light 早期构建归档 |
| `engine-postlight-c42de04` | `c42de04a0fbe5a2ca34f705c9712f90b0289bacd` | post_light 导入修复归档 |
| `engine-indexed-b8aa730` | `b8aa7300e249ae270fcf1786d8bb2bf92405d9e4` | indexed BVH 构建归档 |
| `engine-unjittered-9dd6e36` | `9dd6e3622d81e5e07cbeba4642afebc093caa526` | 未抖动投影构建归档 |
| `engine-geometry-decode-04cf045` | `04cf0451bce2064ffab05b3dc69181e114fb7ec8` | 几何读取构建归档 |
| `engine-indexed-streaming-0395f4b` | `0395f4b842f5de97845653e032e34bca1ec5dc23` | 流式 BVH 构建归档 |
| `engine-viewport-order-9d709ee` | `9d709eec4f81199c5b4732e572ffdabda8800dfe` | viewport 排序修复归档 |
| `engine-ssr-boundary-db1af1e` | `db1af1e99a5025bfbf8b05ef27dd4a5e739ec371` | 上一版发布来源归档，保留源码 ZIP 和构建日志 |
| `engine-color-dynamic-3a6537d06-20260923` | `3a6537d06bca0cbcacfa8f70952ad9451deba199` | 当前发布来源归档，含动态 ColorScheme 构建与 311/201 检查 |
| `engine-indexed-streaming-candidate` | 内嵌基线 `04cf0451b`，候选二进制见下方 hash | 未提供完整构建 manifest 的旧候选；不用于日常工作 |
| `engine-native-viewport-order-candidate-20260909` | 内嵌基线 `0395f4b84`，附 viewport-order.patch | 旧的未提交补丁候选；不用于日常工作 |
| `engine-ssr-motion-candidate-20260909` | 基线 `9d709eec4`，有 candidate-manifest.json | 旧的未提交 SSR 补丁候选；不用于日常工作 |

三个候选 editor 已实际执行 `--version`，均退出 0；其内嵌基线版本不能替代完整源码身份。以下 SHA-256 精确识别本机留存的候选文件，避免把它们误当成已提交构建：

```text
engine-indexed-streaming-candidate
4b85da80d326d0854f1f15f2175de184948dc3327050351bef779c8689fbd7c5

engine-native-viewport-order-candidate-20260909
ca17c826beba28d5b0b286ca389ff27c17e2b6902c3d641682f3cb80435b7ac3

engine-ssr-motion-candidate-20260909
2186c8525506c45919fc3313517522caed8239b2d548d5066f216fdbcf72393e
```

候选身份探测日志位于 [archive-identities-20260910](C:/Users/Public/nas_home/godot/.validation/archive-identities-20260910)。Godot 自身的 `archive/legacy-godot-4-3-1-03e0afdd` 和 `archive/legacy-godot-c69dbaf9` 是此前保留的旧源码目录，不作为本轮构建源或日常入口，也没有重新认证。

## 后续维护约定

在 `engine` 修改和提交源码，按 AGENTS.md 维护个人补丁栈，构建后验证身份与必要运行行为，再发布完整一套文件到 `export`。发布时保留有版本标识的回退备份，同时更新 RoleNPR 的默认构建 pin。不要让 editor、template、console 启动器或 DLL 分别来自未经核验的构建。

[publish_engine.ps1](C:/Users/Public/nas_home/RoleNPR/scripts/publish_engine.ps1) 是本次固定版本的可重复发布脚本，包含源码/文件校验、暂存、版本探测、备份和目录切换；它固定 `3a6537d06`，后续引擎升级须在验证后有意更新 pin。历史 profile 仅用于显式重现实验。本文没有更改系统文件关联，也没有清除其他项目或工具安装的 Godot。
