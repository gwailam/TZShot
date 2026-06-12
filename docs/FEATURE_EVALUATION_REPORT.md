# TZShot 功能评估报告

生成日期：2026-06-04

## 1. 评估范围

本报告评估两个功能调整：

1. 增加截图选区双击复制到剪贴板。
2. 移除 GIF 录制功能。

评估基于当前项目源码结构和现有交互逻辑，不引入外部产品假设。

## 2. 功能一：双击框选区域复制到剪贴板

### 2.1 当前交互

当前截图复制路径主要有两种：

* 框选区域后按 `Enter` / `Return`，执行默认动作。

* 框选区域后点击工具栏中的复制按钮。

如果通过 `Alt+A` 或托盘截图进入截图浮层，默认动作是 `Copy`。用户框选后仍需要按回车或点击复制按钮完成复制。

### 2.2 调整目标

新增交互：

* 用户框选好截图区域后，双击选区内部，直接复制当前输出图到剪贴板，并关闭截图浮层。

其中“当前输出图”应保持与复制按钮一致：

* 如果没有标注，复制原始选区截图。

* 如果有标注，复制截图背景和标注层合成后的结果。

### 2.3 可行性结论

可行，且实现成本低。

原因：

* `CaptureOverlayWidget` 当前没有实现 `mouseDoubleClickEvent()`。

* 复制动作已经存在于 `performAction(CaptureAction::Copy)`。

* 选区判断已经存在于 `currentSelectionRect()` / `m_selection.hasSelection()`。

* 复制输出图逻辑已经统一在 `currentOutputImage()`。

因此新增双击复制不需要新增截图、合成或剪贴板逻辑，只需要增加一个事件入口。

### 2.4 推荐交互规则

建议采用严格触发条件，避免和现有标注、文字编辑、GIF 录制冲突：

1. 仅响应鼠标左键双击。
2. 当前必须有有效选区。
3. 双击位置必须在选区内部。
4. 当前不能处于 GIF 录制模式。
5. 当前标注画布不能处于绘制启用状态。
6. 当前不能处于内联文字编辑状态。
7. 满足条件后执行 `performAction(CaptureAction::Copy)`。

### 2.5 冲突分析

#### 与回车复制的关系

不冲突。

双击复制只是增加更快入口，`Enter` / `Return` 仍保留。

#### 与复制按钮的关系

不冲突。

双击内部调用同一个复制动作，行为应与复制按钮一致。

#### 与选区移动的关系

低风险。

Qt 双击通常会产生 press/release/double-click 事件。需要在实现中只在 `mouseDoubleClickEvent()` 里处理有效选区内部双击，并接受事件。

#### 与文字标注编辑的关系

需要规避。

`StickyCanvasWidget` 中已有文字工具双击编辑文本的逻辑。如果标注画布处于绘制启用状态，双击事件不应被截图浮层抢走。

#### 与 GIF 录制的关系

如果后续保留 GIF，双击复制不应在 GIF 录制模式生效。

如果移除 GIF，此条件可以同步删除。

### 2.6 代码影响范围

主要文件：

* `src/widgets/capture_overlay_widget.h`

* `src/widgets/capture_overlay_widget.cpp`

建议修改：

在 `CaptureOverlayWidget` 中新增：

```cpp
void mouseDoubleClickEvent(QMouseEvent *event) override;
```

推荐实现逻辑：

```cpp
void CaptureOverlayWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    if (m_gifRecordingMode) {
        event->accept();
        return;
    }

    if (m_canvas && m_canvas->drawingEnabled()) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    if (m_inlineTextPanel && m_inlineTextPanel->isVisible()) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    const QRect selection = currentSelectionRect();
    if (!selection.isEmpty() && selection.contains(event->position().toPoint())) {
        event->accept();
        performAction(CaptureAction::Copy);
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}
```

如果后续移除 GIF，`m_gifRecordingMode` 判断可以一并删除。

### 2.7 验证建议

需要验证：

* 普通截图，框选后双击选区内部可以复制并关闭浮层。

* 双击选区外不触发复制。

* 标注后双击选区内部复制的是带标注结果。

* 文字工具启用时，双击文字仍进入编辑，不触发复制。

* 保存模式、贴图模式下已有自动执行逻辑不受影响。

* `Esc`、`Enter`、复制按钮行为不变。

### 2.8 建议结论

建议加入。

该功能能明显减少普通复制截图的操作步骤，改动范围小，风险可控。

## 3. 功能二：移除 GIF 录制功能

### 3.1 当前 GIF 功能概述

当前 GIF 录制功能从截图浮层启动：

1. 用户框选录制区域。
2. 点击“录制 GIF”按钮。
3. 截图浮层进入 GIF 录制模式。
4. 显示 `GifRecordOverlayWidget` 控制条。
5. `GifRecordViewModel` 使用 `GifFrameGrabber` 抓帧。
6. 停止后通过 `GifEncoder` 编码为 GIF。

设置页中还提供 GIF 体积质量预设：

* 高质量。

* 均衡。

* 小体积。

### 3.2 可行性结论

移除可行。

从产品和工程角度看，移除 GIF 录制的收益大于保留，除非项目明确要把录屏/GIF 作为核心能力。

主要原因：

* GIF 不是截图、贴图、OCR、AI 工作流的必要环节。

* GIF 功能跨模块较多，增加维护成本。

* 当前实现对多屏、DPI、窗口遮挡、编码性能和跨平台兼容性都有潜在风险。

* README 中也说明 GIF、OCR、长截图仍有跨平台增强空间。

* 项目中存在旧版 `gif_recorder_view_model.*` 与 `stb_image_write.h`，但它们未接入 CMake，说明 GIF 相关代码已有历史遗留。

### 3.3 当前代码影响范围

#### 构建文件

* `CMakeLists.txt`

  * `src/widgets/gif_record_overlay_widget.cpp`

  * `src/widgets/gif_record_overlay_widget.h`

  * `src/viewmodel/gif_record_view_model.cpp`

  * `src/viewmodel/gif_record_view_model.h`

  * `src/gif_encoder/gif.h`

  * `src/gif_encoder/gif_encoder.cpp`

  * `src/gif_encoder/gif_encoder.h`

  * `src/gif_encoder/gif_frame_grabber.cpp`

  * `src/gif_encoder/gif_frame_grabber.h`

  * `src/gif_encoder` include path

#### 应用服务层

* `src/app/tzshot_services.h`

* `src/app/tzshot_services.cpp`

* `src/app/tzshot_widget_runtime.cpp`

涉及内容：

* `GifRecordViewModel` include。

* `GifRecordViewModel m_gifRecordViewModel`。

* `gifRecordViewModel()` accessor。

* `CaptureOverlayWidget` 构造参数。

* `SettingsDialog` 构造参数。

#### 截图浮层

* `src/widgets/capture_overlay_widget.h`

* `src/widgets/capture_overlay_widget.cpp`

涉及内容：

* `GifRecordViewModel` 前置声明和成员。

* `GifRecordOverlayWidget` 前置声明和成员。

* `CaptureAction::Gif`。

* `m_gifRecordingMode`。

* `m_gifGlobalCaptureRect`。

* `m_gifButton`。

* GIF 按钮创建、布局和 signal 连接。

* GIF overlay 创建和 signal 连接。

* GIF ViewModel 信号连接。

* 鼠标、键盘、工具参数显示中的 GIF 模式分支。

* `performAction(CaptureAction::Gif)` 分支。

#### 设置窗口

* `src/widgets/settings_dialog.h`

* `src/widgets/settings_dialog.cpp`

涉及内容：

* `GifRecordViewModel` 前置声明。

* 构造函数参数。

* `m_gifRecordViewModel`。

* `m_gifPresetCombo`。

* GIF 录制设置区。

#### 设置模型

* `src/model/app_settings.h`

* `src/model/app_settings.cpp`

涉及内容：

* `gifQualityPreset()`。

* `setGifQualityPreset()`。

* `m_gifQualityPreset`。

* `GIF/qualityPreset` 读写。

#### 资源

* `qml.qrc`

  * `resource/img/lc_gif_record.svg`

  * `resource/img/lc_gif_stop.svg`

可删除资源：

* `resource/img/lc_gif_record.svg`

* `resource/img/lc_gif_stop.svg`

#### GIF 专属源码

可删除：

* `src/widgets/gif_record_overlay_widget.cpp`

* `src/widgets/gif_record_overlay_widget.h`

* `src/viewmodel/gif_record_view_model.cpp`

* `src/viewmodel/gif_record_view_model.h`

* `src/gif_encoder/gif.h`

* `src/gif_encoder/gif_encoder.cpp`

* `src/gif_encoder/gif_encoder.h`

* `src/gif_encoder/gif_frame_grabber.cpp`

* `src/gif_encoder/gif_frame_grabber.h`

可选删除历史遗留：

* `src/viewmodel/gif_recorder_view_model.cpp`

* `src/viewmodel/gif_recorder_view_model.h`

* `src/viewmodel/stb_image_write.h`

#### 文档和图片

需要同步更新：

* `README.md`

* `README_CN.md`

* `PROJECT_ANALYSIS_REPORT.md`

* `INTERACTION_LOGIC_REPORT.md`

可选删除：

* `docs/images/gif-record.png`

### 3.4 移除方案

#### 方案 A：仅隐藏入口

做法：

* 删除或隐藏截图浮层中的 GIF 按钮。

* 删除设置页 GIF 质量预设。

* 保留底层 GIF 源码和 CMake 条目。

优点：

* 改动小。

* 回滚简单。

* 风险低。

缺点：

* 死代码仍然存在。

* 构建仍包含 GIF 相关模块。

* 维护成本没有完全消除。

适用场景：

* 只是暂时不对用户开放 GIF 功能。

* 后续可能恢复。

#### 方案 B：产品入口移除 + 构建保留

做法：

* 删除 UI 入口和设置项。

* 保留底层 ViewModel、encoder、overlay 代码，但不再实例化。

优点：

* 用户层面移除彻底。

* 回滚仍较容易。

缺点：

* 构建仍编译无用代码。

* 代码复杂度仍有残留。

适用场景：

* 需要快速降低用户界面复杂度，但不急于清理源码。

#### 方案 C：彻底移除

做法：

* 删除 UI 入口。

* 删除设置项。

* 删除服务层依赖。

* 删除 GIF ViewModel、overlay、grabber、encoder。

* 删除资源和文档说明。

* 清理旧版 GIF recorder 遗留代码。

优点：

* 降低代码体积和维护成本。

* 减少截图浮层状态分支。

* 减少跨平台风险面。

* 产品定位更聚焦。

缺点：

* 改动范围较大。

* 需要完整构建验证。

* 如果未来恢复 GIF，需要从 Git 历史找回。

适用场景：

* 明确不再把 GIF 录制作为产品功能。

### 3.5 推荐方案

推荐采用方案 C：彻底移除。

理由：

* GIF 录制不是项目核心功能。

* 代码耦合点较多，保留会持续增加维护负担。

* 当前核心能力更集中在截图、标注、贴图、OCR 和 AI。

* 移除后截图浮层工具栏更简洁，也更利于加入“双击选区复制”这种高频交互优化。

### 3.6 风险分析

#### 构建风险

中等。

原因是涉及多个 include、构造函数参数和 CMake 条目。漏删容易导致编译错误。

缓解方式：

* 逐模块删除。

* 删除后运行 CMake configure。

* 再运行完整 Release 构建。

#### UI 回归风险

中等偏低。

主要影响截图浮层工具栏按钮排列和设置页内容布局。

缓解方式：

* 检查截图浮层工具栏布局。

* 检查设置页通用配置区域。

#### 配置兼容风险

低。

旧用户 `QSettings` 中可能残留 `GIF/qualityPreset`，删除代码后该键不会再被读取，不影响运行。

#### 文档一致性风险

中等。

README 和截图文档中目前明确宣传 GIF 录制，需要同步删除，否则产品说明会失真。

### 3.7 建议实施步骤

1. 删除截图浮层 GIF 按钮和动作分支。
2. 删除 `CaptureOverlayWidget` 中 GIF 状态、overlay、ViewModel 依赖。
3. 删除服务层中的 `GifRecordViewModel`。
4. 删除设置窗口 GIF 参数。
5. 删除 `AppSettings` 中 GIF 质量预设。
6. 从 CMake 删除 GIF 源码和 include 路径。
7. 从 qrc 删除 GIF 图标资源。
8. 删除 GIF 专属源码和资源文件。
9. 删除旧版未接入的 GIF recorder 遗留文件。
10. 更新 README 和报告文档。
11. 运行 CMake configure。
12. 运行完整构建。

### 3.8 验证建议

需要验证：

* 项目可重新配置。

* 项目可完整构建。

* 截图浮层能正常打开。

* 工具栏无 GIF 按钮。

* 复制、保存、贴图、OCR、长截图按钮仍正常。

* `Esc` 关闭截图浮层正常。

* `Enter` 执行默认动作正常。

* 设置窗口无 GIF 配置项。

* README 不再提到 GIF 录制功能。

## 4. 两项调整的组合影响

双击复制和移除 GIF 可以一起做。

组合后交互会更聚焦：

* 普通截图复制路径更快。

* 工具栏按钮更少。

* 截图浮层状态分支减少。

* `Esc` 逻辑更简单，不再需要处理 GIF 录制取消。

* 标注、复制、保存、贴图、OCR、长截图成为主要截图后动作。

建议顺序：

1. 先移除 GIF。
2. 再增加双击选区复制。
3. 最后统一回归截图浮层交互。

这样可以避免双击复制实现中还要兼容即将删除的 `m_gifRecordingMode` 分支。

## 5. 总体建议

建议执行：

1. 增加“双击选区内部复制到剪贴板”。
2. 彻底移除 GIF 录制功能。

理由：

* 双击复制提升高频操作效率，改动小。

* 移除 GIF 降低维护成本和跨平台风险。

* 两者都能让产品更聚焦于截图、标注、贴图、OCR 和 AI。

## 6. 功能三：标注工具移动与编辑能力

### 6.1 当前问题

当前截图浮层和贴图标注中，画笔、矩形、圆形、箭头等标注工具在创建完成后基本进入静态结果状态。

已存在能力：

* 文字标注支持选中、拖动和双击编辑。

* 标注图形支持撤销。

* 当前工具栏可以调整后续新建图形的颜色和粗细。

缺失能力：

* 画笔、矩形、圆形、箭头创建后不能再次选中。

* 已创建的普通图形不能移动。

* 已创建的普通图形不能修改颜色和粗细。

* 矩形、圆形、箭头不能通过控制点二次调整尺寸或端点。

* 画笔路径不能整体移动或编辑。

### 6.2 代码现状

相关代码集中在：

* `src/widgets/sticky_canvas_widget.h`

* `src/widgets/sticky_canvas_widget.cpp`

* `src/paint_board/shape/shape.h`

* `src/paint_board/shape/two_point_shape.h`

* `src/paint_board/shape/pen_shape.h`

* `src/paint_board/shape/rect_shape.*`

* `src/paint_board/shape/ellipse_shape.*`

* `src/paint_board/shape/arrow_shape.*`

* `src/paint_board/shape/text_shape.*`

* `src/paint_board/shape/number_shape.*`

当前 `Shape` 基类只提供：

* `draw(QPainter*)`

* `setEndPoint(const QPoint&)`

* `setColor(const QColor&)`

* `setSize(int)`

它没有提供通用的：

* 命中测试。

* 包围盒。

* 平移。

* 控制点编辑。

* 选中状态绘制。

因此，普通图形无法像文字标注一样被再次命中和操作。

### 6.3 可行性结论

可行，但不建议一次性实现完整矢量编辑器能力。

推荐先实现“选中 + 整体移动 + 属性编辑”，再实现“两点图形控制点编辑”。

原因：

* 矩形、圆形、箭头继承 `TwoPointShape`，天然具备起点和终点，适合移动、缩放、端点编辑。

* 画笔是点路径，整体移动和属性编辑可控，但逐点编辑成本高。

* 文字标注已有选中和移动模式，可作为交互参考。

* 现有 `StickyCanvasWidget` 鼠标事件已经承担创建图形逻辑，需要谨慎引入编辑模式，避免与新建图形冲突。

### 6.4 推荐交互规则

建议在标注画布启用时，支持以下规则：

1. 单击已有图形命中区域，选中该图形。
2. 选中图形后拖动主体区域，整体移动图形。
3. 选中图形后切换颜色或粗细，应用到当前选中图形。
4. 点击空白区域，清除当前选中图形，并按当前工具逻辑开始新建图形。
5. `Delete` / `Backspace` 删除当前选中图形。
6. `Ctrl+Z` 继续执行撤销，优先级高于选中态。
7. 文字标注保持现有双击编辑逻辑。
8. 画笔第一阶段只支持整体移动和属性编辑，不做逐点编辑。
9. 矩形、圆形、箭头第二阶段再支持控制点调整。

### 6.5 分阶段实施计划

#### 阶段一：选中、移动、属性编辑

目标：

* 支持选中已有普通图形。

* 支持整体拖动移动。

* 支持修改已选图形颜色和粗细。

* 支持删除已选图形。

涉及图形：

* 画笔。

* 高亮。

* 矩形。

* 圆形。

* 箭头。

* 序号。

* 文字。

建议新增 `Shape` 接口：

```cpp
virtual QRect boundingRect() const = 0;
virtual bool contains(const QPoint &point, int tolerance) const = 0;
virtual void translate(const QPoint &offset) = 0;
```

`StickyCanvasWidget` 建议新增状态：

```cpp
int m_selectedShapeIndex = -1;
bool m_draggingSelectedShape = false;
QPoint m_selectedShapePressPoint;
QPoint m_selectedShapeLastPoint;
```

阶段一不做缩放控制点，只绘制选中虚线框或高亮边界。

#### 阶段二：两点图形控制点编辑

目标：

* 矩形和圆形支持 8 个控制点缩放。

* 箭头支持起点、终点两个控制点编辑。

适用图形：

* `RectShape`

* `EllipseShape`

* `ArrowShape`

建议新增：

```cpp
enum class ShapeHandle {
    None,
    Move,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    Start,
    End
};
```

`TwoPointShape` 可以实现默认控制点编辑能力，`ArrowShape` 覆盖为起点/终点编辑。

#### 阶段三：路径类图形增强

目标：

* 画笔、高亮、马赛克、模糊支持整体移动。

* 可选支持路径包围盒显示。

不建议第一版做：

* 逐点编辑。

* 路径局部变形。

* 画笔路径平滑重算。

原因：

* 交互复杂度高。

* 命中测试和控制点密度容易干扰普通绘制。

* 对截图工具而言收益不如整体移动和属性编辑明显。

### 6.6 各图形实现要点

#### 矩形

命中规则：

* 边框附近命中。

* 允许一定 `tolerance`，建议不小于当前线宽和 6px 的最大值。

移动规则：

* 同时平移 `m_startPoint` 和 `m_endPoint`。

编辑规则：

* 阶段二通过 8 个控制点更新起点/终点。

#### 圆形

命中规则：

* 阶段一可以先使用包围盒命中。

* 更精细的实现可判断点到椭圆边界的距离。

移动规则：

* 同时平移 `m_startPoint` 和 `m_endPoint`。

编辑规则：

* 与矩形一致，控制点调整包围盒。

#### 箭头

命中规则：

* 判断点到线段距离。

* 箭头头部可用包围盒或三角形区域近似命中。

移动规则：

* 同时平移起点和终点。

编辑规则：

* 阶段二拖动起点或终点。

#### 画笔/高亮

命中规则：

* 点到路径线段距离小于容差。

移动规则：

* 平移所有路径点。

编辑规则：

* 第一版只支持整体移动、颜色、粗细。

#### 序号

命中规则：

* 圆形或文字包围区域命中。

移动规则：

* 平移中心点。

编辑规则：

* 第一版支持移动、颜色、粗细。

* 是否修改数字内容可后续评估。

#### 文字

已有能力：

* 命中。

* 选中。

* 拖动。

* 双击编辑。

建议：

* 后续将文字选中逻辑统一到通用 `Shape` 选择框架，减少特殊分支。

* 迁移时保持现有双击编辑行为不变。

### 6.7 风险分析

#### 交互冲突风险

中等。

原因：

* 当前左键按下即开始新建图形。

* 加入选中后，需要先判断是否命中已有图形，再决定是移动还是新建。

缓解方式：

* 只在当前工具处于标注画布启用状态时处理图形编辑。

* 命中已有图形优先于新建图形。

* 空白点击才进入新建。

#### 代码结构风险

中等。

原因：

* 当前 `Shape` 抽象过薄。

* 每种图形的几何数据结构不同。

缓解方式：

* 先扩展基类通用能力。

* 让 `TwoPointShape` 承担两点图形的默认实现。

* 路径类图形单独实现命中和平移。

#### 输出一致性风险

低。

原因：

* 最终导出仍然通过 `draw()` 合成。

* 移动和属性编辑只是修改图形内部数据。

需要验证：

* 复制、保存、贴图结果与画布显示一致。

#### 性能风险

低到中等。

原因：

* 标注图形数量通常有限。

* 命中测试需要从顶层往底层遍历。

缓解方式：

* 从 `m_shapes` 尾部向前命中，优先选择最新图形。

* 暂不做复杂路径逐点编辑。

### 6.8 验证建议

阶段一需要验证：

* 矩形、圆形、箭头、画笔创建后可以被选中。

* 选中图形可以整体移动。

* 选中图形可以修改颜色。

* 选中图形可以修改粗细。

* `Delete` / `Backspace` 可以删除选中图形。

* 点击空白区域后可以继续新建图形。

* 文字标注拖动和双击编辑不回退。

* `Ctrl+Z` 撤销不回退。

* 复制、保存、贴图结果包含移动后的标注。

阶段二需要验证：

* 矩形 8 个控制点可调整尺寸。

* 圆形 8 个控制点可调整尺寸。

* 箭头起点和终点可分别拖动。

* 控制点命中不影响主体移动。

* 小尺寸图形控制点仍可用。

### 6.9 推荐结论

建议实施，但分两轮做：

1. 先做通用选中、整体移动、删除、颜色和粗细编辑。
2. 再做矩形、圆形、箭头控制点编辑。

不建议第一轮实现画笔逐点编辑。对于截图标注工具，整体移动和属性编辑已经能解决主要痛点，逐点编辑会显著增加交互复杂度和实现风险。
