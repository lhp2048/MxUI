// Console tests for Locale Tr / Format / SetActive / yaml override / sinks.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "mx/ui/locale.h"

namespace {

int g_failures = 0;

void Expect(bool cond, const char* name) {
  if (!cond) {
    std::printf("FAIL %s\n", name);
    ++g_failures;
  } else {
    std::printf("ok   %s\n", name);
  }
}

void TestDefaultsAndBuiltinZh() {
  using namespace mx::ui;

  Expect(Locale::ActiveName() == "en-US", "default active en-US");
  Expect(Locale::Tr(L"None", L"Combo") == L"None", "en Tr None");

  Expect(Locale::SetActive("zh-CN"), "set zh-CN");
  Expect(Locale::ActiveName() == "zh-CN", "active zh-CN");
  Expect(Locale::Tr(L"None", L"Combo") == L"（未选择）", "zh Combo|None");
  Expect(Locale::Format(L"%1 selected", L"Combo", L"3") == L"已选 3 项",
         "zh Format selected");
  Expect(Locale::Tr(L"Mon", L"DatePicker") == L"一", "zh DatePicker|Mon");
  Expect(Locale::Tr(L"Minimize", L"TitleBar") == L"最小化", "zh TitleBar");
}

void TestSetActiveGenerationAndSink() {
  using namespace mx::ui;

  Expect(!Locale::SetActive(""), "reject empty");
  Locale::SetActive("en-US");
  const uint32_t g0 = Locale::Generation();
  Expect(Locale::SetActive("zh-CN"), "set zh again or switch");
  const uint32_t g1 = Locale::Generation();
  Expect(g1 > g0 || Locale::ActiveName() == "zh-CN", "gen after switch");

  Locale::SetActive("en-US");
  const uint32_t before = Locale::Generation();
  int hits = 0;
  LocaleInvalidateSink sink = [&] { ++hits; };
  Locale::AddInvalidateSink(&sink);
  Expect(Locale::SetActive("zh-CN"), "switch for sink");
  Expect(hits == 1, "sink fired once");
  Expect(Locale::Generation() > before, "gen bumped on switch");
  Locale::RemoveInvalidateSink(&sink);
  const int after_remove = hits;
  Expect(Locale::SetActive("en-US"), "switch after remove");
  Expect(hits == after_remove, "sink silent after remove");
}

void TestYamlOverride() {
  using namespace mx::ui;

  Locale::SetActive("zh-CN");
  const char* path = "locale_test_overlay.yaml";
  {
    std::ofstream out(path);
    out << "\"Combo|None\": \"自定义无选\"\n";
  }
  const uint32_t g0 = Locale::Generation();
  Expect(Locale::RegisterFromFile("zh-CN", path), "register overlay");
  Expect(Locale::Generation() > g0, "gen after overlay");
  Expect(Locale::Tr(L"None", L"Combo") == L"自定义无选", "overlay Tr");
  std::remove(path);

  {
    std::ofstream out(path);
    out << "- not a map\n";
  }
  Expect(!Locale::RegisterFromFile("zh-CN", path), "reject bad yaml");
  Expect(Locale::Tr(L"None", L"Combo") == L"自定义无选", "overlay kept");
  std::remove(path);
}

void TestLocalizedString() {
  using namespace mx::ui;

  LocalizedString ls;
  ls.context = L"Combo";
  ls.english = L"None";
  Locale::SetActive("zh-CN");
  // Force refresh after prior overlay in other tests: clear by gen bump via SetActive
  Locale::SetActive("en-US");
  Locale::SetActive("zh-CN");
  // Reset overlay by not using overlay key — if overlay still set, re-register empty
  // Built-in still under override if previous test left it. Clear by loading empty map.
  {
    const char* path = "locale_test_clear.yaml";
    std::ofstream out(path);
    out << "{}\n";
  }
  // Empty map register still merges; override key remains. Re-set by writing builtin text.
  {
    const char* path = "locale_test_restore.yaml";
    std::ofstream out(path);
    out << "\"Combo|None\": \"（未选择）\"\n";
  }
  Locale::RegisterFromFile("zh-CN", "locale_test_restore.yaml");
  std::remove("locale_test_clear.yaml");
  std::remove("locale_test_restore.yaml");

  ls.seen_gen = 0;
  Expect(ls.Get() == L"（未选择）", "LocalizedString zh");
  Locale::SetActive("en-US");
  Expect(ls.Get() == L"None", "LocalizedString en");
}

}  // namespace

int main() {
  TestDefaultsAndBuiltinZh();
  TestSetActiveGenerationAndSink();
  TestYamlOverride();
  TestLocalizedString();
  if (g_failures > 0) {
    std::printf("%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("all locale tests passed\n");
  return 0;
}
