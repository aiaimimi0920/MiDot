# 当前 Godot 引擎入口

更新：2026-10-02。本文登记当前 MiDot 源码和最新交付包，不再列出历史回退目录。绝对路径仅适用于本机；新环境按 [README](README.md) 初始化。

## 当前源码

- 引擎目录：[engine](C:/Users/Public/nas_home/godot/engine)，独立仓库，分支 `personal/main`。
- 引擎提交：`38b6ddee72e16d9d646057ee6ba533c122afc47c`。
- 锁定官方基线：`5ec4857b340b6284a18b49b2eda462bd250f219a`。
- 当前补丁目录：[customizations](customizations)，包含 56 个补丁、31 个主题。
- `engine/bin/` 是编译输出；交付和手工测试使用下方完整发布包。

`customizations/personal-history.bundle` 与补丁、锁文件共同构成当前版本的初始化输入，不能按历史备份删除。

## 最新交付包

目录：[MiDot-20261002-pr1](C:/Users/Public/nas_home/godot/export/MiDot-20261002-pr1)，版本 `4.8.dev.custom_build.38b6ddee7`。

- [编辑器 EXE](C:/Users/Public/nas_home/godot/export/MiDot-20261002-pr1/godot.windows.editor.x86_64.exe)。
- [Windows x86_64 导出模板](C:/Users/Public/nas_home/godot/export/MiDot-20261002-pr1/MiDot-templates-windows-x86_64.tpz)。
- [编辑器和模板完整 ZIP](C:/Users/Public/nas_home/godot/export/MiDot-20261002-pr1/MiDot-Windows-x86_64-editor-and-templates.zip)。
- [使用说明](C:/Users/Public/nas_home/godot/export/MiDot-20261002-pr1/README.md)、[构建身份](C:/Users/Public/nas_home/godot/export/MiDot-20261002-pr1/build-manifest.json) 和 [验证结果](C:/Users/Public/nas_home/godot/export/MiDot-20261002-pr1/verification-manifest.json)。

编辑器、Debug / Release 模板、console 启动器和 `SpoutLibrary.dll` 应使用同一套已核验文件。导出游戏后还须将该 DLL 复制到游戏 EXE 同目录，详见包内说明。

## 维护边界

当前仓库不再包含 `archive/`，已有克隆中的本地遗留归档可以删除，不作为源码、测试或运行入口。正式测试输入和脚本在 `customizations/tests/` 或各主题的 `verification/`，运行产物写入独立临时目录。

此次整理没有修改引擎源码、原 `export/` 根目录文件、系统文件关联或其他项目的引擎路径与版本 pin。其他项目是否切换到此交付包，应在该项目内单独验证；本文不宣称完成了 RoleNPR 等项目的业务验收。
