# 最终版更新

本文件下方保留最初仅必做版的模板分析与执行契约，不能将其中的旧字号、页数或旧渲染尝试当作最新交付。用户随后明确要求仿照旧报告字体、排版、加粗及白底代码图；该授权已用于最终版。

最终交付采用宋体/Times New Roman 12pt 正文、20pt 行距、黑体加粗标题，包含 35 页、41 处图片和 3 张表格；源代码图目录共 18 张 PNG。教师模板页面几何与保留部件不变。Word 16.0 正常打开并只读导出 PDF，全部页面检查通过。

最终结构、样式、源图与逐页证据以 `structural-audit.json`、`visual-qa.json` 和 `report-build-audit.json` 为准。本轮 canonical 尝试也因 Python runtime PATH 注入意外调用已安装 LibreOffice，产生的 42 页副本不用于交付；最终文件来自 Word 导出。

# 教师报告模板执行契约（历史分析）

## 参考与证据

- 原件：`C:/Users/17169/Documents/Mycode/fzucourses/Majorcourse/2601-fzu-ComputerGraphics-YixinZhuang/homework/teacher/task1/作业报告.docx`。
- SHA256：`cd682fbbda36cce208e9661699b5353fe4429ba343daa9787da6a6793ad24e16`；原件保留且在生成前后验证。
- Word 16.0 隐藏只读打开并导出 `template-word-reference.pdf`，3 页；Poppler 110 dpi 的 `template-word-render/page-1.png` 至 `page-3.png` 均已逐页查看。
- 一节 A4 纵向，无表格、内嵌图、文本框、页眉页脚部件、字段、书签、内容控件、批注、修订。
- 包部件清单及 SHA256：`template-package-inventory.json`；段落及直接格式：`template-paragraph-evidence.json`；样式审计：`template-style-evidence.json`。
- canonical render_docx.py 的 Windows 实现会选择系统 soffice，第一次及一次仅限制父 shell PATH 的尝试仍由 Python runtime PATH 注入而调用了用户 LibreOffice，均仅转换模板副本。后续通过 Python 进程内限制 PATH 成功阻止该回退，`template-restricted-render.log` 记录缺少 bundled LibreOffice。最终报告使用 Word 原生只读导出与 bundled Poppler，不再调用用户 LibreOffice。第一次沙箱 Word COM 登录会话失败，获自动批准的本机只读调用成功。

## 版式系统

- `word/document.xml/w:body/w:sectPr`：页宽 11906 twip，高 16838 twip。
- 页边距：左 1440 twip（25.4 mm），右 2040（35.98 mm），上 1760（31.04 mm），下 2840（50.09 mm）；正文宽 8426 twip（148.63 mm）。
- 页眉/页脚距离均 709 twip（12.51 mm）；装订线 0；单栏（列间距 708）；titlePg 首页面不同标志保留；文档网格 linePitch 326 保留。
- 不添加页眉、页脚、水平线、封面形状、框形摘要等原件未有元素。
- 源主要字体 Kaiti TC，各 run 与段落末 run 直接指定。Word 在本机用可用字体回退，源参考的中文全部可读；生成稿沿用同一请求字体并检查实际渲染。
- 原 `Normal` 为默认段落样式，字号继承为 12 pt；模板各可见 run 直接指定 14 pt，所以报告正文沿用 14 pt。正文黑色，替代原灰色提示；左对齐，默认单倍、无固定行高，无首行缩进。
- 主标题原 p[1]：24 pt，粗体，居中；副标题 p[2]：18 pt，居中；作者槽 p[3]：14 pt，居中。
- 八节标题源 p[5,8,13,18,23,27,36,48]：14 pt、黑色、左对齐、不加装饰。生成稿为同视觉 14 pt 加 keepNext，避免标题孤立。
- 原 p[0,4,7,12,17,22,26,35,47,50] 为间隔段落；扩展时保留标题块周围间隔，正文用段落自然间距，手动分页仅用于完整图组与章节边界。

## 槽位和内容流

所有段落稳定定位为 `word/document.xml/w:body/w:p[从 1 计数]`，下列索引为 python-docx 从 0 计数。

| 源索引 | 角色 | 处理 |
| --- | --- | --- |
| 1 | 课程名主标题 | 保留文字、大小与居中；添加同视觉 Title 语义样式 |
| 2 | 作业名副标题 | 保留课程作业 1 与光栅化器主题，沿用 18 pt 居中 |
| 3 | 姓名学号槽 | 改为 `[姓名待填写-学号待填写]`；不得沿用他人身份 |
| 5–6 | 一 概述 | 保留标题；将灰色提示替换为本次六项实现和证据范围 |
| 8–11 | 二 绘制三角形 | 保留标题；扩展为算法、边界、优化和默认/检查器图 |
| 13–16 | 三 超采样 | 保留标题；扩展为规则子样本、缓冲布局、resolve、spp 1/4/16 图组及观察 |
| 18–21 | 四 几何变换 | 保留标题；扩展为矩阵、SVG 层级、my_robot 动作及 PNG |
| 23–25 | 五 质心坐标 | 保留标题；扩展为重心权重、RGB 三角形、test7 spp 1 图 |
| 27–34 | 六 像素纹理采样 | 保留标题；扩展为 UV、nearest/bilinear、同视角 4 图和观察 |
| 36–46 | 七 MipMap纹理映射 | 模板 XML 段落无七序号，但 Word 源渲染呈现七序号；报告显式使用七；扩展为 UV 足迹、LOD、六组合、自备 PNG、权衡 |
| 48–49 | 八 实践与思考 | 显式八序号；扩展为真实修复、测试证据、限制及改进想法 |

原件各灰色方括号提示是可删除的指导文本；需将全部指导要求转为完成的叙述/图像，不保留作业提示。身份槽除外。

允许克隆源正文/标题段落样式在相应节内扩展。内容容量以 10–14 页为目标；不能通过缩小正文满足页数。允许添加行内 PNG、黑色图注与简短可编辑公式；原模板无图处理规则，采用正文宽度以内、原图纵横比、居中行内图与相邻图注，保持图组完整。

## 包保留策略

- preserve-only，最终逐字节相同：`word/theme/theme1.xml`、`word/settings.xml`、`word/fontTable.xml`、`word/webSettings.xml`、`word/numbering.xml`、`docProps/custom.xml`、`docProps/core.xml`、`docProps/app.xml`、`_rels/.rels`。
- `word/styles.xml` 保留所有原有 style 内容，允许在结尾追加同视觉 Title/ReportHeading/Caption 样式；不改变已有样式节点。
- editable：`word/document.xml` 全正文槽；其 sectPr 必须原值保留。`word/_rels/document.xml.rels` 保留旧关系、只新增图片关系；`[Content_Types].xml` 保留旧记录，新增 PNG 类型。
- 新增：实际报告 PNG 的 `word/media/*`。
- 构建采用原件 python-docx 对象模型来扩展，然后将只允许改变的部件合并回原 zip；其余部件直接使用原件 bytes，保留关系集合。

## 验收

1. 原件前后 SHA256 不变，1 节、页尺寸边距和所有 preserve-only 部件相同。
2. 所有八节正文和图组实际填充，16 spp 按 4×4 网格解释，L_LINEAR 作为必做，不复制旧报告代码、截图、身份或性能结论。
3. 图像必须为本次可执行程序输出 PNG，有文件与参数标注；放大图准确写为像素检查器或 framebuffer 放大，按实际证据写。
4. 通过 Word 16.0 只读打开、PDF 导出、页数统计；bundled Poppler 对最终 PDF 逐页渲染且每页视觉审查。
5. 页面无遗漏图、乱码、截断、重叠、标题孤立、图注错页、大幅空白；正文/图片都在源正文区域。
