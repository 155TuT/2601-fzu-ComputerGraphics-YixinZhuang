# 任务 1 性能加分方案调研与选择记录

调研日期：2026-10-02（Asia/Shanghai）。本文件保存研究与方案比较；下方初始比较先于用户的选择。

后续选择记录：用户已选择 C（SIMD 边函数），根代理授权转实施。实现保留 double 精度，提供 isolated AVX 4-wide 的 runtime dispatch、SSE2 2-wide 和 scalar 回退，以及可切换的直接边函数/增量标量基线；实测结果由 `rasterizer_benchmark` 生成。此处不预先声称任何加速比例或验证结论。

## 教师要求与当前基线

当前教师 PDF 的任务 1 为“绘制单色三角形”，额外加分明确要求提高三角形光栅化速度，示例包括移出冗余运算、减少内存访问、避免逐个检查包围盒中的采样点；要求报告说明优化，并允许用 `clock()` 或 `std::chrono::high_resolution_clock` 比较耗时。任务 2 要求规则网格 SSAA，教师本版没有另列任务 2 的加分条款。因此，下述性能方案属于任务 1 加分；其他 AA 技术只能作为任务 2 的可选扩展，不能代替要求中的 SSAA，也不能自动声称得到任务 1 性能加分。

要求文本证据：`../requirements/计算机图形学作业1 - 光栅化器.txt`，第 4–5 页任务 1 末尾及任务 2 开头。

当前 `../../code/src/rasterizer.cpp` 已采用 double 边函数、预计算三个边系数及面积倒数、包围盒裁剪和内部增量步进。`for_each_triangle_sample` 在每像素/子采样行重新计算起点，每个候选采样点仍做三边判断，并计算质心权重；单色、插值色和纹理三角形共用此函数。sample buffer 是每像素连续 `sample_rate` 个 RGB float `Color` 的布局，resolve 最后转 8bit。当前基线已包含常规增量优化；后续测量须以它为基线，不能用故意放慢的版本制造加速比。

只读检查时源码 SHA256：`1A7672BA59F48AAEAC75389C9550FAB8B1DB18AABDAF8429215CDA816D3F283C`。

## 三个可选方案

以下适配性和风险判断是针对本地 CPU SVG 程序的工程推断；文献并未给出本作业或本机的加速比例，当前也没有执行候选实现的性能实验。

| 方案 | 在本框架中的做法 | 适合的三角形 | 等价性与主要风险 | 工作量 / 建议 |
|---|---|---|---|---|
| A. 分块边函数判定与整块填充 | 先按例如 8×8 像素分块。对每条有向边求该块采样点矩形的最小/最大值；整块在某边外则跳过，三边都完全在内则按行填充单色样本，只有混合块回到现有逐采样判断。小包围盒走原路径。 | 大面积实心 SVG 多边形、较高 SSAA；大量内部样本可绕过边测试。 | 保持同一中心网格、double、方向和当前边界容差；临界块回退。不得把 conservative rasterization 的“碰到像素就涂色”当最终覆盖规则。细长斜三角形多为混合块，收益小；过大块降低剔除效率。 | 中等；首选。保留现有算法较容易，示意图和计数清晰，CPU 指令集无额外要求。 |
| B. 扫描线求连续 span | 每个采样行从三个半平面求 x 的交集，直接填有效连续区间；单色 span 可连续写样本。临界端点用原边函数校正，水平边与极小 x 系数走保护路径。 | 包围盒很大但覆盖率低的细长三角形；标量 CPU；长水平 span。 | 需要明确半整数中心、闭边、CW/CCW、水平边和顶点行规则，不能照搬默认半开扫描线而漏掉要求绘制的边。除法、ceil/floor 与浮点端点可能改变一个样本；必要时扩大候选区间再验证。 | 中等偏高；适合强调算法复杂度和减少无效采样的报告。 |
| C. 4-wide double SIMD 边测试 | 保持边函数思想，一组处理同一行 4 个邻近采样点，向量比较生成覆盖 mask，再按 mask 写单色样本；保留尾部和小三角形标量路径。 | 大包围盒、可形成足够长行的三角形；支持 AVX 的 x86 CPU。 | 当前基线是 double：4-wide double 使用 AVX 256bit；SSE 的 4-wide float 会改变精度，不可未经验证替换。FMA 与不同加法顺序会影响临界边，须保留数值策略或回退。AoS sample buffer 的条件写入可能成为瓶颈；大量小三角形不足 4 lane。 | 高；进阶备选。须 runtime CPU 检测及标量回退，否则可执行文件可能不能在教师机器运行。 |

A 的依据：[McCormack 与 McNamara 2000，Eurographics 官方论文页](https://diglib.eg.org/items/9d5e02df-e13d-4b3d-b760-550184578e80)，讨论半平面边函数与分块遍历；[Akenine-Möller 与 Aila 2005，JGT 原论文摘要](https://www.tandfonline.com/doi/abs/10.1080/2151237X.2005.10129198)，明确可基于边函数构建 tiled rasterization。NVIDIA 上作者撰写的 [GPU Gems 2 第 42 章](https://developer.nvidia.com/gpugems/gpugems2/part-v-image-oriented-computing/chapter-42-conservative-rasterization) 提供保守内/外覆盖定义；这里只借用安全块分类思路，不改变最终采样语义。

B 的依据：[MIT 官方课程的 triangle scan conversion 讲义](https://groups.csail.mit.edu/graphics/classes/6.837/F98/Lecture7/triangles.html)；[Hasselgren 等 2016 原论文](https://fileadmin.cs.lth.se/graphics/research/papers/2016/culling/culling.pdf) 的 §3.1 从每扫描线左右边事件、边斜率和中间顶点开始说明覆盖计算。该论文是 3D 遮挡剔除工作，本作业仅借用 coverage/scanline 思想，不引入深度缓冲或遮挡查询。

C 的依据：[Pineda 1988 原论文（MIT 托管）](https://people.csail.mit.edu/ericchan/bib/pdf/p17-pineda.pdf) §2 给出边函数增量关系，§7 说明相邻采样的并行计算；[Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html) 说明 `_mm256_add_pd` 操作 4 个 double、需 AVX。[Intel 原论文配套仓库](https://github.com/GameTechDev/MaskedOcclusionCulling) 展示按 CPU 能力选择 AVX/SSE 实现及精确覆盖模式，作为兼容性和精度风险的直接证据，不作为此作业的可直接移植实现。

## 像素等价验收

先固定当前基线，再让候选输出与基线比较。任务 1 单色在同一覆盖 mask、同一图元顺序和同一颜色写入下可要求最终 RGB 字节完全一致；不能只凭“截图看不出差别”。若同时优化任务 4/5/6，质心和 UV 的运算顺序也可能影响量化值，应单独标记这类变更，不能混入任务 1 的等价声明。

测试至少包括：CW/CCW、采样点恰在三个边及顶点上、两三角形拼矩形、平顶/平底/水平边、负坐标与越界、零面积、非常窄及近退化、跨块边界、非整像素坐标、宽度不为 4 或块大小倍数的尾部。SSAA 使用 1、4、9、16，若程序提供更大档位再加入其值。随机测试固定种子，并统计不同像素数、最大通道误差、首次差异坐标；出现差异先修正确性，再报告速度。

分块分类的数学基础是仿射函数在矩形上的极值出现于角点。用采样点矩形而不是像素外框能更准确匹配当前中心采样；双精度计算仍要给临界判定留保守裕量，确保 uncertain block 回到原判断，避免错误整块填充或剔除。扫描线与 SIMD 同样不能未经验证改变边界容差、顶点精度或整体图元绘制顺序。

## 公平 benchmark 设计（待选择后实施）

1. 复用现有 Release 编译器和优化参数。基线与候选保持同一 build、同一 framebuffer 尺寸、颜色、图元输入和采样率。记录 CPU 型号、是否启用 AVX、编译器版本、编译参数、源码 SHA256。
2. 分开记录纯三角形 coverage/write 和整帧时间；纯算法计时不包括 SVG 解析、窗口创建、磁盘写 PNG。clear/resolve 若排除，应双方一致；整帧测量包含相同 clear/resolve，并说明其占比。
3. 采用两类输入：老师 basic/test3–test6 的实际绘制负载；确定性合成集，包含大实心三角形、细長低覆盖三角形、亚像素小三角形、大量小三角形和裁剪场景。颜色/重叠顺序保持一致，不能重排 SVG 图元。
4. 预热后重复多次，每次批量足够图元以减少计时噪声。AB 与 BA 交替或确定性随机顺序，报告中位数、离散范围和样本数；可用 `steady_clock` 测稳定间隔，并记录如何对应教师允许的 chrono 计时要求。
5. 每组同时保存 coverage/framebuffer checksum，避免编译器移除工作；记录访问候选样本、实际覆盖样本、拒绝/整填/混合块（A）或 span 长度（B）。SIMD 记录尾部/回退比例（C）。给出各输入的加速比与退化情形，不只展示最快案例。
6. 分辨率至少覆盖 512×512 与 1024×1024；spp 1 为任务 1 主结果，4/9/16 为跨任务兼容性与性能补充。不得把更低采样率、float 替代 double 或省略绘制当作速度优化。

## AA 可选扩展的界限

若另行选择非 MSAA 的画质扩展，可考虑几何解析 coverage：求三角形/像素方格交面积作为 box filter 覆盖率，单色形状更直观，但颜色插值和重叠几何需要正确积分或联合覆盖，逐三角形独立 blend 会产生共享边缝或重叠错误。作者 Sean Barrett 对 [stb_truetype v2 rasterizer 的说明](https://nothings.org/gamedev/rasterize/) 直接指出精确面积方法在重叠形状上的限制。它应作为额外模式，保留教师要求的规则网格 SSAA。

另一方向是 [SMAA 作者项目页](https://iryoku.com/smaa/) 的 image post-processing。限定单帧 `SMAA 1x` 才符合“不采用 MSAA”的扩展范围；不能把 S2x/4x/T2x 混称为同一个不采样方法。CPU SVG 的窗口 RGB 结果上实现多遍 edge detection / weights / neighborhood blending 是额外工程，不能保证恢复未被中心采样记录的极细几何，亦不是任务 1 光栅化性能的默认推荐。

调研阶段未提前实现任何 A/B/C 或 AA 候选；之后仅按用户选择继续 C 的实现。最终报告中的性能和等价结论必须来自实际 benchmark 文件。
