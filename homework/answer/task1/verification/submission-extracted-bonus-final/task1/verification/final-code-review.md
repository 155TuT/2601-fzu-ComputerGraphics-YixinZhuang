# 最终源码与证据只读复核

复核日期：2026-10-02（Asia/Shanghai）。只读检查最终源码、CSV、审计 JSON、实际编译命令及现有测试日志；未重新运行 benchmark、编译或渲染，未改动源码和报告。

根代理收尾：下述 oracle 范围表述已在最终报告中修正为固定 24×24、864 种配置、重复两次合计 1728 次；finalize 脚本说明也已收紧。最终结构审计与对应报告哈希见 `verification/report/structural-audit.json`。以下保留复核时的具体发现和验证依据。

## 结论

未发现当前任务 1–6 与 SIMD 实现的交付阻断缺陷。性能与像素一致性结论有实际数据支持，小三角形退化已保留。需要收紧一处 oracle 范围表述，见下一节；它不否定正确性结果，但关系到实验覆盖范围的准确描述。

## 应修正的证据表述

`code/verification/rasterizer_benchmark.cpp:159` 的 oracle 渲染器固定为 **24×24**，不使用 benchmark 的 512/1024 width 参数。每次运行核对 72 个三角形 × 4 种 spp（1/4/9/16）× 3 个遍历模式 = **864 种配置**；512 和 1024 两次运行分别重复这些配置，总计 **1728 次核对**。

`scripts/finalize_report_results.py:33` 原表述为“两分辨率合计 1728 组核对”，可能被读成 oracle 在两种大尺寸帧缓冲上分别执行。建议明确写为：

> 独立 oracle 在 24×24 帧缓冲上核对 864 种配置，两次 benchmark 运行合计 1728 次核对；错误样本为 0。实际流水线的 384 组对照分别使用 512×512 和 1024×1024。

该意见已传给根代理。复核文件只记录发现时状态，最终报告是否已修正以交付报告为准。

## 已核对的实测数据

| 检查 | 当前证据 |
| --- | --- |
| 流水线等价 CSV | 两尺度合计 384 行，`different_pixels` 和 `different_coverage_samples` 全部 0 |
| SIMD 对增量样本 | 所有 `reference=incremental,candidate=simd` 行的 `different_float_samples` 为 0 |
| 独立 oracle CSV | 合计 1728 行，`wrong_samples` 全部 0；范围如上 |
| 原始计时 | 合计 792 行；72 个尺度/场景/spp/方法组，每组恰有 11 次 |
| 源码对应性 | audit.json 中 rasterizer.cpp、rasterizer.h、rasterizer_benchmark.cpp 的 SHA256 均匹配当前文件 |
| 基础及解析测试 | LastTest.log 实际结果为 41 项基础通过、20 项解析通过，均 0 失败 |
| 1 spp 小三角形 | 512：0.815261×；1024：0.907895×，SIMD 相对增量变慢，原 CSV 保留 |

性能统计计时范围与源码相符：包括真实三角形 setup、coverage、单色着色和 sample buffer 写入；不含 clear、resolve、分配、SVG 解析、GUI、导出与 hash/CSV。这是光栅化阶段测试，报告已明确不能称整帧耗时。

## 编译与兼容性

实际 `build/compile_commands.json` 共有 68 条编译命令，其中全局 `-mavx*`、`-march=native`、`/arch:AVX*` 出现数为 **0**。顶层 CMake 禁用 CGL 原本根据构建机器启用全局 AVX 的检测；GNU/Clang AVX 四路 double 仅在带 `target("avx")` 的函数中使用。runtime CPU/OS 能力检测选择 AVX，否则走 SSE2 两路或标量；尾部调用同一增量标量算法。

当前实际运行的是 AVX 路径；强制 `R_INCREMENTAL` 验证了标量路径。SSE2 源码编译通过，尚无无 AVX 硬件上的运行证据。报告明确区分了这两个层次，没有扩大跨机器验证结论。

SIMD 保持 double 边函数，lane 对应邻近像素，子像素推进使用同一加法顺序；三个三角形着色入口共享遍历。`-ffp-contract=off` 在主要实际编译命令中存在。最终 RGB 是字节级比较；内部样本以 float 数值相等比较，准确表述为“完全同值”，不应扩大为所有可能浮点表示（例如 +0/-0）的位级证明。

## 报告硬编码值与内存

`finalize_report_results.py` 中确有固定的测试个数和存储数字，但**本次与实际一致**：

- 20 项解析测试与 LastTest.log 的实际输出一致。
- 96×96、`sizeof(Color)=12` 时，SSAA 1/4/16 的 sample buffer 分别为 110592、442368、1769472 B，aa-summary.csv 的全部场景都一致。
- RGB framebuffer 为 96×96×3 = 27648 B，CSV 全部行一致。
- 解析持久存储由 `geometry_bytes()` 计算 vector 容量，CSV 五场景为 1184–1792 B；报告范围来自 CSV，包含 16×16 tile 索引容量。

该表是单独渲染器的持久容量，不能称进程内存峰值。报告已明确不含临时裁剪多边形、分配器开销，并说明 GUI 同时持有 SSAA 和解析对象，界限正确。

维护建议：脚本首行“no values ... hardcoded”与固定值的存在不完全一致；后续可直接读取测试计数及 aa-summary 的存储字段并验证尺寸。此为复现维护建议，本次没有数值错误，不必为此重新运行实验。

AA CSV 的 `p95_ms` 使用 11 次计时中的最大值；它可视为 nearest-rank P95（ceil(0.95×11)=11），若后续需要插值分位数应显式定义，不能默认它与 SIMD 的插值 percentile 相同。报告当前只引用 AA 中位数，不受影响。

## AA 结论边界

解析模式对不透明几何按可见子多边形面积和一阶矩积分，解决共享边与 painter 顺序；仿射颜色在 float 舍入范围内积分。纹理仅在可见子多边形质心采样，源码和报告均明确是近似，没有声称纹理积分精确。4096 spp 独立参考是有限规则采样，报告明确没有称数学真值。实测解析模式更慢，报告保留成本与临时碎片限制，未将其冒充任务 1 的加速方案。
