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
