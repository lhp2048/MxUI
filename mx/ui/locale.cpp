#include "mx/ui/locale.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace mx::ui {
namespace {

using StringMap = std::unordered_map<std::wstring, std::wstring>;
using LocaleMap = std::unordered_map<std::string, StringMap>;

std::string g_active_name = "en-US";
uint32_t g_generation = 1;
LocaleMap g_builtin;
LocaleMap g_override;
std::vector<LocaleInvalidateSink*> g_sinks;
bool g_builtin_ready = false;

void NotifySinks() {
  for (LocaleInvalidateSink* sink : g_sinks) {
    if (sink && *sink) {
      (*sink)();
    }
  }
}

std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                    static_cast<int>(utf8.size()), nullptr, 0);
  if (n <= 0) {
    return {};
  }
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                      &out[0], n);
  return out;
}

std::wstring MakeKey(const wchar_t* english, const wchar_t* context) {
  const wchar_t* en = english ? english : L"";
  if (context && context[0] != L'\0') {
    return std::wstring(context) + L"|" + en;
  }
  return en;
}

void EnsureBuiltIn() {
  if (g_builtin_ready) {
    return;
  }
  g_builtin_ready = true;
  StringMap& zh = g_builtin["zh-CN"];
  zh[L"Combo|None"] = L"（未选择）";
  zh[L"Combo|No matches"] = L"（无匹配）";
  zh[L"Combo|Filter…"] = L"输入筛选…";
  zh[L"Combo|%1 selected"] = L"已选 %1 项";
  zh[L"DatePicker|Mon"] = L"一";
  zh[L"DatePicker|Tue"] = L"二";
  zh[L"DatePicker|Wed"] = L"三";
  zh[L"DatePicker|Thu"] = L"四";
  zh[L"DatePicker|Fri"] = L"五";
  zh[L"DatePicker|Sat"] = L"六";
  zh[L"DatePicker|Sun"] = L"日";
  zh[L"DatePicker|%1 Year"] = L"%1年";
  zh[L"DatePicker|%1 Month"] = L"%1月";
  zh[L"TitleBar|Minimize"] = L"最小化";
  zh[L"TitleBar|Maximize"] = L"最大化";
  zh[L"TitleBar|Restore"] = L"还原";
  zh[L"TitleBar|Close"] = L"关闭";
  zh[L"MenuBar|Menu bar"] = L"菜单栏";
  zh[L"StatusBar|Status bar"] = L"状态栏";
}

const std::wstring* FindIn(const LocaleMap& maps, const std::string& locale,
                           const std::wstring& key) {
  const auto loc = maps.find(locale);
  if (loc == maps.end()) {
    return nullptr;
  }
  const auto it = loc->second.find(key);
  if (it == loc->second.end()) {
    return nullptr;
  }
  return &it->second;
}

std::wstring ReplaceAll(std::wstring s, const std::wstring& from,
                        const std::wstring& to) {
  if (from.empty()) {
    return s;
  }
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::wstring::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
  return s;
}

bool IsYamlExtension(const std::filesystem::path& p) {
  const auto ext = p.extension().string();
  return ext == ".yaml" || ext == ".yml" || ext == ".YAML" || ext == ".YML";
}

// Minimal flat map loader for Locale overlays:
//   "Context|English": "译文"
// Supports {} empty map. Rejects list roots (line starting with '-').
bool ParseFlatLocaleYaml(const std::string& text, StringMap* out) {
  if (!out) {
    return false;
  }
  std::istringstream in(text);
  std::string line;
  bool saw_entry = false;
  bool empty_map = false;
  while (std::getline(in, line)) {
    // strip CR
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    // trim leading spaces
    size_t b = 0;
    while (b < line.size() && (line[b] == ' ' || line[b] == '\t')) {
      ++b;
    }
    if (b >= line.size() || line[b] == '#') {
      continue;
    }
    if (line[b] == '-') {
      return false;  // list root / invalid for locale pack
    }
    if (line[b] == '{' && line.find('}') != std::string::npos) {
      empty_map = true;
      continue;
    }
    if (line[b] != '"') {
      return false;
    }
    const size_t key_end = line.find('"', b + 1);
    if (key_end == std::string::npos) {
      return false;
    }
    const std::string key8 = line.substr(b + 1, key_end - b - 1);
    size_t colon = line.find(':', key_end + 1);
    if (colon == std::string::npos) {
      return false;
    }
    size_t vb = colon + 1;
    while (vb < line.size() && (line[vb] == ' ' || line[vb] == '\t')) {
      ++vb;
    }
    if (vb >= line.size() || line[vb] != '"') {
      return false;
    }
    const size_t val_end = line.find('"', vb + 1);
    if (val_end == std::string::npos) {
      return false;
    }
    const std::string val8 = line.substr(vb + 1, val_end - vb - 1);
    (*out)[Utf8ToWide(key8)] = Utf8ToWide(val8);
    saw_entry = true;
  }
  return saw_entry || empty_map || text.find_first_not_of(" \t\r\n") ==
                                       std::string::npos;
}

}  // namespace

bool Locale::SetActive(const std::string& id) {
  EnsureBuiltIn();
  if (id.empty()) {
    return false;
  }
  if (id == g_active_name) {
    return true;
  }
  g_active_name = id;
  ++g_generation;
  NotifySinks();
  return true;
}

const std::string& Locale::ActiveName() {
  EnsureBuiltIn();
  return g_active_name;
}

uint32_t Locale::Generation() {
  EnsureBuiltIn();
  return g_generation;
}

std::wstring Locale::Tr(const wchar_t* english, const wchar_t* context) {
  EnsureBuiltIn();
  const wchar_t* en = english ? english : L"";
  const std::wstring key = MakeKey(english, context);
  if (const std::wstring* hit = FindIn(g_override, g_active_name, key)) {
    return *hit;
  }
  if (const std::wstring* hit = FindIn(g_builtin, g_active_name, key)) {
    return *hit;
  }
  if (g_active_name != "en-US") {
    if (const std::wstring* hit = FindIn(g_override, "en-US", key)) {
      return *hit;
    }
    if (const std::wstring* hit = FindIn(g_builtin, "en-US", key)) {
      return *hit;
    }
  }
  return en;
}

std::wstring Locale::Format(const wchar_t* english, const wchar_t* context,
                            const std::wstring& a1) {
  return ReplaceAll(Tr(english, context), L"%1", a1);
}

std::wstring Locale::Format(const wchar_t* english, const wchar_t* context,
                            const std::wstring& a1, const std::wstring& a2) {
  std::wstring s = ReplaceAll(Tr(english, context), L"%1", a1);
  return ReplaceAll(std::move(s), L"%2", a2);
}

bool Locale::RegisterFromFile(const std::string& locale_id,
                              const std::string& path) {
  EnsureBuiltIn();
  if (locale_id.empty() || path.empty()) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string text = ss.str();
  StringMap parsed;
  if (!ParseFlatLocaleYaml(text, &parsed)) {
    return false;
  }
  StringMap& table = g_override[locale_id];
  for (auto& kv : parsed) {
    table[kv.first] = std::move(kv.second);
  }
  ++g_generation;
  NotifySinks();
  return true;
}

bool Locale::RegisterFromDir(const std::string& dir) {
  EnsureBuiltIn();
  std::error_code ec;
  const std::filesystem::path dir_path(dir);
  if (!std::filesystem::is_directory(dir_path, ec)) {
    return false;
  }
  int succeeded = 0;
  for (const auto& entry :
       std::filesystem::directory_iterator(dir_path, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file(ec)) {
      continue;
    }
    if (!IsYamlExtension(entry.path())) {
      continue;
    }
    const std::string id = entry.path().stem().string();
    if (RegisterFromFile(id, entry.path().string())) {
      ++succeeded;
    }
  }
  return succeeded >= 1;
}

void Locale::AddInvalidateSink(LocaleInvalidateSink* sink) {
  if (sink == nullptr) {
    return;
  }
  if (std::find(g_sinks.begin(), g_sinks.end(), sink) != g_sinks.end()) {
    return;
  }
  g_sinks.push_back(sink);
}

void Locale::RemoveInvalidateSink(LocaleInvalidateSink* sink) {
  if (sink == nullptr) {
    return;
  }
  const auto it = std::find(g_sinks.begin(), g_sinks.end(), sink);
  if (it != g_sinks.end()) {
    g_sinks.erase(it);
  }
}

const std::wstring& LocalizedString::Get() {
  const uint32_t gen = Locale::Generation();
  if (seen_gen != gen) {
    cached = Locale::Tr(english, context);
    seen_gen = gen;
  }
  return cached;
}

const std::wstring& LocalizedString::GetFormat1(const std::wstring& a1) {
  const uint32_t gen = Locale::Generation();
  if (seen_gen != gen) {
    cached = Locale::Format(english, context, a1);
    seen_gen = gen;
  }
  return cached;
}

}  // namespace mx::ui
