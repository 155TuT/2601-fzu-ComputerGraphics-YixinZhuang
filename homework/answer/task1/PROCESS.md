# 作业 1 的完整过程与提交方式

`task1/` 是作业的唯一正本，保存实现、报告源文件、资源、构建入口、实验脚本和验证记录。`submission/task1-submit/` 是从正本生成的交付子集：每个交付文件都在 task1 中有同路径、同内容的来源。只编辑 task1 中的文件，填写报告后在这里重新导出 PDF，再重新打包。

## 文件归属

| task1 正本路径 | 用途 | 进入提交目录 |
| --- | --- | --- |
| `README.md`、`run.cmd`、`build.ps1`、`.gitignore`、`.vscode/` | 运行说明、启动、独立编译及 VS Code 配置 | 原样复制 |
| `code/` | 核心实现、头文件、算法验证源码、教师场景与纹理、CGL/GLEW/GLFW/FreeType 源码及许可证 | 原样复制；排除测试输出缓存 |
| `docs/` | DOCX/PDF、报告 Markdown、自定义 SVG/纹理、结果图及代码图 | 原样复制；排除截图工作缓存 |
| `bin/` | Windows 程序、三个 DLL、启动兼容入口及许可证 | 原样复制；本机产物不进入 Git |
| `scripts/` | 报告生成、截图、实验、审计、打包、迁移验证与构建兼容入口 | 保留在完整过程目录 |
| `verification/` | 要求核对、实验原始数据、日志、报告 QA、历史解压测试与本次整理记录 | 保留在完整过程目录 |
| `.tools/`、`build/` | 本机工具和构建缓存，可重建 | 不提交、不进入 Git |
| `submission/` | 提交目录、ZIP、解压运行验证的临时副本 | 整个目录由 `.gitignore` 忽略 |

已有 `verification/submission-extracted-*` 是过去的解压验证快照，保留其历史文件和证据；以后新解压副本位于忽略的 `submission/` 中。历史日志对应当时产物，当前包的整理与运行记录位于 `verification/submission-layout/`。

## 生成提交包

在 task1 中运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_submission.ps1
```

生成 `submission/task1-submit/` 和 `submission/task1-submit.zip`。Python 入口 `scripts/package_submission.py` 转发同一个 PowerShell 打包脚本，不再维护另一种包结构。提交目录不包含这些打包或审计脚本。

如果现有提交目录含有正本中没有的交付文件，打包脚本会停止，要求先补回 task1。生成的运行结果和构建缓存不会进入下一次提交。修改实现、资源或报告后，直接重新打包即可。

直接运行提交程序不需要 Python，也不依赖本机 `.tools/`；重新编译需要 README 所列的 CMake 和 MinGW。提交前在正本 `docs/作业报告.docx` 填写姓名、学号，重新导出正本 PDF 后打包。

## 开发与复现实验

直接打开 task1 的 `.vscode/`，可按 README 编译和调试；课程仓库根目录的现有 VS Code 任务继续调用 `scripts/build.ps1`。开发构建使用：

```powershell
# 构建主体和全部算法、基准测试、截图辅助程序，再运行 CTest
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test
# 根入口默认只构建交付程序；-Verification 构建全部，-Test 再运行 CTest
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

统一构建入口优先使用本机 `.tools/cmake*/bin/cmake.exe`，否则在 PATH 查找 CMake；MinGW 优先在 PATH 查找，也支持 `-CMake`、`-CompilerDir`、`-Jobs` 指定工具。FreeType 2.14.1 的源码与许可证完整保存在 `code/CGL/deps/freetype/`，随 CGL 静态构建，无需 `.tools/freetype`。原本安装的本机工具仍保留，Python 绘图依赖位于 `.tools/python-packages/`。

| 过程入口 | 对应证据或产物 |
| --- | --- |
| `verification/requirements/requirements-audit.md` | 当前教师任务、报告模板与旧版报告的差异核对 |
| `code/verification/algorithm_tests.cpp`、`analytic_tests.cpp` | 基础光栅化和解析覆盖率的 CTest 验证 |
| `scripts/benchmark_simd.py --run` | `verification/performance/` 的原始计时和覆盖率核对 |
| `scripts/benchmark_aa.py` | `verification/aa/` 的几何、数值参考、质量误差与耗时 |
| `scripts/check_viewport.py` | 视口旋转验证与结果图 |
| `scripts/capture_report.py`、`capture_aa_gui.py` | 必做功能与解析模式的 GLFW/OpenGL 截图和参数 |
| `scripts/capture_code.py` | 源码行号、可复制代码文本与白底代码图 |
| `scripts/finalize_report_results.py`、`build_report.py` | 结果写入报告 Markdown，再生成 DOCX |
| `scripts/audit_report.py`、`verification/report/` | 文档结构、Word 打开、导出和逐页检查记录 |
| `verification/submission-layout/` | 本次文件梳理和解压独立运行的记录 |

报告、实验原始数据和已有验证记录保留。先前开发说明另存于 `verification/submission-layout/previous-development-readme.md`，作为历史说明；当前使用方式以本文件及 README 为准。

SIMD 比较排除 clear/resolve，解析覆盖率比较包含 clear/resolve，二者耗时不能横比。自动截图来自程序回调与真实 GLFW/OpenGL，不能作为人工操作证明。解析模式精确处理二维不透明三角形的可见面积和仿射 RGB 积分，纹理采用可见区域质心采样近似；点、线、透明度和一般曲线路径的限制仍以报告为准。
