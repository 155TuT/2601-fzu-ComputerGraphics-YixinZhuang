# 作业 3 物理渲染器

本作业在教师的 C++ 渲染框架中完成相机射线、三角形与球体求交、BVH、直接光照及漫反射全局光照。直接光支持均匀半球与光源重要性采样；全局光支持累积反弹、指定反弹次数和 Russian Roulette。加分部分包含镜面、折射、玻璃、Beckmann 微表面金属，以及 F/Z/U 同场景渲染。

完整目录为 `task3/`，提交目录为 `submission/task3-submit/`。两者的程序、源码、报告和资源保持相同的相对路径。

## 启动与渲染

Windows 64 位系统中，解压完整提交包，双击 `run.cmd` 打开漫反射双球场景。使用英文输入状态，按 `R` 开始渲染，按 `E` 返回编辑预览，`S` 保存渲染结果；鼠标拖动旋转，滚轮缩放，`Space` 恢复相机。

在当前文件夹的 PowerShell 中可直接导出 PNG：

```powershell
.\run.cmd -t 4 -s 64 -l 8 -m 5 -r 480 360 -f bunny.png code/dae/sky/CBbunny.dae
```

| 参数 | 含义 | 示例 |
| --- | --- | --- |
| `-t` | 渲染线程数 | `-t 4` |
| `-s` | 每像素相机样本数 | `-s 64` |
| `-l` | 每个面积光源的样本数 | `-l 8` |
| `-m` | 最大反弹次数，0 显示发光表面 | `-m 5` |
| `-o` | 1 累积到最大反弹，0 仅显示指定反弹 | `-o 0 -m 2` |
| `-H` | 使用均匀半球估计直接光 | 默认采用光源重要性采样 |
| `-U` | 使用固定最大深度终止 | 默认启用 RR，存活概率 0.65 |
| `-q` | 随机种子 | `-q 20261025` |
| `-N` | 按表面法线显示求交结果 | 用于检查几何 |
| `-B` | 使用暴力遍历求交 | 用于与 BVH 对比 |
| `-f`、`-r` | 输出文件与图像尺寸 | `-f result.png -r 480 360` |

以下命令分别查看法线、直接光与单次反弹：

```powershell
.\run.cmd -N -s 4 -r 480 360 -f teapot.png code/dae/meshedit/teapot.dae
.\run.cmd -s 16 -l 16 -m 1 -H -r 480 360 -f hemisphere.png code/dae/sky/CBbunny.dae
.\run.cmd -s 64 -l 8 -m 2 -o 0 -r 480 360 -f bounce2.png code/dae/sky/CBbunny.dae
```

逐像素随机流由像素坐标和种子确定，在相同参数下切换线程数可复现相同图像。

F/Z/U 场景保留教师提供的三个字母模型，统一尺寸后放入同一个面积光源场景。可直接渲染：

```powershell
.\run.cmd -t 4 -s 128 -l 8 -m 8 -r 640 480 -f fzu.png code/dae/fzu/FZU-gallery.dae
```

完整目录中的 `scripts/create_fzu_scene.py` 保存场景生成方法，使用 Python 3 可重新生成 DAE。额外材质的场景和参数见报告第六章。

以下命令分别渲染镜面与玻璃组合、粗糙度为 0.25 的金属双球：

```powershell
.\run.cmd -t 4 -s 128 -l 8 -m 8 -r 480 360 -f glass.png code/dae/sky/CBspheres.dae
.\run.cmd -t 4 -s 128 -l 8 -m 8 -r 480 360 -f metal.png code/dae/custom/CBspheres_metal_a025.dae
```

## 报告与源码

[作业报告.docx](docs/作业报告.docx) 为可编辑版本，[作业报告.pdf](docs/作业报告.pdf) 为阅读版本；姓名和学号保留待填写。[报告源文件](docs/报告.md) 和 `docs/screenshots/` 保存实验解释与实际渲染图。

| 源码 | 实现 |
| --- | --- |
| `code/src/pathtracer/camera.cpp` | 针孔相机射线 |
| `code/src/scene/triangle.cpp`、`sphere.cpp` | 图元求交 |
| `code/src/scene/bbox.cpp`、`bvh.cpp` | 包围盒与 BVH |
| `code/src/pathtracer/bsdf.cpp`、`pathtracer.cpp` | 漫反射材质、采样与光线传输 |
| `code/src/pathtracer/advanced_bsdf.cpp` | 镜面、折射、玻璃和微表面金属 |

完整目录的 `code/verification/` 包含算法与材质测试，检查几何求交、光照估计、Snell 定律、Fresnel 分支和微表面分布。验证代码与脚本保存在完整目录。

## 编译

使用 CMake 3.13 或更新版本，以及含 `gcc`、`g++`、`mingw32-make` 的 64 位 MinGW-w64。在当前文件夹运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

脚本编译程序并更新 `bin/`。本机完整目录可使用 `.tools/` 中已有的 CMake；提交包通过 PATH 查找 CMake，也可传入实际工具位置：

```powershell
.\build.ps1 -CMake 'C:\Program Files\CMake\bin\cmake.exe' -CompilerDir 'C:\msys64\mingw64\bin'
```

使用 VS Code 时，直接打开 `task3` 或 `task3-submit` 文件夹，按 `Ctrl+Shift+B` 编译；安装 C/C++ 扩展并准备 GDB 后可按 `F5` 调试。重新编译推荐使用较短路径，例如 `C:\cg-task3`。

完整目录可运行 `.\scripts\test.ps1` 检查算法与材质；`scripts/render_report.py` 可复现报告实验，日志和参数位于 `verification/`。重新生成提交包的方法见完整目录中的 `PROCESS.md`。
