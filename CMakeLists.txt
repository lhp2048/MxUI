cmake_minimum_required(VERSION 3.20)
project(MxUI LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
  message(FATAL_ERROR "MxUI targets Windows only.")
endif()

set(MXUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Match VS OutDir: bin|lib/<Platform>/<Configuration>/
if(CMAKE_GENERATOR_PLATFORM)
  set(MXUI_PLATFORM "${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(MXUI_PLATFORM "x64")
else()
  set(MXUI_PLATFORM "Win32")
endif()
# VS uses "Win32"; empty / "x86" normalize to Win32.
if(MXUI_PLATFORM STREQUAL "" OR MXUI_PLATFORM STREQUAL "x86")
  set(MXUI_PLATFORM "Win32")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MXUI_ROOT}/bin/${MXUI_PLATFORM}/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MXUI_ROOT}/lib/${MXUI_PLATFORM}/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MXUI_ROOT}/lib/${MXUI_PLATFORM}/$<CONFIG>")

include("${MXUI_ROOT}/cmake/LegacySources.cmake")

# ---- Direct2D canvas (next-gen) ----
add_library(mx_d2d STATIC
  mx/canvas_d2d.cpp
)
add_library(MxUI::D2D ALIAS mx_d2d)

target_include_directories(mx_d2d PUBLIC "${MXUI_ROOT}")
target_compile_definitions(mx_d2d
  PUBLIC
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    NTDDI_VERSION=0x0A000000
    UNICODE
    _UNICODE
)
target_link_libraries(mx_d2d PUBLIC d2d1 dwrite windowscodecs ole32 msimg32)
if(MSVC)
  target_compile_options(mx_d2d PRIVATE /W3 /utf-8 /permissive-)
endif()

# ---- Base (MessageLoop; required by mx_ui) ----
option(MXUI_BUILD_BASE "Build Mx.Base (MessageLoop)" ON)

if(MXUI_BUILD_BASE)
  add_library(Mx.Base STATIC ${MXUI_BASE_SOURCES})
  add_library(MxUI::Base ALIAS Mx.Base)
  target_include_directories(Mx.Base PUBLIC "${MXUI_ROOT}")
  target_compile_definitions(Mx.Base
    PUBLIC
      MXUI_STATIC
      NOMINMAX
      WINVER=0x0601
      _WIN32_WINNT=0x0601
      NTDDI_VERSION=0x06010000
      UNICODE
      _UNICODE
      _CRT_RAND_S
  )
  # Legacy sources are C++14-era; keep C++14 for this target.
  set_target_properties(Mx.Base PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
  if(MSVC)
    target_compile_options(Mx.Base PRIVATE /W3)
  endif()
  if(MSVC)
    # zlib .c files
    set_source_files_properties(
      ${MXUI_ROOT}/rfc_algorithm/zlib/adler32.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/compress.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/crc32.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/deflate.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/gzio.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/infback.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/inffast.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/inflate.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/inftrees.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/trees.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/uncompr.c
      ${MXUI_ROOT}/rfc_algorithm/zlib/zutil.c
      PROPERTIES LANGUAGE C
    )
  endif()
endif()

# ---- yaml-cpp (declarative UI loader) ----
include(FetchContent)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

# ---- Declarative UI (mx::ui); public CMake alias MxUI::UI ----
add_library(mx_ui STATIC
  mx/ui/node.cpp
  mx/ui/acc.cpp
  mx/ui/accel.cpp
  mx/ui/anim.cpp
  mx/ui/column.cpp
  mx/ui/row.cpp
  mx/ui/tile.cpp
  mx/ui/tab.cpp
  mx/ui/title_bar.cpp
  mx/ui/toast.cpp
  mx/ui/toast_overlay.cpp
  mx/ui/absolute.cpp
  mx/ui/label.cpp
  mx/ui/text_layout.cpp
  mx/ui/button.cpp
  mx/ui/user_control.cpp
  mx/ui/native_host.cpp
  mx/ui/image_view.cpp
  mx/ui/image_button.cpp
  mx/ui/text_field.cpp
  mx/ui/checkbox.cpp
  mx/ui/radio.cpp
  mx/ui/switch_control.cpp
  mx/ui/progress_bar.cpp
  mx/ui/slider.cpp
  mx/ui/combo.cpp
  mx/ui/spin_box.cpp
  mx/ui/civil_date.cpp
  mx/ui/date_picker.cpp
  mx/ui/color_picker.cpp
  mx/ui/data_grid.cpp
  mx/ui/menu_bar.cpp
  mx/ui/menu_item.cpp
  mx/ui/status_bar.cpp
  mx/ui/text_area.cpp
  mx/ui/scroll_view.cpp
  mx/ui/vertical_scrollbar.cpp
  mx/ui/horizontal_scrollbar.cpp
  mx/ui/list_view.cpp
  mx/ui/list_columns.cpp
  mx/ui/item_list.cpp
  mx/ui/virtual_list.cpp
  mx/ui/tree_view.cpp
  mx/ui/split_view.cpp
  mx/ui/context_menu.cpp
  mx/ui/popup_host.cpp
  mx/ui/notify_icon.cpp
  mx/ui/submenu.cpp
  mx/ui/tooltip_overlay.cpp
  mx/ui/uia/provider.cpp
  mx/ui/window.cpp
  mx/ui/theme.cpp
  mx/ui/theme_yaml.cpp
  mx/ui/locale.cpp
  mx/ui/application.cpp
  mx/ui/factory.cpp
  mx/ui/yaml_loader.cpp
  mx/reactive/observe.cpp
  mx/async/awaiters.cpp
)
set_target_properties(mx_ui PROPERTIES OUTPUT_NAME "Mx.UI")
add_library(MxUI::UI ALIAS mx_ui)
target_include_directories(mx_ui PUBLIC "${MXUI_ROOT}")
target_compile_features(mx_ui PUBLIC cxx_std_20)
target_compile_definitions(mx_ui
  PUBLIC
    NOMINMAX
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
target_link_libraries(mx_ui
  PUBLIC
    MxUI::D2D
    yaml-cpp
    imm32
    shcore
    UIAutomationCore
    oleacc
    oleaut32
    ole32
    uuid
  PRIVATE
    user32
    gdi32
    shell32
    dwmapi
)
if(NOT MXUI_BUILD_BASE)
  message(FATAL_ERROR "mx_ui requires MXUI_BUILD_BASE for MessageLoop")
endif()
target_link_libraries(mx_ui PUBLIC MxUI::Base)
if(MSVC)
  target_compile_options(mx_ui PRIVATE /W3 /utf-8 /permissive- /wd4828)
endif()

# ---- reactive_test (Signal / Computed / Observe; console) ----
add_executable(reactive_test examples/reactive_test/main.cpp)
target_link_libraries(reactive_test PRIVATE MxUI::UI)
target_compile_definitions(reactive_test PRIVATE
  NOMINMAX UNICODE _UNICODE
  WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
if(MSVC)
  target_compile_options(reactive_test PRIVATE /W3 /utf-8 /permissive-)
endif()

# ---- async_test (ResumeOnUi / Delay / RunAsync / SpawnUi; console) ----
add_executable(async_test examples/async_test/main.cpp)
target_link_libraries(async_test PRIVATE MxUI::UI)
target_compile_definitions(async_test PRIVATE
  NOMINMAX UNICODE _UNICODE
  WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
if(MSVC)
  target_compile_options(async_test PRIVATE /W3 /utf-8 /permissive- /wd4828)
endif()

# ---- D2D demo + legacy smoke tests (same OutDir as library.sln) ----
option(MXUI_BUILD_D2D_DEMO "Build Direct2D smoke demo" ON)
if(MXUI_BUILD_D2D_DEMO)
  add_executable(d2d_demo WIN32 examples/d2d_demo/main.cpp)
  target_link_libraries(d2d_demo PRIVATE MxUI::D2D)
  target_compile_definitions(d2d_demo PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(d2d_demo PRIVATE /W3 /utf-8 /permissive-)
  endif()
endif()

option(MXUI_BUILD_UI_SMOKE "Build mx::ui smoke demo" ON)
if(MXUI_BUILD_UI_SMOKE)
  add_executable(ui_smoke WIN32 examples/ui_smoke/main.cpp)
  target_link_libraries(ui_smoke PRIVATE MxUI::UI)
  target_compile_definitions(ui_smoke PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(ui_smoke PRIVATE /W3 /utf-8 /permissive-)
  endif()
endif()

option(MXUI_BUILD_UI_DEMOS "Build Phase-2 login_demo + ui_gallery" ON)
if(MXUI_BUILD_UI_DEMOS)
  add_executable(login_demo WIN32 examples/login/main.cpp)
  target_link_libraries(login_demo PRIVATE MxUI::UI)
  target_compile_definitions(login_demo PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(login_demo PRIVATE /W3 /utf-8 /permissive-)
  endif()
  add_custom_command(TARGET login_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${MXUI_ROOT}/examples/login/login_window.yaml"
      "$<TARGET_FILE_DIR:login_demo>/login_window.yaml"
    COMMENT "Copy login_window.yaml next to login_demo"
  )

  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
  endif()
  include(FetchContent)
  set(MXUI_WEBVIEW2_VERSION "1.0.2849.39")
  FetchContent_Declare(webview2_sdk
    URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${MXUI_WEBVIEW2_VERSION}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_GetProperties(webview2_sdk)
  if(NOT webview2_sdk_POPULATED)
    FetchContent_Populate(webview2_sdk)
  endif()
  if(MXUI_PLATFORM STREQUAL "Win32")
    set(MXUI_WEBVIEW2_ARCH "x86")
  elseif(MXUI_PLATFORM STREQUAL "ARM64")
    set(MXUI_WEBVIEW2_ARCH "arm64")
  else()
    set(MXUI_WEBVIEW2_ARCH "x64")
  endif()
  set(MXUI_WEBVIEW2_LIB
    "${webview2_sdk_SOURCE_DIR}/build/native/${MXUI_WEBVIEW2_ARCH}/WebView2LoaderStatic.lib")
  if(NOT EXISTS "${webview2_sdk_SOURCE_DIR}/build/native/include/WebView2.h")
    message(FATAL_ERROR "WebView2 SDK missing after download (need WebView2.h)")
  endif()
  add_executable(webview_browser WIN32 examples/webview_browser/main.cpp)
  target_include_directories(webview_browser PRIVATE
    "${webview2_sdk_SOURCE_DIR}/build/native/include")
  target_link_libraries(webview_browser PRIVATE
    MxUI::UI
    "${MXUI_WEBVIEW2_LIB}"
    version
    ole32
    user32
  )
  target_compile_definitions(webview_browser PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(webview_browser PRIVATE /W3 /utf-8 /permissive- /wd4828)
  endif()

  add_executable(ui_gallery WIN32 examples/ui_gallery/main.cpp)
  target_link_libraries(ui_gallery PRIVATE MxUI::UI)
  target_compile_definitions(ui_gallery PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(ui_gallery PRIVATE /W3 /utf-8 /permissive-)
  endif()
  add_custom_command(TARGET ui_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${MXUI_ROOT}/examples/ui_gallery/gallery.yaml"
      "$<TARGET_FILE_DIR:ui_gallery>/gallery.yaml"
    COMMENT "Copy gallery.yaml next to ui_gallery"
  )
  add_custom_command(TARGET ui_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${MXUI_ROOT}/examples/ui_gallery/dialog.yaml"
      "$<TARGET_FILE_DIR:ui_gallery>/dialog.yaml"
    COMMENT "Copy dialog.yaml next to ui_gallery"
  )
  add_custom_command(TARGET ui_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${MXUI_ROOT}/examples/ui_gallery/dialog_square.yaml"
      "$<TARGET_FILE_DIR:ui_gallery>/dialog_square.yaml"
    COMMENT "Copy dialog_square.yaml next to ui_gallery"
  )
  add_custom_command(TARGET ui_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${MXUI_ROOT}/examples/ui_gallery/popup_menu.yaml"
      "$<TARGET_FILE_DIR:ui_gallery>/popup_menu.yaml"
    COMMENT "Copy popup_menu.yaml next to ui_gallery"
  )
  add_custom_command(TARGET ui_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${MXUI_ROOT}/examples/ui_gallery/menu_classic.yaml"
      "$<TARGET_FILE_DIR:ui_gallery>/menu_classic.yaml"
    COMMENT "Copy menu_classic.yaml next to ui_gallery"
  )
  add_custom_command(TARGET ui_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${MXUI_ROOT}/examples/ui_gallery/menu_dark.yaml"
      "$<TARGET_FILE_DIR:ui_gallery>/menu_dark.yaml"
    COMMENT "Copy menu_dark.yaml next to ui_gallery"
  )
  add_custom_command(TARGET ui_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${MXUI_ROOT}/examples/ui_gallery/themes"
      "$<TARGET_FILE_DIR:ui_gallery>/themes"
    COMMENT "Copy themes next to ui_gallery"
  )

  add_executable(ui_layout_test examples/ui_layout_test/main.cpp)
  target_link_libraries(ui_layout_test PRIVATE MxUI::UI)
  target_compile_definitions(ui_layout_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(ui_layout_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(reactive_demo WIN32 examples/reactive_demo/main.cpp)
  target_link_libraries(reactive_demo PRIVATE MxUI::UI)
  target_compile_definitions(reactive_demo PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(reactive_demo PRIVATE /W3 /utf-8 /permissive- /wd4828)
  endif()

  add_executable(reactive_gallery WIN32 examples/reactive_gallery/main.cpp)
  target_link_libraries(reactive_gallery PRIVATE MxUI::UI)
  target_compile_definitions(reactive_gallery PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(reactive_gallery PRIVATE /W3 /utf-8 /permissive- /wd4828)
  endif()
  add_custom_command(TARGET reactive_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${MXUI_ROOT}/examples/ui_gallery/themes"
      "$<TARGET_FILE_DIR:reactive_gallery>/themes"
    COMMENT "Copy themes next to reactive_gallery"
  )

  add_executable(theme_picker WIN32 examples/theme_picker/main.cpp)
  target_link_libraries(theme_picker PRIVATE MxUI::UI)
  target_compile_definitions(theme_picker PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(theme_picker PRIVATE /W3 /utf-8 /permissive- /wd4828)
  endif()

  add_executable(theme_test examples/theme_test/main.cpp)
  target_link_libraries(theme_test PRIVATE MxUI::UI)
  target_compile_definitions(theme_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(theme_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(locale_test examples/locale_test/main.cpp)
  target_link_libraries(locale_test PRIVATE MxUI::UI)
  target_compile_definitions(locale_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(locale_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(control_anim_test examples/control_anim_test/main.cpp)
  target_link_libraries(control_anim_test PRIVATE MxUI::UI)
  target_compile_definitions(control_anim_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(control_anim_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(anim_test examples/anim_test/main.cpp)
  target_link_libraries(anim_test PRIVATE MxUI::UI)
  target_compile_definitions(anim_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(anim_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(toast_test examples/toast_test/main.cpp)
  target_link_libraries(toast_test PRIVATE MxUI::UI)
  target_compile_definitions(toast_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(toast_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(widget_test examples/widget_test/main.cpp)
  target_link_libraries(widget_test PRIVATE MxUI::UI)
  target_compile_definitions(widget_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(widget_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(acc_test examples/acc_test/main.cpp)
  target_link_libraries(acc_test PRIVATE MxUI::UI)
  target_compile_definitions(acc_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(acc_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(uia_test examples/uia_test/main.cpp)
  target_link_libraries(uia_test PRIVATE MxUI::UI)
  target_compile_definitions(uia_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(uia_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(dpi_test examples/dpi_test/main.cpp)
  target_link_libraries(dpi_test PRIVATE MxUI::UI)
  target_compile_definitions(dpi_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(dpi_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(dialog_test examples/dialog_test/main.cpp)
  target_link_libraries(dialog_test PRIVATE MxUI::UI)
  target_compile_definitions(dialog_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(dialog_test PRIVATE /W3 /utf-8 /permissive-)
  endif()

  add_executable(drag_test examples/drag_test/main.cpp)
  target_link_libraries(drag_test PRIVATE MxUI::UI)
  target_compile_definitions(drag_test PRIVATE
    NOMINMAX UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(drag_test PRIVATE /W3 /utf-8 /permissive-)
  endif()
endif()

option(MXUI_BUILD_TESTS "Build test_base" ON)
if(MXUI_BUILD_TESTS AND MXUI_BUILD_BASE)
  add_executable(test_base test_base/main.cpp)
  target_link_libraries(test_base PRIVATE MxUI::Base)
  set_target_properties(test_base PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
  if(MSVC)
    target_compile_options(test_base PRIVATE /W3)
  endif()
endif()

message(STATUS "MxUI outputs: bin|lib/${MXUI_PLATFORM}/<Config>/")
message(STATUS "MxUI: D2D canvas + Base MessageLoop via CMake")
