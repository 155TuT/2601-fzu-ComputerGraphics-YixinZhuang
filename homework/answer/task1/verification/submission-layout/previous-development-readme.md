# 整理前的开发说明（历史快照）

此文件保留原开发说明；当前构建和提交方式以 README.md、PROCESS.md 及统一脚本为准。以下验证数字是先前作业完成时记录，不能替代当前提交包验证。

# 计算机图形学作业 1：光栅化器

本目录依据本次 `homework/teacher/task1` 的任务说明和报告模板完成六项实现：三角形光栅化、规则网格超采样、几何变换、重心插值、最近邻/双线性纹理采样和 mipmap 层级采样。开发环境使用 **VS Code + MinGW + CMake**。

扩展已按选择实现：任务一 **AVX/SSE2 双精度 SIMD**，提供直接边函数、增量边函数和 SIMD 三阶段对照；任务三 **Q/E 视口中心旋转**；任务二另做 **解析覆盖率** 实验，保留规则 SSAA。报告使用宋体正文、黑体加粗标题、表格及白底真实代码图。

旧报告仅用于参考。本次与旧版要求的偏差、报告截图清单及原文依据，见 [要求核对](verification/requirements/requirements-audit.md)。本次 `L_LINEAR` 是必做；每像素 16 次采样是 4×4 网格。

## 提交内容的位置

以下路径均相对于 `homework/answer/task1/`。

| 内容 | 路径 | 提交说明 |
| --- | --- | --- |
| 修改后的三个核心源码 | [`code/src/rasterizer.cpp`](code/src/rasterizer.cpp)、[`code/src/transforms.cpp`](code/src/transforms.cpp)、[`code/src/texture.cpp`](code/src/texture.cpp) | 三份必交源码，分别包含光栅化/超采样/插值、几何变换、纹理与 mipmap 实现 |
| 编译后的 Windows 程序 | `bin/draw.exe` | 连同整个 `bin/` 提交，保留同目录的三个运行时 DLL |
| 程序运行时依赖 | `bin/libstdc++-6.dll`、`bin/libgcc_s_seh-1.dll`、`bin/libwinpthread-1.dll` | 缺失时目标电脑可能无法启动 `draw.exe`；FreeType、GLEW、GLFW 已静态链接 |
| 可编辑作业报告 | [`docs/作业报告.docx`](docs/作业报告.docx) | 按本次模板组织，提交前填写姓名和学号 |
| 固定版式报告 | [`docs/作业报告.pdf`](docs/作业报告.pdf) | 与 DOCX 配套；填写身份信息后重新导出 PDF |
| 自定义机器人源文件 | [`docs/my_robot.svg`](docs/my_robot.svg) | 保留 SVG 的层级变换与可编辑性 |
| 自备纹理与测试场景 | [`docs/sampling_texture.png`](docs/sampling_texture.png)、[`docs/texture_demo.svg`](docs/texture_demo.svg) | 两者一起提交，SVG 的相对纹理路径依赖 PNG |
| 报告截图原图 | [`docs/screenshots/`](docs/screenshots/) | 无损 PNG，包含报告各项对比所用图像 |
| 整理后的提交包 | `submission/task1-submit.zip` | 包含源码、程序、报告及配套资源，便于一次上传 |

请一并提交完整 `code/` 项目：除了三份必交 CPP，扩展还需要 `rasterizer.h`、`drawrend.cpp/.h`、`analytic_rasterizer.cpp/.h` 和 CMake 配套修改。完整项目包含头文件、CGL、GLEW/GLFW 源码及 SVG/PNG 资源。教师原件没有修改。任务说明没有规定统一的上传包名或平台；上表是本项目的整理方式。

最终验证：41 项基础算法、20 项解析覆盖率、13 项视口检查通过；384 组流水线对照与 1728 次独立覆盖率核对均无差异，24 张 OpenGL 原生 `S` 流程截图成功。`bin/draw.exe` 在 PATH 仅含 Windows System32 的环境中渲染全部 32 个教师 SVG，证明同目录 DLL 足够运行。GDB 实际命中 `rasterize_triangle` 断点。日志与参数位于 `verification/`。

姓名和学号按要求保留待填写。最新报告的页数、Microsoft Word 打开与逐页渲染检查记录位于 `verification/report/`。`submission/task1-submit.zip` 的 CRC 与文件哈希由打包脚本核对；修改报告后可用 `scripts/package_submission.py` 重新打包。

## VS Code 使用

用 VS Code 打开课程仓库根目录 `2601-fzu-ComputerGraphics-YixinZhuang`，让根目录的 `.vscode/` 配置生效。

- `Ctrl+Shift+B`：运行默认任务 `task1: build and test`，调用 `scripts/build.ps1 -Test`，配置、编译、复制程序及 DLL，再执行算法测试。
- `F5`：选择 `task1: draw (MinGW GDB)`。先执行同一构建任务，再用 GDB 启动 `bin/draw.exe`；输入框默认加载 `code/svg/basic/test4.svg`。
- “终端 → 运行任务 → task1: run SVG”：调用 `scripts/run.ps1`，也会先构建与测试。输入可以是 SVG 文件、SVG 目录或绝对路径。

输入路径相对于本作业目录。例如：`code/svg/basic/test7.svg`、`code/svg/basic/`、`docs/my_robot.svg` 或 `docs/texture_demo.svg`。

`.vscode/settings.json` 已将原来失效的 macOS 源目录改为本作业的 `code/`，并配置本地 CMake、MinGW、FreeType 与构建目录。`c_cpp_properties.json` 使用 C++11 和 `build/compile_commands.json` 提供代码补全；`launch.json` 使用现有 MinGW GDB。VS Code 的 C/C++ 与 CMake Tools 扩展沿用现有安装。

## 命令行编译与运行

在课程仓库根目录打开 PowerShell：

```powershell
# 配置、编译、打包 bin/ 并运行算法测试
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\homework\answer\task1\scripts\build.ps1 -Test

# 加载默认三角形场景
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\homework\answer\task1\scripts\run.ps1

# 加载自定义机器人或纹理场景
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\homework\answer\task1\scripts\run.ps1 -Svg docs\my_robot.svg
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\homework\answer\task1\scripts\run.ps1 -Svg docs\texture_demo.svg

# 一次加载 basic 目录，通过数字键切换
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\homework\answer\task1\scripts\run.ps1 -Svg code\svg\basic
```

`build.ps1` 使用 `MinGW Makefiles`、`RelWithDebInfo`，构建到 `build/`。程序与三个 DLL 随后复制到 `bin/`。`RelWithDebInfo` 保留 GDB 所需调试信息，并启用优化。`-Test` 开启 CTest；测试结果以本次终端输出和 `build/Testing/Temporary/LastTest.log` 为准。

也可以在本作业目录直接运行：

```powershell
Set-Location .\homework\answer\task1
.\bin\draw.exe .\code\svg\basic\test4.svg
.\bin\draw.exe .\docs\my_robot.svg
.\bin\draw.exe .\docs\texture_demo.svg
```

双击 `bin/run-basic.cmd` 可启动默认场景。单独双击 `draw.exe` 没有传入 SVG 参数，程序会直接退出。

## 程序操作

| 操作 | 功能 |
| --- | --- |
| `Space` | 恢复初始视角 |
| `Q` / `E` | 绕当前视口中心逆/顺时针旋转 15°，拖动、缩放与每 SVG 独立状态仍可使用 |
| `A` | 切换规则 SSAA / 解析覆盖率；解析模式保留 spp 供切回使用，不以它离散采样 |
| `B` | 规则 SSAA 中循环直接边函数、增量边函数、SIMD；默认 SIMD |
| `-` / `=` | 降低/提高每像素采样次数，依次为 1、4、9、16 |
| `Z` | 开关像素检查器；将鼠标移到要观察的区域 |
| `P` | 切换最近邻、双线性像素采样 |
| `L` | 切换第 0 层、最近 mip 层、相邻 mip 层线性混合 |
| `S` | 将当前 GUI 绘制结果保存为带时间名称的 PNG，保存到当前工作目录 |
| `1`–`9` | 启动时加载 SVG 目录后，切换其中最多九个 SVG |

`P` 与 `L` 独立切换；`P_LINEAR + L_LINEAR` 是三线性过滤。观察任务 5 的像素采样差异时保持 `L_ZERO`，观察任务 6 的 mipmap 差异时保持视角和每像素采样次数一致。

## 依赖位置

| 工具/依赖 | 安装或使用位置 | 状态 |
| --- | --- | --- |
| GCC/G++ 15.1.0、MinGW Make、GDB | `C:\msys64\mingw64\bin\` | 沿用本机已有工具链 |
| CMake 3.31.10 | `homework\answer\task1\.tools\cmake-3.31.10-windows-x86_64\` | 为本次作业安装的本地工具；无需修改系统 PATH |
| FreeType 2.14.1 | `homework\answer\task1\.tools\freetype\` | 为本次作业从源码编译并安装的静态库；源码位于 `.tools/sources/freetype-VER-2-14-1/` |
| Matplotlib 3.10.8 及绘图依赖 | `homework\answer\task1\.tools\python-packages\` | 报告性能图使用的本地 Python 包，安装日志在 `verification/matplotlib-install.log`；不影响 EXE 运行 |
| CGL、GLEW、GLFW | `homework\answer\task1\code\CGL\` | 项目自带，随构建静态链接 |
| VS Code C/C++、CMake Tools | 本机现有 VS Code 扩展目录 | 已安装；推荐扩展 ID 记录在 `.vscode/extensions.json` |

`.tools/` 是本机开发依赖，`build/` 是可重新生成的编译产物；交付运行时使用 `bin/`。在另一台电脑重新编译时，需要具备对应的 CMake、MinGW 和 FreeType，并按当地安装位置调整脚本及 VS Code 配置。直接运行提交的 Windows 程序时，保留 `bin/` 的 EXE 与 DLL，以及所加载 SVG 引用的 PNG。

## 实现与复现依据

三角形样本索引为 `(y * width + x) * sample_rate + sy * side + sx`，`side = sqrt(sample_rate)`；子样本位置为 `(x + (sx+0.5)/side, y + (sy+0.5)/side)`。采样率 1 自动退化为像素中心采样。纹理 mip 层计算的 UV 差分对应一整屏幕像素，与子样本间距无关。

`code/verification/algorithm_tests.cpp` 是算法验证入口，`code/verification/screenshot_runner.cpp` 是截图复现辅助程序。要求提取、依赖记录与报告检查资料位于 `verification/`。截图生成方法、参数及验证边界以报告和相应运行证据为准。

## 扩展实验复现

构建后，用 Python 运行 `scripts/benchmark_simd.py --run` 可重做 512²/1024²、1/4/16 spp 的性能实验；`scripts/benchmark_aa.py` 重做解析覆盖率与独立 4096 spp 数值参考。原始逐次计时、几何、输出一致性、误差和源码哈希分别留在 `verification/performance/`、`verification/aa/`。SIMD 比较排除 clear/resolve，AA 比较包含 clear/resolve，二者耗时不能横比。强制增量模式验证标量路径，本机实际使用 AVX4，未在无 AVX 硬件上执行 SSE2 回退。

`scripts/check_viewport.py` 复现任务三13项验证和3张截图，`scripts/capture_aa_gui.py` 复现 A/S 的实验模式截图，`scripts/capture_report.py` 生成19张必做功能图。截图来自真实 GLFW/OpenGL 与程序回调，输入自动提供，未声称人工按键验证。

`scripts/capture_code.py` 从最终源码提取白底代码图、源行号、可复制 `.cpp.txt` 和哈希；`scripts/finalize_report_results.py` 将保留实验结果写入报告 Markdown；`scripts/build_report.py` 生成 DOCX。绘图脚本使用 `.tools/python-packages`，本机文档脚本使用 Codex bundled Python。移到另一台电脑复现实验时，需要 Python/Pillow/python-docx/lxml/Matplotlib；程序运行只需要 `bin/` 内的 EXE 与 DLL。

解析模式精确处理二维不透明三角形的可见面积和仿射 RGB 积分；纹理是可见区域质心采样近似，点/线保留整像素方式，没有扩展透明度或一般曲线路径。实测显示画质收益伴随更高裁剪成本。SIMD 的大量小三角形输入可能更慢，报告保留了这些结果。
