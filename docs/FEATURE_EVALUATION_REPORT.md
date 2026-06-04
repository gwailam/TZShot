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

- 框选区域后按 `Enter` / `Return`，执行默认动作。
- 框选区域后点击工具栏中的复制按钮。

如果通过 `Alt+A` 或托盘截图进入截图浮层，默认动作是 `Copy`。用户框选后仍需要按回车或点击复制按钮完成复制。

### 2.2 调整目标

新增交互：

- 用户框选好截图区域后，双击选区内部，直接复制当前输出图到剪贴板，并关闭截图浮层。

其中“当前输出图”应保持与复制按钮一致：

- 如果没有标注，复制原始选区截图。
- 如果有标注，复制截图背景和标注层合成后的结果。

### 2.3 可行性结论

可行，且实现成本低。

原因：

- `CaptureOverlayWidget` 当前没有实现 `mouseDoubleClickEvent()`。
- 复制动作已经存在于 `performAction(CaptureAction::Copy)`。
- 选区判断已经存在于 `currentSelectionRect()` / `m_selection.hasSelection()`。
- 复制输出图逻辑已经统一在 `currentOutputImage()`。

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

- `src/widgets/capture_overlay_widget.h`
- `src/widgets/capture_overlay_widget.cpp`

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

- 普通截图，框选后双击选区内部可以复制并关闭浮层。
- 双击选区外不触发复制。
- 标注后双击选区内部复制的是带标注结果。
- 文字工具启用时，双击文字仍进入编辑，不触发复制。
- 保存模式、贴图模式下已有自动执行逻辑不受影响。
- `Esc`、`Enter`、复制按钮行为不变。

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

- 高质量。
- 均衡。
- 小体积。

### 3.2 可行性结论

移除可行。

从产品和工程角度看，移除 GIF 录制的收益大于保留，除非项目明确要把录屏/GIF 作为核心能力。

主要原因：

- GIF 不是截图、贴图、OCR、AI 工作流的必要环节。
- GIF 功能跨模块较多，增加维护成本。
- 当前实现对多屏、DPI、窗口遮挡、编码性能和跨平台兼容性都有潜在风险。
- README 中也说明 GIF、OCR、长截图仍有跨平台增强空间。
- 项目中存在旧版 `gif_recorder_view_model.*` 与 `stb_image_write.h`，但它们未接入 CMake，说明 GIF 相关代码已有历史遗留。

### 3.3 当前代码影响范围

#### 构建文件

- `CMakeLists.txt`
  - `src/widgets/gif_record_overlay_widget.cpp`
  - `src/widgets/gif_record_overlay_widget.h`
  - `src/viewmodel/gif_record_view_model.cpp`
  - `src/viewmodel/gif_record_view_model.h`
  - `src/gif_encoder/gif.h`
  - `src/gif_encoder/gif_encoder.cpp`
  - `src/gif_encoder/gif_encoder.h`
  - `src/gif_encoder/gif_frame_grabber.cpp`
  - `src/gif_encoder/gif_frame_grabber.h`
  - `src/gif_encoder` include path

#### 应用服务层

- `src/app/tzshot_services.h`
- `src/app/tzshot_services.cpp`
- `src/app/tzshot_widget_runtime.cpp`

涉及内容：

- `GifRecordViewModel` include。
- `GifRecordViewModel m_gifRecordViewModel`。
- `gifRecordViewModel()` accessor。
- `CaptureOverlayWidget` 构造参数。
- `SettingsDialog` 构造参数。

#### 截图浮层

- `src/widgets/capture_overlay_widget.h`
- `src/widgets/capture_overlay_widget.cpp`

涉及内容：

- `GifRecordViewModel` 前置声明和成员。
- `GifRecordOverlayWidget` 前置声明和成员。
- `CaptureAction::Gif`。
- `m_gifRecordingMode`。
- `m_gifGlobalCaptureRect`。
- `m_gifButton`。
- GIF 按钮创建、布局和 signal 连接。
- GIF overlay 创建和 signal 连接。
- GIF ViewModel 信号连接。
- 鼠标、键盘、工具参数显示中的 GIF 模式分支。
- `performAction(CaptureAction::Gif)` 分支。

#### 设置窗口

- `src/widgets/settings_dialog.h`
- `src/widgets/settings_dialog.cpp`

涉及内容：

- `GifRecordViewModel` 前置声明。
- 构造函数参数。
- `m_gifRecordViewModel`。
- `m_gifPresetCombo`。
- GIF 录制设置区。

#### 设置模型

- `src/model/app_settings.h`
- `src/model/app_settings.cpp`

涉及内容：

- `gifQualityPreset()`。
- `setGifQualityPreset()`。
- `m_gifQualityPreset`。
- `GIF/qualityPreset` 读写。

#### 资源

- `qml.qrc`
  - `resource/img/lc_gif_record.svg`
  - `resource/img/lc_gif_stop.svg`

可删除资源：

- `resource/img/lc_gif_record.svg`
- `resource/img/lc_gif_stop.svg`

#### GIF 专属源码

可删除：

- `src/widgets/gif_record_overlay_widget.cpp`
- `src/widgets/gif_record_overlay_widget.h`
- `src/viewmodel/gif_record_view_model.cpp`
- `src/viewmodel/gif_record_view_model.h`
- `src/gif_encoder/gif.h`
- `src/gif_encoder/gif_encoder.cpp`
- `src/gif_encoder/gif_encoder.h`
- `src/gif_encoder/gif_frame_grabber.cpp`
- `src/gif_encoder/gif_frame_grabber.h`

可选删除历史遗留：

- `src/viewmodel/gif_recorder_view_model.cpp`
- `src/viewmodel/gif_recorder_view_model.h`
- `src/viewmodel/stb_image_write.h`

#### 文档和图片

需要同步更新：

- `README.md`
- `README_CN.md`
- `PROJECT_ANALYSIS_REPORT.md`
- `INTERACTION_LOGIC_REPORT.md`

可选删除：

- `docs/images/gif-record.png`

### 3.4 移除方案

#### 方案 A：仅隐藏入口

做法：

- 删除或隐藏截图浮层中的 GIF 按钮。
- 删除设置页 GIF 质量预设。
- 保留底层 GIF 源码和 CMake 条目。

优点：

- 改动小。
- 回滚简单。
- 风险低。

缺点：

- 死代码仍然存在。
- 构建仍包含 GIF 相关模块。
- 维护成本没有完全消除。

适用场景：

- 只是暂时不对用户开放 GIF 功能。
- 后续可能恢复。

#### 方案 B：产品入口移除 + 构建保留

做法：

- 删除 UI 入口和设置项。
- 保留底层 ViewModel、encoder、overlay 代码，但不再实例化。

优点：

- 用户层面移除彻底。
- 回滚仍较容易。

缺点：

- 构建仍编译无用代码。
- 代码复杂度仍有残留。

适用场景：

- 需要快速降低用户界面复杂度，但不急于清理源码。

#### 方案 C：彻底移除

做法：

- 删除 UI 入口。
- 删除设置项。
- 删除服务层依赖。
- 删除 GIF ViewModel、overlay、grabber、encoder。
- 删除资源和文档说明。
- 清理旧版 GIF recorder 遗留代码。

优点：

- 降低代码体积和维护成本。
- 减少截图浮层状态分支。
- 减少跨平台风险面。
- 产品定位更聚焦。

缺点：

- 改动范围较大。
- 需要完整构建验证。
- 如果未来恢复 GIF，需要从 Git 历史找回。

适用场景：

- 明确不再把 GIF 录制作为产品功能。

### 3.5 推荐方案

推荐采用方案 C：彻底移除。

理由：

- GIF 录制不是项目核心功能。
- 代码耦合点较多，保留会持续增加维护负担。
- 当前核心能力更集中在截图、标注、贴图、OCR 和 AI。
- 移除后截图浮层工具栏更简洁，也更利于加入“双击选区复制”这种高频交互优化。

### 3.6 风险分析

#### 构建风险

中等。

原因是涉及多个 include、构造函数参数和 CMake 条目。漏删容易导致编译错误。

缓解方式：

- 逐模块删除。
- 删除后运行 CMake configure。
- 再运行完整 Release 构建。

#### UI 回归风险

中等偏低。

主要影响截图浮层工具栏按钮排列和设置页内容布局。

缓解方式：

- 检查截图浮层工具栏布局。
- 检查设置页通用配置区域。

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

- 项目可重新配置。
- 项目可完整构建。
- 截图浮层能正常打开。
- 工具栏无 GIF 按钮。
- 复制、保存、贴图、OCR、长截图按钮仍正常。
- `Esc` 关闭截图浮层正常。
- `Enter` 执行默认动作正常。
- 设置窗口无 GIF 配置项。
- README 不再提到 GIF 录制功能。

## 4. 两项调整的组合影响

双击复制和移除 GIF 可以一起做。

组合后交互会更聚焦：

- 普通截图复制路径更快。
- 工具栏按钮更少。
- 截图浮层状态分支减少。
- `Esc` 逻辑更简单，不再需要处理 GIF 录制取消。
- 标注、复制、保存、贴图、OCR、长截图成为主要截图后动作。

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

- 双击复制提升高频操作效率，改动小。
- 移除 GIF 降低维护成本和跨平台风险。
- 两者都能让产品更聚焦于截图、标注、贴图、OCR 和 AI。

