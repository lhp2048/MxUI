# MxUI Locale（内置多语言）设计

**日期:** 2026-08-31  
**状态:** 已确认  
**范围:** MxUI 控件内置文案的 locale API、资源结构、缓存与切语言刷新。  
**不在范围:** 应用业务文案框架（MXSetup / Family 等自管）；RTL；`.ts`/`.qm`；ICU 复数。

---

## 1. 目标与决策摘要

| 项 | 选择 |
|---|---|
| 库内置串 | C++ 内嵌表 + 可选 `lang/<id>.yaml` **按 key 覆盖**（方案 B） |
| Key 形态 | **英文源串** 为 msgid；内置一律带控件 context：`控件\|英文`（如 `Combo\|None`） |
| 外挂文件 | 按语言分文件、扁平 `msgid → 译文`（方案 1） |
| 运行时 | `Locale::SetActive` → `generation++` → Invalidate；控件缓存译文，用到时按 generation 刷新 |
| 一期语言 | `en-US`（源语 / identity）+ `zh-CN` |
| 与应用 | **两套目录**；应用业务 `Tr` 不进 MxUI 表，同英文可不同译 |

成功标准：Combo / DatePicker / TitleBar Acc / MenuBar·StatusBar 默认名可在 en↔zh 间切换；应用 yaml 可覆盖库内同 key；Paint 路径不每帧查词典。

---

## 2. 边界

| MxUI 管 | 不管 |
|--------|------|
| 控件内置可见文案、日期星期与简单格式模式、默认 Acc 名 | 应用 `text:` / 业务字符串 |
| `Locale` API、内嵌表、可选覆盖加载 | 应用自己的语言包加载器与目录 |
| generation 缓存 + Window InvalidateSink | RTL、系统 UI 语言自动侦测（可后加薄封装）、复数规则 |

应用若写入 **MxUI** 的 `lang/zh-CN.yaml`，只影响 `mx::ui::Locale::Tr` 的全局 key，无法做到「同一库 key 在不同控件不同译」——那是 context 或应用自管文案的职责。

---

## 3. 资源结构

### 3.1 目录

```text
<资源根>/lang/
  zh-CN.yaml
  en-US.yaml          # 可省略；缺省 identity = 去掉 context 后的英文，或整 key 的英文部分
```

库内编译期有一份同结构内嵌表（至少 `zh-CN`）。**不强制** exe 旁存在 `lang/`。

### 3.2 Key 约定（内置一律带 context）

- 查找键：`context + "|" + msgid_english`  
- 代码：`Locale::Tr(L"None", L"Combo")` → 查 `"Combo|None"`  
- **内置调用必须传控件 context**，禁止裸 `Tr(L"None")` 作为库正式路径（避免和应用短词撞车）  
- 回退顺序见 §4；应用覆盖 yaml 的 key 必须写全名，例如 `"Combo|None"`

### 3.3 YAML 形态（示例）

```yaml
# lang/zh-CN.yaml
"Combo|None": "（未选择）"
"Combo|No matches": "（无匹配）"
"Combo|Filter…": "输入筛选…"
"Combo|%1 selected": "已选 %1 项"
"DatePicker|Mon": "一"
"DatePicker|Tue": "二"
"DatePicker|Wed": "三"
"DatePicker|Thu": "四"
"DatePicker|Fri": "五"
"DatePicker|Sat": "六"
"DatePicker|Sun": "日"
"DatePicker|%1 Year": "%1年"
"DatePicker|%1 Month": "%1月"
"TitleBar|Minimize": "最小化"
"TitleBar|Maximize": "最大化"
"TitleBar|Restore": "还原"
"TitleBar|Close": "关闭"
"MenuBar|Menu bar": "菜单栏"
"StatusBar|Status bar": "状态栏"
```

带参：`%1`、`%2`… 宽字符替换；一期不做 ICU 复数。

### 3.4 一期内置清单

| Context | 英文 msgid |
|---------|------------|
| `Combo` | `None`, `No matches`, `Filter…`, `%1 selected` |
| `DatePicker` | `Mon`…`Sun`, `%1 Year`, `%1 Month` |
| `TitleBar` | `Minimize`, `Maximize`, `Restore`, `Close` |
| `MenuBar` | `Menu bar` |
| `StatusBar` | `Status bar` |

Demo / factory 示例数据（「行」「备注」等）不纳入正式 Locale API，除非后续单列。

---

## 4. 查找与合并

```text
Tr(english, context)：
  key = context + "|" + english   （context 空则 key = english；库内置不走空 context）
  1. 当前 locale 覆盖表[key]
  2. 当前 locale 内嵌表[key]
  3. 若 locale ≠ en-US：对 en-US 重复 1→2（通常无条目）
  4. 返回 english 原文（不含 context 前缀）
```

- 覆盖仅按 **同 locale + 同 key** 替换。  
- `RegisterFromFile` / `RegisterFromDir`：合并进覆盖表；成功后 `generation++` 并通知 sinks。  
- 未知 locale id → 行为等同 `en-US`。  
- 单文件 yaml 解析失败：跳过该文件并日志，保留已载入项。

**库目录 vs 应用目录：** 同英文在两个 map 里可有不同译文。业务按钮用应用 `AppTr`；不要把业务串塞进 MxUI `lang/`，除非有意全局改库文案。

---

## 5. API（对齐 Theme）

```text
Locale::SetActive("zh-CN")
Locale::ActiveName()
Locale::Generation()                 // uint64 或 uint32 单调增
Locale::Tr(english, context)
Locale::Format(english, context, ...)  // Tr 后再替换 %1…
Locale::RegisterFromFile(path)
Locale::RegisterFromDir(dir)         // 加载 dir 下 <id>.yaml
Locale::AddInvalidateSink / RemoveInvalidateSink
```

公开头：`mx/ui/locale.h`，并由 `mx/ui.h` 纳入。

默认 `Active`：一期 **`en-US`**；应用启动时自行 `SetActive`（可后加跟系统 UI 语言的辅助函数，非本期必做）。

---

## 6. 控件缓存与切语言

控件（或共享 helper）持有：

- 逻辑源：`context` + `english`（或已拼好的 lookup key）  
- `cached_text_`  
- `seen_generation_`

`EnsureLocalized()`（于 `Paint` / `Measure` / `AccName` **之前**调用）：

```text
if (seen_generation_ != Locale::Generation()) {
  cached_text_ = Locale::Tr(...) 或 Format(...)
  seen_generation_ = Generation()
  // 若尺寸 hug 依赖文案 → 标布局脏
}
```

切语言：

```text
Locale::SetActive
  → Generation++
  → InvalidateSinks（Window 客户区脏；需要时 RequestLayout）
→ 下一帧 EnsureLocalized 后绘制
```

允许至多一帧旧像素；禁止「先 Draw 旧串再刷新缓存」。

库内置 **不** 要求切语言时 DFS 整树 `set_text`；应用层业务文案可用绑定、Apply 遍历或重建，与本设计独立。

---

## 7. 测试

- `Tr`：内嵌 zh、yaml 覆盖、缺 key 回退英文、错误 yaml 不崩溃  
- `SetActive`：generation 递增、sink 调用  
- Combo / DatePicker / TitleBar Acc：en↔zh 下一帧正确  
- 覆盖 key 必须为 `Combo|None` 形式才生效  
- 可选小目标 `locale_test`；gallery 可手动验证  

---

## 8. 文档与实现后续

- 实现时更新 `README.md` 边界/公开 API 表，增加 Locale 一行  
- 实现计划另开 `docs/superpowers/plans/`（本文件仅设计）  

---

## 9. 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 初稿：方案 1 资源 + 内嵌/覆盖 + 英文 msgid + 内置一律 `控件\|英文` + generation 缓存 |
