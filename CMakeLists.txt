# 可选构建方式：与 binding.gyp 等价，产出 build/Release/ffmpeg_player_napi.node
# 使用：npm run build:cmake
# 依赖：本机已安装 CMake 3.15+；npm install 后存在 node-addon-api、node-api-headers

cmake_minimum_required(VERSION 3.15)
project(ffmpeg_player_napi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# 与 electron-rendering 等 CMake 项目一致：供 clangd / C++ 扩展「转到定义」
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 与 binding.gyp 一致
add_compile_definitions(NAPI_DISABLE_CPP_EXCEPTIONS)

# ---- Node / N-API 头文件（与 node-gyp 使用同一套 npm 包）----
execute_process(
  COMMAND node -p "require('node-addon-api').include"
  OUTPUT_VARIABLE NODE_ADDON_API_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)
string(REPLACE "\"" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")

execute_process(
  COMMAND node -p "require('node-api-headers').include_dir"
  OUTPUT_VARIABLE NODE_API_HEADERS_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)
string(REPLACE "\"" "" NODE_API_HEADERS_DIR "${NODE_API_HEADERS_DIR}")

# ---- FFmpeg：使用仓库内 deploy 静态库（与 binding.gyp 一致）----
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/src/third-party/ffmpeg/deploy")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")

# 顺序满足静态链接依赖；libswresample 为 libavcodec 内部分编解码器所需（与仅写 -lav* 时链接器拉取范围可能不同）
set(FFMPEG_LIBS
  "${FFMPEG_LIB_DIR}/libavformat.a"
  "${FFMPEG_LIB_DIR}/libavcodec.a"
  "${FFMPEG_LIB_DIR}/libswscale.a"
  "${FFMPEG_LIB_DIR}/libswresample.a"
  "${FFMPEG_LIB_DIR}/libavutil.a"
)

foreach(_lib IN LISTS FFMPEG_LIBS)
  if(NOT EXISTS "${_lib}")
    message(FATAL_ERROR "Missing ${_lib}. Run FFmpeg deploy or adjust FFMPEG_ROOT.")
  endif()
endforeach()

# ---- 目标：.node 与 node-gyp 输出路径一致，便于 bindings / index.js ----
add_library(ffmpeg_player_napi SHARED
  src/addon.cpp
  src/core/ff_decoder_bootstrap.cpp
  src/core/ffdecode.cpp
  src/core/ff_resource_guard.cpp
  src/core/ffrotate.cpp
)

set_target_properties(ffmpeg_player_napi PROPERTIES
  PREFIX ""
  SUFFIX ".node"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/Release"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/Release"
)

target_include_directories(ffmpeg_player_napi PRIVATE
  "${NODE_ADDON_API_DIR}"
  "${NODE_API_HEADERS_DIR}"
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/src/core"
  "${FFMPEG_INCLUDE_DIR}"
)

target_link_libraries(ffmpeg_player_napi PRIVATE
  "${FFMPEG_LIBS}"
)

find_package(Threads REQUIRED)
target_link_libraries(ffmpeg_player_napi PRIVATE Threads::Threads)

if(APPLE)
  # N-API 符号由宿主 node/electron 在加载 .node 时提供，编译链接阶段需允许未定义
  target_link_options(ffmpeg_player_napi PRIVATE "LINKER:-undefined,dynamic_lookup")
  target_link_libraries(ffmpeg_player_napi PRIVATE
    "-framework AudioToolbox"
    "-framework CoreVideo"
    "-framework VideoToolbox"
    "-framework Security"
    z
    bz2
    iconv
  )
elseif(UNIX)
  target_link_options(ffmpeg_player_napi PRIVATE "LINKER:--allow-shlib-undefined")
  target_link_libraries(ffmpeg_player_napi PRIVATE
    z
    m
  )
endif()
