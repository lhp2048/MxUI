#include "mx/ui/notify_icon.h"

#include <shellapi.h>

#include <vector>

namespace mx::ui {
namespace {

constexpr UINT kMsgInvokeClick = WM_APP + 0x4E49;

DWORD BalloonFlags(NotifyBalloonKind kind) {
  switch (kind) {
    case NotifyBalloonKind::Warning:
      return NIIF_WARNING;
    case NotifyBalloonKind::Error:
      return NIIF_ERROR;
    default:
      return NIIF_INFO;
  }
}

}  // namespace

NotifyIcon::NotifyIcon() = default;

NotifyIcon::~NotifyIcon() {
  Remove();
  if (owns_hwnd_ && hwnd_ && IsWindow(hwnd_)) {
    DestroyWindow(hwnd_);
  }
  hwnd_ = nullptr;
}

void NotifyIcon::EnsureCallbackMessage() {
  if (callback_msg_ == 0) {
    callback_msg_ = WM_APP + 1;
  }
}

bool NotifyIcon::Create(const wchar_t* class_name, const wchar_t* title) {
  if (hwnd_) {
    return added_;
  }
  const wchar_t* cls = (class_name && class_name[0]) ? class_name : L"MxUI.NotifyIcon";
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &NotifyIcon::WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = cls;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }

  hwnd_ = CreateWindowExW(0, cls, title ? title : L"", WS_OVERLAPPED, CW_USEDEFAULT,
                            CW_USEDEFAULT, 0, 0, nullptr, nullptr, wc.hInstance, this);
  if (!hwnd_) {
    return false;
  }
  owns_hwnd_ = true;
  EnsureCallbackMessage();
  return AddOrModify(NIF_MESSAGE | NIF_ICON | NIF_TIP);
}

bool NotifyIcon::Attach(HWND hwnd, UINT id, UINT callback_msg) {
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  Remove();
  if (owns_hwnd_ && hwnd_ && IsWindow(hwnd_)) {
    DestroyWindow(hwnd_);
  }
  hwnd_ = hwnd;
  owns_hwnd_ = false;
  id_ = id ? id : 1;
  callback_msg_ = callback_msg ? callback_msg : (WM_APP + 1);
  return AddOrModify(NIF_MESSAGE | NIF_ICON | NIF_TIP);
}

NotifyIcon& NotifyIcon::icon(HICON icon) {
  icon_ = icon;
  if (hwnd_) {
    AddOrModify(NIF_ICON);
  }
  return *this;
}

NotifyIcon& NotifyIcon::tip(const std::wstring& text) {
  tip_ = text;
  if (hwnd_) {
    AddOrModify(NIF_TIP);
  }
  return *this;
}

NotifyIcon& NotifyIcon::on_click(ClickHandler handler) {
  on_click_ = std::move(handler);
  return *this;
}

NotifyIcon& NotifyIcon::on_message(MessageHandler handler) {
  on_message_ = std::move(handler);
  return *this;
}

bool NotifyIcon::AddOrModify(UINT flags) {
  if (!hwnd_) {
    return false;
  }
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd_;
  nid.uID = id_;
  nid.uFlags = flags;
  if (flags & NIF_MESSAGE) {
    nid.uCallbackMessage = callback_msg_;
  }
  if (flags & NIF_ICON) {
    nid.hIcon = icon_ ? icon_ : LoadIcon(nullptr, IDI_APPLICATION);
  }
  if (flags & NIF_TIP) {
    lstrcpynW(nid.szTip, tip_.c_str(), ARRAYSIZE(nid.szTip));
  }
  if (!added_) {
    nid.uFlags |= NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = callback_msg_;
    nid.hIcon = icon_ ? icon_ : LoadIcon(nullptr, IDI_APPLICATION);
    lstrcpynW(nid.szTip, tip_.c_str(), ARRAYSIZE(nid.szTip));
    added_ = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
    return added_;
  }
  return Shell_NotifyIconW(NIM_MODIFY, &nid) != FALSE;
}

bool NotifyIcon::ShowBalloon(const std::wstring& title, const std::wstring& text,
                             NotifyBalloonKind kind) {
  if (!hwnd_ || !added_) {
    return false;
  }
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd_;
  nid.uID = id_;
  nid.uFlags = NIF_INFO;
  nid.dwInfoFlags = BalloonFlags(kind);
  lstrcpynW(nid.szInfoTitle, title.c_str(), ARRAYSIZE(nid.szInfoTitle));
  lstrcpynW(nid.szInfo, text.c_str(), ARRAYSIZE(nid.szInfo));
  return Shell_NotifyIconW(NIM_MODIFY, &nid) != FALSE;
}

void NotifyIcon::Remove() {
  if (!added_ || !hwnd_) {
    added_ = false;
    return;
  }
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd_;
  nid.uID = id_;
  Shell_NotifyIconW(NIM_DELETE, &nid);
  added_ = false;
}

HICON NotifyIcon::MakeGrayIcon(HICON color) {
  if (!color) {
    return nullptr;
  }
  ICONINFO ii = {};
  if (!GetIconInfo(color, &ii) || !ii.hbmColor) {
    if (ii.hbmColor) {
      DeleteObject(ii.hbmColor);
    }
    if (ii.hbmMask) {
      DeleteObject(ii.hbmMask);
    }
    return nullptr;
  }
  BITMAP bm = {};
  GetObject(ii.hbmColor, sizeof(bm), &bm);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = bm.bmWidth;
  bmi.bmiHeader.biHeight = bm.bmHeight;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  HDC hdc = GetDC(nullptr);
  const int n = bm.bmWidth * bm.bmHeight;
  std::vector<DWORD> pixels(static_cast<size_t>(n));
  GetDIBits(hdc, ii.hbmColor, 0, static_cast<UINT>(bm.bmHeight), pixels.data(), &bmi,
            DIB_RGB_COLORS);
  for (int i = 0; i < n; ++i) {
    const DWORD p = pixels[i];
    const BYTE a = static_cast<BYTE>(p >> 24);
    const BYTE r = static_cast<BYTE>(p >> 16);
    const BYTE g = static_cast<BYTE>(p >> 8);
    const BYTE b = static_cast<BYTE>(p);
    const BYTE y = static_cast<BYTE>((r * 77 + g * 150 + b * 29) >> 8);
    pixels[i] = (static_cast<DWORD>(a) << 24) | (static_cast<DWORD>(y) << 16) |
                (static_cast<DWORD>(y) << 8) | y;
  }
  SetDIBits(hdc, ii.hbmColor, 0, static_cast<UINT>(bm.bmHeight), pixels.data(), &bmi,
            DIB_RGB_COLORS);
  ReleaseDC(nullptr, hdc);
  HICON gray = CreateIconIndirect(&ii);
  DeleteObject(ii.hbmColor);
  DeleteObject(ii.hbmMask);
  return gray;
}

LRESULT NotifyIcon::Handle(UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == callback_msg_) {
    const UINT mouse = static_cast<UINT>(lparam);
    if (mouse == WM_LBUTTONUP || mouse == WM_RBUTTONUP || mouse == WM_CONTEXTMENU ||
        mouse == WM_LBUTTONDBLCLK) {
      if (!click_pending_) {
        click_pending_ = true;
        PostMessageW(hwnd_, kMsgInvokeClick, 0, static_cast<LPARAM>(mouse));
      }
    }
    return 0;
  }
  if (msg == kMsgInvokeClick) {
    click_pending_ = false;
    if (on_click_) {
      on_click_(static_cast<UINT>(lparam));
    }
    return 0;
  }
  if (on_message_) {
    LRESULT result = 0;
    if (on_message_(msg, wparam, lparam, &result)) {
      return result;
    }
  }
  if (msg == WM_DESTROY) {
    Remove();
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

LRESULT CALLBACK NotifyIcon::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  NotifyIcon* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<NotifyIcon*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    if (self) {
      self->hwnd_ = hwnd;
    }
  } else {
    self = reinterpret_cast<NotifyIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  return self->Handle(msg, wparam, lparam);
}

}  // namespace mx::ui
