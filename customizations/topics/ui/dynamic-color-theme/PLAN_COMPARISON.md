# Dynamic Color Theme：两份优化计划的对比与裁决

日期：2026-09-22。

对比对象：

- 计划 A：[既有 DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md)，原文保留。
- 计划 B：[独立审查与优化计划](INDEPENDENT_REVIEW_PLAN.md)，在首次读取 A 的正文前完成。
- 执行依据：[最终优化计划](FINAL_OPTIMIZATION_PLAN.md)。

A 的 SHA-256 为 `3d4f1644403eaf394b7c84cc5d8c7fd50f8d98a790d8fb3384c8f0bf60db469f`。本次没有修改 A。B 冻结后新增的核验只记录在本文和最终计划中，没有回填成“独立阶段已发现”的问题。

两份计划都有有效发现。A 的 F-01 是 B 初始阶段遗漏的真实问题；追加运行验证后采纳。B 则发现了 A 只归为“缺测试”的实际保存和编辑器状态错误，以及 STATIC 覆盖、默认键名和两处缓存成本。最终排序依据用户可见影响、可达调用路径和实测结果，不依据报告作者或篇幅。

## 对 A 的逐项裁决

| A 的条目 | 与 B 的关系 | 复核结果 | 最终处理 |
| --- | --- | --- | --- |
| F-01：`has_theme_color()` 忽略动态颜色 | B 未覆盖 | Control/Window 均实测确认，动态 scale 也被跳过 | 采纳为 P1，进入 T02 |
| F-02：类型辅助函数未发通知 | B 关注了失效链，未列为缺陷 | 直接 C++ helper 缺通知属实；仓内只从 Theme 的分派入口使用，公开脚本入口实测刷新正常 | 降为内部契约补强，进入 T08；不排在数据丢失之前 |
| F-03：源纹理变化不刷新 | B 的 R03 | 两份结论一致，Texture2D changed 探针确认 | 采纳为 P1，进入 T04；修正“重新赋同一纹理即可恢复”的表述 |
| F-04：全图量化同步且无预算 | B 的 R07 | 调用链和重复取图已确认，真实大图峰值尚未测量 | T06 先缓存提取种子；采样/异步另做测量与结果兼容验证 |
| F-05：不可读纹理静默变黑 | B 的边界风险项 | 空图生成黑色种子已复现；全透明图走 MCU 的 Google Blue 回退 | 采纳失败策略/诊断需求，进入 T08；不自动新增 status/last-valid 公共 API |
| F-06：本地 role 比静态 color 优先 | B 的 R02 和保留项 | 现有测试有意保护这一优先级；真正错误是 STATIC 未阻断继承 role | 保留有效动态 role 的现有优先级，在 T02 补文档和矩阵 |
| F-07：Control/Window 逻辑重复 | B 的 R08/R09 及维护建议 | 重复存在，但全面抽取不是修复所有问题的前置条件 | 行为修复后只抽取小型解析规则；节点存储/通知仍各自维护 |
| F-08：保存和编辑器行为缺测试 | B 的 R01/R04 | 已从测试空白升级为可复现的数据状态错误 | 提升为 P1 的 T01、T03，并在 T10 做真实往返和编辑器验收 |

## 关键复核证据

### F-01：补充发现，完整采纳

`engine/scene/gui/control.cpp:4225-4239` 和 `engine/scene/main/window.cpp:3097-3111` 的 `has_theme_color()` 只查看静态 color。`get_theme_color()` 的动态解析位于相应文件 `3919-3963`、`2844-2888`，且通过 `has_theme_color(name + "_scale")` 决定是否使用 scale。

追加探针创建没有静态 color、只有有效 role 和 scheme 的主题项目；另创建一个只有 role 的 `_scale`。两个节点类型得到同样结果：

```text
has_dynamic_color = false
get_dynamic_matches_role = true
has_dynamic_scale = false
get_dynamic_scale_matches_role = true
scale_was_applied = false
actual = #ffb781
expected_with_scale = #4e1b00
```

修复应使有效声明的动态色参与 `has_theme_color()`，同时保留原有“找不到项目时 get 返回 fallback、has 仍可为 false”的语义。不能简单用 getter 是否返回 Color 判断存在，也不能把 STATIC role 的存在直接当作可用颜色。

### F-02：通知缺口属实，原排序高估了当前影响

`Theme::remove_color_role_type()`、`rename_color_role_type()` 和 `rename_color_scheme_type()` 确实不发通知，位置分别在 `engine/scene/resources/theme.cpp:1076-1087、1198-1205`。

但复核当前调用者和绑定后发现：

- 这些 helper 没有直接 GDScript 绑定（`theme.cpp:2526-2543`）。
- 仓内定义之外的引用只出现在 `remove_theme_item_type()` / `rename_theme_item_type()` 分派中；该分派的调用者是 `remove_type()` / `rename_type()`。
- 两个公开入口分别在 `theme.cpp:1776、1805` 发出最终通知。
- 预热 Control 颜色缓存后执行公开入口，再等待正常 deferred 通知，rename 和 remove 都刷新了结果。

本次小场景中 rename 发出 1 次 Theme changed，remove 发出 2 次。A 提到的“外层单次通知”不能当作当前已经成立的事实。`_freeze_change_propagation()` 是布尔开关（`theme.cpp:2251-2257`），机械地在每层加入 freeze/unfreeze 也不能保证嵌套操作正确。

最终保留为 P2 的 C++ helper 契约和通知合并工作。测试必须区分直接 C++ helper 与公开组合操作。没有证据支持把公开类型操作的缓存刷新描述为已失效，也不应为此先重构整个 Theme 通知系统。

### F-03 / F-04：一致结论中的两处精度修正

重新赋值同一 Texture2D 引用会被 `color_scheme.cpp:92-94` 的比较拦截，无法修复内容更新后的 stale palette。必须有纹理内容变更的失效机制。独立探针还证明，在现状下切换 dark/contrast 会意外重新提取纹理。

`set_source_color()` 会先清空 `source_texture`（同文件 `81-84`），因此不能把纯色设置描述为仍会扫描旧图像。性能优化首先分开种子提取与 scheme 生成，避免暗示所有 setter 都有同样的图像量化成本。

### F-05：失败事实采纳，API 扩张暂缓

追加探针中，空 Image 对应的源色为 `#000000`；全透明 Image 对应 `#4285f4`。原因分别是包装层的 `Color()` 回退和 MCU 排名函数的默认 fallback，见 `color_scheme.cpp:48-73、96-99` 及 `thirdparty/material-color-utilities/score/score.h:37-40`。

失败应有清晰的行为契约和诊断。不过“保留上次有效种子”会引入历史状态，必须回答首次加载、保存重载、源纹理原地变坏时如何保持一致。新增 extraction status 或 last-valid-source API 也有兼容和维护成本。最终计划先保留合法输入的既有结果，补失败路径测试与明确文档；状态 API、事务式拒绝更新或新 fallback 策略分别作为有针对性的后续变更，不直接混入信号修复。

### F-06 / F-08：保持已证明的优先级，优先修复真实丢失

`tests/scene/test_control.cpp:68-75` 和 `tests/scene/test_window.cpp:81-88` 明确保护有效本地动态 role 高于本地静态 color。这一行为在最终计划中保持。

B 的探针则发现：本地 role 为 STATIC 时，读取角色与读取颜色互相矛盾；Button 和 AcceptDialog 的支持项覆盖在 PackedScene 往返中丢失；Theme 编辑器用解析 getter 记录空 scheme 项，撤销把 null 写成有效 fallback。这些问题有确定触发路径，应优先于“是否颠倒两种覆盖优先级”的重新设计。

## B 对最终计划的新增贡献

| B 的条目 | 证据强度 | 纳入位置 |
| --- | --- | --- |
| R01：覆盖项未列入存储属性 | 运行复现；含 Window 成功对照 | T01 |
| R02：STATIC 落回继承 role | Control 与 Window 双复现 | T02 |
| R03：纹理内容变更不刷新 | changed 信号和取图计数 | T04，与 A 的 F-03 合并 |
| R04：空 scheme 的编辑/undo 状态错误 | 源码加编辑器同参数 UndoRedo 重放 | T03 |
| R05：RichTextLabel / GraphEdit 键名错误 | RichTextLabel getter 复现；两处消费绑定核对 | T05 |
| R06：ColorRect 绕过本地/类型默认 | 源码调用链；尚未做像素验证 | T05，作为需对齐的解析行为 |
| R07：dark/contrast 重复提取纹理 | 取图计数 1 → 2 → 3 | T06 |
| R08：本地 role 绕过颜色缓存 | 2,000 次读取的 dev 构建测量 | T07 |
| R09：静态 StyleBox 被重复复制 | 两节点产生两个副本的资源身份检查 | T07，收益按后续分配/耗时测量验收 |
| 边界、模块、头文件建议 | 源码风险，部分尚未运行 | T08、T09；不冒充复现缺陷 |

B 还排除了两个容易产生误报的方向：ColorRole 枚举/绑定/hint 目前没有漏项；bind(false) 与不带 bind 的断开在当前 Object 的 base-comparator 机制下可匹配。最终计划不包含无依据的枚举重建或这类“修复信号泄漏”。

## 顺序调整与范围控制

最终先修复保存、解析和编辑器状态，再修复纹理生命周期及默认消费者。已明确的行为不要求先做一轮产品重设计。每个修复先有失败复现或等价的黑盒断言，再修改实现，避免把回归测试整体推迟到所有修复之后。

性能阶段优先处理已经观察到的重复工作：不变纹理重复提取、本地 role 重复求色、静态样式副本。采样、异步、多线程、全局 StyleBox 缓存、PImpl 和新的公共状态 API 都有额外契约成本，不作为本轮前置工程。

## 本轮追加验证

比较阶段脚本：`.tmp/dynamic-color-theme-review-20260922/comparison_probes.gd`。

```powershell
rtk gdscript-post-check --format .tmp/dynamic-color-theme-review-20260922/comparison_probes.gd
rtk proxy engine/bin/godot.windows.editor.dev.x86_64.console.exe --headless --path .tmp/dynamic-color-theme-review-20260922 --script res://comparison_probes.gd
```

格式/静态检查通过；探针正常完成，无引擎 warning/error；执行后运行了专属进程清理 helper。二进制及与 HEAD 的相关源码一致性依据见独立计划。未重复执行已经通过的 14 个基础用例，追加验证仅针对比较后新增或仍需确定的问题。
