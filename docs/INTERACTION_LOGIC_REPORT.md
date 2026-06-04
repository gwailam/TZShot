# TZShot 交互逻辑规则报告

生成日期：2026-06-04

## 1. 交互入口

项目的主交互最终都汇聚到截图浮层 `CaptureOverlayWidget`。

### 1.1 程序启动入口

入口文件 `src/main.cpp` 创建 `QApplication` 后交给 `TZShotApp` 初始化。

`TZShotApp` 初始化后会连接以下交互入口：

- 单实例激活。
- 系统托盘。
- 全局快捷键。
- 设置窗口。
- OCR 结果窗口。

### 1.2 单实例激活规则

`InstanceActivation` 使用 `QLocalServer` 和 `QLocalSocket` 实现单实例。

规则：

1. 新进程启动时尝试连接已有实例。
2. 如果连接成功，发送 `ACTIVATE` 后退出。
3. 主实例收到激活请求后打开截图浮层。
4. 激活默认模式是 `copy`。

### 1.3 托盘入口规则

`TrayIconHelper` 提供系统托盘交互。

规则：

- 托盘单击：打开截图浮层，默认 `copy`。
- 托盘双击：打开截图浮层，默认 `copy`。
- 托盘右键：显示菜单。
- 菜单“截图”：打开截图浮层，默认 `copy`。
- 菜单“设置”：打开设置窗口。
- 菜单“关于”：打开关于窗口。
- 菜单“退出”：退出应用。

### 1.4 全局快捷键规则

`GlobalShortcut` 默认提供四个动作：

| 快捷键 | 动作 | 截图浮层默认模式 |
| --- | --- | --- |
| `Alt+A` | 截图 | `copy` |
| `Alt+S` | 截图并保存 | `save` |
| `Alt+P` | 贴图到桌面 | `sticky` |
| `Alt+Q` | 显示或隐藏截图浮层 | `copy` |

Windows 使用 `RegisterHotKey` 实现。Linux/Unix 使用 X11/xcb grab key 和 pipe 分发事件。

## 2. 截图浮层交互

核心类：`CaptureOverlayWidget`。

截图浮层承担以下职责：

- 捕获桌面快照。
- 覆盖虚拟桌面。
- 管理截图选区。
- 管理标注画布。
- 管理工具栏和工具参数面板。
- 管理放大镜。
- 执行复制、保存、贴图、OCR、长截图、GIF 等动作。

### 2.1 打开浮层规则

调用 `showAndActivate(mode)` 时：

1. 根据 `mode` 设置默认动作。
2. 调用 `refreshSnapshot()` 抓取桌面快照。
3. 重置内部状态。
4. 覆盖整个虚拟桌面区域。
5. 显示并激活浮层窗口。

支持的模式：

- `copy`
- `save`
- `sticky`
- `ocr`

未匹配到的模式默认按 `copy` 处理。

### 2.2 显示/隐藏规则

调用 `toggleCapture(mode)` 时：

- 如果浮层已经可见，则执行 `finishCapture()` 关闭。
- 如果浮层不可见，则调用 `showAndActivate(mode)` 打开。

### 2.3 鼠标按下规则

`mousePressEvent()` 的核心规则：

- 只有左键参与选区交互。
- 如果当前处于 GIF 录制模式，普通选区交互被限制。
- 如果点击工具栏、工具参数面板、文本编辑器等子控件，事件交给子控件处理。
- 如果点击选区：
  - 命中内部区域则进入移动模式。
  - 命中边缘或控制点则进入缩放模式。
- 如果点击空白区域，则开始新建选区。

### 2.4 鼠标移动规则

`mouseMoveEvent()` 的核心规则：

- 拖拽中：更新选区位置或大小。
- 非拖拽中：更新 hover 命中状态和鼠标光标。
- 有截图快照时：更新放大镜。
- 有选区时：同步工具栏位置和标注画布区域。

### 2.5 鼠标释放规则

`mouseReleaseEvent()` 的核心规则：

- 结束当前选区拖拽状态。
- 如果选区有效，显示工具栏和标注画布。
- 如果选区太小，则清空选区。
- 根据释放时的上下文决定是否触发默认动作。

### 2.6 键盘规则

`keyPressEvent()` 的核心规则：

- `Esc`：
  - 普通截图模式下关闭截图浮层。
  - GIF 录制模式下先取消 GIF 录制，再关闭浮层。
- `Enter` / `Return`：
  - 如果当前有有效选区，执行默认动作。
- 其他按键继续交给 QWidget 默认处理。

## 3. 选区规则

核心类：`SelectionMaskController`。

### 3.1 状态机

选区交互状态：

- `None`
- `Selecting`
- `Moving`
- `ResizeTopLeft`
- `ResizeTop`
- `ResizeTopRight`
- `ResizeRight`
- `ResizeBottomRight`
- `ResizeBottom`
- `ResizeBottomLeft`
- `ResizeLeft`

### 3.2 新建选区规则

如果当前没有有效选区，或者鼠标按下位置没有命中已有选区，则进入 `Selecting`。

规则：

1. 记录鼠标按下点。
2. 鼠标移动时用按下点和当前位置生成矩形。
3. 矩形会被限制在浮层边界内。
4. 鼠标释放时宽或高小于 2 像素则丢弃。

### 3.3 移动选区规则

如果鼠标按下点位于已有选区内部，则进入 `Moving`。

规则：

- 选区跟随鼠标偏移移动。
- 移动结果不会超出浮层边界。
- 鼠标释放后结束移动。

### 3.4 缩放选区规则

如果鼠标按下点命中选区边缘或控制点，则进入对应缩放模式。

规则：

- 支持八方向缩放。
- 缩放时会更新对应边或角。
- 缩放结果会 normalized。
- 缩放结果不会超出浮层边界。

### 3.5 Hover 和光标规则

`updateHover()` 根据鼠标位置进行 hit-test。

光标规则：

- 移动区域：`SizeAllCursor`。
- 左上/右下缩放：`SizeFDiagCursor`。
- 右上/左下缩放：`SizeBDiagCursor`。
- 上/下缩放：`SizeVerCursor`。
- 左/右缩放：`SizeHorCursor`。
- 空白或新建选区：`CrossCursor`。

## 4. 工具栏动作规则

截图浮层工具栏动作最终进入 `CaptureOverlayWidget::performAction()`。

### 4.1 输出图规则

动作使用的输出图不是单纯桌面截图，而是：

1. 选区背景截图。
2. 当前标注画布内容。
3. 两者合成后的图片。

如果没有标注，则输出原始选区截图。

### 4.2 复制规则

动作：`Copy`

规则：

1. 获取当前输出图。
2. 写入系统剪贴板。
3. 关闭截图浮层。

Linux 下还会写入 Selection 剪贴板。

### 4.3 保存规则

动作：`Save`

规则：

1. 获取当前输出图。
2. 生成默认保存路径。
3. 保存为图片文件。
4. 关闭截图浮层。

默认保存目录来自 `AppSettings::savePath()`。

### 4.4 贴图规则

动作：`Sticky`

规则：

1. 获取当前输出图。
2. 存入 `StickyImageStore`。
3. 获得 `image://sticky/<uuid>`。
4. 调用 `StickyViewModel::requestSticky()`。
5. 创建贴图窗口。
6. 关闭截图浮层。

### 4.5 OCR 规则

动作：`Ocr`

规则：

1. 获取当前输出图。
2. 调用 `OcrViewModel::recognize(image)`。
3. 关闭截图浮层。
4. OCR 识别结果稍后通过信号显示到 OCR 结果窗口。

### 4.6 长截图规则

动作：`LongCapture`

规则：

1. 获取当前全局选区矩形。
2. 通过 `WidgetWindowBridge::requestLongCapture()` 进入长截图控制器。
3. 关闭截图浮层。

长截图不使用当前标注层。

### 4.7 GIF 规则

动作：`Gif`

规则：

1. 获取当前全局选区矩形。
2. 进入 GIF 录制模式。
3. 显示 GIF 录制控制浮层。
4. 调用 `GifRecordViewModel::startRecording()`。
5. 截图浮层暂不关闭。
6. 录制停止、取消、编码完成或编码失败后关闭浮层。

## 5. 标注交互规则

核心类：`StickyCanvasWidget`。

截图浮层和贴图窗口都复用该标注画布。

### 5.1 绘制开关规则

`setDrawingEnabled(true)` 后画布响应鼠标事件。

`setDrawingEnabled(false)` 后：

- 结束当前未完成 Shape。
- 清空选中文字状态。
- 画布不再接收鼠标绘制事件。

### 5.2 Shape 列表规则

画布维护：

- 已完成图形列表：`m_shapes`
- 当前绘制图形：`m_currentShape`

绘制时：

1. 鼠标按下创建图形。
2. 鼠标移动更新图形。
3. 鼠标释放将图形加入 `m_shapes`。

撤销时删除 `m_shapes` 最后一个元素。

### 5.3 普通图形规则

适用于：

- 画笔。
- 矩形。
- 椭圆。
- 箭头。

规则：

- 鼠标按下：创建图形。
- 鼠标移动：更新终点。
- 鼠标释放：确认图形。

### 5.4 马赛克和模糊规则

适用于：

- 马赛克。
- 模糊。

规则：

- 创建时记录起点。
- 使用当前背景图作为处理源。
- 笔触大小根据当前 pen size 换算。
- 鼠标释放后入栈。

### 5.5 文字规则

文字工具规则：

- 点击空白处：发出 `textPlacementRequested`，外层显示内联文本编辑器。
- 双击已有文字：删除原文字 Shape，并发出 `textEditRequested` 进入编辑。
- 单击已有文字：选中文字。
- 拖动已选文字：移动文字位置。
- 文本为空时不创建文字 Shape。

### 5.6 序号规则

序号工具规则：

- 点击时立即创建序号 Shape。
- 如果开启自动递增，创建后序号加一。
- 序号变化通过 `numberValueChanged` 通知外层 UI。

### 5.7 合成输出规则

`compositedImage()` 输出：

1. 创建透明 ARGB 图片。
2. 绘制背景图。
3. 按顺序绘制全部 Shape。
4. 绘制当前未完成 Shape。

截图复制、保存、贴图、OCR 使用合成结果。

## 6. 贴图窗口交互规则

核心类：`StickyPinWidget`。

### 6.1 创建规则

贴图创建流程：

1. `StickyImageStore` 存图。
2. 返回 `image://sticky/<uuid>`。
3. `StickyViewModel::requestSticky()` 根据 URL 取图。
4. Windows 下直接创建 `StickyPinWidget`。

### 6.2 生命周期规则

贴图窗口关闭时：

- 调用 `releaseStoredImage()`。
- 释放 `StickyImageStore` 中的对应图片。
- 防止重复释放。

析构时也会再次尝试释放，但有标志位保护。

### 6.3 工具栏规则

贴图窗口工具栏支持：

- 画笔。
- 矩形。
- 椭圆。
- 箭头。
- 高亮。
- 马赛克。
- 模糊。
- 文字。
- 序号。
- 撤销。
- AI 编辑。
- 视觉理解。
- 复制。
- 保存。
- 工具栏显隐。
- 关闭。

### 6.4 右键菜单规则

右键菜单提供：

- 复制。
- 保存。
- AI 编辑。
- 视觉理解。
- 关闭。

如果缺少对应 ViewModel，相关动作不可用或不会执行。

### 6.5 键盘规则

贴图窗口中：

- `Esc` 关闭窗口。
- 其他按键走 QWidget 默认处理。

### 6.6 滚轮规则

贴图窗口滚轮交互：

- 普通滚轮：调整缩放比例。
- `Alt + 滚轮`：调整透明度。

缩放规则：

- 向上滚动放大。
- 向下滚动缩小。
- 缩放会同步调整画布 view scale。
- 缩放后重新计算窗口布局和大小。

透明度规则：

- 透明度被限制在 `0.05` 到 `1.0`。
- 调整后同步到标注画布内容透明度。

### 6.7 保存规则

贴图保存时：

1. 合成当前图片和标注层。
2. 弹出或使用目标路径。
3. 保存到本地文件。

### 6.8 复制规则

复制时通过 `StickyImageStore::copyImageToClipboard()` 将当前图片写入剪贴板。

注意：复制规则主要复制缓存图片；保存规则会考虑标注合成。

## 7. OCR 交互规则

核心类：

- `OcrViewModel`
- `OcrEngine`
- `OcrResultWidget`

### 7.1 发起规则

OCR 可从截图浮层或贴图相关入口发起。

规则：

1. 输入图片不能为空。
2. 如果已经在识别中，新的识别请求会被忽略。
3. 有效图片会复制一份传入子线程。

### 7.2 异步规则

`OcrViewModel` 使用 `QtConcurrent::run`。

子线程流程：

1. 创建 `OcrEngine`。
2. 检查引擎是否 ready。
3. 调用同步识别。
4. 返回识别结果或错误标记。

主线程流程：

1. `QFutureWatcher` 收到 finished。
2. 清除识别中状态。
3. 如果失败，发出 `recognizeFailed`。
4. 如果成功，更新 `resultText` 并发出 `resultReady`。

### 7.3 结果显示规则

`TZShotApp` 监听 OCR 状态：

- 识别开始时显示 OCR 结果窗口。
- 识别完成时显示并激活 OCR 结果窗口。
- 识别失败时通过托盘消息提示。

## 8. 长截图交互规则

核心类：

- `ScrollCaptureViewModel`
- `ScrollStitcher`
- `LongCaptureController`

### 8.1 启动规则

长截图从截图浮层选区启动。

规则：

1. 选区宽高必须至少 20 像素。
2. 抓取第一帧作为 base frame。
3. 第一帧抓取失败则启动失败。
4. 启动成功后开始定时采样。

### 8.2 采样规则

采样定时器间隔为 90ms。

每次 tick：

1. 重新抓取当前选区。
2. 将图片转灰度缩略探针。
3. 与上一探针比较平均差异。
4. 差异低于阈值时认为没有有效滚动。
5. 差异达到阈值时进入拼接。

### 8.3 拼接规则

`ScrollStitcher` 使用上一帧和当前帧进行重叠匹配。

规则：

- 搜索重叠范围为图像高度的 20% 到 85%。
- 使用灰度差平均值作为匹配分数。
- 最佳分数大于阈值时认为匹配失败。
- 新增高度小于 8 像素时不追加。
- 匹配成功则把当前帧非重叠区域追加到结果图底部。

### 8.4 停止规则

用户停止长截图时：

- 如果结果为空，失败。
- 如果结果高度没有明显超过原选区，认为未检测到有效滚动。
- 成功时：
  - 复制结果到剪贴板。
  - 保存为 `long_yyyyMMdd_HHmmss.png`。
  - 打开保存目录。
  - 发出 `captureSucceeded`。

## 9. GIF 交互规则

核心类：

- `GifRecordViewModel`
- `GifFrameGrabber`
- `GifEncoder`
- `GifRecordOverlayWidget`

### 9.1 状态机

GIF 录制状态：

```text
Idle -> Recording -> Encoding -> Idle
Idle -> Recording -> Cancel -> Idle
```

### 9.2 开始录制规则

调用 `startRecording(x, y, w, h)` 时：

1. 如果已经在录制或编码，忽略。
2. 标准化录制区域。
3. 区域为空则失败。
4. 清空旧帧。
5. 设置录制状态。
6. 启动抓帧器。
7. 启动秒计时器。

抓帧帧率固定为 15 FPS。

### 9.3 录制中规则

录制中：

- 每帧由 `GifFrameGrabber` 抓取当前屏幕区域。
- 抓到帧后追加到 `m_frames`。
- 发出帧数变化信号。
- 达到 450 帧时自动停止。

### 9.4 更新录制区域规则

录制中允许调用 `updateCaptureRect()`。

规则：

- 新区域为空则忽略。
- 新区域从下一帧开始生效。

### 9.5 停止规则

调用 `stopRecording()`：

1. 停止抓帧。
2. 停止秒计时器。
3. 退出录制状态。
4. 如果没有帧，发出失败。
5. 如果有帧，进入编码。

### 9.6 取消规则

调用 `cancelRecording()`：

- 停止抓帧。
- 停止秒计时器。
- 清空状态和帧。
- 回到 Idle。

### 9.7 编码规则

编码在子线程执行。

质量预设：

| 预设 | 规则 |
| --- | --- |
| 高质量 | 不抽帧，不缩放，15 FPS |
| 均衡 | 每 2 帧取 1 帧，缩放 0.85，12 FPS |
| 小体积 | 每 3 帧取 1 帧，缩放 0.7，10 FPS |

编码成功：

- 发出 `encodingFinished`。
- 打开保存目录。
- 清空帧。

编码失败：

- 发出 `encodingFailed`。
- 清空帧。

截图浮层监听编码完成或失败后关闭。

## 10. AI 图像编辑交互规则

核心类：

- `AIViewModel`
- `QwenAICall`
- `SeedreamAiCall`
- `StickyPinWidget`

### 10.1 发起规则

AI 编辑从贴图窗口发起。

规则：

1. 用户打开 AI 编辑对话。
2. 输入 prompt。
3. prompt 不能为空。
4. 如果已有 AI 请求正在进行，新的请求会被忽略。
5. 当前贴图从 `StickyImageStore` 读取。
6. 图片编码为 PNG Base64 Data URI。

### 10.2 模型选择规则

`AIViewModel` 根据 `selectedModel` 创建调用对象：

- `0`：`QwenAICall`
- `1`：`SeedreamAiCall`

切换模型或 API Key 时会重建 AI call 对象，并取消旧请求。

### 10.3 请求完成规则

AI 接口成功响应后通常返回新图片 URL。

后续流程：

1. 下载新图片。
2. 解码为 `QImage`。
3. 保留原图片 DPR。
4. 存入 `StickyImageStore`。
5. 发出 `signalRequestComplete(oldImageUrl, newImageUrl)`。

### 10.4 贴图替换规则

贴图窗口收到 AI 完成信号后：

- 如果 `oldImageUrl` 与当前窗口图片 URL 不一致，则忽略。
- 如果一致，则读取新图并替换显示。
- 释放旧图。
- 更新布局。

## 11. AI 视觉理解交互规则

核心类：

- `VisionViewModel`
- `DoubaoVisionCall`
- `VisionResultWidget`

### 11.1 发起规则

视觉理解从贴图窗口发起。

规则：

1. 用户打开视觉理解窗口。
2. 输入 prompt。
3. prompt 不能为空。
4. 如果已有视觉分析任务正在进行，新的请求失败。
5. 当前贴图从 `StickyImageStore` 读取。
6. 图片最长边限制到 1600。
7. 图片编码为 PNG Data URL。

### 11.2 Provider 和模型规则

Provider：

- `0`：火山方舟。
- `1`：阿里 DashScope。

联网搜索启用时使用 Responses API，并可启用工具。

默认模型根据 provider 和联网搜索开关变化。

### 11.3 流式输出规则

`DoubaoVisionCall` 支持 SSE 风格流式响应。

规则：

- readyRead 时读取数据块。
- 按事件块解析。
- 提取 delta 文本。
- 发出 `analysisDelta`。
- 响应完成后发出 `analysisSucceeded`。
- 错误时发出 `analysisFailed`。

## 12. 设置窗口交互规则

核心类：`SettingsDialog`。

设置窗口分为通用配置和快捷键配置。

### 12.1 AI 设置规则

规则：

- 模型下拉框变化后更新 AI 模型。
- API Key 点击保存后写入 AI 和 Vision ViewModel。
- Vision provider 变化后刷新默认模型列表。
- Vision 模型输入变化后写入设置。
- 联网搜索开关变化后更新 Vision 设置。
- 代理设置变化后立即同步到 Vision call。

### 12.2 保存路径规则

规则：

- 显示当前保存路径。
- 用户点击浏览后选择目录。
- 选择成功后写入 `StorageViewModel` 和 `AppSettings`。

### 12.3 语言规则

规则：

- 中文对应 `zh_CN`。
- English 对应 `en`。
- 切换语言后写入设置。
- `LanguageManager` 尝试重启应用。
- 重启失败则回滚语言设置并提示。

### 12.4 GIF 质量规则

规则：

- 质量下拉框变化后写入 `GifRecordViewModel`。
- 最终持久化到 `GIF/qualityPreset`。

### 12.5 OCR 自检规则

规则：

- 点击自检后调用 `OcrViewModel::runSelfCheck()`。
- 自检输出显示到文本框。
- 点击打开 tessdata 会尝试创建或打开推荐 tessdata 目录。
- 打开失败时刷新自检报告。

### 12.6 快捷键编辑规则

规则：

1. 用户点击某个快捷键的编辑按钮。
2. 打开快捷键录制对话框。
3. 对话框捕获按键和修饰键。
4. 生成标准 Qt 快捷键字符串。
5. 保存前检查与其他动作冲突。
6. 无冲突后调用 `GlobalShortcut::updateShortcut()`。
7. 更新成功后写入 `QSettings` 并刷新 UI。

## 13. 数据持久化规则

`AppSettings` 使用 `QSettings` 管理配置。

主要配置键：

- `AI/apiKey`
- `AI/selectedModel`
- `Vision/apiKey`
- `Vision/provider`
- `Vision/model`
- `Vision/webSearchEnabled`
- `Vision/proxyEnabled`
- `Vision/proxyType`
- `Vision/proxyHost`
- `Vision/proxyPort`
- `Shortcuts/screenshot`
- `Shortcuts/screenshotSave`
- `Shortcuts/sticky`
- `Shortcuts/toggle`
- `App/language`
- `ImageSaver/savePath`
- `GIF/qualityPreset`

规则：

- ViewModel setter 通常会立即写入 `QSettings`。
- 应用启动时 `AppSettings` 读取持久化配置。
- 如果保存目录不存在，会尝试创建默认目录。
- 如果没有语言设置，则根据系统语言选择中文或英文。

## 14. 总体交互状态图

```text
应用启动
  -> 初始化服务和窗口
  -> 托盘常驻
  -> 等待触发

触发截图
  -> 抓取桌面快照
  -> 显示截图浮层
  -> 用户框选区域
  -> 可选标注
  -> 执行动作
      -> 复制 -> 关闭浮层
      -> 保存 -> 关闭浮层
      -> 贴图 -> 创建贴图窗口 -> 关闭浮层
      -> OCR -> 异步识别 -> 显示 OCR 窗口 -> 关闭浮层
      -> 长截图 -> 长截图控制器 -> 保存/复制 -> 关闭浮层
      -> GIF -> 录制控制 -> 编码 -> 关闭浮层

贴图窗口
  -> 拖动/缩放/透明度
  -> 二次标注
  -> 保存/复制
  -> AI 编辑 -> 替换图片
  -> 视觉理解 -> 显示分析结果
  -> 关闭 -> 释放缓存图片
```

## 15. 关键设计结论

1. 截图浮层是主工作流中心。
2. 选区控制被独立成状态机，逻辑清晰。
3. 标注画布可复用于截图浮层和贴图窗口。
4. 贴图图片通过 `image://sticky/<uuid>` 解耦窗口和图片存储。
5. OCR、GIF 编码、AI 网络请求均采用异步模型。
6. 长截图使用轻量灰度差匹配，不依赖重型图像库。
7. 设置项基本即时持久化。
8. Windows 交互路径最完整，Linux 主要依赖 X11。

