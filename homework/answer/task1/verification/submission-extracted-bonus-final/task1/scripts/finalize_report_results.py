"""Insert retained measurements and verified experiment parameters into the report."""
from pathlib import Path
import csv
import json
import re
ROOT=Path(__file__).resolve().parents[1]
MD=ROOT/'docs/报告.md'
def read(path):
    with path.open(encoding='utf8') as file:return list(csv.DictReader(file))
def replace(text,name,content):
    wrapped=f'<!-- results-begin {name} -->\n\n{content.strip()}\n\n<!-- results-end {name} -->'
    pattern=rf'<!-- results-begin {name} -->[\s\S]*?<!-- results-end {name} -->'
    if re.search(pattern,text):return re.sub(pattern,lambda _:wrapped,text)
    marker=f'<!-- {name}-results -->'
    if marker not in text:raise ValueError(f'Missing {marker}')
    return text.replace(marker,wrapped)
def main():
    text=MD.read_text(encoding='utf8')
    audit=json.loads((ROOT/'verification/performance/audit.json').read_text(encoding='utf8'))
    rows=audit['rows'];labels={'large':'大面积','tiny':'小三角形','thin':'细长','clipped':'裁剪'}
    def row(case,width,spp,method):return next(r for r in rows if r['case']==case and int(r['width'])==width and int(r['spp'])==spp and r['method']==method)
    table=['| 场景 | 分辨率 | 直接/ms | 增量/ms | SIMD/ms | 相对增量 |','| --- | --- | ---: | ---: | ---: | ---: |']
    for width in [512,1024]:
        for case in labels:
            d,i,s=[row(case,width,1,m) for m in ['direct_edges','incremental','simd']]
            table.append(f"| {labels[case]} | {width}² | {float(d['median_ms']):.4f} | {float(i['median_ms']):.4f} | {float(s['median_ms']):.4f} | {float(s['speedup_vs_incremental']):.2f}× |")
    simd=f'''<!-- page -->

### 2.3 正确性与性能评估

环境为 Windows x64、AMD Ryzen AI 7 H 350、GCC 15.1.0，RelWithDebInfo 和 -ffp-contract=off；本机选择 AVX 四路 double。固定种子 20261002，四组输入分别含 10 个大三角形、512 个亚像素至小尺寸三角形、12 个细长三角形、12 个越界裁剪三角形，并交替顶点方向。分辨率 512²、1024²，计时采样率 1、4、16 spp；预热 2 次，每种模式重复 11 次，轮换执行顺序，报告中位数。计时包括真实 rasterize 调用的设置、覆盖测试、单色着色与采样缓冲写入，**不包括清空、resolve、SVG 解析、窗口、CSV 或 PNG 导出**，不能将其称为程序整帧耗时。

独立 long-double 端点叉积判定在固定 24×24 校验窗口中核对 72 个边界、裁剪、退化和随机三角形，包含 1/4/9/16 spp 与三个内核，共 864 种配置；两个基准运行各核对一次，合计 {audit['oracle_comparisons']} 次，错误样本为 0。实际 512²/1024² 窗口中，单色、质心颜色及纹理三条管线合计 {audit['equivalence_comparisons']} 组对照，覆盖与最终 RGB 字节差异为 0，SIMD 相对增量版本的内部 float 颜色样本也完全同值。强制增量模式实际验证标量路径；当前 AVX 机器未模拟无 AVX 硬件，SSE2 回退的编译成功不等同于跨机器执行验证。

表 2.1 任务一主结果，1 spp。相对增量 = 增量中位耗时 / SIMD 中位耗时，超过 1 表示加速。

{chr(10).join(table)}

<!-- page -->

<!-- images: simd_speedup.png -->

图 2.4 两分辨率、1/4/16 spp 的 SIMD 相对增量加速比；虚线为 1。数据来自实际 chrono 测量，柱顶给出数值，完整 p10/p90、极值与逐次耗时保存在 verification/performance/。

大三角形、细长及裁剪场景提供足够多的相邻位置，向量算术与覆盖 mask 减少标量比较开销。**小三角形可出现退化**：较短的行很快落入标量尾部，向量设置、调度与条件散写无法摊薄；该输入的总时间也很短，更易受计时噪声影响。1 spp 时没有像素内部的增量推进收益，增量阶段本身未必比直接求值快，因此必须保留两个实际基准，不能把每一步演进都写成必然加速。

此 SIMD 实现保持逐样本着色，改善的是覆盖计算，未解决 AoS 颜色散写或纹理读取带宽。收益依赖几何、采样率、CPU 与编译器，不沿用文献的加速数字。原始几何、校验、计时和源码哈希全部保留，可通过 scripts/benchmark_simd.py --run 重现。
'''
    text=replace(text,'SIMD',simd)
    aarows=read(ROOT/'verification/aa/aa-summary.csv')
    aalabels={'diagonal':'斜边','thin':'细三角形','shared_edge':'共享边','overlap':'重叠','gradient_clipped':'插值及裁剪'}
    def aa(case,mode):return next(r for r in aarows if r['scene']==case and r['mode']==mode)
    quality=['| 场景 | SSAA 1 MAE | SSAA 16 MAE | 解析 MAE | SSAA 16/ms | 解析/ms |','| --- | ---: | ---: | ---: | ---: | ---: |']
    memory=['| 模式 | 持久几何或样本存储 | 计时及精度范围 |','| --- | --- | --- |']
    for case,label in aalabels.items():
        s1,s16,a=[aa(case,m) for m in ['ssaa_1','ssaa_16','analytic']]
        quality.append(f"| {label} | {float(s1['mae_8bit']):.4f} | {float(s16['mae_8bit']):.4f} | {float(a['mae_8bit']):.4f} | {float(s16['median_ms']):.3f} | {float(a['median_ms']):.3f} |")
    analytic_memory=[int(r['retained_geometry_or_sample_bytes']) for r in aarows if r['mode']=='analytic']
    memory+=['| SSAA 1 | 110592 B（108 KiB） | 1 个中心样本/像素 |',
             '| SSAA 4 | 442368 B（432 KiB） | 2×2 规则样本/像素 |',
             '| SSAA 16 | 1769472 B（1728 KiB） | 4×4 规则样本/像素 |',
             f'| 解析模式 | {min(analytic_memory)}–{max(analytic_memory)} B | 本五场景的几何与16×16瓦片索引容量 |']
    aaresult=f'''<!-- page -->

### 3.4 画质与成本评估

20 项解析检查通过，含手算半像素、顺逆时针、零面积、0.01 像素面积的细三角形、大片坐标稳定性、共享边无缝、不透明重叠顺序、仿射 RGB 质心积分、边界裁剪、末像素线段、清空及 resize 安全性。对于单位直角三角形，面积 0.5，黑白平均得到 128；对于 RGB 三顶点，覆盖区域质心处三色各 1/3，乘面积后加白底得到每通道 170。

数值对比使用 96×96 帧缓冲、五个固定场景和四种模式（1/4/16 spp SSAA、解析覆盖率）。参考程序独立在每像素 64×64 中心网格上直接查找最后覆盖的三角形并着色，**4096 spp 参考仍是有限采样，不能称为数学真值**。误差用相对参考的全图 RGB 平均绝对误差 MAE，单位为 0–255 通道值。解析几何的舍入及参考采样误差使 MAE 不必恰为零。

表 3.1 画质与耗时；MAE 越小越接近独立数值参考。耗时含 clear、提交图元及 resolve，2 次预热、11 次重复、中位数；与表 2.1 的覆盖计时范围不同，不能直接横向比较。

{chr(10).join(quality)}

解析覆盖率能保留未击中任何规则样本的细几何，并在共享边与重叠处按可见区域积分。代价是像素内多边形裁剪、分裂、面积矩和临时分配；本实现重视可核对的正确性，不将解析模式宣称为实时速度优势。复杂重叠会增加碎片数量，后续可加入全覆盖快速路径和固定容量临时多边形存储。

<!-- page -->

<!-- images: width=5.6; aa_comparison.png -->

图 3.5 四类几何场景，从左到右为 SSAA 1、SSAA 16、解析面积和独立 4096 spp 参考。图由实际 96×96 framebuffer PNG 进行 4 倍最近邻放大，方便检查像素；这是数值实验图，不是 GUI 截图。

<!-- page -->

<!-- images: aa_gui_ssaa.png | aa_gui_analytic.png -->

图 3.6 自建 docs/analytic_demo.svg，800×800、相同默认视角，左为 1 spp SSAA，右为 A 键解析模式。通过隐藏 GLFW 窗口的真实 A/S 回调取得无损 PNG，参数与 GL 检查记录在 verification/aa/gui-captures.json。

表 3.2 96×96 单独渲染器的持久存储比较。

{chr(10).join(memory)}

四种模式均另需相同的 RGB framebuffer 27648 B。表中未包含临时裁剪多边形、vector 分配器开销或进程峰值；GUI 为便于切换同时持有 SSAA 和解析对象，因此 GUI 进程内存不等于这张表的单独渲染器存储。实验原始计时、RMSE、最大误差、全部模式输出 PNG 与哈希留存在 verification/aa/。
'''
    text=replace(text,'AA',aaresult)
    MD.write_text(text,encoding='utf8')
    print('Inserted measured SIMD and analytic AA results into report Markdown')
if __name__=='__main__':main()
