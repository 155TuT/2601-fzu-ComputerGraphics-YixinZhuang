# 作业 1：光栅化器

本说明同时用于完整作业目录 `task1/` 和提交目录 `task1-submit/`。提交目录由完整作业目录选取文件原样复制生成；程序、代码、文档和配套资源的相对路径一致。

本作业实现三角形光栅化、规则网格超采样、几何变换、重心颜色插值、最近邻/双线性纹理采样和 mipmap 层级采样，另含 SIMD、解析覆盖率和视口旋转扩展。

## 1. 启动程序

在 **Windows 64 位**系统中，解压整个文件夹，双击 **`run.cmd`**，即可打开默认三角形场景。请保持 `bin/` 中的 `draw.exe` 与三个 DLL 在一起。

要打开其他场景，在本文件夹打开 PowerShell，运行：

```powershell
.\run.cmd code\svg\basic\test7.svg
.\run.cmd docs\my_robot.svg
.\run.cmd docs\texture_demo.svg
```

也可以直接执行 `bin/draw.exe`，但必须传入 SVG 文件或目录，例如：

```powershell
.\bin\draw.exe .\code\svg\basic\test4.svg
```

## 2. 复现作业结果

每次重新启动程序，初始状态都是 **1 次采样/像素、最近邻纹理采样、第 0 层 mipmap、规则 SSAA、SIMD 三角形内核**。按下述命令打开场景，再按对应按键观察结果。

| 作业内容 | 启动命令 | 操作与观察 |
| --- | --- | --- |
| 三角形光栅化 | `.\run.cmd code\svg\basic\test4.svg` | 默认即可看到三角形；按 `Z` 打开像素检查器，将鼠标移到边缘观察像素覆盖 |
| 超采样 | 同上 | 默认 1 spp；按 `=` 一次为 4 spp，再按两次为 16 spp。固定鼠标位置对比边缘变化 |
| 几何变换 | `.\run.cmd docs\my_robot.svg` | 查看自定义机器人；原始场景为 `code\svg\transforms\robot.svg` |
| 重心颜色插值 | `.\run.cmd code\svg\basic\test7.svg` | 查看顶点颜色之间的平滑渐变；`docs\barycentric.svg` 是红绿蓝三角形示例 |
| 像素纹理采样 | `.\run.cmd code\svg\texmap\test4.svg` | 保持 `L_ZERO`，按 `P` 切换最近邻/双线性；滚轮放大纹理、按 `Z` 观察。分别用 1、16 spp 对比 |
| mipmap 层级采样 | `.\run.cmd docs\texture_demo.svg` | 按 `L` 在 `L_ZERO`、`L_NEAREST`、`L_LINEAR` 间循环，按 `P` 切换像素采样；固定视角和 spp，比较六种组合 |

`spp` 表示每像素采样次数：1、4、9、16 分别对应 1×1、2×2、3×3、4×4 网格。`P_LINEAR + L_LINEAR` 即三线性过滤。

原有结果图见 [docs/screenshots](docs/screenshots/)，解释和对比见 [作业报告.pdf](docs/作业报告.pdf)。报告截图使用 800×800 渲染尺寸；纹理像素采样对比采用放大视角，其余必做对比采用初始视角。`S` 保存的是当前窗口图像，窗口大小、视角和鼠标位置都会影响截图。

扩展可分别操作：`B` 在直接边函数、增量边函数和 SIMD 间循环；`Q` / `E` 每次逆/顺时针旋转 15°；用 `.\run.cmd docs\analytic_demo.svg` 打开解析覆盖率示例，按 `A` 在规则 SSAA 和解析覆盖率间切换。解析模式是额外实验，纹理使用可见区域质心采样近似。

常用操作：鼠标左键拖动、滚轮缩放；`Space` 恢复初始视角；`-` / `=` 减少/增加 spp；`Z` 开关像素检查器；`S` 将当前画面保存为本文件夹中的 `screenshot_*.png`；关闭窗口退出程序。`Space` 只重置视角，要恢复所有初始参数请重启程序。

还可直接导出固定尺寸的默认渲染结果：

```powershell
.\bin\draw.exe .\code\svg\basic\test4.svg nogl 800 800
```

命令结束后生成本文件夹中的 `test.png`。此方式使用初始采样参数，不含 GUI 的像素检查器；再次执行会覆盖该图片。

## 3. 提交文件

| 路径 | 内容 |
| --- | --- |
| [docs/作业报告.docx](docs/作业报告.docx)、[PDF](docs/作业报告.pdf) | 可编辑报告和阅读版；**提交前填写姓名、学号并重新导出 PDF** |
| `code/src/rasterizer.cpp`、`transforms.cpp`、`texture.cpp` | 三份核心实现；其余源码、头文件和依赖也已随完整项目保留 |
| `bin/` | Windows 可执行程序、三个运行时 DLL 和许可证 |
| `code/svg/`、`code/img/` | 教师场景及配套纹理，保持相对目录关系 |
| `docs/*.svg`、`docs/sampling_texture.png`、`docs/screenshots/` | 自定义场景、纹理和报告结果图；`texture_demo.svg` 与 PNG 须在同一目录 |

## 4. 重新编译（可选）

准备 **CMake 3.x**（本次使用 3.31.10；脚本要求至少 3.13）和 **64 位 MinGW-w64**（含 `gcc`、`g++`、`mingw32-make`），将它们的 `bin` 目录加入 PATH。在本文件夹的 PowerShell 中执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

工具未加入 PATH 时，可指定实际位置：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 `
  -CMake "C:\Program Files\CMake\bin\cmake.exe" `
  -CompilerDir "C:\msys64\mingw64\bin"
```

脚本编译主体程序，并更新 `bin/draw.exe` 和三个 DLL。CGL、GLEW、GLFW、FreeType 源码与许可证已包含在 `code/` 内并静态编译，无需另装 FreeType。`build/` 是自动生成的编译目录。重新编译建议将项目放在较短路径中（例如 `C:\task1-submit`），避免 MinGW 构建路径过长。

使用 VS Code 时，直接打开 **当前文件夹（`task1` 或 `task1-submit`）**，按 `Ctrl+Shift+B` 编译；安装 C/C++ 扩展且 `gdb` 位于 PATH 时可按 `F5` 调试。迁移前已生成的 `build/` 不应搬到其他位置，重新编译会从源码生成它。
