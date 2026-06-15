cmake_minimum_required(VERSION 3.16)

project(TZshot VERSION 1.1.2 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    LinguistTools
)

qt_standard_project_setup()

# ── 翻译文件 ──────────────────────────────────────────────
set(TS_FILES
    i18n/app_zh_CN.ts
    i18n/app_en.ts
)

# ── 平台无关源文件 ────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/app/tzshot_app.cpp
    src/app/tzshot_app.h
    src/app/tzshot_services.cpp
    src/app/tzshot_services.h
    src/app/tzshot_widget_runtime.cpp
    src/app/tzshot_widget_runtime.h
    src/tray_icon_helper.cpp
    src/tray_icon_helper.h
    src/language_manager.cpp
    src/language_manager.h
    src/sticky_image_store.cpp
    src/sticky_image_store.h
    src/instance_activation.cpp
    src/instance_activation.h
    src/singleton.h
    src/app_version.h
    src/widgets/about_dialog.cpp
    src/widgets/about_dialog.h
    src/widgets/sticky_pin_widget.cpp
    src/widgets/sticky_pin_widget.h
    src/widgets/sticky_canvas_widget.cpp
    src/widgets/sticky_canvas_widget.h
    src/widgets/capture_overlay_widget.cpp
    src/widgets/capture_overlay_widget.h
    src/widgets/long_capture_controller.cpp
    src/widgets/long_capture_controller.h
    src/widgets/magnifier_widget.cpp
    src/widgets/magnifier_widget.h
    src/widgets/selection_mask_controller.cpp
    src/widgets/selection_mask_controller.h
    src/widgets/settings_dialog.cpp
    src/widgets/settings_dialog.h
    src/widgets/widget_window_bridge.cpp
    src/widgets/widget_window_bridge.h

    # Model
    src/model/desktop_snapshot.cpp
    src/model/desktop_snapshot.h
    src/model/app_settings.cpp
    src/model/app_settings.h

    # ViewModel
    src/viewmodel/screenshot_view_model.cpp
    src/viewmodel/screenshot_view_model.h
    src/viewmodel/sticky_view_model.cpp
    src/viewmodel/sticky_view_model.h
    src/viewmodel/storage_view_model.cpp
    src/viewmodel/storage_view_model.h
    src/viewmodel/scroll_capture_view_model.cpp
    src/viewmodel/scroll_capture_view_model.h

    src/scroll/scroll_stitcher.cpp
    src/scroll/scroll_stitcher.h

    src/paint_board/shape/shape.cpp
    src/paint_board/shape/shape.h
    src/paint_board/shape/shape_type.h
    src/paint_board/shape/shape_factory.cpp
    src/paint_board/shape/shape_factory.h
    src/paint_board/shape/two_point_shape.cpp
    src/paint_board/shape/two_point_shape.h
    src/paint_board/shape/arrow_shape.cpp
    src/paint_board/shape/arrow_shape.h
    src/paint_board/shape/blur_shape.cpp
    src/paint_board/shape/blur_shape.h
    src/paint_board/shape/ellipse_shape.cpp
    src/paint_board/shape/ellipse_shape.h
    src/paint_board/shape/mosaic_shape.cpp
    src/paint_board/shape/mosaic_shape.h
    src/paint_board/shape/pen_shape.cpp
    src/paint_board/shape/pen_shape.h
    src/paint_board/shape/rect_shape.cpp
    src/paint_board/shape/rect_shape.h
    src/paint_board/shape/text_shape.cpp
    src/paint_board/shape/text_shape.h
    src/paint_board/shape/number_shape.cpp
    src/paint_board/shape/number_shape.h

    # Shortcut — 跨平台公共部分
    src/shortcut_key/globalshortcut.cpp
    src/shortcut_key/globalshortcut.h

    # Resources
    qml.qrc
)

# ── 平台专属源文件 ────────────────────────────────────────
if(UNIX)
    list(APPEND SOURCES
        src/shortcut_key/linux_grab.c
        src/shortcut_key/linux_grab.h
    )
endif()

if(WIN32)
    list(APPEND SOURCES
        resource/windows/tzshot.rc
        src/shortcut_key/win_grab.c
        src/shortcut_key/win_grab.h
    )
endif()

qt_add_executable(TZshot
    ${SOURCES}
)

# ── include 路径 ──────────────────────────────────────────
target_include_directories(TZshot PRIVATE
    src
    src/paint_board
    src/paint_board/shape
    src/model
    src/viewmodel
    src/scroll
    src/shortcut_key
)

# ── 编译定义 ──────────────────────────────────────────────
# 版本号唯一真源:由上方 project(VERSION) 注入 TZSHOT_VERSION 宏,
# 供 UI（about/settings）引用,避免散落硬编码导致漂移。
target_compile_definitions(TZshot PRIVATE
    QT_DEPRECATED_WARNINGS
    TZSHOT_VERSION="${PROJECT_VERSION}"
)

target_compile_definitions(TZshot PRIVATE
    $<$<CONFIG:Debug>:STICKY_MEM_DEBUG>
)

# ── Qt 模块链接 ───────────────────────────────────────────
target_link_libraries(TZshot PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# ── 平台系统库 ────────────────────────────────────────────
if(UNIX)
    target_link_libraries(TZshot PRIVATE
        X11
        xcb
        pthread
    )
endif()

if(WIN32)
    target_link_libraries(TZshot PRIVATE
        user32   # RegisterHotKey / UnregisterHotKey / EnumWindows
        gdi32
    )

    # Windows 子系统：Release 用 /SUBSYSTEM:WINDOWS 隐藏控制台窗口
    set_target_properties(TZshot PROPERTIES
        WIN32_EXECUTABLE $<IF:$<CONFIG:Debug>,FALSE,TRUE>
    )
endif()

# ── 翻译 ──────────────────────────────────────────────────
qt_add_translations(TZshot
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)

# Package generated .qm files into the app resource at :/i18n.
foreach(qm_file IN LISTS QM_FILES)
    get_filename_component(qm_name "${qm_file}" NAME)
    set_source_files_properties("${qm_file}" PROPERTIES
        QT_RESOURCE_ALIAS "${qm_name}"
    )
endforeach()

qt_add_resources(TZshot "i18n_translations"
    PREFIX "/i18n"
    FILES ${QM_FILES}
)

# ── 安装 ──────────────────────────────────────────────────
if(UNIX AND NOT ANDROID)
    install(TARGETS TZshot
        DESTINATION /opt/TZshot/bin
    )
endif()

if(WIN32)
    install(TARGETS TZshot
        DESTINATION bin
    )
endif()
