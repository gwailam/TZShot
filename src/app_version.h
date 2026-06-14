#ifndef TZSHOT_APP_VERSION_H
#define TZSHOT_APP_VERSION_H

// 版本号唯一真源:由 CMakeLists.txt 的 project(VERSION) 经
// target_compile_definitions 注入 TZSHOT_VERSION 宏（形如 "1.1.0"）。
// 此处仅提供脱离 CMake 编译时的兜底,正常构建不会用到。
#ifndef TZSHOT_VERSION
#define TZSHOT_VERSION "0.0.0-dev"
#endif

#endif // TZSHOT_APP_VERSION_H
