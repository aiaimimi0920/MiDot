# MiDot

MiDot 是 Godot 定制功能的**补丁与构建编排仓库**，不是一份压平后的 Godot fork。它保存功能主题、补丁、精确的个人提交栈和维护脚本；`engine/` 是单独从官方仓库建立的 Git checkout，根仓库不跟踪其中的源码。

```text
MiDot/                         Git origin: aiaimimi0920/MiDot
├── customizations/            功能主题、补丁、锁文件及维护脚本
├── archive/                   历史源码与追溯资料
├── AGENTS.md                  工作区与上游维护约定
├── engine/                    独立 Git 仓库，根仓库忽略
│   ├── master                 锁定的官方上游镜像
│   └── personal/main          官方基线 + 按主题组织的个人提交
└── export/                    编译发布输出，根仓库忽略
```

## 新环境初始化

在有 Git 和 Windows PowerShell 5.1 或更新版本的环境中执行：

```powershell
git clone --depth 1 https://github.com/aiaimimi0920/MiDot.git
Set-Location MiDot
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\customizations\scripts\initialize-engine.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\customizations\scripts\verify-stack.ps1
```

初始化脚本会校验 catalog 的 SHA-256，从 `https://github.com/godotengine/godot.git` 获取锁定官方基线，创建独立的 `engine/.git`、官方 `origin`、`master` 和 `origin/master`，然后恢复精确的 `personal/main` 提交栈，并运行完整分支与提交身份校验。初始化不会默默选用不确定的最新版。

`personal-history.bundle` 是导出器生成的**增量 Git 提交归档**：只包含锁定官方基线之后的个人提交及所需对象，官方基线是其前置条件。它不是完整 Godot 仓库备份。保留它是为了恢复原始 commit hash；普通 `git am` 使用新的 committer date，不能保证与锁文件中的提交身份相同。可阅读、可独立重放的功能补丁仍在 `customizations/patches/`。

重复初始化只校验已有引擎，不修改文件或 ref。已有目录不独立、工作区脏、分支或提交不匹配时会拒绝覆盖；新初始化失败时保留中间目录并报告位置，不会自动清空已有内容。

`--depth 1` 用于避免下载 MiDot 首次误导入源码快照的旧历史；本次修正采用后续提交，没有强制改写已发布历史。

## 更新 Godot 上游

先确认引擎工作区干净，当前个人提交已经导出并通过校验，然后执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\customizations\scripts\update-engine.ps1
```

脚本建立安全引用，获取官方 `master`，将本地 `personal/main` rebase 到新基线，推进纯净的本地 `master`，重新生成补丁、增量提交归档及锁文件，最后校验。若浅仓库缺少 merge base，会停止并提示补充历史；不要用 merge 替代 rebase。

遇到不兼容时按当前功能主题处理冲突，显式 `git rebase --continue` 或 `git rebase --abort`，不自动跳过补丁或猜测冲突。手工完成 rebase 后，执行 `update-engine.ps1 -Finalize`：它核验本次更新记录、安全引用及目标基线，推进本地 `master`，重新导出并校验 catalog。若导出或校验失败，也可修复原因后用 `-Finalize` 重试；成功后才清除位于引擎 Git 目录中的 `patch-stack-update.json`。完成前不要再次 fetch 或启动新的更新。显式 `git rebase --abort` 后，如决定放弃这次更新，应先检查分支与安全引用，再手工移除该 pending 文件；脚本不会自动重置、清理或跳过提交。新导出的 catalog 应作为一个整体提交到 MiDot，供其他环境恢复。

## 修改定制功能

源码修改发生在 `engine/personal/main`。每个个人提交代表一个可审查的功能意图，并包含 `stack.json` 声明的唯一 `Godot-Patch-Topic` trailer。然后执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\customizations\scripts\export-patches.ps1 -Replace
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\customizations\scripts\verify-stack.ps1
```

不要直接编辑生成的 `patches/`、`series.txt`、`stack.lock.json` 或 `personal-history.bundle`。按改动主题执行相应功能验证，再构建引擎并发布到 `export/`。构建依赖和平台说明见初始化后 `engine/README.md` 及其中的构建脚本。

只校验 catalog、不获取引擎时可以执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\customizations\scripts\verify-stack.ps1 -SkipBranchComparison
```

## 本地资料与兼容性

原工作环境已有的 `engine/.git`、`customizations/.git` 和发布文件保持独立，不通过根仓库上传 Git 元数据。新克隆中的 `customizations/` 由 MiDot 根仓库管理，不要求额外的子仓库。初始化和维护脚本同时支持这两种 catalog 布局。

历史源码 bundle、补丁与复现项目保留；编译包、工具安装、日志、缓存和重复的临时源码 checkout 由 `.gitignore` 排除。`ENGINE_LOCATIONS.md` 中的绝对路径与旧发布记录是本地登记，不是新克隆后的默认入口。

根 `.gitattributes` 保留生成补丁和锁文件的原始字节，避免 Windows 换行转换破坏校验值；独立的 `engine` 使用上游自身的属性规则。

## 许可证

Godot 引擎许可证见初始化后的 `engine/LICENSE.txt`，第三方版权和许可证见 `engine/COPYRIGHT.txt` 及相关源码目录。个人集成项目的来源、固定版本和许可证记录见 `customizations/audits/external_upstreams.md`。使用和发布仍须遵守各组件许可证。


## 维护脚本回归测试

执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/tests/run.ps1`。
测试仅使用临时 Git 仓库，覆盖精确提交恢复、补丁重放、真实 rebase 冲突后的完成流程、自定义引擎路径验证，以及导出备份/安装八个移动边界的故障回滚。它不编译 Godot，也不替代功能主题的运行测试。

`verify-stack.ps1 -RunTests` 对内置 `verify-topic.ps1` 显式传递所选 `-EnginePath`，并使用当前脚本安装中的绝对路径；其他自定义 verification 命令的参数保持原样。并发修改同一 catalog 不受支持，运行更新或导出时不要同时启动另一实例。
