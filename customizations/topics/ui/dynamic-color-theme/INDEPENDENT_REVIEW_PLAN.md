# Dynamic Color Theme：独立代码审查与优化计划

日期：2026-09-22。独立审查冻结时间：2026-09-22 12:15 UTC 之后、首次读取既有计划正文之前。

## 审查基线与方法

本轮先阅读源码、功能契约和测试，再形成本文。形成本文时未读取 `DEVELOPMENT_PLAN.md` 正文，也未使用其他优化报告的结论。既有文档只做了文件定位和 SHA-256 指纹记录：`3d4f1644403eaf394b7c84cc5d8c7fd50f8d98a790d8fb3384c8f0bf60db469f`，大小 11,420 bytes。

- 工作区：`C:/Users/Public/nas_home/godot`。
- 引擎分支：`personal/main`；HEAD：`db1af1e99a5025bfbf8b05ef27dd4a5e739ec371`。
- 实际版本：`engine/version.py` 为 Godot `4.8.dev`。
- 功能 topic：`ui.dynamic-color-theme`；基础功能提交：`5884522a3dd1b906d8d08086decc30dfa53e3202`。
- 基础提交涉及 119 个文件，其中 60 个位于 Material Color Utilities vendor 目录，59 个位于其外。审查重点为 Godot 接入层及其消费者，未重新审计全部第三方色彩算法。

覆盖了 `ColorScheme`、枚举和绑定、Theme 数据项与回退、Control/Window 覆盖和缓存、StyleBox 解析、ColorRect、默认主题元数据、Theme 编辑器，以及相关测试。`ColorRoleTransform` 属于下游独立 topic，本轮只约束兼容性，不扩大为该功能的专项重构。

并行探查尝试因默认子代理通道返回 503 而失败，未取得任何子代理审查结论。本文证据来自主审直接检查和本地运行。全文源码位置均相对于工作区，行号对应上述 HEAD。

优先级含义：P1 为数据保存、覆盖语义和实时更新的正确性；P2 为可见功能遗漏、有实证的性能机会及后续边界补强。运行探针的退出码 0 表示探针执行完成，不能解读为被测行为正确。

## 已确认的正确性问题

### R01 · P1：部分节点的动态覆盖项无法随场景保存

`engine/scene/gui/control.cpp:496-530` 从 `ThemeDB::get_class_items()` 生成存储属性。增加了 ColorRole/ColorScheme 分支，但常规 Button 的 `font_color_role`、`font_color_scheme`、`default_color_scheme` 没有相应 class item 注册；新增的 `font_color_scale` 也不在原有颜色绑定中。脚本可以设置这些覆盖，属性列表却不含其存储条目。

Window 采用另一条路径：`engine/scene/main/window.cpp:202-302` 只遍历默认主题中当前精确类名的项目，不包含完整继承链及已设置覆盖的并集。

运行证据：对节点设置 role、单项 scheme、默认 scheme 和 scale，执行 `PackedScene.pack()` 再 `instantiate()`，pack 返回 OK；Button 的四类覆盖全部丢失。Window 的 `title_color_*` 全部保留，AcceptDialog 的同名继承项目全部丢失。不能据此声称所有 Window 覆盖都无法保存。

建议：以实际消费的 theme item 名称为基础补齐可编辑条目；存储属性同时纳入已设置覆盖，并去重。Window 要覆盖基类条目。保留现有资源属性路径，不要求用户重新创建主题。

验收：Button、Label、Window、AcceptDialog 的已支持颜色/样式对应 role、scheme、默认 scheme、scale，在属性列表、`duplicate()`、PackedScene 和磁盘 `.tscn`/`.scn` 往返后保持值与共享资源关系；检查 Inspector 中的勾选和取消覆盖。

### R02 · P1：显式 `STATIC` 覆盖仍会落回继承的动态角色

`engine/scene/gui/control.cpp:3919-3963` 和 `engine/scene/main/window.cpp:2844-2888` 在本地 role 为 `STATIC` 时让 `resolve_dynamic_color()` 返回 false，之后仍从 ThemeOwner 重新取得继承 role 并解析动态颜色。

运行证据：主题静态颜色为 `#123456`，主题 role 为 PRIMARY，本地 role override 为 STATIC；Control 和 Window 的 `get_theme_color_role()` 都返回 STATIC，`get_theme_color()` 都返回动态色 `#005da6`。

建议：区分“没有本地 role”与“本地明确指定 STATIC”。后者终止继承 role 的查找，然后使用本地静态颜色或主题静态颜色。保持已经测试过的优先级：有效本地动态 role 优先于本地静态 color；无本地 role 时本地 color 优先于继承 role。

验收：覆盖不存在、STATIC、有效动态角色三种状态；分别组合本地 color 有/无、继承 role 有/无、scheme 有/无，以及清除覆盖后的缓存刷新。Control 与 Window 使用同一套期望结果。

### R03 · P1：源纹理原地更新不能触发重新配色

`engine/modules/color_scheme/color_scheme.cpp:91-100` 只比较 Texture2D 的引用并立即计算，没有订阅源纹理的 `changed`。同一纹理引用再次赋值也会提前返回。正常 ImageTexture 的 `set_image()` 和 `update()` 会发出 `changed`，见 `engine/scene/resources/image_texture.cpp:68-110`，但 ColorScheme 不接收它。

运行证据：一个可读取 Image 的 Texture2D 先提供红色图像，再替换为蓝色图像并发出 `changed`；取图次数保持 1，ColorScheme 更新信号次数为 0，PRIMARY 不变。之后切换 dark 才重新取到蓝色图像。

建议：为源纹理建立明确的连接/断开生命周期，原地变化使提取结果失效。切换为纯色、清空纹理和更换纹理后，旧纹理不再影响 scheme。纹理变更与 R07 的提取缓存共用一条失效路径。

验收：原地更新、更换引用、相同引用重复赋值、texture → color → texture、清空和旧纹理再次发信号；每次有效更新能传到 Theme、Control、Window 和 StyleBox，读取结果保持同步 API 契约。

### R04 · P1：Theme 编辑器用解析后的回退值记录空项，撤销会改变资源内容

运行时 `Theme::get_color_scheme()` 有意执行回退，见 `engine/scene/resources/theme.cpp:1114-1134`。原始序列化读取 `_get()` 则能保留空值，见同文件 `110-115`。

编辑器在以下位置使用了解析 getter：

- `engine/editor/scene/gui/theme_editor_plugin.cpp:3217-3227`：为已存在的空 scheme 项显示回退资源，并允许进入资源编辑。
- 同文件 `3623-3626`：删除项目的 undo 值来自 `get_color_scheme()`。
- 同文件 `3822-3833`：修改项目的 undo 值也来自 `get_color_scheme()`。
- 同文件 `904-908`：完整导入使用会回退的 `get_theme_item()`，需要一并检查空项的原始值保存。

运行证据：按编辑器删除/撤销所用的相同读取和 UndoRedo 参数重放，原本为 null 的 `Button/color_schemes/font_color_scheme` 在 undo 后变成了有效回退 scheme。此证据验证数据流，尚未自动驱动真实 Theme 编辑器界面。

建议：编辑器读写及历史快照使用原始数据，显式保留“不存在、存在且 null、存在且有效”三个状态；预览单独使用解析值。不要通过改变 `get_color_scheme()` 或 `has_color_scheme()` 的运行时回退语义修复编辑器。

验收：添加空项、资源选择器清空、修改、删除、完整导入，各自执行 undo/redo 并保存重载；原始状态、资源身份及连接关系不变。清空的项目仍显示为空，编辑该项不会意外修改全局 fallback。

### R05 · P2：默认元数据存在与实际消费键名不一致的项目

已定位两个明确错误：

1. `engine/scene/theme/default_theme_dynamic_color.inc:562-582` 配置 `font_default_color_*`，但 RichTextLabel 的消费绑定是 `default_color`，见 `engine/scene/gui/rich_text_label.cpp:8258`。颜色读取寻找的是 `default_color_role` 和 `default_color_scheme`。
2. 同一 inc 文件 `594、599` 使用 `activity_color_scheme` / `activity_color_role`，GraphEdit 实际通过 `BIND_THEME_ITEM_CUSTOM(..., activity_color, "activity")` 读取 `activity`，见 `engine/scene/gui/graph_edit.cpp:3156`。C++ 成员名与主题键不同。

运行证据：RichTextLabel 上存在 `font_default_color_role`，缺少 `default_color_role`；指定深色 scheme 后，实际文字主题色仍为 `#ffffff`，按已声明的 ON_PRIMARY 意图应为 `#5f1600`。

自动扫描还得到 11 个“精确类型下没有对应静态颜色项”的 role 候选。这是定位清单，包含继承和别名因素，不能直接当作 11 个缺陷，更不能全部删除。

建议：按照实际绑定键和消费者修正默认元数据，并核验 role/scheme/scale 三者的名称一致性。先补齐正确默认项，不批量重写用户已有 `.tres` 中的自定义键。建立小型代表控件验收表，再扩展到扫描确认的其他遗漏。

验收：RichTextLabel 默认正文、GraphEdit 连线活动色能随 source/dark/contrast 更新；静态覆盖仍然有效。截图验证需要在实际显示后端执行，不能用此次 getter 探针代替完整视觉验收。

### R06 · P2：ColorRect 的默认 scheme 解析路径绕过了节点/类型覆盖

`engine/scene/gui/color_rect.cpp:83-92` 在显式 `color_scheme` 为空时调用 `get_theme_default_color_scheme()`。该方法只走 Theme 对象级默认和上下文默认，见 `engine/scene/theme/theme_owner.cpp:330-353`。它不会采用 `Control::get_theme_color_scheme()` 在 `engine/scene/gui/control.cpp:4021-4043` 实现的本地 `default_color_scheme` override 与类型默认。

默认元数据却包含 `ColorRect/default_color_scheme`，见 `default_theme_dynamic_color.inc:44`。这使同一节点可查询到一个有效覆盖，而 ColorRect 的绘制路径使用另一个 scheme。

建议：明确 ColorRect 的查找契约，建议顺序为显式属性、本地/主题中的默认 scheme、Theme/上下文 fallback；用既有解析入口实现，保留显式属性最高优先级。该项为源码调用链确认，未运行像素级 ColorRect 复现。

验收：显式 scheme、有/无本地默认覆盖、类型默认、Theme 默认、父节点继承和 fallback 的组合；绘制颜色、无障碍颜色值与选中 scheme 一致。

## 有实证的性能机会

### R07 · P2：切换 dark/contrast 会重复进行纹理量化

`color_scheme.cpp:48-73` 把取图、解压、逐像素复制、Celebi 量化、排名和 SchemeContent 构造放在一起。`set_dark()` 与 `set_contrast_level()` 都走这条完整路径（`106-129`）。图像尺寸直接决定输入数组大小（`53-61`），没有采样预算。

运行证据：源纹理初次设置读取 1 次，切换 dark 后为 2 次，再改 contrast 后为 3 次；两次后续操作都没有改变图像。

先做保持结果不变的优化：缓存提取后的种子色，区分“源失效”和“scheme 参数变化”。只有源颜色、纹理引用或其内容变化才重新提取。此时保持同步更新、原始量化输入与金样结果。

缩图、抽样、像素布局加速和异步任务另设性能门槛。它们可能改变选色结果、纹理读取时机及 API 时序，需要图像金样、尺寸/alpha/压缩格式用例和真实大图耗时证据后再选方案。当前 Celebi 已过滤非不透明像素，见 `thirdparty/material-color-utilities/quantize/celebi.cc:42-55`；不要重复宣称“完全未处理透明像素”。

验收：纹理不变时，任意次数 dark/contrast 切换的取图/量化次数增量均为 0；源纹理每次有效更新只重新提取一次；相同种子和参数的角色颜色保持一致。

### R08 · P2：本地 role override 每次读取绕过颜色缓存

`control.cpp:3939-3953` 和 `window.cpp:2864-2878` 在检查颜色缓存之前就解析本地 role。`ColorScheme::get_primary()` 又直接调用 MCU getter（`color_scheme.cpp:358-360`）；MCU 的 `DynamicScheme::GetPrimary()` 与 `DynamicColor::GetArgb()` 会继续计算色调，见 `dynamic_scheme.cc:170-172` 和 `dynamic_color.cc:124-139`。

运行证据：dev 二进制下，对同一节点/同一颜色各读取 2,000 次，3 轮结果为：

- 普通主题缓存路径：2,196 / 1,938 / 1,722 μs。
- 本地 role override 路径：23,721 / 21,140 / 22,629 μs。

这是隔离探针的路径成本证据，不能外推为生产 FPS 或真实整帧收益。

建议：先让两条路径复用同一套可失效缓存，再评估 ColorScheme 按角色、按修订缓存最终 Color 的收益。保留公开命名 getter；若增加 scheme 级缓存，各命名 getter 与 `get_color()` 必须一致，不能只缓存其中一个入口。避免用一个共享缓存掩盖失效链问题。

验收：预热后的相同 role/scheme/scale 读取不重复执行 MCU；源/对比度/深浅色、覆盖增加/移除、父主题切换和 fallback 变化均能使缓存失效。正常路径与覆盖路径输出逐项一致。

### R09 · P2：没有动态角色的 StyleBox 也会产生每节点副本

`control.cpp:3826-3843` 和 `window.cpp:2751-2768` 只根据默认 scheme 是否相等决定复制，未判断 StyleBox 是否使用动态颜色。普通静态 StyleBox 的默认 scheme 通常为空，查询则取得有效 fallback，因而触发 `duplicate()`。

运行证据：两个 Control 共享一个全静态 StyleBoxFlat，查询结果为两个不同副本，均不等于源资源；同一节点再次查询会复用缓存。这说明额外分配存在，尚未测量大型场景的峰值内存或帧耗时。

建议：先增加可证明安全的静态快速路径，再考虑动态样式的副本复用。必须保持不同节点使用不同 scheme 时的隔离，并保留源样式 authored color。不要直接恢复对共享 StyleBox 写入节点 scheme 的做法。

验收：常见静态内建 StyleBox 不因纯查询被复制；动态共享样式在不同 scheme 下颜色正确、源资源不变；源样式属性变化、scheme 变化、切换静态/动态及节点释放后均正常。自定义脚本 StyleBox 不能仅凭内建类的角色字段误判为静态。

## 需要补充证据或明确契约的项目

这些项目不计入上面的已复现缺陷：

- 有限数与枚举边界：`set_contrast_level()` 有区间 clamp，但没有显式 NaN 检查；role 接口也缺少统一的值域验证。先定义非法输入行为并写定点测试，避免改动 `get_color(STATIC)` 现有返回源色的行为。
- 图像失败策略：空图、读取失败、不可解压图、全透明图及低彩度图的种子回退需要文档和测试。MCU `ScoreOptions` 自带 Google Blue 回退（`score/score.h:37-40`），不能把它当作本项目已经明确承诺的产品策略。
- 模块构建边界：`modules/color_scheme/config.py:1-6` 未声明不可关闭；Theme、StyleBox、Control 等无条件依赖 ColorScheme，如 `scene/resources/style_box.h:36`。需要验证 `module_color_scheme_enabled=no` 的结果，并选择“明确必需/拒绝关闭”或真正支持可选模块。本轮没有执行禁用构建。
- 头文件与重复代码：`color_scheme.h:34-40` 把 quantizer/score/texture 头文件扩散到常用主题头文件中。先移动实现专用 include，并测增量构建；PImpl、模块迁移、生成全部角色映射等大改暂不列为前置条件。
- Inspector 通知：`_update_color_scheme()` 每次都会发 `updated_color_scheme`、属性列表变化和 `changed`。先测交互刷新成本及源属性联动需求，再决定能否减少属性列表通知；不先改成异步/延迟可见的 setter。

## 已验证的行为与排除项

已有测试覆盖静态颜色保存与 resolved color 的区分、StyleBox 显式 scheme 优先、scale 乘法、Control/Window 本地动态覆盖优先、fallback 变更信号、MCU 颜色金样和 contrast 越界 clamp。优化必须保留这些结果。

ColorRole 当前有 55 个枚举值、55 个 Inspector hint 项和 55 个 core 绑定；非 STATIC 的 `get_color()` switch 覆盖完整，没有发现漏映射，不能因为重复代码较多就断言映射已错。

Theme 的 `connect_changed(...bind(false))` 与不带 bind 的 disconnect 不能直接判为连接泄漏。当前 `engine/core/object/object.cpp:1624-1649、1672、1713-1728` 使用 `get_base_comparator()` 忽略 binds，并处理引用计数；这里有引擎实现依据。

## 独立实施顺序

1. 先将 R01、R02、R03、R04 的复现转成有效回归测试，再分别修复持久化、STATIC 语义、源纹理生命周期和编辑器原始值读写。每个提交只解决一个可审查问题。
2. 修复 R05 的实际键名，并统一 R06 的 ColorRect 查找契约；补代表控件的集成和视觉验证。
3. 将 R07 的种子提取与 SchemeContent 生成分离；在已有即时更新契约下实施缓存。此步骤依赖纹理失效路径已正确。
4. 根据 R08 的测量优化角色读取；根据 R09 的资源身份和分配证据优化样式副本。先验证正确性，再比较相同机器、相同构建模式下的耗时和分配。
5. 补非法输入、图像回退、文档和模块构建约束；大型图像异步化、全局 StyleBox 缓存和大规模结构迁移仅在额外测量支持时启动。

在 `engine/personal/main` 上以 `Godot-Patch-Topic: ui.dynamic-color-theme` 提交对应改动；不手改生成的 patch。个人提交栈变更后，执行 `customizations/scripts/export-patches.ps1 -Replace`、`verify-stack.ps1` 和相关 topic 检查。本次只产出审查材料，没有修改引擎代码或提交栈。

## 本次验证记录及限制

路径维护说明（2026-10-02）：下列探针路径已更新到正式测试目录；本文的历史源码身份、执行日期和结果不变。

已执行：

```powershell
rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/scripts/verify-topic.ps1 -Topic ui.dynamic-color-theme
rtk proxy engine/bin/godot.windows.editor.dev.x86_64.console.exe --headless --test '--test-case=*[ColorScheme]*,*[Theme]*,*Dynamic theme colors*'
rtk gdscript-post-check --format customizations/tests/dynamic_color_theme_review/review_probes.gd
rtk proxy engine/bin/godot.windows.editor.dev.x86_64.console.exe --headless --path customizations/tests/dynamic_color_theme_review --script res://review_probes.gd
```

- topic verifier 通过。`customizations/scripts/verify-topic.ps1:49-69、151-159` 只验证文件/文本标记，不能代表运行行为正确。
- 相关 C++ 测试：14 / 14 用例通过，706 / 706 断言通过。
- 定点探针位于 `customizations/tests/dynamic_color_theme_review/`；最后一次执行无引擎 warning/error。首次探针遗漏了非 RefCounted UndoRedo 的释放，修正后已复跑，不将探针自身泄漏归因于功能包。
- GDScript 后置格式/静态检查通过；运行后调用了专属 Godot 测试清理 helper。
- 实测使用现有 `4.8.dev.custom_build.661246efd` 二进制。其提交为 `661246efd04f9fb4c040e9ae48b987817414fb5f`。已对比该提交与当前 HEAD：本 topic 的 119 个文件及 `core/object/object.cpp`、`core/io/resource.cpp`、`scene/theme/theme_context.cpp` 没有差异。这是接入层的运行证据，不是对当前 HEAD 整体重新构建的声明。
- 未执行完整 editor/template 构建、关闭模块的构建、真实 Theme 编辑器交互和像素级视觉验证。它们列入后续实施验收，未伪报为已通过。

本文在读取既有计划前固定。后续对比和采纳决定另见 `PLAN_COMPARISON.md` 与 `FINAL_OPTIMIZATION_PLAN.md`。
