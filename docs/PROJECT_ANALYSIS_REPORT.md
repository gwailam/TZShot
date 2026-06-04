# TZShot 项目代码分析报告

生成日期：2026-06-04

## 1. 项目定位

`TZShot` 是一个基于 Qt 6 Widgets 和 C++ 的桌面截图与贴图工具。项目围绕桌面截图工作流实现了一组完整功能，包括多屏截图、选区标注、贴图窗口、OCR 识别、长截图、GIF 录制、全局快捷键、系统托盘、AI 图像编辑和 AI 视觉理解。

项目不包含数据库。用户配置通过 `QSettings` 持久化，图片结果主要保存在本地文件系统或运行期内存缓存中。

## 2. 技术栈

### 2.1 语言与构建

- C++17：`CMakeLists.txt` 中设置 `CMAKE_CXX_STANDARD 17`。
- C：用于平台快捷键底层实现，如 `win_grab.c`、`linux_grab.c`、`x11_grab.c`。
- CMake：最低版本 `3.16`。
- 项目版本：`TZshot VERSION 1.0`。

### 2.2 Qt 模块

项目依赖 Qt 6，使用模块包括：

- `Qt6::Core`
- `Qt6::Gui`
- `Qt6::Widgets`
- `Qt6::Network`
- `Qt6::Concurrent`
- `Qt6::LinguistTools`

界面实现是 Qt Widgets，不是 QML。仓库中 `qml.qrc` 实际只打包图标和 QSS 资源，没有 QML 文件。

### 2.3 第三方与系统能力

- OCR：Tesseract + Leptonica。
  - Windows 默认 CMake 逻辑可通过 `ExternalProject` 拉取并构建 OCR 依赖。
  - 配置中明确版本包括 Tesseract `5.5.2`、Leptonica `1.87.0`、zlib `v1.3.1`、libpng `v1.6.43`、libjpeg-turbo `3.0.3`、libtiff `v4.7.0`。
  - CI 构建中 `BUILD_THIRDPART_OCR=OFF`，因此发布构建不会自动带内置 OCR。
- GIF：使用项目内的 `gif.h` 单头文件库进行 GIF 写入。
- AI 网络调用：使用 `QNetworkAccessManager`，对接 DashScope 和火山方舟接口。
- Windows 截屏：使用 Win32 GDI，包括 `GetSystemMetrics`、`BitBlt`、`GetDIBits`。
- Windows 快捷键：使用 `RegisterHotKey`。
- Linux 快捷键：使用 X11/xcb 的 grab key 机制。
- CI：GitHub Actions `windows-latest`，Qt `6.8.3`，MSVC 2022，Ninja。

## 3. 顶层架构

项目入口很薄：

- `src/main.cpp` 创建 `QApplication`。
- `TZShotApp` 执行应用初始化、单实例控制、服务和窗口运行时组装。

核心分层如下：

- `src/app/`
  - 应用启动、服务组装、Widget 运行时管理。
  - `TZShotServices` 持有 Model 和 ViewModel。
  - `TZShotWidgetRuntime` 持有主要窗口、托盘、控制器。
- `src/model/`
  - 纯数据和系统状态模型。
  - `AppSettings` 管理 `QSettings`。
  - `DesktopSnapshot` 管理桌面快照、多屏坐标和窗口矩形。
- `src/viewmodel/`
  - 业务编排层。
  - 包括截图、贴图、存储、AI、OCR、GIF、长截图、视觉理解等 ViewModel。
- `src/widgets/`
  - Qt Widgets UI 层。
  - 包括截图浮层、贴图窗口、设置窗口、OCR 结果窗、视觉结果窗、长截图控制器等。
- `src/paint_board/shape/`
  - 标注图形系统。
  - 包括矩形、椭圆、箭头、画笔、文字、序号、马赛克、模糊等。
- `src/ai_call/`
  - AI 接口调用层。
  - 包括统一基类和具体服务实现。
- `src/ocr/`
  - OCR 引擎封装。
- `src/gif_encoder/`
  - GIF 抓帧和编码。
- `src/scroll/`
  - 长截图拼接算法。
- `src/shortcut_key/`
  - 跨平台全局快捷键实现。

整体架构接近手工依赖注入的 MVVM：Model 负责状态和持久化，ViewModel 负责业务流程，Widget 负责界面交互。对象之间主要通过 Qt 信号槽通信。

## 4. 应用启动与运行时

`TZShotApp::initialize()` 的主要流程：

1. 设置组织名、应用名、窗口图标，并关闭“最后一个窗口关闭后退出”行为。
2. 通过 `InstanceActivation` 创建单实例本地服务。
3. 创建 `TZShotServices`。
4. 创建 `TZShotWidgetRuntime`。
5. 连接 OCR、托盘、快捷键、设置窗口等信号。

`InstanceActivation` 使用 `QLocalServer` 和 `QLocalSocket` 实现单实例：

- 第二个进程启动时先连接本地 server。
- 如果连接成功，写入 `ACTIVATE` 并退出。
- 主进程收到激活消息后打开截图浮层。

`TrayIconHelper` 使用 `QSystemTrayIcon`，提供截图、设置、关于、退出菜单，并支持托盘单击/双击触发截图。

## 5. 功能实现分析

### 5.1 截图和多屏支持

核心类：

- `DesktopSnapshot`
- `ScreenshotViewModel`
- `CaptureOverlayWidget`
- `SelectionMaskController`

`DesktopSnapshot` 负责抓取虚拟桌面：

- Windows 使用 `GetSystemMetrics(SM_XVIRTUALSCREEN/SM_YVIRTUALSCREEN/SM_CXVIRTUALSCREEN/SM_CYVIRTUALSCREEN)` 获取虚拟桌面范围。
- Windows 使用 GDI 截取整块虚拟桌面。
- Linux/Unix 通过 `QScreen::grabWindow(0)` 分屏抓取，再合成为整张桌面图。
- 维护 `ScreenEntry`，记录每个屏幕的逻辑几何、物理矩形和 DPR。

`ScreenshotViewModel` 在此基础上提供：

- 截取桌面快照。
- 释放快照。
- 获取像素颜色。
- 获取鼠标点所在窗口矩形。
- 将选区复制到剪贴板。
- 将选区保存为 Base64。
- 将选区存入贴图缓存并返回 `image://sticky/<uuid>`。
- 将选区输出为 `QImage` 供 OCR、长截图等使用。

### 5.2 截图浮层与选区交互

`CaptureOverlayWidget` 是截图体验的主界面，职责包括：

- 显示全屏截图浮层。
- 鼠标框选截图区域。
- 拖动和八方向调整选区。
- 显示工具栏、工具参数面板、提示气泡、放大镜。
- 处理快捷键：
  - `Esc` 取消。
  - `Enter/Return` 执行默认动作。
- 发起截图动作：
  - 复制。
  - 保存。
  - 贴图。
  - OCR。
  - 长截图。
  - GIF 录制。

`SelectionMaskController` 是选区状态机，包含状态：

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

它负责 hit-test、矩形约束、鼠标光标、遮罩和控制点绘制。

### 5.3 标注系统

核心类：

- `StickyCanvasWidget`
- `Shape`
- `ShapeFactory`
- 各具体 `Shape` 子类

支持的标注类型包括：

- 画笔
- 矩形
- 椭圆
- 箭头
- 文字
- 序号
- 高亮
- 马赛克
- 模糊

`StickyCanvasWidget` 维护 `QList<Shape*> m_shapes` 和当前绘制中的 `m_currentShape`。绘制流程是：

1. 鼠标按下时根据当前工具创建 Shape。
2. 鼠标移动时更新终点或拖动文字。
3. 鼠标释放时将当前 Shape 放入列表。
4. `paintEvent()` 中绘制背景图和所有 Shape。
5. `compositedImage()` 将背景与标注层合成为输出图片。

马赛克和模糊工具需要依赖背景快照，因此 `MosaicShape` 和 `BlurShape` 会接收 canvas snapshot。

### 5.4 贴图窗口

核心类：

- `StickyImageStore`
- `StickyViewModel`
- `StickyPinWidget`

贴图数据生命周期：

1. 截图区域转成 `QImage`。
2. `StickyImageStore::storeImage()` 生成 UUID 并保存到 `QHash<QString, QImage>`。
3. 返回 `image://sticky/<uuid>`。
4. `StickyViewModel::requestSticky()` 根据 URL 创建贴图窗口。
5. 贴图窗口关闭时释放缓存图片。

`StickyImageStore` 使用 `QMutex` 保证缓存读写线程安全。

`StickyPinWidget` 支持：

- 顶置贴图显示。
- 拖动窗口。
- 缩放。
- 透明度调整。
- 复制图片。
- 保存图片。
- 旋转。
- 镜像。
- 二次标注。
- AI 图像编辑。
- AI 视觉理解。
- 右键菜单。
- 工具栏显隐。

### 5.5 OCR

核心类：

- `OcrEngine`
- `OcrViewModel`
- `OcrResultWidget`

`OcrViewModel::recognize()` 使用 `QtConcurrent::run` 在子线程中执行 OCR，避免阻塞 UI。每次任务内部创建一个 `OcrEngine`，识别完成后通过 `QFutureWatcher` 回到主线程更新结果。

`OcrEngine` 有两种运行模式：

- 编译启用 `ENABLE_OCR` 时，直接使用 Tesseract C++ API。
- 未启用时，尝试寻找本机 `tesseract` 可执行文件，使用 CLI 回退。

OCR 支持语言：

- `chi_sim`
- `eng`

启动时会查找多个 tessdata 候选目录，并要求同时存在：

- `chi_sim.traineddata`
- `eng.traineddata`

识别前会将图片转为 `RGB888`，并添加白边，以降低文本贴边时 Leptonica/Tesseract 的识别问题。

### 5.6 长截图

核心类：

- `ScrollCaptureViewModel`
- `ScrollStitcher`
- `LongCaptureController`

长截图流程：

1. 用户选中区域并触发长截图。
2. `ScrollCaptureViewModel::start()` 抓取第一帧作为 base。
3. 定时器每 90ms 抓取当前区域。
4. `makeProbe()` 将图像转为灰度小图，用平均差判断是否发生有意义变化。
5. 变化达到阈值后调用 `ScrollStitcher::appendFrame()`。
6. 停止时输出拼接结果、复制到剪贴板并保存为 `long_yyyyMMdd_HHmmss.png`。

拼接算法：

- 在上一帧底部和当前帧顶部之间搜索最佳重叠区域。
- 搜索范围是图像高度的 20% 到 85%。
- 使用灰度差平均值作为匹配分数。
- 如果最佳分数超过阈值 `22`，认为无法可靠匹配。
- 如果新增高度小于 8 像素，认为增量太小，不追加。

该算法适合一般垂直滚动内容，但对动画、固定头尾、复杂动态页面可能不稳定。

### 5.7 GIF 录制

核心类：

- `GifFrameGrabber`
- `GifRecordViewModel`
- `GifEncoder`
- `GifRecordOverlayWidget`

录制状态机：

- Idle
- Recording
- Encoding
- Idle

关键参数：

- 固定抓帧帧率：15 FPS。
- 最大帧数：450 帧，约 30 秒。
- 质量预设：
  - 0：高质量。
  - 1：均衡，抽帧为每 2 帧取 1 帧，缩放 0.85，编码 FPS 12。
  - 2：小体积，抽帧为每 3 帧取 1 帧，缩放 0.7，编码 FPS 10。

`GifFrameGrabber` 在主线程用 `QTimer` 抓屏。`GifRecordViewModel` 在停止后将帧移动到子线程编码，避免长时间阻塞 UI。

### 5.8 AI 图像编辑

核心类：

- `AIViewModel`
- `AICallBase`
- `QwenAICall`
- `SeedreamAiCall`

支持的图像编辑模型：

- `QWENAI = 0`
  - 默认接口：`https://dashscope.aliyuncs.com/api/v1/services/aigc/multimodal-generation/generation`
  - 默认模型：`wan2.6-image`
- `SEEDREAMAI = 1`
  - 默认接口：`https://ark.cn-beijing.volces.com/api/v3/images/generations`
  - 默认模型：`doubao-seedream-5-0-260128`

调用流程：

1. 从 `StickyImageStore` 取出当前贴图。
2. 将图片编码为 PNG Base64 Data URI。
3. 调用选定 AI 接口。
4. 接口返回生成图片 URL。
5. 用 `QNetworkAccessManager` 下载图片。
6. 将新图片保存到 `StickyImageStore`。
7. 发出 `signalRequestComplete(oldImageUrl, newImageUrl)`。
8. 贴图窗口检查 old URL 是否匹配当前窗口，再替换图像。

### 5.9 AI 视觉理解

核心类：

- `VisionViewModel`
- `DoubaoVisionCall`
- `VisionResultWidget`

支持 provider：

- `0`：火山方舟。
- `1`：阿里 DashScope。

根据是否启用联网搜索选择不同接口和默认模型：

- 火山普通视觉：`doubao-vision-pro-32k-2410128`
- 火山 responses/web search：`doubao-seed-1-6-250615`
- DashScope 普通视觉：`qwen-vl-plus`
- DashScope responses/web search：`qwen3-max-2026-01-23`

图片会先经 `normalizeImage()` 限制最长边为 1600，然后转成 PNG Base64 Data URL。

`DoubaoVisionCall` 同时支持：

- Chat Completions 风格响应。
- Responses API。
- 流式 SSE 增量输出。
- 代理配置。
- 联网搜索工具配置。

### 5.10 设置、快捷键与语言

`SettingsDialog` 提供：

- AI provider/model 选择。
- API Key 配置。
- 视觉模型配置。
- 联网搜索开关。
- 代理开关、类型、地址、端口。
- 语言切换。
- 保存目录选择。
- GIF 质量预设。
- OCR 自检和打开 tessdata 目录。
- 全局快捷键编辑。

`AppSettings` 使用 `QSettings` 保存配置：

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

默认快捷键：

- `Alt+A`：截图。
- `Alt+S`：截图并保存。
- `Alt+P`：贴图到桌面。
- `Alt+Q`：显示或隐藏截图浮层。

语言切换由 `LanguageManager` 处理。中文是源语言，英文通过 `:/i18n/app_en.qm` 加载。切换语言后会尝试重启应用。

## 6. 关键数据结构与算法

### 6.1 多屏坐标映射

`DesktopSnapshot::ScreenEntry`：

```cpp
struct ScreenEntry {
    QRect logicalGeometry;
    QRect physicalRect;
    qreal dpr = 1.0;
};
```

该结构用于处理逻辑坐标、物理像素、DPI 缩放和虚拟桌面偏移。截图裁剪、像素取色、贴图定位都依赖这套映射。

### 6.2 贴图缓存

`StickyImageStore` 使用：

```cpp
QHash<QString, QImage> m_images;
mutable QMutex m_mutex;
```

缓存 key 是 UUID，外部统一通过 `image://sticky/<uuid>` URL 访问。这样 UI、AI、存储层不需要直接持有大图对象。

### 6.3 标注图形列表

`StickyCanvasWidget` 使用：

```cpp
QList<Shape*> m_shapes;
Shape *m_currentShape;
```

通过多态 `Shape::draw()` 实现统一绘制。撤销操作直接删除列表最后一个 Shape。

### 6.4 长截图重叠匹配

`ScrollStitcher` 通过灰度差搜索上一帧和当前帧的最佳 overlap。核心思想是比较上一帧底部区域和当前帧顶部区域，找到平均差最小的重叠高度。

优点是实现轻量，不依赖 OpenCV。缺点是对动态内容和重复纹理敏感。

### 6.5 异步任务模型

项目主要异步方式：

- 网络：Qt 信号槽和 `QNetworkReply`。
- OCR：`QtConcurrent::run` + `QFutureWatcher<QString>`。
- GIF 编码：`QtConcurrent::run` + `QFutureWatcher<bool>`。
- 录屏抓帧：主线程 `QTimer`。
- 长截图采样：主线程 `QTimer`。

## 7. 代码质量评估

### 7.1 优点

- 项目功能边界清晰，目录划分合理。
- 应用启动层较薄，业务对象集中在 `TZShotServices` 中组装。
- 截图、贴图、AI、OCR、GIF、长截图都有独立 ViewModel。
- 平台相关逻辑基本集中在少数模块中。
- 对多屏、高 DPI、虚拟桌面坐标有专门处理。
- OCR 和 GIF 编码放到子线程，避免阻塞主 UI。
- 贴图图片缓存有互斥锁保护。
- AI 请求有统一基类 `AICallBase`，具备超时、取消和错误分类。

### 7.2 主要问题与风险

1. 大型 Widget 文件复杂度偏高。

`capture_overlay_widget.cpp` 约 1465 行，`sticky_pin_widget.cpp` 约 1609 行。它们同时承担 UI 创建、布局、事件处理、业务触发、状态管理和部分图像逻辑，后续修改容易引入回归。

2. 缺少自动化测试。

仓库未发现测试目录或单元测试文件。长截图拼接、DPI 坐标转换、AI 响应解析、GIF 编码、OCR 路径发现等都属于高风险逻辑，但没有测试保护。

3. 源码或终端编码存在风险。

在当前 PowerShell 环境读取部分 README、注释和字符串时出现乱码。说明项目需要明确统一编码策略，建议统一 UTF-8 无 BOM 和 LF，并检查现有文件实际编码。

4. `StorageViewModel` 网络保存存在并发问题。

网络图片保存使用单个 `m_pendingTargetUrl` 保存目标路径。如果同时发起多个网络保存请求，后一次请求可能覆盖前一次目标路径。

5. API Key 明文保存。

`AppSettings` 将 API Key 写入 `QSettings`。这符合简单桌面工具常见做法，但安全性有限。如果面向正式用户发布，应考虑系统凭据存储或至少在文档中明确提示。

6. AI 下载缺少资源限制。

AI 生成图下载后直接尝试解码，没有明显的响应大小、content type、重定向或超大图片保护。

7. CI 与本地 OCR 能力不一致。

CI 构建关闭内置 OCR，因此 CI 产物默认不包含 Tesseract/Leptonica 自动构建成果。用户运行 OCR 时依赖外部 tessdata 和 OCR 可执行环境。

8. 跨平台能力不均衡。

README 和代码都显示 Windows 支持最完整。Linux 快捷键依赖 X11/xcb，Wayland 没有原生实现；置顶贴图、截图体验也更偏 Windows。

## 8. 改进建议

1. 拆分大型 Widget。

建议把 `CaptureOverlayWidget` 拆为：

- 选区层。
- 工具栏组件。
- 标注参数面板。
- 放大镜控制。
- 动作执行器。
- GIF 录制 UI 适配。

建议把 `StickyPinWidget` 拆为：

- 贴图窗口壳。
- 工具栏组件。
- AI 编辑对话。
- 视觉结果入口。
- 缩放/透明度/定位控制。

2. 增加核心单元测试。

优先测试：

- `ScrollStitcher::appendFrame()`。
- `SelectionMaskController` 拖拽/缩放状态。
- `DesktopSnapshot` 坐标转换中的纯函数部分。
- AI 响应解析。
- `StickyImageStore` URL 解析、替换、释放。
- `AppSettings` 默认值边界。

3. 改善网络保存并发。

`StorageViewModel` 可以把 target path 绑定到每个 `QNetworkReply` 属性上，避免共享 `m_pendingTargetUrl`。

4. 强化 AI 下载安全。

建议增加：

- 最大响应字节数。
- 图片格式校验。
- 下载超时。
- 非图片 content type 处理。
- 错误消息透传到 UI。

5. 明确编码规范。

建议加入 `.editorconfig` 或 CI 检查，确保源码、README、翻译文件统一 UTF-8。

6. 明确 OCR 发布策略。

如果发布包希望开箱可用，应在 CI 构建中加入 OCR runtime 和 tessdata 的打包策略；如果不内置，则设置窗口和 README 应更明确地引导用户安装。

## 9. 结论

`TZShot` 已经具备一个成熟桌面截图工具的主体能力，尤其是截图、贴图、标注、OCR、GIF、长截图和 AI 能力的集成度较高。架构上采用 Model/ViewModel/Widget 的分层，业务模块划分清楚，平台能力也有针对性处理。

当前主要工程风险集中在 UI 大文件复杂度、缺少自动化测试、编码一致性、网络并发和安全边界上。如果后续要继续扩展功能，建议优先治理大型 Widget 和补充核心算法测试，否则功能增长会明显推高维护成本。

