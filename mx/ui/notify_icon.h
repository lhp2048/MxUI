#pragma once

#include <functional>
#include <string>

#include <windows.h>

namespace mx::ui {

enum class NotifyBalloonKind { Info, Warning, Error };

// Shell tray icon. Either Create() a hidden owner HWND, or Attach() an
// existing window (e.g. the app host). Click coalescing posts one callback
// after WM_RBUTTONUP + WM_CONTEXTMENU so menus are not shown re-entrantly.
class NotifyIcon {
 public:
  using ClickHandler = std::function<void(UINT mouse_msg)>;
  using MessageHandler =
      std::function<bool(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result)>;

  NotifyIcon();
  ~NotifyIcon();

  NotifyIcon(const NotifyIcon&) = delete;
  NotifyIcon& operator=(const NotifyIcon&) = delete;

  bool Create(const wchar_t* class_name = L"MxUI.NotifyIcon",
              const wchar_t* title = L"");
  bool Attach(HWND hwnd, UINT id = 1, UINT callback_msg = WM_APP + 1);

  HWND hwnd() const { return hwnd_; }
  bool is_added() const { return added_; }

  NotifyIcon& icon(HICON icon);
  NotifyIcon& tip(const std::wstring& text);
  NotifyIcon& on_click(ClickHandler handler);
  NotifyIcon& on_message(MessageHandler handler);

  bool ShowBalloon(const std::wstring& title, const std::wstring& text,
                   NotifyBalloonKind kind = NotifyBalloonKind::Info);
  void Remove();

  // Caller DestroyIcon. Returns null if |color| is null or conversion fails.
  static HICON MakeGrayIcon(HICON color);

 private:
  void EnsureCallbackMessage();
  bool AddOrModify(UINT flags);
  LRESULT Handle(UINT msg, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                   LPARAM lparam);

  HWND hwnd_ = nullptr;
  UINT id_ = 1;
  UINT callback_msg_ = WM_APP + 1;
  HICON icon_ = nullptr;
  std::wstring tip_;
  ClickHandler on_click_;
  MessageHandler on_message_;
  bool owns_hwnd_ = false;
  bool added_ = false;
  bool click_pending_ = false;
};

}  // namespace mx::ui
