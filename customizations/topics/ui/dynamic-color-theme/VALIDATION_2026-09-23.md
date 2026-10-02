# Dynamic Color Theme 验收记录

验收日期：2026-09-23。对应 [最终优化计划](FINAL_OPTIMIZATION_PLAN.md) 的 T01–T10。本次完成源码、构建、运行、编辑器、性能与补丁目录验收。

源码提交为 `3a6537d06bca0cbcacfa8f70952ad9451deba199`，分支为 `engine/personal/main`。计划基线为 `db1af1e99a5025bfbf8b05ef27dd4a5e739ec371`，其后的 13 个提交分别保留了计划要求的意图边界和 topic trailer。`master` 与 `origin/master` 均保持 `5ec4857b340b6284a18b49b2eda462bd250f219a`。

## 验收结果

| 验证层 | 结果 | 原始记录，均相对于证据根目录 |
| --- | --- | --- |
| C++ ColorScheme / Theme / 动态节点 / StyleBox | 52 / 52 用例，2,281 / 2,281 断言；包含 ColorRoleTransform | `cpp-current/cpp.log` |
| Windows 运行场景 | 311 项检查，0 失败 | `runtime-final/runtime.log` |
| 真实 Theme 编辑器及 Inspector | 201 项检查，0 失败 | `editor-raw-states/editor.log` |
| 常规 Debug template + PCK | 311 项检查，0 失败 | `template-debug-standard/template.log` |
| Release template + PCK | 311 项检查，0 失败 | `template-release-packed/template.log` |
| dev Debug template + PCK | 311 项检查，0 失败 | `template-debug-packed/template.log` |
| 纹理工作次数 | 9 / 9 输入组合通过 | `benchmark-current/benchmark.log` |
| GDScript 后置检查 | 3 个修改文件均通过 `gdscript-post-check --format` | 文件身份见 JSON 清单 |
| PowerShell 兼容性 | Windows PowerShell 5.1 解析与实际执行通过 | `tests/verify-dynamic-color-theme.ps1` |
| 补丁目录 | 导出及完整栈、本 topic、ColorRoleTransform topic 检查通过 | `export-patches.log`、`verify-*.log` |

证据根目录为 `.validation/dynamic-theme-final/closeout-20260923/`。所有最终运行/编辑器日志记录的引擎 hash 均与源码提交一致，stderr 为空。构建日志中保留了本机未安装可选 ANGLE 依赖的提示；验收使用 Windows 的 `gl_compatibility` 显示后端。

运行场景验证了消费者元数据、静态覆盖、父主题/variation、源纹理更新、ColorRect 的五种解析路径及像素值、文本/二进制场景往返。新增的指针输入覆盖折叠标题的悬停与焦点、上下标题位置、Tab 选中与禁用、SpinBox 按下与禁用，以及滚动后的显示状态。

编辑器检查使用实际资源选择器、导入树、Inspector 属性编辑器和 EditorUndoRedoManager。null 项与有效资源分别验证删除的 undo/redo；有效资源使用不同于 fallback 的源色，并检查恢复后的资源身份和磁盘重载内容。资源重载使用 `CACHE_MODE_IGNORE`。

最终截图位于 `final-output/`，共 15 张，覆盖 6 组 light/dark × contrast、7 组交互状态和 2 张编辑器画面；同目录保留 7 个资源往返文件。截图和资源的 SHA-256 均已记录。模板的独立输出还保留在各自目录的 `output/` 中。

## 构建与复现

本机工具链为 SCons 4.8.1、Visual Studio 14.3、Windows SDK 10.0.22621.0。四个构建及其 console 启动器均报告 `4.8.dev.custom_build.3a6537d06`。

| 构建 | 可执行文件，相对于工作区 | 模式 |
| --- | --- | --- |
| editor_dev | `engine/bin/godot.windows.editor.dev.x86_64.exe` | `dev_build=yes` / `tests=yes` |
| template_debug_dev | `engine/bin/godot.windows.template_debug.dev.x86_64.exe` | `dev_build=yes` / `tests=no` |
| template_debug | `engine/bin/godot.windows.template_debug.x86_64.exe` | `dev_build=no` / `tests=no` |
| template_release | `engine/bin/godot.windows.template_release.x86_64.exe` | `dev_build=no` / `tests=no` |

从工作区执行测试的命令见 [topic README](README.md)。构建时进入 `engine/`，本次使用的命令如下；`$scons` 指向工作区内已准备的 SCons 4.8.1：

```powershell
$toolchain = (Resolve-Path ../.validation/dynamic-theme-final/toolchain/scons-4.8.1).Path
$env:PYTHONPATH = $toolchain
$scons = Join-Path $toolchain 'bin/scons.exe'
rtk proxy $scons platform=windows target=editor dev_build=yes tests=yes accesskit=no d3d12=no module_color_scheme_enabled=yes
rtk proxy $scons platform=windows target=template_debug dev_build=no tests=no accesskit=no d3d12=no module_color_scheme_enabled=yes
rtk proxy $scons platform=windows target=template_release dev_build=no tests=no accesskit=no d3d12=no module_color_scheme_enabled=yes
```

dev Debug template 使用相同选项并设置 `target=template_debug dev_build=yes tests=no`。本次为现有对象缓存上的增量构建，没有把旧版本号的二进制作为当前提交的构建交付。

配置验证均从同一 SCons 入口执行 `--dry-run`：`module_color_scheme_enabled=no` 和仅设置 `modules_enabled_by_default=no` 均以 255 退出并报告 ColorScheme 必需依赖；`modules_enabled_by_default=no module_color_scheme_enabled=yes` 以 0 退出。精简模块组合仅完成配置 dry-run，完整编译与运行覆盖上表中的常规模块集。

模板默认禁止命令行路径覆盖。验证器的 `Template` 模式使用 editor 导出 PCK，复制选定的模板及 SpoutLibrary.dll，然后通过可执行文件旁的同名 PCK 启动。无需更改模板的路径覆盖选项。项目入口为 `main.tscn` 和 `DynamicThemeSmoke` 主循环；独立模板将截图与资源保存到自身目录的 `output/`。

| 主二进制 | SHA-256 |
| --- | --- |
| editor_dev | `5a458b468484ce241bb8b724c572e0eafdbd932cdf85ebcd04d3e558dfbb7db1` |
| template_debug_dev | `e9b6b9c9feee6724f8c651eae2529629dba0008bcac5f1b14fe649123a3a17da` |
| template_debug | `540d0dce35544bed2d467f2edab2df033ea1793be6fe7455b9801bd5942303ae` |
| template_release | `899784fee2cc6881ebf6cdef0c8025ab3b121ee5d5466de1db250d8ad0bbc248` |

console 启动器、运行依赖、三个 PCK、验证脚本和全部最终日志的完整身份见 [validation-20260923.json](validation-20260923.json)。

清单中的 `sha256` 和 `bytes` 对应验收时的文件字节。Git 对三个 GDScript 和 `stack.lock.json` 进行了 CRLF/LF 规范化，这四项另记 `git_blob_sha256` 和 `git_blob_bytes`，用于核对提交内容；已逐项确认两种身份仅有换行符差异。其余版本化文件的提交内容与清单中的文件身份一致。

## 性能结果与测量范围

颜色读取使用同机 dev editor，预热后每组 20,000 次读取，共 7 组，记录中位数。对照文件为 `engine/bin/godot.dynamic-theme-baseline.exe`，SHA-256 为 `b023a093d0666b7539fd905ed4d6870c6cdfd06cf4ca839c6aff2d7087f2b6ba`；它与前序会话保存的 `.validation/dynamic-theme-final/godot.baseline.exe` 字节一致。

该对照文件来自前序未单独提交的工作区快照，已经包含纹理种子缓存。内嵌的 `db1af1e` 版本号不能确定其完整源码状态。因此，以下倍数描述两个明确记录的二进制；T06 的验证依据是工作次数和兼容回归。

| 读取路径 | 对照中位数，μs | 当前中位数，μs | 本次观察 |
| --- | --- | --- | --- |
| Control 本地 role | 353,196 | 21,536 | 16.40 倍 |
| Control 继承路径 | 21,229 | 22,796 | 未显示墙钟收益 |
| Window 本地 role | 350,076 | 26,066 | 13.43 倍 |
| Window 继承路径 | 26,416 | 28,081 | 未显示墙钟收益 |

静态样式的单批 256 节点测试中，独立样式实例数由 256 降到 1，当前结果复用源资源。解析时间由 22,710 μs 降到 2,745 μs；Godot allocator 的增量由 639,972 bytes 降到 166,356 bytes。C++ 回归另行检查颜色重复读取的求值次数、全部相关失效路径，以及动态样式和脚本样式的隔离。

纹理矩阵为 256²、1024²、4096²，分别覆盖不透明 RGBA8、混合透明像素和 S3TC，共 9 组。CPU 测试纹理每次取图返回独立快照，以保持压缩源在多次读取间的一致性。所有组合在 12 次 dark/contrast 参数更新后取图累计仍为 1，源内容信号后为 2，`STATIC` 输出保持一致。

4096² 首次提取为 2.79–3.89 秒；大图首次读取、解压与量化仍同步执行。当前测量使用确定性纹理，未覆盖所有真实照片的颜色分布。`OS.get_static_memory_*` 记录 Godot allocator，不能代表整个进程 RSS 或所有 MCU 分配。墙钟数据用于这次同机比较；自动验收依赖工作次数、资源身份和实际输出，未设置脆弱的耗时阈值。

## 补丁目录与保留记录

已依次运行 `export-patches.ps1 -Replace`、`verify-stack.ps1`、`verify-topic.ps1 -Topic ui.dynamic-color-theme` 和 `verify-topic.ps1 -Topic ui.color-role-transform`，退出码全部为 0。目录包含 51 个补丁和 31 个 topic；本 topic 为 14 个补丁。原有 38 个补丁的 source commit、路径、topic 和 SHA-256 逐项相同，新增 13 个优化补丁。

最初的两个失败尝试保留：`build-editor.log` 记录 PowerShell 对可选 ANGLE stderr 提示的处理问题，随后改用原生进程重定向并构建成功；`runtime-template-debug/runtime.stderr.log` 记录模板拒绝未打包 `--path` 调用，随后 PCK 验收通过。

每次验证均执行专属进程清理；最终审计匹配的 Godot 测试进程为 0。已有 graphify 缓存保留，并通过 `engine/.git/info/exclude` 作本地排除。源码工作区保持干净，`master` 未改变。

构建验收当时的交付位置是 `engine/bin/`、源码提交栈和 `customizations/`；随后已完成独立发布步骤：日常 `export/` 当时替换为 `a87330bfbdc5e03971cb76dae4d1ba248f2118fd` 构建，build manifest SHA-256 和发布回执当时记录在 `export/publication-manifest.json`。2026-10-02 工作区整理已停止维护旧 export 归档，历史回退目录不再是有效入口；当前入口见根目录的 `ENGINE_LOCATIONS.md`，本记录不代表当前发布身份。本 topic 的 204 editor、311 runtime 和 C++ 检查只证明 color-dynamic 引擎功能，不延伸为其他项目的视觉验收。
