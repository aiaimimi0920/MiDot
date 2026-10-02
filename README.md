# MiDot

本仓库以整个 Godot 工作区为版本控制根目录，保存定制引擎的完整源码、个人补丁栈、维护脚本和历史源码资料。`engine/` 与 `customizations/` 是普通版本控制目录，不是 Git submodule；克隆本仓库即可获得其中的文件，无需初始化子模块。

## 目录

- `engine/`：Godot 引擎源码及当前个人定制功能。构建方式、依赖和平台说明见 `engine/README.md` 与其中的构建脚本。
- `customizations/`：个人补丁主题、生成的补丁文件、锁文件、验证与维护脚本。维护规则见 `customizations/README.md` 和各级 `AGENTS.md`。
- `archive/`：历史源码 bundle、原始补丁、资产与追溯资料；历史编译发布包不纳入 Git。
- `.validation/`、`.runtime/`、`.tmp/`：保留其中的维护与复现源码；日志、构建输出、缓存和重复的引擎回放工作区不纳入 Git。
- `ENGINE_LOCATIONS.md`：已有本地引擎发布目录登记；其中绝对路径和历史发布记录仅适用于原工作环境。
- `export/`：编译发布输出，已由根 `.gitignore` 排除。克隆后需要自行编译或另外获取二进制文件。

## 初始源码来源

本仓库初始导入的是已有工作区的源码快照，没有改写原有个人补丁栈：

- Godot upstream base：`5ec4857b340b6284a18b49b2eda462bd250f219a`。
- 本地 `engine/personal/main`：`38b6ddee72e16d9d646057ee6ba533c122afc47c`。
- 本地 `customizations/main`：`bda4843`；准确的引擎来源和 56 个补丁记录见 `customizations/stack.lock.json`。

本地原有 `engine/.git` 和 `customizations/.git` 保持不变，但 Git 元数据不会上传。新克隆只有 MiDot 根仓库的历史，不会自动拥有 Godot upstream 分支或 `personal/main` 的提交对象。

因此，直接在 MiDot 克隆中维护工作区文件与构建源码是可行的；需要执行依赖 Godot 分支历史的补丁导出、rebase 或上游更新时，应使用单独的 Godot upstream checkout，按 `customizations/README.md` 重建个人补丁栈并遵循各级 `AGENTS.md`。不要将 MiDot 根仓库的 `origin` 当作 Godot upstream 使用。

仅校验已发布补丁目录与锁文件时，可在仓库根目录执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\customizations\scripts\verify-stack.ps1 -SkipBranchComparison
```

根 `.gitattributes` 保留补丁、锁文件和历史资料的原始字节，避免 Windows 换行转换破坏校验值；`engine/.gitattributes` 继续使用引擎自身的规则。

## 许可证

Godot 引擎许可证见 `engine/LICENSE.txt`，第三方版权与许可证见 `engine/COPYRIGHT.txt` 及相关源码目录。个人集成的外部项目来源、固定版本和许可证记录见 `customizations/audits/external_upstreams.md`；发布或使用时仍须遵守各组件的许可证。
