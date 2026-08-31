# MxUI

Windows 声明式 UI 库（MX 品牌）。定位：**轻量、现代、快捷**。

曾用名 **AuraLite**。仓库：[`lhp2048/MxUI`](https://github.com/lhp2048/MxUI)（`git@github.com:lhp2048/MxUI.git`）。Family 子模块：`3rd-party/MxUI`。

本文描述 **4.x**（`master`）：`mx::ui` 控件树 + Direct2D 画布 + YAML / C++ fluent 双轨。相对 3.x **更名并清理旧栈**；公开控件契约与 3.x 声明式面一致。

| 版本 | 内容 |
|------|------|
| **1.x** / **2.x** | AuraLite Views（GDI+ / D2D），用法与现栈不同 |
| **3.x** | AuraLite 声明式栈：`auralite::ui` / `AuraLite::UI`（YAML + fluent + D2D） |
| **4.x**（当前 `master`） | 更名为 **MxUI**：`mx::` / `MxUI::UI`、头文件 `mx/`、产物 `Mx.UI.lib`；**已移除** Chromium Views（`view_framework` / UILegacy）；MessageLoop 仍在 `Mx.Base` |

上游：https://github.com/lhp2048/MxUI

| 文档 | 内容 |
|------|------|
| [`LAYOUT.md`](LAYOUT.md) | Column / Row / Absolute、`h_align` / `v_align`、锚点、Fill/Hug |
| [`CODING.md`](CODING.md) | 编码风格（`mx/` 新栈 vs legacy） |
| [`WIN7.md`](WIN7.md) | 可选 Win7 特殊编译（默认不做） |
| [`LIBRARY_SCORE.md`](LIBRARY_SCORE.md) | 界面库评估（维度分、DataGrid/ColorPicker、边界） |

## 这是什么

- 一层 **控件树**，在 `mx::Canvas` 上自绘（Direct2D + DirectWrite + WIC）。
- **YAML 与 C++ fluent** 同一套布局和属性；`ui_gallery --check` 要求 Dump 对齐。
- 单位 **DIP**（96 DIP = 1 逻辑英寸）。`Window::Create(w, h)` 是 DIP；Per-Monitor V2 下 `WM_DPICHANGED` 重布局。
- **主题**：稀疏色 > 窗口 `set_theme` / `window.theme` > 进程 `Theme::SetActive`。弹出层跟所属窗口。
- 应用 include：`#include "mx/ui.h"`。C++20，链 `MxUI::UI`（传递 `MxUI::D2D`、yaml-cpp、`MxUI::Base`）。

默认已移除 Chromium Views（`view_framework` / `gfx` / `animation`）。MessageLoop 在 `Mx.Base`（`base/` + `message_framework/`）。

## 边界

模型如此，不是待办：

| 做 | 不做 |
|----|------|
| 键鼠 Win32 桌面：窗、表单、列表/树、菜单、日期时间、Toast、嵌 HWND | 跨平台、可视化设计师、触摸手势、命令冒泡 |
| token + 每窗主题 + 控件 `bg` / `text_color` | 样式表 / 选择器、控件级 `set_theme`、控件级 `opacity` |
| 控件级 UIA（角色、名字、Invoke / Value / Toggle / RangeValue、Tab Selection）；内置文案 `Locale`（`控件|英文` + 可选 `lang/*.yaml` 覆盖） | 列表/树项级 pattern、高对比、RTL；应用业务语言包 |
| Fill / Hug / Fixed + 锚点 | 百分比尺寸、`elevation` / `z-index` |
| 单向绑定 + 控件事件写回 | YAML 绑定表达式、自动双向、`ObservableList` diff |
| `UserControl` / 列表 + 对话框组合复杂 UI | Accordion、富文本 |

输入是鼠标消息 + 窗级 `AddAccelerator`；触摸屏靠系统合成单击。`NativeHost` 是空气墙：不转发输入、不随 `ScrollView` 半裁。系统 API 按 **Windows 10+**；Win7 若需要见 [`WIN7.md`](WIN7.md)（独立开关，不是默认产物）。

## 公开 API

应用侧一个头即可：

```cpp
#include "mx/ui.h"
```

这是公开面的索引（控件、窗、主题、YAML、DSL、绑定、reactive、async）。单头仍可按文件 include，路径与下表一致。

| 组 | 头文件 |
|----|--------|
| 入口 | `mx/ui.h` |
| 画布 | `mx/canvas.h` |
| 宿主 | `mx/ui/application.h` · `window.h` · `theme.h` · `theme_yaml.h` · `locale.h` · `types.h` · `acc.h` · `anim.h` · `node.h` |
| 布局 | `column.h` · `row.h` · `tile.h` · `tab.h` · `absolute.h` · `split_view.h` · `scroll_view.h` |
| 控件 | `title_bar.h` · `label.h` · `button.h` · `image_button.h` · `image_view.h` · `text_field.h` · `text_area.h` · `checkbox.h` · `radio.h` · `switch_control.h` · `progress_bar.h` · `slider.h` · `combo.h` · `spin_box.h` · `date_picker.h` · `color_picker.h` · `civil_date.h` · `menu_bar.h` · `menu_item.h` · `status_bar.h` · `list_view.h` · `item_list.h` · `virtual_list.h` · `data_grid.h` · `tree_view.h` · `list_columns.h` · `user_control.h` · `native_host.h` · `toast.h` · `submenu.h` · `popup_host.h` · `context_menu.h` · `text_layout.h` |
| 声明 | `factory.h` · `yaml_loader.h` · `dsl.h` · `bind.h` |
| 绑定 | `mx/reactive/signal.h` · `observe.h` |
| 异步 | `mx/async/awaiters.h` |

**不要从应用 include（内部实现）：** `ui/tooltip_overlay.h`、`ui/toast_overlay.h`、`ui/uia/provider.h`、`ui/vertical_scrollbar.h`、`ui/horizontal_scrollbar.h`、`reactive/detail/tracker.h`、`async/task_lambda.h`。Tooltip 用 `Node::tooltip()`；Toast 用 `Window` / `Toast` 控件。滚动条随列表 / `ScrollView`。

## 快速开始

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target ui_gallery ui_layout_test mx_ui
.\bin\x64\Debug\ui_gallery.exe
.\bin\x64\Debug\ui_gallery.exe --fluent
.\bin\x64\Debug\ui_gallery.exe --check
.\bin\x64\Debug\ui_layout_test.exe
.\bin\x64\Debug\webview_browser.exe
```

产物：`bin|lib/<Platform>/<Config>/`。YAML 会复制到 exe 旁。

建窗前调用 `Application::EnableDpiAwareness()`（或等价 `SetProcessDpiAwarenessContext`）。

**对话框按键：** 模态 `RunModal` 时 **Esc** 为 `EndModal(IDCANCEL)`。`Button` 设 `default: true`（C++ `is_default(true)`）后，**Enter** 在焦点不在 `TextArea` / 当前按钮 / 打开的 `Combo` 时点这个默认按钮（焦点在别的按钮上则点那个按钮）。禁用的 default 会被跳过。

**快捷键表：** `Window::AddAccelerator("Ctrl+S", ...)` 或按钮 `accelerator: "F1"` / `.accelerator("Ctrl+S")`。只接受带 Ctrl/Alt 的组合、F1–F24、Esc；普通字母不会抢走输入。后登记的窗口快捷键优先于按钮。`HandleKey` 供测试或嵌入调用。

## 工程

| 目标 | 说明 | 产物 |
|------|------|------|
| `mx_d2d` (`MxUI::D2D`) | `mx::Canvas` / `Image` | `mx_d2d.lib` |
| `mx_ui` (`MxUI::UI`) | 控件树 + YAML + DSL + reactive/async | `Mx.UI.lib` |
| `Mx.Base` | MessageLoop（新栈依赖，不能删） | `Mx.Base.lib` |

CMake 开关：

- `MXUI_BUILD_BASE` 默认 ON（MessageLoop；`mx_ui` 需要它）

## YAML 子集

- UTF-8；UI 字符串宽字符；2 空格缩进
- `TypeName:` 作为唯一 map key（如 `Column:` / `Button:`）
- 容器：`children:`；`ScrollView.content`；`SplitView.leading` / `trailing`
- 根级可选 `theme: dark`（进程 `Theme::SetActive`）
- 根级可选 `window:`（HWND 元数据，不进控件树）：`title` / `width` / `height` / `kind`（`main` \| `dialog` \| `popup`）/ `theme` / `corner_radius` / `border_width` / `resizable` / `min_width` / `min_height` / `topmost` / `center_on_owner`
- `on_click: handler_name` → 应用侧 `HandlerMap`
- 布局细则见 [`LAYOUT.md`](LAYOUT.md)

```yaml
Column:
  width: fill
  height: fill
  h_align: center
  v_align: center
  children:
    - Button: { text: "确定", width: hug }
```

C++ 等价：

```cpp
#include "mx/ui.h"
using namespace mx::ui::dsl;
auto root = Column()
    .fill_width()
    .fill_height()
    .h_align(Align::Center)
    .v_align(Align::Center)
    .child(Button().text(L"确定").hug_width())
    .Build();
```

### 尺寸与对齐（摘要）

- `width` / `height`：数字（Fixed DIP）、`fill`、`hug`。省略时看控件默认（Button / TextField：**宽 fill、高 fixed**；Checkbox：**hug×hug**；Label：**宽 fill、高 hug**）。
- 横排工具按钮请显式 `width: hug`。
- `Column` / `Row`：仅主轴 **fill** 子项参与 `weight`。`h_align` / `v_align` 按屏幕轴（左右 / 上下），不是 main/cross。Label 的 `align` 仍是**文本**对齐。
- `Absolute`：每轴 **双边锚点 > 单边 > `x`/`y` > `h_align`/`v_align` > 0**。左+右忽略该轴 `width`。

不做百分比尺寸、不做 `elevation` / `z-index`。叠层 = `children` 声明顺序。半透明只用 `bg: "#RRGGBBAA"`，没有控件级 `opacity`。

### 与 DuiLib 对照

| DuiLib | MxUI |
|--------|----------|
| Vertical / HorizontalLayout | `Column` / `Row` + weight + h_align / v_align |
| TileLayout | `Tile` |
| TabLayout | `Tab`（可带 `headers`） |
| float + 边距 | `Absolute` + 四边锚定 / `x`·`y` |
| 同容器混 float | 浮动子项放进 `Absolute`（不要在 Column 里写 `left`） |

## 控件

| 类型 | 说明 |
|------|------|
| `Column` / `Row` / `Tile` / `Tab` / `Absolute` / `SplitView` / `ScrollView` | 布局 |
| `TitleBar` | 无 `children`：`[icon?][title][min][max/restore][close]`（min/max/close 默认开；无 `icon` 路径则省略图标）。有 `children`：列表即布局。`name` 覆盖 `icon` / `title` / `minimize` / `maximize` / `close` 槽参数。空白 / Label / 图标可拖窗 |
| `Label` | 默认单行 `trim: clip`；`wrap: true` 软换行；单行可 `trim: start\|middle\|end` 省略号 |
| `TextField` / `TextArea` | 单行 / 多行（`wrap` 软换行） |
| `Checkbox` / `Radio` / `Switch` | 选择 |
| `ProgressBar` / `Slider` / `Combo` / `SpinBox` / `DatePicker` / `ColorPicker` | 进度、滑块、下拉、步进、日期、取色。DatePicker 需 `BindWindow`；弹出层点年/月/时/分/秒数字可选，箭头或滚轮微调；`time: true` 时分，再加 `seconds: true` / `second` 显示秒。ColorPicker 同样需 `BindWindow`；`mode: simple\|full`，`alpha: true` 编辑透明度；YAML `color: "#RRGGBB"` / `#RRGGBBAA` 或 `r/g/b/a` |
| `MenuBar` / `MenuItem` / `StatusBar` | 顶栏菜单（PopupHost）、默认菜单行、底栏状态。MenuBar `items` 一律生成 `MenuItem`。弹出层是普通 `Column`：`MenuItem` 当默认行，可与 `Button` / `Submenu` 等混排（Button 即自定义菜单行）。`MenuItem`：`separator` / `checkable`+`checked` / `radio_group` / `icon` |
| `VirtualList` / `DataGrid` / `ItemList` / `ListView` / `TreeView` | 列表与树。`DataGrid` 继承 `VirtualList`（表头、排序、列宽、冻结、虚拟滚动），内置二维 `cells_`；**默认自动排序**（`auto_sort`；列 `sort_kind: auto|text|number`；可 `sort_compare` 自定义、`auto_sort: false` 完全手动）；双击 / F2 / Enter 编辑（需 `BindWindow`）；列 YAML `editable: false` 只读 |
| `UserControl` | 自绘扩展 |
| `NativeHost` | HWND 黑盒洞（YAML 只占位；`Attach` / `AttachBorrowed` 在代码里） |
| `PopupHost` / `Submenu` | 自绘弹出层（推荐）。`window.corner_radius` / `window.border_width`（默认 8 / 1） |
| `Toast` | 轻提示 |
| `ContextMenu` | Legacy `TrackPopupMenu`；新菜单用 `PopupHost` |

`window.title` 只用于任务栏。无边框窗可见标题用 `TitleBar`：

```yaml
TitleBar: { title: App, icon: app.png }
```

要加新按钮时把要用的默认槽也写进 `children`（顺序即布局）。只写一个新按钮时，标题栏就只有那一个按钮：

```yaml
TitleBar:
  title: Dialog
  children:
    - Label: { name: title }
    - Button: { text: "?", width: 32 }
    - Button: { name: close, text: "关" }
```

`name: close` 覆盖默认关闭键（glyph / 尺寸 / `Close()`）；未写进 `children` 的槽不会出现。最大化在还原态显示 restore glyph。DSL：`TitleBar().title(L"...").icon(L"app.png")`。

无边框窗（`caption: false`）默认可拖边 / 角缩放（约 6 DIP，光标随边变化）。`kind: dialog` / `WindowOptions::Dialog()` 默认 `resizable: false`。最大化时关掉。边上的 Button 优先于缩放。`window.resizable` / `min_width` / `min_height` 可配。

**窗口铬与呈现：**

| | 系统标题栏 `caption: true` | 无边框可缩放（应用窗 / NativeHost demo） | Dialog（`resizable: false`） | Tooltip / 菜单 Popup |
|--|--|--|--|--|
| 外形 | 系统非客户区 | 自绘 `TitleBar`；Win11 DWM 圆角 | `SetWindowRgn` 切圆角 | 分层窗 alpha 圆角 |
| 呈现 | 不透明 DIB → `AlphaBlend` 到 `BeginPaint` DC | 同左 | 同左 | `UpdateLayeredWindow` |

无边框可缩放窗仍带 `WS_CAPTION`（任务栏、最小化、贴边），用 `DWMWA_COLOR_NONE` 藏掉系统标题栏，可见标题只来自 `TitleBar`。不要给这类窗 `SetWindowRgn`：DWM 会把新露出来的像素当成玻璃，拖边缩放会闪、会透桌面。Dialog 不能缩放，可以继续 RGN。

客户区是 32 位 DIB。贴到 DWM 窗口必须走 `AlphaBlend`（A=255）。`BitBlt` 不写 alpha，拖大小时新边是全透明。父窗不设 `WS_CLIPCHILDREN`：先铺满 DIB，再 `NativeHost::RedrawGuests`，避免子 HWND 在合成表面挖出透明洞。

**NativeHost（空气墙）：** 只同步 DIP 矩形和显隐，不画、不转发输入。**不**随 `ScrollView` 半裁或离屏合成；滚出视口时 HWND 仍完整显示。YAML 的 `NativeHost:` 只是占位，HWND 必须在代码里 `Attach(hwnd)`（关宿主销毁）或 `AttachBorrowed(hwnd)`（只拆父子）。特殊裁剪需求用 `UserControl` 自绘。Gallery：P 页 **Open NativeHost**。

## 主题

优先级：控件稀疏色（`bg` / `text_color`）> 窗口主题 > 进程 `Theme::SetActive`。

- `Theme::Active()`：当前绘制用的 token（内置 `light` / `dark`，可 YAML 注册）
- `Theme::SetActive("dark")`：进程默认；未 `set_theme` 的窗跟着变
- `Window::set_theme("dark")` / `window.theme: dark`：这一窗及其弹出层；空名跟进程
- 根 YAML `theme:` 只切进程，不是窗口字段
- 未设 `font_size` 时回落 `fonts.size`
- Gallery：`examples/ui_gallery/themes/`；**Open Dialog** 为 `window.theme: dark`

## 语言（Locale）

控件**内置**文案（Combo 占位、DatePicker 星期、TitleBar Acc 等）走 `Locale`；应用业务字符串自管。

- Key：`控件|英文`（如 `Combo|None`）；`Locale::Tr(L"None", L"Combo")`
- 默认 active：`en-US`（identity）；内嵌 `zh-CN`
- `Locale::SetActive("zh-CN")` → `generation++` → 窗口 `RequestLayout`
- 可选覆盖：`Locale::RegisterFromDir("lang")` 或 `RegisterFromFile("zh-CN", path)`，扁平 yaml：`"Combo|None": "（未选择）"`
- 设计：`docs/superpowers/specs/2026-08-31-mxui-locale-design.md`

## 弹出层

`PopupHost` + `Window::CreatePopup`：分层窗口、逐像素 alpha，未绘制区域透明。

| API | 说明 |
|-----|------|
| `PopupHost::Show` / `ShowFromYaml` | 根层弹出 |
| `Submenu` | hover/click → `Push` 子层 |
| `WrapDismiss` | 包装 `on_click`：执行后关整栈 |

面板背景在根 `Column`/`Row` 上设 `bg: "#RRGGBBAA"`。Gallery 右键默认直角 `menu_classic.yaml`（圆角见 `menu_dark.yaml`；`popup_menu.yaml` 演示 MenuItem + Button 混用）。

## 响应式与异步

| 模块 | 说明 |
|------|------|
| `mx::reactive` | `Signal` / `Computed` / `Observe` / `Batch`（**仅 UI 线程**，Debug assert） |
| `mx::async` | `ResumeOnUi` / `Delay` / `RunAsync` / `SpawnUi`（挂 `MessageLoopForUI`） |
| 绑定 | 单向 `BindText` / `BindVisible` / `BindEnabled` / `BindChecked` / `BindValue` / `BindItems`；写回用控件事件 |

关窗后不再 resume：把 `Window::alive_flag()` 传给 `Delay` / `RunAsync`。**MSVC 禁止用临时 lambda 启动协程**，用自由函数或具名可调用对象。

```cpp
using namespace mx::reactive;
using namespace mx::ui;
using namespace mx::async;

Signal<int> n{0};
Computed<std::wstring> text{[&] {
  return L"n=" + std::to_wstring(n.Get());
}};
label->OwnSubscription(BindText(*label, text));
button->on_click([&] { n.Set(n.Peek() + 1); });
```

## 无障碍 / 动画

- 控件级 UIA：角色、名字、Invoke / Value / Toggle；Slider·Progress·Spin 的 RangeValue；Combo 展开；Tab Selection；焦点 / 值 / 勾选会发事件。列表与树没有项级对象。
- `anim`：属性动画；hover 等可走主题过渡
- Tooltip 由窗口托管 overlay

## Demo / 测试

| 目标 | 用途 |
|------|------|
| `ui_gallery` | 全控件面（YAML 默认 / `--fluent` / `--check`） |
| `login_demo` | 登录窗；`--fluent` 为链式等价树 |
| `webview_browser` | 无边框 + TitleBar + 地址栏 + WebView2（`NativeHost`）；需本机 Evergreen Runtime |
| `ui_layout_test` | 布局单测 |
| `theme_test` / `dpi_test` / `dialog_test` | 主题、DPI、模态对话框 |
| `theme_picker` | ColorPicker 调色 → `Theme::Register` / `SetActive` 即时换肤 |
| `reactive_test` / `async_test` / `reactive_demo` / `reactive_gallery` | Signal / 协程 |
| `acc_test` / `uia_test` / `widget_test` / `toast_test` / `anim_test` / `drag_test` | 无障碍、新控件、Toast、动画、拖放 |

```powershell
cmake --build build --config Debug --target ui_gallery ui_layout_test theme_test
.\bin\x64\Debug\ui_layout_test.exe
.\bin\x64\Debug\theme_test.exe
```

## 目录

```
MxUI/
  mx/                 # 新栈：ui / reactive / async / canvas
  examples/           # gallery、login、各类 *_test
  cmake/
  LAYOUT.md  README.md  CODING.md
  base/  message_framework/   # Mx.Base（MessageLoop，新栈依赖）
  lib/<Platform>/<Config>/
  bin/<Platform>/<Config>/
```

## 编译环境

| 项 | 取值 |
|----|------|
| 生成器 | VS 2022，`v143` |
| 平台 | x64（亦可 Win32） |
| 配置 | Debug / Release |
| CRT | `/MD`（Debug `/MDd`） |
| 新栈语言 | C++20（`MxUI::UI`） |
| Base（MessageLoop） | C++14 |
| 新栈系统宏 | `WINVER` / `_WIN32_WINNT` = `0x0A00` |
| 库形态 | 静态库 |

接入应用：头文件搜索路径加本仓库根目录；链接 `Mx.UI.lib` + `Mx.Base.lib` + `mx_d2d.lib`，以及 `d2d1`、`dwrite`、`windowscodecs`、`msimg32`、`dwmapi`、`imm32`、`shcore`、`UIAutomationCore` 等。预处理器：`MXUI_STATIC`、`NOMINMAX`、`_WIN32_WINNT=0x0A00`。
