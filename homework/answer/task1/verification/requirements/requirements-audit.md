# 作业 1 要求核对

本清单以本次 `homework/teacher/task1/计算机图形学作业1 - 光栅化器.pdf`（9 页）、`作业报告.docx` 为准。编译说明为通用说明，旧版报告仅供参考。教师原件未修改。祖先目录和 homework 子树未发现 `AGENTS.md`。

## 必做实现（合计 100 分）

| 任务 | 分值 | 本次要求 | 验证场景 |
| --- | ---: | --- | --- |
| 1 单色三角形 | 20 | 在 `rasterizer.cpp` 实现 `rasterize_triangle`；像素中心 `(x+0.5,y+0.5)`；默认边界采样点计入；顶点顺/逆时针均正确；遍历不慢于三角形包围盒；图像边缘不能漏绘 | `svg/basic/test3.svg`、`test4.svg`、`test5.svg`、`test6.svg` |
| 2 规则网格超采样 | 20 | 每像素 `sqrt(sample_rate) × sqrt(sample_rate)` 个规则网格子样本，存入 `sample_buffer`；窗口大小及采样率变化时重新分配；每帧清空；完成所有图元后算术平均 resolve 到 8-bit RGB；点/线写满该像素的子样本即可，不要求点/线抗锯齿 | `basic/test4.svg`；采样率 1、4、16；窗口 resize 与 `-/=` |
| 3 几何变换 | 10 | 在 `transforms.cpp` 实现 `translate`、`scale`、`rotate`；3×3 齐次矩阵；旋转输入单位是角度 | `svg/transforms/robot.svg` |
| 4 质心插值 | 10 | 在 `rasterizer.cpp` 实现 `rasterize_interpolated_color_triangle`；用重心权重对三个顶点颜色进行线性插值，且与超采样共用正确缓冲 | `svg/basic/test7.svg` |
| 5 像素纹理采样 | 15 | `rasterize_textured_triangle` 用重心坐标插值 UV；`Texture::sample_nearest`、`sample_bilinear` 实现最近邻、双线性；任务 5 默认只采样第 0 层；`P` 键独立切换 P_NEAREST/P_LINEAR | `svg/texmap/` 的纹理 SVG |
| 6 mipmap 层级纹理采样 | 25 | 同一屏幕采样点的 `(x,y)`、`(x+1,y)`、`(x,y+1)` 分别插值 UV，写入 SampleParams；`get_level` 对两 UV 差向量按第 0 层宽高分别缩放后计算足迹；`sample` 支持 L_ZERO、第一个最近层、相邻层线性混合；P 与 L 六种组合均可工作；通过引用/指针访问 MipLevel，避免复制大层 | 自备纹理 SVG；`L` 键切换 L_ZERO/L_NEAREST/L_LINEAR；各层边界 |

任务 6 的 `L_LINEAR + P_LINEAR` 即三线性过滤，为本次必做能力。

## 报告逐节内容及截图

| 报告节 | 必须说明 | 必须图像/资源及参数 |
| --- | --- | --- |
| 标题、概述 | 课程作业 1：光栅化器；姓名、学号；实现内容 | 姓名学号未知应保留待填写，不能沿用旧报告作者 |
| 二、绘制三角形 | 像素中心、包围盒、边函数/覆盖判定、顶点方向处理 | `basic/test4.svg` 默认视角图；另一个启用像素检查器、关注有趣区域的图。模板原文写“一张……分别是……”，按默认图与检查器图两张提供更明确 |
| 三、超采样 | 算法、缓冲数据结构、为何有效、管线改动；解释效果差异 | `basic/test4.svg` 默认视角采样率 1、4、16 并排对比；像素检查器固定在同一明显区域（细三角形角落），体现边缘变化 |
| 四、几何变换 | 说明小人做了什么；矩阵与层级组合 | 修改后的 `my_robot.svg` 保存在项目 `docs/`；报告附该文件渲染 PNG。建议兼附原 `robot.svg` 正确渲染便于核对矩阵实现 |
| 五、质心坐标 | 解释重心权重、和为 1、顶点属性插值；图解红绿蓝三角形 | `basic/test7.svg` 默认视角、采样率 1；红绿蓝顶点渐变三角形可另制图或测试场景 |
| 六、像素采样纹理 | 解释屏幕采样到 UV 到 texel；最近邻及双线性；区别最大场景及原因 | 选 `svg/texmap/` 中明显体现双线性优点的同一场景、同一视角，4 张 PNG：P_NEAREST × spp 1、16；P_LINEAR × spp 1、16。控制 L_ZERO 不变，以隔离像素采样因素 |
| 七、MipMap | 层级采样、UV 导数/LOD、三种层级策略；比较像素采样、层级采样、每像素样本数的速度/内存/抗锯齿权衡 | 自己准备 PNG，并建立/复制 SVG 修改纹理路径。同场景同视角固定 spp，4 张 PNG：L_ZERO+P_NEAREST、L_ZERO+P_LINEAR、L_NEAREST+P_NEAREST、L_NEAREST+P_LINEAR。可补 L_LINEAR 的两种 P 组合来验证本次必做能力 |
| 八、实践与思考 | 实际调试、观察到的优点和问题、改进想法与实际效果；区分测得结果和理论推断 | 若给性能数据，应记录参数、样本数、运行环境及真实原始数据，不能复制旧报告的定性“明显加速” |

截图必须 PNG，不能转 JPG。GUI 保存为无损 PNG。若使用程序导出 framebuffer 与离线放大展示，应准确标注为实际渲染结果及像素放大，不能声称是人工 GUI 截图/按键操作验证。

模板“1 像素/采样”“16 像素/采样”语序有误，应依据程序 sample_rate 与任务 PDF 解释为每像素 1/16 次采样（spp）；16 spp 是 4×4，不能写为 16×16。

## 加分要求与额外实验边界

- 任务 1：包围盒之外的优化，可做增量边函数、预计算、减少访存等；报告说明，若声称性能提高应对比基础实现实测。
- 任务 2：本次 PDF 未单列其他抗锯齿方法的加分条款。用户选择的解析覆盖率作为额外实验，与规则网格超采样对比，不能替代规则网格必做算法或直接宣称获得额外分数。
- 任务 3：PDF 的额外 GUI 功能可选，需说明 SVG→NDC→screen 矩阵栈变化与示例图。报告模板的更有趣机器人动作也标为加分，同时明确要求保存修改后的 `docs/my_robot.svg`，故应完成此资源。
- 任务 6：额外过滤方法才是加分；本次已有明确 L_LINEAR 必做任务，三线性不能再当“额外过滤方法”。

## 与旧版作业报告的偏差及不可照搬点

1. 旧报告第 35 页将三线性过滤作为加分。本次任务 PDF 第 8–9 页明确 L_LINEAR，须按必做实现和验证。
2. 旧报告第 13–17 页介绍非规则“MSAA”图案。任务 PDF 第 5 页要求规则 `sqrt(sample_rate)` 网格；旧方法只能作为附加实验。并且非规则采样位置本身不构成 MSAA 与 SSAA 的完整区分，不能照搬其“每子样本避免着色”的结论。
3. 旧报告第 37 页将“1×1 到 4×4、16×16”与 1/4/16 sample_rate 混用；正确关系是 1×1、2×2、4×4。
4. 旧报告第 38 页称 sample_buffer 内存按“采样率平方”增长；按本程序 sample_rate（样本数）应为 `width × height × sample_rate × sizeof(Color)`，对 spp 线性增长，仅对网格边长平方增长。
5. 旧报告关于上邻像素的说法不适合本程序屏幕 y 朝下的坐标系；`(x,y+1)` 是屏幕下邻，导数范数公式仍一致。
6. 旧报告的作者、代码、截图、性能和调试经历不属于本次成果，不可代入新报告。
7. 本次报告模板明确要求自己准备 PNG、自定义 SVG 路径及 `docs/my_robot.svg`；旧报告图题只称 texmap/test4.svg，不能据此省略自备纹理资源或 robot 源文件。
8. 旧报告 Top-Left 规则为可选；本次默认要求边界点覆盖，允许但不强制 OpenGL 边界规则。

## 编译说明与实际作业接口

- 编译说明是通用 CMake/C++/OpenGL/Freetype 环境指南；其中多次出现 `meshedit`、`quad_test`、作业 2，是沿用示例，并非本作业输出名称。
- 本次可执行程序是 `draw`（Windows 为 `draw.exe`），用 SVG 文件或 SVG 目录作为参数。实际 CMake target 应从本次 CMakeLists.txt 确认。
- README/提交指引应给出 Windows 实际可运行的命令与工作目录，携带必要动态库和所需 SVG/PNG，不能仅提交一个依赖缺失的 EXE。
- 主作业 PDF/模板没有给出上传命名、压缩包格式、具体平台与截止时间；不能杜撰教师统一规则。用户明确要求源码三文件、编译后程序、报告，可整理这些资源并说明位置。

## 起始源码检查发现

- `RasterizerImp` 构造函数已按 `width*height*sample_rate` 分配，但 `set_sample_rate` 与 `set_framebuffer_target` 缩回 `width*height`，三处必须保持同一布局。
- `fill_pixel`、`resolve_to_framebuffer` 起始实现只访问一个样本，任务 2 必须一并修改。
- `rotate` 输入单位明确 degree；CGL Matrix3x3 九参数构造按行给值，内部虽列存储，不需要额外转置。
- `texture.cpp::generate_mips` 起始代码存在最后一级未生成问题：startLevel 为 0 时只填至 `numSubLevels-1`，最粗 mip 缓冲虽分配却仍全零。为保证缩小及 LOD 上界的正确性，需要修复其循环上界并纳入 mip 验证。
- main.cpp 提供 `draw <svg> nogl <width> <height>` framebuffer 导出路径，原导出文件名固定 `test.png`；该路径可支持真实离屏验证，但原始参数不支持选择采样率、P/L，需通过额外可复现驱动/可选参数完成报告对比。

## 原文依据位置

- 主任务 PDF 第 1 页：六部分结构、禁止抄袭、报告模板、PNG 要求；第 2 页：程序名、快捷键、主要三个源码。
- 第 4 页：任务 1；第 5–6 页：任务 2；第 7 页：任务 3–4；第 8–9 页：任务 5–6。
- 作业报告 DOCX：八节结构与所有必需截图、my_robot.svg 和自备 PNG 要求。
- 通用编译说明第 1–4 页：环境与构建；第 4 页示例输出名为 meshedit，需辨别。
- 旧报告：第 13–17、35、37–38 页是上述偏差主要来源。
