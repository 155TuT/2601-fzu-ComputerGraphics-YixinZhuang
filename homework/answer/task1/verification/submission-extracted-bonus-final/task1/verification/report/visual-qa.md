# 最终报告检查

状态：通过。Microsoft Word 16.0 只读正常打开并导出 PDF，35 页；最终版 DOCX 与 PDF 的 SHA-256 分别为 `04f93f90466c89015595f8e7d38a2f7caeb5631b846c88c54ddab93178d3e411`、`431f9f46e1e063f79bf19e02eef88604344708729315a5964bb197c829c2f7b2`。

## 结构与内容

- OOXML ZIP CRC 及 13 个 XML/关系部件解析成功，八个要求章节和待填写身份字段均保留。
- 41 处图片、3 张表格；嵌入图与报告源图 SHA-256 对应。
- 18 张白底代码 PNG、可复制代码片段与最终源码 SHA-256 全部对应。
- 宋体/Times New Roman 12pt 正文、20pt 行距、黑体加粗标题及正文重点加粗已核对。
- 教师模板 SHA-256 不变，原有页面设置与保留部件不变。

## 逐页可视检查

全部 35 页实际查看；最后两轮仅对变化页重新查看，其余页面 PNG 字节与已查看版本完全相同。独立记录为 qa-pages-01-18.json 和 qa-pages-19-35.json，均绑定上述最终报告 SHA-256。图注与对应图片同页，代码与图表在页边距内，无缺字、裁切、重叠、空白页或异常拉伸字距。源页 PNG 位于 render-bonus-final-v2/。

## 渲染工具记录

Canonical render_docx.py attempt resolved C:/Program Files/LibreOffice/program/soffice.exe despite a restricted launch PATH and produced an alternate 42-page reflow. This was not the bundled LibreOffice required by the document workflow; its output is excluded from the deliverable and final visual QA. Final PDF is the successful Microsoft Word 16.0 read-only export (35 pages), rendered using bundled Poppler at 120 dpi.

此工具记录仅说明渲染来源；最终提交的是经过 Word 打开、导出及逐页检查的 DOCX/PDF。
