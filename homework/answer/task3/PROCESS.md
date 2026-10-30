# 作业 3 的完整过程与提交方式

`task3/` 保存本次作业的源码、资源、报告源文件、构建入口、实验脚本和验证记录。`submission/task3-submit/` 与同名 ZIP 由完整目录选取文件原样复制生成。

## 目录关系

| 路径 | 用途 | 提交方式 |
| --- | --- | --- |
| `code/` 中的运行与构建文件 | 作业实现、教师框架、场景和依赖源码 | 选取后原样复制 |
| `code/verification/`、依赖中的测试目录 | 算法及依赖测试 | 保存完整过程 |
| `docs/` | 报告 Markdown、Word、PDF 和实验结果图 | 原样复制 |
| `bin/` | Windows 程序、运行时 DLL 和许可证 | 原样复制 |
| `build.ps1`、`run.cmd`、`.vscode/`、`README.md` | 构建、运行和 VS Code 使用入口 | 原样复制 |
| `scripts/` | 场景生成、报告生成、实验复现和打包脚本 | 保存完整过程 |
| `verification/` | 算法测试、运行日志、报告校对及提交验证 | 保存完整过程 |
| `build/`、`.tools/` | 构建缓存和本机工具 | 可重新生成 |
| `submission/` | 提交目录、压缩包及独立验证副本 | 整个目录由 Git 忽略 |

先修改完整目录中的源码或报告，再重新构建、导出报告，最后执行打包：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_submission.ps1
```

提交包中的每个文件均有相同相对路径的正本来源。打包脚本检查必要文件，将选定文件复制到暂存目录，再生成 ZIP。若提交目录中出现单独新增的文件，脚本会提示先将其保存回完整目录。

报告里的姓名和学号保留待填写。填写后重新导出 PDF，再执行打包即可更新提交版本。
