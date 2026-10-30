# 作业 2 几何建模与处理

本作业在教师的 C++ 网格编辑器中完成贝塞尔曲线与曲面、面积加权顶点法线、边翻转、边分割和 Loop 细分，同时支持边界分割与边界细分。加分部分包含自建人形网格、细分展示和环境反射着色。

完整目录为 `task2/`，提交目录为 `submission/task2-submit/`。两者的程序、源码、报告和资源保持相同的相对路径。

## 启动与操作

Windows 64 位系统中，解压完整提交包，双击 `run.cmd` 打开茶壶网格。程序和配套 DLL 放在 `bin/`，着色器、环境纹理与输入模型放在 `code/`。

在当前文件夹的 PowerShell 中可打开其他输入：

```powershell
.\run.cmd code\bzc\curve1.bzc
.\run.cmd code\bez\teapot.bez
.\run.cmd code\dae\cube.dae
.\run.cmd code\dae\custom\humanoid.dae
```

| 内容 | 操作 | 观察 |
| --- | --- | --- |
| 贝塞尔曲线 | `E` 展示下一层，`C` 显示完整曲线，滚轮调整参数 t | 中间控制点逐层收敛到曲线上的一点 |
| 曲面与法线 | 打开 `.bez` 或 `.dae`，按 `Q` | 比较平面着色与顶点法线插值 |
| 边翻转 | 单击选中内部边，按 `F` | 两相邻三角形更换共享对角线 |
| 边分割 | 单击选中边，按 `S` | 边中点插入新顶点，重连相邻三角形 |
| Loop 细分 | 按 `L` | 每个三角形分为四个，顶点按邻域权重更新 |

鼠标在背景上拖动可旋转，滚轮缩放，`Space` 恢复相机，`W` 切换线框，`I` 切换信息，`0` 至 `9` 切换着色器。字母快捷键使用英文输入状态。

打开人形模型后，按 `L` 观察细分，按 `2` 查看环境反射。`docs/models/humanoid.json` 保存造型参数，`docs/models/humanoid.obj` 保存可编辑网格；在完整目录中安装 Python 3 后运行 `python scripts/build_humanoid.py`，可重新生成 OBJ 和程序使用的 DAE。原始人形为单连通闭合网格，共 1,062 个顶点、2,120 个三角形。

## 报告与源码

[作业报告.docx](docs/作业报告.docx) 为可编辑版本，[作业报告.pdf](docs/作业报告.pdf) 为阅读版本；身份栏保留姓名、学号待填写。[报告源文件](docs/报告.md) 和 `docs/screenshots/` 保存解释与实际实验图。

六项主要算法集中在 `code/src/student_code.cpp`，环境反射着色位于 `code/shader/frag`。其余源码包含教师框架、输入资源与构建所需依赖。完整目录中的 `code/verification/geometry_tests.cpp` 检查数值结果、半边连接、边界和细分后的拓扑，共 5 组 CTest；验证代码与脚本保存在完整目录。

## 编译

使用 CMake 3.13 或更新版本，以及含 `gcc`、`g++`、`mingw32-make` 的 64 位 MinGW-w64。在当前文件夹运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

脚本编译程序并更新 `bin/`。本机完整目录可使用 `.tools/` 中已有的 CMake；提交包通过 PATH 查找 CMake，也可指定工具位置：

```powershell
.\build.ps1 -CMake 'C:\Program Files\CMake\bin\cmake.exe' -CompilerDir 'C:\msys64\mingw64\bin'
```

使用 VS Code 时，直接打开 `task2` 或 `task2-submit` 文件夹，按 `Ctrl+Shift+B` 编译；安装 C/C++ 扩展并准备 GDB 后可按 `F5` 调试。重新编译推荐使用较短的解压路径，例如 `C:\cg-task2`。

完整目录可运行 `.\scripts\test.ps1` 检查算法；实验脚本和验证记录见 `scripts/`、`verification/`，重新生成提交包的方法见完整目录中的 `PROCESS.md`。
