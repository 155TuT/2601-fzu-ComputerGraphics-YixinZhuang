# 报告样式参考审计

仅只读检查参考文件。教师模板、旧版 PDF 与 Linux 课程报告均未修改。当前报告正文由根代理维护；本轮只更新报告 builder 与真实源码截图生成器，尚未生成新版最终 DOCX/PDF。

## 旧版图形学报告

参考：`old/homework/作业报告1.pdf`，38 页。实际 PDF 字体与坐标证据见 `old-pdf-evidence.json`；第一页、第二页的只读渲染见 `old-report-01.png`、`old-report-02.png`。

- 纸张 A4。正文左界约 72 pt（25.4 mm）；保留教师 DOCX 的页面几何与页眉页脚。
- 正文中文为嵌入 SimSun，英文与数字主要 Times New Roman，字号 12 pt。中英字符 top 值存在约 2 pt 的字形偏差，但相邻行基线约 20 pt。
- 正文首行约缩进 24 pt，即两个 12 pt 中文字宽。
- 章节标题主要 SimHei，约 14 pt，呈黑色加粗视觉；正文有局部重点加粗。
- 第一页主标题 Arial Unicode MS 24 pt，副标题 18 pt。当前本机无 Arial Unicode MS 字体文件，采用已安装黑体作为明确记录的替代。
- 图片与图注居中；页面采用连续段落、明确标题与图文，不添加装饰框、彩色章标题或分隔线。

## Linux 课程报告

实际路径为 Linux 课程 `homework/task1/report.docx`、`task2/report.docx`、`task3/report.docx`，不是名为“task1到3”的单个目录。文件 hash、段落样例与图片清单见 `docx-evidence.json`、`media-evidence.json`。

- A4，左右约 20 mm，上下约 18 mm。
- 正文样式中文宋体，英文 Times New Roman，10.5 pt、固定 15 pt 行距；首行缩进 21 pt。
- Code Block 样式为可编辑 Consolas 9 pt；Inline Code 为 Consolas 9.5 pt。
- 原文档媒体是白底黑字终端截图；源码主要由可编辑 Code Block 段落承载。这里将用户要求的白底视觉用于真实 C++ 源码 PNG，保留等宽字与纯白背景，加入克制的语法色及浅灰行号。

## 新 builder 的约定

`scripts/build_report.py` 保留教师模板原件、页面几何、已有样式、页眉页脚、其他包部件；新增 Report 样式与当前正文。字体遵循旧 PDF：12 pt 宋体 + Times New Roman、固定 20 pt 行距、24 pt 首行缩进；章标题 14 pt 黑体加粗，子标题 12 pt 黑体加粗，图注 11 pt 居中。手动分页标记仍受根代理正文控制。

支持 `**重点**`、`### 子标题`、反引号行内源码、简单 pipe 表格、`<!-- images: name.png -->`、`<!-- code: name.png -->`。表格具轻灰边框、浅灰加粗表头与重复表头；不使用固定行高。图片默认来自 `docs/screenshots`，代码图片来自 `docs/code-screenshots`。

代码图嵌入时检查 `verification/code-screenshots.json` 中源码 hash 与 PNG hash。源码改变后会要求重新运行 `capture_code.py`，防止报告引用过期代码图。

## 代码截图生成与边界

`scripts/capture_code.py` 按最终 C++ 的函数或 struct 实际范围提取，输出白底 PNG 与 `.cpp.txt` 源码片段。图片显示源路径、原始行号与分图序号。长行只在显示层换行，优先空白/标点边界，不丢弃字符；长片段平衡分图，避免末张只剩结束花括号。

manifest 保存源文件完整 hash、每个函数片段的起止行号/hash、sidecar hash、PNG hash 与尺寸、字体文件/hash、制图参数。图像由源文件直接生成，不是手工操作 IDE 的截图，也不据此声称手工编辑经历。

默认稳定名称：triangle_setup、triangle_traversal（AVX simd4_samples）、triangle_dispatch、triangle_scalar、sample_buffer_resolve、transforms、barycentric、texture_mapping、texture_pixel、texture_lod、viewport_rotate、analytic_clip、analytic_visibility。第一张 `name.png`，续图 `name_02.png` 等。最终代码仍在演进，完成后应统一重跑生成器。

已检查：两个脚本可编译；builder 样式/加粗/行内代码/escaped pipe 的内存检查通过，未写任何 DOCX。已生成并查看 transforms、SIMD、纹理、解析可见区域代码图；后续最终报告仍需 Word 打开、PDF 导出及全部页面视觉检查。
