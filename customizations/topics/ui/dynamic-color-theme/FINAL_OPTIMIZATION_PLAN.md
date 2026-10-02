# Dynamic Color Theme：最终优化计划

日期：2026-09-22；最终验收：2026-09-23。状态：T01–T10 已完成，最终证据见文末记录与验收报告。

本计划综合了[独立代码审查](INDEPENDENT_REVIEW_PLAN.md)与[既有计划的逐项复核](PLAN_COMPARISON.md)。执行时以本文为准；两份输入文档保留审查过程和证据来源。

基线为 `engine/personal/main` 的 `db1af1e99a5025bfbf8b05ef27dd4a5e739ec371`，Godot `4.8.dev`；topic 为 `ui.dynamic-color-theme`，基础功能提交为 `5884522a3dd1b906d8d08086decc30dfa53e3202`。引擎源码位置以 `engine/` 为根，`customizations/` 和 `.tmp/` 路径以工作区为根；行号对应该 HEAD。

## 目标与优先级

先保证动态主题能正确保存、恢复和刷新，再减少配色与样式解析中的重复工作。已有 Material 算法金样、静态 authored color、覆盖优先级和用户资源格式都作为兼容约束。

| 工作项 | 优先级 | 要解决的问题 | 来源 |
| --- | --- | --- | --- |
| T01 | P1 | Button/AcceptDialog 等节点的覆盖项保存丢失 | 独立 R01；原 F-08 |
| T02 | P1 | STATIC 语义错误；动态颜色/scale 的 has/get 不一致 | 独立 R02；原 F-01、F-06 |
| T03 | P1 | Theme 编辑器把 null 项写成回退资源 | 独立 R04；原 F-08 |
| T04 | P1 | Texture2D 内容变化不能驱动重新配色 | 独立 R03；原 F-03 |
| T05 | P2 | 默认键名与消费者不一致；ColorRect 解析路径差异 | 独立 R05、R06 |
| T05a | P2 | 新增/复合控件缺少 dynamic-color-theme 元数据（不含已确认无需适配者） | 本轮控件交叉复查 |
| T06 | P2 | dark/contrast 重复执行纹理提取 | 独立 R07；原 F-04 |
| T07 | P2 | role override 重复求色；静态 StyleBox 多余副本 | 独立 R08、R09；原 F-07 |
| T08 | P2 | 失败输入、底层 helper 通知和文档契约 | 原 F-02、F-05；独立边界检查 |
| T09 | P2 | ColorScheme 的实际必需依赖与构建选项不一致 | 独立构建边界检查 |
| T10 | 验收 | 当前源码构建、资源往返、编辑器与运行场景验证 | 两份计划合并 |

P1 优先修复可复现的数据和行为错误。P2 内部也有先后：先修可见行为，再做有证据的性能优化，最后补低可达的内部接口约束。T10 的具体测试随各项修复逐步落地，最后集中验收。

## 已确定的兼容规则

本地覆盖仅在当前类/当前 variation 的查找范围内生效，保留现有指定其他 `theme_type` 时的行为。颜色解析采用下表，Control 与 Window 必须一致。

| 有效本地 role override | 本地静态 color override | 目标结果 |
| --- | --- | --- |
| 有效动态 role，且能取得 scheme | 有或无 | 动态 role × 有效 scale；保留现有优先级 |
| STATIC | 有 | 本地静态 color |
| STATIC | 无 | 主题静态 color/fallback；不重新启用继承的动态 role |
| 无 | 有 | 本地静态 color |
| 无 | 无 | 继承/主题动态 role 可解析时使用它，否则使用静态 color/fallback |
| 动态 role，但没有可用 scheme | 有或无 | 静态 color 的正常回退路径 |

`has_theme_color()` 判断静态项目或有效声明的动态颜色是否可用。有效的非 STATIC role 加可用 scheme 应计为存在；只有 getter 的终端默认返回值不计为存在。Theme 资源的原始数据存在性、Theme 的 scheme 回退语义和节点的有效颜色存在性保持区分。

Theme 编辑器必须保留“不存在、存在且 null、存在且有效资源”三个状态。运行时 getter 可以回退，序列化、导入和 undo/redo 使用原始数据。

ColorScheme 的合法输入继续同步更新，setter 返回后即可读取新值。先保留当前 MCU 版本、合法输入的输出、ColorRole 数值、已有属性名及 `get_color(STATIC)` 的源色行为。StyleBox 的 `get_*_color()` 返回 authored color，`get_resolved_*()` 用于动态绘制；不得恢复对共享源样式写入节点 scheme 的实现。

## 第一阶段：修复保存、解析和更新

### T01 · 补齐动态覆盖项的存储与继承

证据：`scene/gui/control.cpp:496-530` 只从 class item 绑定生成属性，普通 Button 的动态 role/scheme/scale 没有注册；`scene/main/window.cpp:202-302` 只枚举默认主题的精确类名。探针中 Button 和 AcceptDialog 的 role、单项 scheme、默认 scheme、scale 在 PackedScene 往返后丢失，Window 同名支持项保留。

实施：

1. 定义属性清单的来源：真实消费绑定、支持的动态伴随项目及已设置覆盖；合并后去重。只补动态功能涉及的条目，不顺带重写所有主题数据类型。
2. 颜色主题项补齐 `<color>_role`、`<color>_scheme`、`<color>_scale`，样式主题项补齐 `<style>_scheme`，并支持 `default_color_scheme` 的 Inspector 展示和存储。样式自身的 role/scale 继续属于 StyleBox 资源，不新增节点级 `<style>_role` 或 `<style>_scale`。已设置的自定义动态覆盖也不能静默丢失。
3. Window 清单包含基类支持项，验证 AcceptDialog 等派生类。保持现有 `theme_override_color_roles/`、`theme_override_color_schemes/`、`theme_override_colors/` 路径。

验收：先让当前失败的 Button/AcceptDialog 用例成为回归测试，再覆盖 Label/Window 对照；测试属性列表、节点 duplicate、PackedScene、`.tscn`/`.scn` 保存重载，确保资源值与预期共享关系保留。Inspector 勾选、清空、撤销和重载后状态一致。

### T02 · 统一 STATIC、有效存在性与动态 scale 的语义

证据：`control.cpp:3939-3963` / `window.cpp:2864-2888` 在本地 STATIC 后又读取继承 role；`control.cpp:4225-4239` / `window.cpp:3097-3111` 的 has 只检查静态 color。两类节点均已复现动态 scale 被跳过：实际 `#ffb781`，应用已声明 scale 后应为 `#4e1b00`。

实施：先按兼容表修正两类节点的有效 role 决策，再让 has 与 get 共用这部分规则。有效动态 `_scale` 应参与乘法，保留 `_scale` 不再递归寻找自身 scale 的终止规则。不要通过“所有 role 项都算有颜色”或“所有 get 的默认返回都算存在”实现。

在回归结果一致后，可以抽取一个小型内部解析 helper，承接角色选择、scheme 可用性和 scale 规则；节点自己的 override map、线程 guard、类型查找和通知仍留在各自实现中。

验收：两类节点使用同一矩阵测试无 role、STATIC、动态 role，本地 color 有/无，继承、variation、显式其他类型、缺 scheme、role-only color、role-only scale、删除覆盖、scheme 变化。既有“本地动态 role 优先于本地静态 color”的测试继续通过。

### T03 · 让编辑器和历史记录使用原始 scheme 值

证据：`editor/scene/gui/theme_editor_plugin.cpp:3217-3227、3623-3626、3822-3833` 使用会回退的 `get_color_scheme()`；同参数 UndoRedo 重放后，null 项变成有效 fallback。完整导入的 `get_theme_item()` 路径也需要核验（`904-908`）。

实施：建立明确的原始读取入口或复用 `_get()` 对原始属性的语义；编辑器显示已存储项、历史快照、完整导入使用原始值。只在只读预览中执行动态回退。保留当前 `get_color_scheme()`、`has_color_scheme()` 的运行时意义。

验收：不存在/null/有效资源各做添加、修改、清空、删除、完整导入、undo、redo、保存重载；恢复原值及原始存在状态，空项目不会自动变为共享全局资源。测试真实 Theme 编辑器资源选择器，不能仅靠 getter 单元测试交付。

### T04 · 补齐源纹理变更的连接与失效生命周期

证据：`modules/color_scheme/color_scheme.cpp:91-100` 没有连接 `source_texture.changed`，同一引用赋值会提前返回。Texture2D 内容变化探针没有产生 scheme 更新。

实施：增加专用源变更回调；连接新纹理，断开旧纹理，在改用纯色、清空或释放时保持一致。同步更新的发布顺序应为更新输入/内部状态、使相关缓存失效、发布新 scheme、发出既有更新信号。要检查资源加载及纹理读取的线程约束，不把任意来源的回调直接扩展为不受控后台任务。

验收：原地更新、更换引用、重复赋同一引用、纹理与纯色互换、清空、旧资源再次变化、资源释放；新 scheme 到 Theme/Control/Window/StyleBox 的传播正确，旧资源不再触发当前结果更新。ImageTexture 的真实 `set_image()` / `update()` 至少覆盖一种；CPU 可读的测试 Texture2D 用于确定性单元测试。

## 第二阶段：补齐消费者并减少重复工作

### T05 · 修复默认键名，明确 ColorRect 的默认查找

先修两处已确认映射：

- RichTextLabel：`scene/theme/default_theme_dynamic_color.inc:562-582` 的 `font_default_color_*` 与 `scene/gui/rich_text_label.cpp:8258` 的 `default_color` 不一致。
- GraphEdit：inc 的 `activity_color_scheme` / `activity_color_role`（`594、599`）应与 `scene/gui/graph_edit.cpp:3156` 的实际主题键 `activity` 对齐。

修复默认定义和伴随的 scheme/scale；扫描得到的其他孤立键逐个对照继承、别名和消费者后处理。不要把扫描候选全部删除，也不要批量重写用户主题中的同名自定义项目。

另以单独提交对齐 ColorRect。`scene/gui/color_rect.cpp:83-92` 当前直接调用 Theme 对象级默认解析，绕过节点和类型 scheme 覆盖。目标顺序为显式 `color_scheme` 属性、节点/类型默认 scheme、继承 Theme/上下文/fallback，复用已修正的解析机制。

验收：RichTextLabel 正文和 GraphEdit 活动连线随配色变化，静态覆盖仍有效；ColorRect 的本地、类型、父主题和显式属性组合得到一致结果。保留 authored color，检查绘制与无障碍颜色值。真实显示后端的截图验收包括浅色/深色及 contrast 边界。

#### T05a · 新控件覆盖复查（本轮新增）

本轮按控件的真实 `BIND_THEME_ITEM*`、默认主题项目和绘制缓存逐项交叉核对。以下条目已在 `default_theme_dynamic_color.inc` 中建立对应的 role/scheme/scale 元数据；它们继续走 `Control`/`Window` 的通用颜色或 StyleBox 解析路径，没有新增 C++ 解析分支。

| 控件 | 现有消费者与默认项目 | 计划动作 | 级别 |
| --- | --- | --- | --- |
| `FoldableContainer` | `scene/gui/foldable_container.cpp:595-609` 绑定 `title_panel`、`panel`、`focus` 及四个标题颜色；`scene/theme/default_theme.cpp:1397-1426` 提供默认值 | 为四个标题颜色补 `<role>/_scheme/_scale`，为六个 StyleBox 项补 `<scheme>`；验证折叠、悬停、焦点和上下翻转。无需改绘制代码。 | P2 |
| `GraphFrame` / `GraphElement` | `graph_frame.cpp:207-213` 绑定四个面板/标题栏 StyleBox 和 `resizer_color`；`default_theme.cpp:909-922` 提供默认值；`GraphElement` 仅绑定 resizer 图标（`graph_element.cpp:247`） | 为 `GraphFrame` 的 `resizer_color` 和四个 StyleBox 增加元数据；把 `GraphFrameTitleLabel` variation 的文字颜色单独核对，不能把编辑器标题语义误套到普通 `Label`。不为 `GraphElement` 虚构颜色项。 | P2 |
| `TabBar` | 颜色元数据已有 `default_color_scheme`、字体颜色 role/scale（`default_theme_dynamic_color.inc:512-531`）；默认样式和图标在 `default_theme.cpp:1111-1146` | 保留现有字体映射，补四个 `icon_*_color` 的 role/scheme/scale，并为 `tab_*`、`button_*` StyleBox 补 scheme；覆盖选中、悬停、禁用、焦点和滚动按钮。 | P2 |
| `SpinBox` | `spin_box.cpp:694-730` 绑定八个按钮 StyleBox、八个 icon-modulate 颜色；默认值在 `default_theme.cpp:672-729` | 已为按钮背景 StyleBox 补 scheme，为 icon-modulate 颜色补 role/scheme/scale；验证 `SpinBoxLineEdit` 的继承颜色不被误改。无需新增 C++ 分支。 | P2 |
| `SplitContainer` / `HSplitContainer` / `VSplitContainer` | `split_container.cpp:1806-1818` 绑定三种 touch-dragger 颜色、方向图标和 split-bar StyleBox；默认项目及变体在 `default_theme.cpp:1327-1368` | 已为三种 touch-dragger 颜色和 `split_bar_background` 补元数据，并覆盖基类、水平和垂直 variation；图标本身不增加颜色项。 | P2 |
| `ScrollContainer` | `scroll_container.cpp:1032-1042` 绑定 `panel`、`focus` 和两个滚动提示颜色；默认值在 `default_theme.cpp:712-729` | 已为 panel/focus StyleBox 和两个提示颜色补 scheme/role/scale；提示颜色继续受滚动方向和透明度语义控制。 | P2 |
| `HSeparator` / `VSeparator` | `separator.cpp:61-62` 只有 `separator` StyleBox；`default_theme.cpp:1148-1156` 按横竖类型提供样式 | 已补 `separator_scheme`，分别覆盖横竖 StyleBox；未增加不存在的颜色 role，也未修改 `separation` 常量。 | P2 |

这些项目应作为 T05 的消费者补齐子任务分开提交或至少分组测试。验收矩阵新增：每个控件至少验证默认动态 scheme、静态颜色/StyleBox override、父主题继承、variation（尤其 `HSplitContainer`/`VSplitContainer`）和资源保存重载；共享 StyleBox 仍不得被节点级 scheme 原地修改。编辑器专用的 `GraphFrameTitleLabel`、`GraphEdit` 以及用户内容色控件继续按语义单独审查，不能因“绑定了 COLOR”就批量套用角色。

### T06 · 将纹理提取种子与 scheme 参数更新分开缓存

依赖 T04。`color_scheme.cpp:48-73、106-129` 当前将取图/量化与 SchemeContent 构造绑定。探针中设置一次纹理、改变 dark、改变 contrast，取图累计为 1、2、3 次。

实施：保存已提取种子和源失效状态；源内容未变化时，只重建需要的 scheme。首次保留相同像素输入、Celebi 和排名参数，避免把性能变更与选色结果变化混在一起。每次重建要统一使角色缓存失效。

验收：不变纹理切换 dark/contrast 的取图及量化增量为 0；源内容改变后执行一次新提取；纯色路径不取图；现有 MCU 金样、直接 getter、资源保存重载结果不变。

大图优化另行测量 256²、1024²、4096² 等代表尺寸和不同透明/压缩情况的时间与峰值分配。只有证据说明种子缓存仍不足时，才加入固定预算采样或异步方案。采样必须有图像金样，异步必须有结果版本校验、取消/销毁保护和明确的新时序契约；两者都不作为本轮正确性修复的前置条件。

### T07 · 优化热路径缓存与样式副本，分开提交

颜色读取：`control.cpp:3939-3953`、`window.cpp:2864-2878` 的本地动态 role 在缓存之前执行求色。dev 探针中，每组 2,000 次读取，缓存路径为 1,722–2,196 μs，本地 role 路径为 21,140–23,721 μs。先把有效本地覆盖纳入正确的可失效缓存，再测是否需要 ColorScheme 的按角色缓存。若增加 scheme 级缓存，公开命名 getter 与 `get_color()` 使用一致结果，并保留 STATIC 的现有含义。

样式副本：`control.cpp:3826-3843`、`window.cpp:2751-2768` 在静态样式与有效 fallback 不同的情况下也复制。先为可证明不需要注入 scheme 的内建静态样式增加快速路径。动态样式仍需按源样式、有效 scheme 及各自变化正确隔离。未知扩展/脚本 StyleBox 采用保守路径；不建立未经测量的全局共享缓存。

验收：预热后重复同一有效颜色不会重复求值；所有输入和继承变化均正确失效。两个节点共享静态样式时没有无意义副本；共享动态样式使用不同 scheme 时源资源不变、颜色不串用。记录构建模式、样本数、分配数和峰值内存；性能自动测试优先检查工作次数，墙钟时间用于同机对照，避免脆弱的 CI 耗时阈值。

## 第三阶段：补强边界与构建约束

### T08 · 明确失败策略和内部通知契约

纹理失败：空/不可读/不可解压图当前会落到黑色种子，全透明图会使用 MCU 默认蓝色。先固定现状用例并给出可定位的失败诊断，区分“读图失败”和“成功读取但无合适候选”。合法输入结果保持不变。若后续改为事务式拒绝更新或保留上次有效种子，单独定义首次加载、保存重载及源资源原地变化的行为，再单独提交；不默认新增公共 extraction status/last-valid API。

输入边界：补 setter 与构造入口的一致性测试，检查 NaN/非有限参数、非法 ColorRole、无效纹理。选定明确的拒绝或回退规则后实施最小 guard，不改变已有合法范围及现有 enum 数值。

通知：原计划 F-02 中三个 C++ type helper 无通知的事实成立，但公开 rename/remove 实测可以刷新。优先明确 helper 是否允许独立调用，再决定它们负责通知还是由外层批量操作负责。若调整 batching，保证可嵌套且资源信号引用计数正确；当前布尔 freeze/unfreeze 不能直接当作嵌套事务。测试直接 C++ helper 和公开组合操作，避免只追求表面上的代码对称。

文档：说明 role/scheme/scale 命名、STATIC、覆盖矩阵、源纹理实时更新、空项与回退区别、失败策略和同步时序。`Theme.xml:47-52` 把 `has_color_scheme()` 推荐为清除前的原始存在性检查并不严谨，因为它会计入默认 scheme；修正文档或给出精确的存在性查询方式。

### T09 · 明确 ColorScheme 是当前主题系统的必需依赖

`engine/SConstruct:486、1096-1098` 会接受关闭该模块并跳过编译；主题与样式代码却无条件引用 ColorScheme，如 `scene/resources/style_box.h:36`。本轮未执行关闭模块的完整构建，不能宣称已记录具体链接错误。

建议在本轮维护范围内将其明确为当前个人引擎的必需主题依赖，对不支持的关闭组合尽早给出清晰构建错误，并更新对应构建说明。真正支持完全关闭该功能需要跨 core/scene/editor 的条件化设计，另立任务，避免在这次修复中铺设大量条件编译。

验收：常规 editor、debug/release template 构建成功；显式关闭 ColorScheme 的配置按定义早期拒绝；精简模块构建明确启用此依赖。实现专用的 quantizer/score include 可移入 `.cpp` 并检查增量构建；不以 PImpl、移动全部模块或重建枚举系统为完成条件。

## 实施顺序与提交边界

1. T01、T02、T03、T04 各先建立回归断言，再独立修复。T02 的共同解析 helper 只能在双方行为已对齐后抽取。
2. T05 的键名修复、ColorRect 对齐和 T05a 控件元数据补齐分开提交。T06 在纹理失效路径稳定后推进。
3. T07 的颜色缓存和样式分配分开验证、分开提交。遇到没有量化收益的结构优化即可停止，不扩大改造范围。
4. T08 的失败策略、输入 guard、通知契约和 T09 构建约束各有单独验收；只纳入证据支持的变更。
5. 完成 T10。若性能方案未达到同机验证目标，不影响已经独立通过的正确性修复交付。

每个个人提交只包含一个可审查意图，并带唯一 `Godot-Patch-Topic: ui.dynamic-color-theme` trailer。下游 `ui.color-role-transform` 等只做兼容回归；确需修改其独立行为时，使用其自己的 topic，不混入此功能包。

保持 `engine/master` 为 upstream 镜像。不得修改生成的 `customizations/patches/`、`series.txt`、`stack.lock.json` 来代替修改源码提交。个人提交栈变更后按顺序执行：

```powershell
rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/scripts/export-patches.ps1 -Replace
rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/scripts/verify-stack.ps1
rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/scripts/verify-topic.ps1 -Topic ui.dynamic-color-theme
```

## T10 · 验收矩阵与退出条件

回归测试放在既有测试层中，使用真实行为断言：

| 层 | 主要位置 | 必须验证 |
| --- | --- | --- |
| 色彩资源 | `tests/modules/color_scheme/test_color_scheme.cpp` | 现有金样、纹理原地变化/脱离、失败输入、缓存工作次数、getter 一致性 |
| Theme / StyleBox | `tests/scene/test_theme.cpp` 及样式测试 | raw/null/回退、资源共享与信号、authored/resolved、不同 scheme 隔离 |
| 节点 | `tests/scene/test_control.cpp`、`test_window.cpp` | T02 矩阵、T01 往返、派生 Window、类型变化与失效 |
| 消费者 | 合适的现有 GUI 测试及小型集成场景 | RichTextLabel、GraphEdit、ColorRect、Button、Label、Window，以及 T05a 中 FoldableContainer、GraphFrame、TabBar、SpinBox、SplitContainer、ScrollContainer、H/VSeparator 的真实主题结果 |
| 编辑器 | 真实 Theme 编辑器 smoke 场景 | 空项、有效项、导入、清空、删除、undo/redo、保存重载 |
| 下游兼容 | `tests/modules/color_scheme/test_color_role_transform.cpp` 等相关主题测试 | 公共取色与资源契约未被缓存/绑定变化破坏 |

磁盘 round-trip 使用明确的资源加载缓存模式，确保验证的是重新载入的数据，而非命中旧内存对象。编辑器测试检查原始存储状态，预览测试检查解析结果，不能互相替代。

在已配置好的 Windows 构建环境中，使用项目 SCons 入口，例如从 `engine/` 运行：

```powershell
rtk scons platform=windows target=editor dev_build=yes tests=yes accesskit=no d3d12=no module_color_scheme_enabled=yes
rtk proxy bin/godot.windows.editor.dev.x86_64.console.exe --headless --test '--test-case=*[ColorScheme]*,*[Theme]*,*Dynamic theme*,*Static built-in*,*StyleBox*'
```

新增用例若采用其他名称，必须把对应名称加入过滤器，并检查实际执行用例数，不能接受“过滤后 0 个测试通过”。相关模板使用同一源码和模块配置构建。未触及的项目和全引擎测试不作为无条件扩大范围的理由。

验收场景至少涵盖：改变 source color、更新同一纹理、切换 dark/contrast、本地/父主题/variation、派生窗口、共享样式、STATIC、动态 scale、保存重启、编辑器 null 项。需要图像结果的项目使用真实显示后端；headless getter 检查只证明数据解析。

每次自动运行后使用专属 `cleanup-godot-test-processes.ps1` / `godot-test-cleanup` 清理本次 Godot 路径匹配的测试进程，复核没有新增测试进程残留，保留用户正在使用的编辑器。

退出条件：P1 回归全部通过；T05 的消费者行为完成验证；已实施的性能项有同机对照且不改变合法输入结果；约定的构建组合、资源往返和编辑器操作通过；生成的 patch stack 能验证。新的公共 API、异步化、采样政策和大范围结构迁移未获得证据支持时不纳入退出条件。

## 最终实施与验收记录（2026-09-23）

T01–T10 的本轮退出条件已经满足。引擎源码为 `personal/main` 的 `3a6537d06bca0cbcacfa8f70952ad9451deba199`；13 个优化提交均带唯一的 `Godot-Patch-Topic: ui.dynamic-color-theme` trailer。

| 工作项 | 实现提交 | 最终证据 |
| --- | --- | --- |
| T01 | `c6963d8` | Button、AcceptDialog、Label、Window 的属性 usage、duplicate、PackedScene、`.tscn`/`.scn` 往返；真实 Inspector 勾选、清空、undo/redo 与磁盘重载 |
| T02 | `21db102` | Control/Window 的 STATIC、动态 has/get、role-only scale、variation、显式其他类型及覆盖失效矩阵 |
| T03 | `4424576` | 原始 null/有效资源读取；真实选择器、完整导入、删除与清空、undo/redo；有效资源身份及不同源色的保存重载 |
| T04 | `95aeb35` | CPU 纹理与 ImageTexture 原地变化、旧源脱离、同步更新以及 Theme/Control/Window/StyleBox 传播 |
| T05 | `425e16c`、`5727c04` | RichTextLabel/GraphEdit 键名对齐；ColorRect 的父主题、本地、类型、显式 scheme 与 STATIC 像素断言 |
| T05a | `b6c3ace` | 新控件元数据、静态覆盖、父主题、variation、保存重载；真实悬停、折叠、底部标题、选中/禁用 Tab、SpinBox 按下/禁用和滚动状态 |
| T06 | `ce4acab` | 256²、1024²、4096² × 不透明、混合 alpha、S3TC 共 9 组；参数更新后取图累计仍为 1，源变化后为 2 |
| T07 | `b90efd1`、`3b91748` | 颜色缓存求值次数与失效回归；同机角色读取对照；256 个节点的静态样式实例数由 256 降为 1，脚本样式保留保守副本 |
| T08 | `94fc911`、`6db67e7` | 非有限输入、不同纹理失败回退、嵌套通知与共享订阅、公开组合操作和文档契约 |
| T09 | `3a6537d` | 当前提交的 editor、常规 Debug/Release template 及 dev Debug template 构建通过；关闭必需模块的配置早期拒绝；精简配置显式启用依赖的 dry-run 通过 |
| T10 | 验收项目与本次记录 | C++ 52 个用例 / 2,281 条断言；运行场景 311 项；真实编辑器 201 项；三个模板构建分别以 PCK 运行并通过 311 项；补丁栈导出及验证通过 |

Windows 真实显示后端采用 `gl_compatibility`。保留了 15 张最终截图与 7 个序列化资源文件；浅色/深色及 contrast -1/0/1、交互状态和编辑器均有对应记录。三个模板均按默认路径限制构建，以同名 PCK 启动，不依赖 `--path`。

本机使用 SCons 4.8.1、Visual Studio 14.3、Windows SDK 10.0.22621.0；构建显式设置 `accesskit=no d3d12=no module_color_scheme_enabled=yes`。完整编译覆盖常规模块集，精简模块组合的证据限于配置 dry-run。

生成目录包含 51 个补丁、31 个 topic，其中本 topic 为 14 个补丁。原有 38 个补丁的 source commit、路径和 SHA-256 全部保持一致。已依次执行 `export-patches.ps1 -Replace`、`verify-stack.ps1`、本 topic 与 `ui.color-role-transform` 的验证。

完整结果、性能口径和复现命令见 [验收报告](VALIDATION_2026-09-23.md)，构建、脚本、日志、PCK 和截图的身份见 [JSON 清单](validation-20260923.json)。原始证据目录为 `.validation/dynamic-theme-final/closeout-20260923/`，失败尝试保留。最终进程复核没有本次 Godot 测试进程残留。

大图首次提取仍同步执行，本轮未引入采样或异步方案。新构建位于 `engine/bin/`；本计划的源码与补丁验收没有替换日常发布目录 `export/`。
