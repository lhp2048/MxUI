# MxUI Locale Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 MxUI 内置控件文案落地 `Locale` API（内嵌 zh-CN + 可选 yaml 覆盖、`控件|英文` key、generation 缓存刷新）。

**Architecture:** 仿 `Theme`：进程级 `Locale` 持有 active id、`generation`、内嵌表与覆盖表、InvalidateSink。`Tr`/`Format` 按 key 查覆盖→内嵌→回退英文。控件在 Paint/Measure/Acc 前按 generation 刷新缓存字符串；`Window` 注册 sink，切语言时 `Invalidate`（必要时 `RequestLayout`）。

**Tech Stack:** C++20、yaml-cpp、现有 `MxUI::UI` / console `*_test` 模式（对齐 `theme_test`）。

**Spec:** `docs/superpowers/specs/2026-08-31-mxui-locale-design.md`

## Global Constraints

- 内置 key 一律 `控件|英文`（如 `Combo|None`）；库代码必须传 context。
- 一期语言：`en-US`（默认 active / identity）+ `zh-CN`（内嵌表）。
- 应用业务文案不进本表；Paint 不每帧查词典（generation 未变则用缓存）。
- 不引入 RTL / `.ts`/`.qm` / ICU 复数。
- UTF-8 yaml；UI 字符串宽字符（与 README YAML 约定一致）。

## File map

| File | Responsibility |
|------|----------------|
| `mx/ui/locale.h` | 公开 API：`Locale`、`LocalizedString` helper |
| `mx/ui/locale.cpp` | 内嵌表、查表、yaml 覆盖、sink、generation |
| `examples/locale_test/main.cpp` | 控制台单测 |
| `mx/ui.h` | include `locale.h` |
| `CMakeLists.txt` | `locale.cpp` + `locale_test` 目标 |
| `mx/ui/window.h` / `window.cpp` | `locale_sink_`，与 theme sink 并列 |
| `mx/ui/combo.cpp` | 内置串走 Locale + generation |
| `mx/ui/date_picker.cpp` | 星期/年月格式走 Locale |
| `mx/ui/title_bar.cpp` | 标题栏按钮 Acc 走 Locale |
| `mx/ui/menu_bar.cpp` / `status_bar.cpp` | `AccDefaultName` 走 Locale |
| `README.md` | 公开 API / 边界补充 Locale |
| `lang/zh-CN.yaml`（可选示例，放 `examples/locale_test/`） | 覆盖测试用样例，非运行时强制 |

---

### Task 1: Locale core API + console tests

**Files:**
- Create: `mx/ui/locale.h`
- Create: `mx/ui/locale.cpp`
- Create: `examples/locale_test/main.cpp`
- Modify: `CMakeLists.txt`（`mx_ui` 源列表 + `locale_test` 可执行文件）
- Modify: `mx/ui.h`（`#include "mx/ui/locale.h"`）

**Interfaces:**
- Produces:
  - `using LocaleInvalidateSink = std::function<void()>;`
  - `class Locale` with:
    - `static bool SetActive(const std::string& id);` — empty → false；任意非空 id 可设为 active（无表时查译回退英文）
    - `static const std::string& ActiveName();` — 默认 `"en-US"`
    - `static uint32_t Generation();` — 单调增；`SetActive` 成功且 id 变化、或成功合并覆盖后递增
    - `static std::wstring Tr(const wchar_t* english, const wchar_t* context);` — context 空则 key=`english`；非空则 `context|english`
    - `static std::wstring Format(const wchar_t* english, const wchar_t* context, const std::wstring& a1);`
    - `static std::wstring Format(const wchar_t* english, const wchar_t* context, const std::wstring& a1, const std::wstring& a2);`
    - `static bool RegisterFromFile(const std::string& locale_id, const std::string& path);`
    - `static bool RegisterFromDir(const std::string& dir);` — 每个 `*.yaml`/`*.yml`，locale id = 文件名 stem（如 `zh-CN.yaml` → `zh-CN`）
    - `static void AddInvalidateSink(LocaleInvalidateSink* sink);`
    - `static void RemoveInvalidateSink(LocaleInvalidateSink* sink);`
  - `struct LocalizedString` in same header:
    - members: `const wchar_t* context; const wchar_t* english; std::wstring cached; uint32_t seen_gen = 0;`
    - `const std::wstring& Get();` — generation 变则 `cached = Locale::Tr(english, context); seen_gen = Generation(); return cached;`
    - `const std::wstring& GetFormat1(const std::wstring& a1);` — 同上但用 `Format(..., a1)`

- [ ] **Step 1: Add `locale.h` / stub `locale.cpp` that compile but fail tests**

`mx/ui/locale.h`（完整公开面）:

```cpp
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace mx::ui {

using LocaleInvalidateSink = std::function<void()>;

class Locale {
 public:
  static bool SetActive(const std::string& id);
  static const std::string& ActiveName();
  static uint32_t Generation();

  static std::wstring Tr(const wchar_t* english, const wchar_t* context);
  static std::wstring Format(const wchar_t* english, const wchar_t* context,
                             const std::wstring& a1);
  static std::wstring Format(const wchar_t* english, const wchar_t* context,
                             const std::wstring& a1, const std::wstring& a2);

  // Merge flat "Context|English": "译文" map into override table for locale_id.
  // Returns false if file missing/unreadable/invalid root; true if parsed (0 keys ok).
  static bool RegisterFromFile(const std::string& locale_id,
                               const std::string& path);
  // Load each *.yaml / *.yml; stem = locale id. Returns true if >=1 file succeeded.
  static bool RegisterFromDir(const std::string& dir);

  static void AddInvalidateSink(LocaleInvalidateSink* sink);
  static void RemoveInvalidateSink(LocaleInvalidateSink* sink);
};

struct LocalizedString {
  const wchar_t* context = L"";
  const wchar_t* english = L"";
  std::wstring cached;
  uint32_t seen_gen = 0;

  const std::wstring& Get();
  const std::wstring& GetFormat1(const std::wstring& a1);
};

}  // namespace mx::ui
```

Stub `locale.cpp`：返回空/`en-US`/`0`/`english` 原文，使能链接。

- [ ] **Step 2: Wire CMake + `mx/ui.h`**

在 `CMakeLists.txt` 的 `mx_ui` 源列表中 `theme_yaml.cpp` 旁加入 `mx/ui/locale.cpp`。

在 `theme_test` 块旁增加：

```cmake
add_executable(locale_test examples/locale_test/main.cpp)
target_link_libraries(locale_test PRIVATE MxUI::UI)
target_compile_definitions(locale_test PRIVATE
  NOMINMAX UNICODE _UNICODE
  WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
if(MSVC)
  target_compile_options(locale_test PRIVATE /W3 /utf-8 /permissive-)
endif()
```

`mx/ui.h` 在 `#include "mx/ui/theme.h"` 附近加入 `#include "mx/ui/locale.h"`。

- [ ] **Step 3: Write `examples/locale_test/main.cpp`（先红）**

模式对齐 `examples/theme_test/main.cpp`（`Expect` / `g_failures` / `main` 返回失败数）。

必测用例：

```cpp
// 1) 默认 ActiveName == "en-US"；Tr(L"None", L"Combo") == L"None"
// 2) SetActive("zh-CN") 后 Tr(L"None", L"Combo") == L"（未选择）"（依赖内嵌表）
// 3) Format(L"%1 selected", L"Combo", L"3") on zh-CN == L"已选 3 项"
// 4) SetActive("") == false；Generation 在成功 SetActive 到不同 id 时递增
// 5) sink：AddInvalidateSink 后 SetActive 触发；Remove 后不再触发
// 6) 写临时 overlay yaml：
//      "Combo|None": "自定义无选"
//    RegisterFromFile("zh-CN", path) 后 Tr == L"自定义无选"；Generation 递增
// 7) 写坏 yaml（非 map）RegisterFromFile == false；原覆盖仍在
// 8) LocalizedString：zh 下 Get() 出中文；再 SetActive en 后 Get() 出英文
```

内嵌表至少包含 spec §3.4 全部 key（与设计文档 yaml 示例一致）。

- [ ] **Step 4: Run test — expect FAIL on zh-CN Tr**

```powershell
cmake --build build --config Debug --target locale_test
.\bin\x64\Debug\locale_test.exe
```

Expected: 链接成功但多项 `FAIL`（内嵌表/覆盖未实现）。

- [ ] **Step 5: Implement `locale.cpp`**

实现要点：

```text
g_active_name 默认 "en-US"
g_generation 从 1 起；SetActive 若 id 非空且与当前不同 → ++generation + NotifySinks
内嵌：unordered_map<string /*locale*/, unordered_map<wstring /*key*/, wstring>>
  仅填充 "zh-CN"；启动静态初始化或 EnsureBuiltIn()
覆盖表结构相同
MakeKey(context, english): context[0]? context + L"|" + english : english
Lookup: override[active][key] → builtin[active][key] → if active!="en-US" 对 en-US 再查 → return english
UTF-8 yaml: LoadFile → 遍历 map；key/value as<string> → MultiByteToWideChar CP_UTF8
RegisterFromFile: 成功解析后合并覆盖；++generation；NotifySinks；返回 true
RegisterFromDir: filesystem directory_iterator，扩展名 .yaml/.yml，stem 为 id
Format: Tr 后把 L"%1" 换成 a1，L"%2" 换成 a2（只替换首次或全部：替换全部非重叠出现）
LocalizedString::Get / GetFormat1 按 generation 刷新
```

内嵌 zh-CN 条目（必须）：

| key | value |
|-----|-------|
| `Combo\|None` | `（未选择）` |
| `Combo\|No matches` | `（无匹配）` |
| `Combo\|Filter…` | `输入筛选…` |
| `Combo\|%1 selected` | `已选 %1 项` |
| `DatePicker\|Mon` … `Sun` | `一`…`日` |
| `DatePicker\|%1 Year` | `%1年` |
| `DatePicker\|%1 Month` | `%1月` |
| `TitleBar\|Minimize` | `最小化` |
| `TitleBar\|Maximize` | `最大化` |
| `TitleBar\|Restore` | `还原` |
| `TitleBar\|Close` | `关闭` |
| `MenuBar\|Menu bar` | `菜单栏` |
| `StatusBar\|Status bar` | `状态栏` |

注意 ellipsis：`Filter…` 使用 Unicode `…`（U+2026），与 yaml/代码一致。

- [ ] **Step 6: Run `locale_test` — expect all ok**

```powershell
cmake --build build --config Debug --target locale_test
.\bin\x64\Debug\locale_test.exe
```

Expected: 全部 `ok`，exit 0。

- [ ] **Step 7: Commit**

```bash
git add mx/ui/locale.h mx/ui/locale.cpp mx/ui.h CMakeLists.txt examples/locale_test/main.cpp
git commit -m "feat(ui): add Locale API with embedded zh-CN and yaml overrides"
```

---

### Task 2: Window InvalidateSink for Locale

**Files:**
- Modify: `mx/ui/window.h`（在 `theme_sink_` 旁增加 `locale_sink_`）
- Modify: `mx/ui/window.cpp`（Create 路径注册；析构移除；回调 `Invalidate` + `RequestLayout`）

**Interfaces:**
- Consumes: `Locale::AddInvalidateSink` / `RemoveInvalidateSink`
- Produces: 切语言时已创建窗口客户区失效并请求布局

- [ ] **Step 1: 在 `window.h` 增加成员**

```cpp
LocaleInvalidateSink locale_sink_;
```

（需 `#include "mx/ui/locale.h"` 或前向不够，用已有 theme 同样包含方式。）

- [ ] **Step 2: 在所有 `Theme::AddInvalidateSink(&theme_sink_)` 成功创建窗体处并列注册**

```cpp
locale_sink_ = [this] {
  RequestLayout();  // 文案长度可能变；内部已 Invalidate
};
Locale::AddInvalidateSink(&locale_sink_);
```

在 `Theme::RemoveInvalidateSink` 处并列：

```cpp
Locale::RemoveInvalidateSink(&locale_sink_);
```

确认 Create / CreateDialog / CreatePopup 等所有注册 theme sink 的路径都覆盖（当前约两处 Create 成功分支 + 析构）。

- [ ] **Step 3: 手工或最小断言**

无独立窗测也可：在 `locale_test` 增加「sink 被调用」已有；窗口联调放到 Task 3。本任务以编译通过为准：

```powershell
cmake --build build --config Debug --target mx_ui locale_test
```

Expected: 成功。

- [ ] **Step 4: Commit**

```bash
git add mx/ui/window.h mx/ui/window.cpp
git commit -m "feat(ui): invalidate windows on Locale::SetActive"
```

---

### Task 3: Wire Combo + DatePicker + chrome Acc

**Files:**
- Modify: `mx/ui/combo.cpp`（及如需 `combo.h`）
- Modify: `mx/ui/date_picker.cpp`
- Modify: `mx/ui/title_bar.cpp`
- Modify: `mx/ui/menu_bar.cpp`
- Modify: `mx/ui/status_bar.cpp`

**Interfaces:**
- Consumes: `Locale::Tr` / `Locale::Format` / `LocalizedString` / `Locale::Generation`
- Produces: 内置可见/Acc 文案随 active locale 变化

- [ ] **Step 1: Combo**

替换硬编码：

| 原中文 | 调用 |
|--------|------|
| `（无匹配）` | `Locale::Tr(L"No matches", L"Combo")` |
| `（未选择）` | `Locale::Tr(L"None", L"Combo")` |
| `已选 N 项` | `Locale::Format(L"%1 selected", L"Combo", std::to_wstring(n))` |
| `输入筛选…` | `Locale::Tr(L"Filter…", L"Combo")` |

修复与中文字面量比较的逻辑（约 `label == L"（未选择）"`）：改为与 **当前** `Tr(L"None", L"Combo")` / `Tr(L"Filter…", L"Combo")` 比较，或在成员里用 `LocalizedString` 缓存后再比 `cached`。

显示路径（`DisplayText` / `Paint`）：用 `LocalizedString` 或局部 `seen_gen`，generation 变才重新 `Tr`/`Format`，避免无谓逻辑也可接受先直接 `Tr`（Acc/低频）；**Paint 热路径**用 `LocalizedString`。

- [ ] **Step 2: DatePicker**

删除或停用 `kDow[]` 中文常量；绘制星期时：

```cpp
static const wchar_t* kDowEn[] = {L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat", L"Sun"};
// Paint: Locale::Tr(kDowEn[i], L"DatePicker")
```

年/月 chip：

```cpp
Locale::Format(L"%1 Year", L"DatePicker", std::to_wstring(year));
Locale::Format(L"%1 Month", L"DatePicker", std::to_wstring(month));
```

`CalendarPopup` 增加 `uint32_t text_gen_ = 0` 与缓存的 dow/year/month 字符串数组，或每次 Paint 开头若 `Generation()!=text_gen_` 则重建缓存。**禁止**在 `DrawText` 之后才更新缓存。

- [ ] **Step 3: TitleBar Acc**

`ConfigureCaptionButton` / `SyncMaximizeGlyph` 中 Acc 字符串改为：

```cpp
Locale::Tr(L"Minimize", L"TitleBar");
Locale::Tr(L"Maximize", L"TitleBar");
Locale::Tr(L"Restore", L"TitleBar");
Locale::Tr(L"Close", L"TitleBar");
```

因 Acc 写在子 `Button` 上，TitleBar 需在 `Paint` 或 `Measure` 入口调用 `SyncLocalizedAcc()`：若 `seen_gen_ != Locale::Generation()`，按槽位重设各 caption 按钮 `acc_name(...)` 并更新 `seen_gen_`。

- [ ] **Step 4: MenuBar / StatusBar**

```cpp
// menu_bar.cpp AccDefaultName
return Locale::Tr(L"Menu bar", L"MenuBar");

// status_bar.cpp AccDefaultName
return Locale::Tr(L"Status bar", L"StatusBar");
```

（Acc 低频，可直接 `Tr`，不必缓存。）

- [ ] **Step 5: Build + extend `locale_test` smoke（可选）**

至少：

```powershell
cmake --build build --config Debug --target mx_ui locale_test widget_test
.\bin\x64\Debug\locale_test.exe
.\bin\x64\Debug\widget_test.exe
```

Expected: 均 exit 0。若 `widget_test` 断言了旧中文硬编码，同步改为 en 默认或显式 `SetActive("zh-CN")` 后再断言中文。

- [ ] **Step 6: Commit**

```bash
git add mx/ui/combo.cpp mx/ui/combo.h mx/ui/date_picker.cpp mx/ui/title_bar.cpp mx/ui/menu_bar.cpp mx/ui/status_bar.cpp examples/locale_test/main.cpp
git commit -m "feat(ui): localize Combo, DatePicker, and chrome Acc via Locale"
```

---

### Task 4: README + sample overlay + spec status

**Files:**
- Modify: `README.md`（公开 API 表增加 Locale；边界表注明内置文案可 Locale，仍不做 RTL）
- Create: `examples/locale_test/overlay_zh-CN.yaml`（覆盖示例，测试可选用）
- Modify: `docs/superpowers/specs/2026-08-31-mxui-locale-design.md`（若仍为待审则保持「已确认」）

- [ ] **Step 1: README 增补**

公开 API 宿主组增加：`locale.h`。

边界「做」列补一句：控件内置文案 `Locale`（`控件|英文` + 可选 `lang/*.yaml` 覆盖）。

简短用法：

```cpp
mx::ui::Locale::SetActive("zh-CN");
mx::ui::Locale::RegisterFromDir("lang");  // optional overrides
```

- [ ] **Step 2: 示例 yaml**

`examples/locale_test/overlay_zh-CN.yaml`：

```yaml
"Combo|None": "（无选择-覆盖）"
```

测试已用临时文件亦可保留此文件作文档样例。

- [ ] **Step 3: Final verify**

```powershell
cmake --build build --config Debug --target locale_test mx_ui ui_gallery
.\bin\x64\Debug\locale_test.exe
```

Expected: exit 0；gallery 可手工 `SetActive` 切换（若未做 UI 开关，不强制）。

- [ ] **Step 4: Commit**

```bash
git add README.md examples/locale_test/overlay_zh-CN.yaml docs/superpowers/specs/2026-08-31-mxui-locale-design.md
git commit -m "docs: document MxUI Locale and sample overlay"
```

---

## Spec coverage checklist

| Spec 项 | Task |
|---------|------|
| 内嵌 + yaml 覆盖 | Task 1 |
| `控件\|英文` key | Task 1 / 3 |
| `Tr` / `Format` / `RegisterFrom*` / sinks / generation | Task 1 |
| 默认 en-US | Task 1 |
| Window Invalidate + layout | Task 2 |
| Combo / DatePicker / TitleBar / MenuBar / StatusBar | Task 3 |
| Paint 前刷新缓存 | Task 3 |
| README | Task 4 |
| 应用业务目录分离 | 文档说明（不实现 AppTr） |

## Self-review notes

- 无 TBD；`Format` 固定 1～2 个参数重载，避免模糊 `...`。
- `RegisterFromFile` 显式传 `locale_id`（目录加载用 stem）；与 Theme「文件内 name 字段」不同，因语言包无 `name:` 字段、扁平 msgid。
- Combo 旧逻辑与中文常量比较必须改掉，否则 en 下筛选占位会坏。
