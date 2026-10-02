# 已选择的扩展实验

用户于 2026-10-02 明确选择：任务一 C（SIMD 并行边函数），另增加解析覆盖率抗锯齿实验；任务三完成额外 GUI 视口旋转。原 `task1-performance-options.md` 保留选择前的调研状态，不能将它读作最终实现状态。

## SIMD

以 Pineda 的仿射边函数及相邻位置可并行求值为理论依据，当前实现使用四个相邻像素作为 AVX double lanes，在每个 lane 内保留原规则 SSAA 的子样本顺序。AVX 只出现在带 target 属性的函数中，运行时检测 CPU/OS 支持；不以全局编译开关要求教师机器具备 AVX。SSE2 两 lane 或标量为回退。不是将 double 改为 float，也没有改变着色频率。

- [Pineda 1988 原论文](https://people.csail.mit.edu/ericchan/bib/pdf/p17-pineda.pdf)，增量边函数与并行求值。
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)，AVX double 操作的指令集条件。
- [Masked Software Occlusion Culling 作者页](https://fileadmin.cs.lth.se/graphics/research/papers/2016/culling/)，用于了解现代 SIMD CPU 光栅化工程；本实验不移植其遮挡剔除算法，也不沿用其性能数字。

## 解析覆盖率

三角形与像素单位方格求交，面积作为 box filter 覆盖率；通过凸多边形半平面裁剪得到交集，面积与一阶矩得到质心。为避免共享边和遮挡错误，按绘制顺序逆向处理，将后绘制的不透明图形从尚未覆盖区域中扣除。可见子区域的不透明常量颜色、仿射 RGB 颜色可在双精度几何范围内精确积分；任意纹理采用可见子区域质心取样，仅几何覆盖精确，纹理积分是近似。

- [Sean Barrett 2015 原作者说明](https://nothings.org/gamedev/rasterize/) 解释像素内面积与 box filter，特别指出简单 signed-area 在重叠形状上的不足。本实现不是其扫描线 signed-area 源码。
- [NVIDIA 2021 解析可见性研究](https://research.nvidia.com/labs/rtr/publication/zhou2021vectorization/) 提供从点采样转向可见区域解析计算的研究背景。本次范围是二维不透明 SVG 三角形，未实现该论文完整三维可见性或可微渲染系统。
- Sutherland 与 Hodgman 的 *Reentrant Polygon Clipping*（1974），DOI [10.1145/360767.360802](https://doi.org/10.1145/360767.360802) 为半平面连续裁剪的经典出处；出版商全文访问返回 403，未复制其源代码或未读全文细节。

本实验不实现透明度混合、曲线的直接解析积分或一般 SVG path fill rule；点与线仍按原作业允许的整像素方式处理。以多边形构成的细三角形作为细线实验，不能声称任意 stroke 都已解析抗锯齿。规则网格 SSAA 保留为默认，并与实验模式独立切换。

## 评估界限

SIMD 使用同一真实 rasterize 调用的三种模式，数据记录真实输出等价性、随机及边界输入、预热、重复计时和当前指令集。解析覆盖率使用独立逐位置点测试的 4096 spp 数值参考（并非数学真值）与可手算面积的验证用例。耗时包括相同明确阶段，内存记录只代表 retained geometry/bin 或 sample buffer，不能宣称是进程峰值或 GPU 内存。
