#include "mx/ui/window.h"

#include "mx/async/task_lambda.h"
#include "mx/ui/button.h"
#include "mx/ui/item_list.h"
#include "mx/ui/native_host.h"
#include "mx/ui/popup_host.h"
#include "mx/ui/theme.h"
#include "mx/ui/title_bar.h"
#include "mx/ui/toast.h"
#include "mx/ui/toast_overlay.h"
#include "mx/ui/tooltip_overlay.h"
#include "mx/ui/uia/provider.h"
#include "mx/ui/virtual_list.h"
#include "message_framework/message_loop.h"

#include <dwmapi.h>
#include <imm.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace mx::ui {
namespace {

constexpr wchar_t kWindowClassName[] = L"MxUI.Window";
constexpr float kDragSlopDip = 4.f;
constexpr float kResizeEdgeDip = 6.f;
constexpr float kResizeCornerDip = 12.f;
constexpr int kDefaultMinWidthDip = 160;
constexpr int kDefaultMinHeightDip = 80;
// Win11 dwmapi.h attributes (safe no-ops on Win10).
constexpr DWORD kDwmwaWindowCornerPreference = 33;
constexpr DWORD kDwmwaBorderColor = 34;
constexpr DWORD kDwmwaCaptionColor = 35;
constexpr DWORD kDwmwaSystemBackdropType = 38;
constexpr DWORD kDwmsbtNone = 1;
constexpr DWORD kDwmwcpDoNotRound = 1;
constexpr DWORD kDwmwcpRound = 2;
// dwmapi: hide Win11 caption/border fill (not RGB).
constexpr COLORREF kDwmwaColorNone = 0xFFFFFFFE;

bool FramelessAppWindow(const Window::WindowOptions& o) {
  return !o.caption && o.resizable;
}

LPCWSTR CursorForResizeHit(int ht) {
  switch (ht) {
    case HTLEFT:
    case HTRIGHT:
      return IDC_SIZEWE;
    case HTTOP:
    case HTBOTTOM:
      return IDC_SIZENS;
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
      return IDC_SIZENWSE;
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
      return IDC_SIZENESW;
    default:
      return IDC_ARROW;
  }
}

float QueryMonitorDpiNearCursor() {
  using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
  static GetDpiForMonitorFn fn = nullptr;
  static bool tried = false;
  if (!tried) {
    tried = true;
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
      fn = reinterpret_cast<GetDpiForMonitorFn>(
          GetProcAddress(shcore, "GetDpiForMonitor"));
    }
  }
  POINT pt = {};
  GetCursorPos(&pt);
  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  UINT dx = 96;
  UINT dy = 96;
  // MDT_EFFECTIVE_DPI = 0
  if (fn && mon && SUCCEEDED(fn(mon, 0, &dx, &dy)) && dx > 0) {
    return static_cast<float>(dx);
  }
  return mx::kDipDpi;
}

float QueryHwndDpi(HWND hwnd) {
  using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
  static GetDpiForWindowFn fn = nullptr;
  static bool tried = false;
  if (!tried) {
    tried = true;
    fn = reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
  }
  if (fn && hwnd) {
    const UINT d = fn(hwnd);
    if (d > 0) {
      return static_cast<float>(d);
    }
  }
  return mx::kDipDpi;
}

int SystemMetricForDpi(int index, UINT dpi) {
  using Fn = int(WINAPI*)(int, UINT);
  static Fn fn = nullptr;
  static bool tried = false;
  if (!tried) {
    tried = true;
    fn = reinterpret_cast<Fn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"),
                       "GetSystemMetricsForDpi"));
  }
  if (fn && dpi > 0) {
    const int v = fn(index, dpi);
    if (v > 0) {
      return v;
    }
  }
  return GetSystemMetrics(index);
}

int DipToOuterPx(float dip, float dpi) {
  return static_cast<int>(std::ceil(mx::PxFromDip(dip, dpi)));
}

bool IsImeUiWindow(HWND hwnd, HWND dialog) {
  if (!hwnd) {
    return false;
  }
  wchar_t cls[128] = {};
  GetClassNameW(hwnd, cls, 128);
  if (wcsstr(cls, L"IME") != nullptr || wcsstr(cls, L"MSCTFIME") != nullptr) {
    return true;
  }
  return GetWindow(hwnd, GW_OWNER) == dialog;
}

class ModalDispatcher : public MessageLoopForUI::Dispatcher {
 public:
  explicit ModalDispatcher(HWND focus) : focus_(focus) {}
  bool Dispatch(const MSG& msg) override {
    if (focus_ && IsWindow(focus_)) {
      const HWND focus = GetFocus();
      if (focus != focus_ && !IsChild(focus_, focus) &&
          !IsImeUiWindow(focus, focus_)) {
        if (GetForegroundWindow() != focus_) {
          SetForegroundWindow(focus_);
        }
        SetActiveWindow(focus_);
        SetFocus(focus_);
      }
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    return true;
  }

 private:
  HWND focus_;
};

std::wstring ImmGetString(HIMC himc, DWORD index) {
  if (!himc) {
    return {};
  }
  const LONG bytes = ImmGetCompositionStringW(himc, index, nullptr, 0);
  if (bytes <= 0) {
    return {};
  }
  std::wstring out(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
  ImmGetCompositionStringW(himc, index, out.data(),
                           static_cast<DWORD>(bytes));
  // Imm may not null-terminate; size already matches char count.
  while (!out.empty() && out.back() == L'\0') {
    out.pop_back();
  }
  return out;
}

bool IsDescendantOf(const Node* node, const Node* ancestor) {
  while (node) {
    if (node == ancestor) {
      return true;
    }
    node = node->parent();
  }
  return false;
}

bool ListHeaderResizeCursorAt(const Node* hit, float x, float y) {
  if (const auto* list = dynamic_cast<const VirtualList*>(hit)) {
    return list->HeaderResizeCursorAt(x, y);
  }
  if (const auto* items = dynamic_cast<const ItemList*>(hit)) {
    return items->HeaderResizeCursorAt(x, y);
  }
  return false;
}

}  // namespace

Window::Window() = default;

Window::~Window() {
  if (alive_) {
    alive_->store(false);
  }
  Theme::RemoveInvalidateSink(&theme_sink_);
  HideTooltip();
  tooltip_.reset();
  toast_queue_.clear();
  if (hwnd_) {
    KillTimer(hwnd_, kToastTimerId);
  }
  toast_overlay_.reset();
  DisconnectUia();
  if (hwnd_) {
    if (hwnd_) {
      KillTimer(hwnd_, kAnimTimerId);
      anim_clients_ = 0;
    }
    DestroyHostHwnd();
    popup_mode_ = false;
  }
}

void Window::RegisterAnimation() {
  ++anim_clients_;
  SyncAnimTimer();
}

void Window::UnregisterAnimation() {
  if (anim_clients_ <= 0) {
    return;
  }
  --anim_clients_;
  SyncAnimTimer();
}

void Window::SyncAnimTimer() {
  if (!hwnd_) {
    return;
  }
  if (anim_clients_ > 0 || !anim_driver_.empty()) {
    SetTimer(hwnd_, kAnimTimerId, 33, nullptr);
  } else {
    KillTimer(hwnd_, kAnimTimerId);
  }
}

double Window::NowSec() {
  static LARGE_INTEGER freq = {};
  if (freq.QuadPart == 0) {
    QueryPerformanceFrequency(&freq);
  }
  LARGE_INTEGER counter = {};
  QueryPerformanceCounter(&counter);
  if (freq.QuadPart == 0) {
    return 0.0;
  }
  return static_cast<double>(counter.QuadPart) /
         static_cast<double>(freq.QuadPart);
}

uint64_t Window::Animate(float duration_sec, Easing easing,
                         AnimationDriver::TickFn on_tick,
                         AnimationDriver::DoneFn on_done) {
  const uint64_t id = anim_driver_.Start(duration_sec, easing, std::move(on_tick),
                                         std::move(on_done), NowSec());
  SyncAnimTimer();
  if (id != 0) {
    Invalidate();
  }
  return id;
}

void Window::CancelAnimation(uint64_t id) {
  anim_driver_.Cancel(id);
  SyncAnimTimer();
}

void Window::set_layered_opacity(float a) {
  canvas_.set_layered_opacity(a);
  Invalidate();
}

float Window::layered_opacity() const {
  return canvas_.layered_opacity();
}

void Window::ShowToast(std::unique_ptr<Toast> toast) {
  if (!toast || !hwnd_) {
    return;
  }
  toast_queue_.push_back(std::move(toast));
  if (has_toast()) {
    return;
  }
  PresentNextToast();
}

void Window::ShowToast(std::unique_ptr<Node> node) {
  Toast* t = dynamic_cast<Toast*>(node.get());
  if (!t) {
    return;
  }
  node.release();
  ShowToast(std::unique_ptr<Toast>(t));
}

void Window::DismissToast() {
  if (hwnd_) {
    KillTimer(hwnd_, kToastTimerId);
  }
  Toast* cur = toast();
  const float fade = cur ? cur->effective_fade_sec() : 0.f;
  if (toast_overlay_ && toast_overlay_->showing() && fade > 0.f &&
      !toast_fading_) {
    toast_fading_ = true;
    toast_overlay_->FadeOut(fade, [this]() {
      toast_fading_ = false;
      if (toast_overlay_) {
        toast_overlay_->Hide();
      }
      PresentNextToast();
    });
    return;
  }
  toast_fading_ = false;
  if (toast_overlay_) {
    toast_overlay_->CancelFade();
    toast_overlay_->Hide();
  }
  PresentNextToast();
}

bool Window::has_toast() const {
  return toast_fading_ || (toast_overlay_ && toast_overlay_->showing());
}

Toast* Window::toast() const {
  return toast_overlay_ ? toast_overlay_->toast() : nullptr;
}

void Window::SyncToastFade() {
  if (!toast_fading_) {
    return;
  }
  Toast* cur = toast();
  if (cur && cur->effective_fade_sec() > 0.f) {
    return;
  }
  toast_fading_ = false;
  if (toast_overlay_) {
    toast_overlay_->CancelFade();
    toast_overlay_->Hide();
  }
  PresentNextToast();
}

void Window::PresentNextToast() {
  if (!hwnd_ || toast_fading_ || toast_queue_.empty()) {
    return;
  }
  auto toast = std::move(toast_queue_.front());
  toast_queue_.pop_front();
  const float dur = toast->duration_sec();
  if (toast->dismiss_on_click()) {
    Toast::DismissHandler prior = toast->release_on_dismiss();
    toast->on_dismiss([this, prior = std::move(prior)]() mutable {
      if (prior) {
        prior();
      }
      if (hwnd_) {
        PostMessageW(hwnd_, kWmDismissToast, 0, 0);
      }
    });
  }
  if (!toast_overlay_) {
    toast_overlay_ = std::make_unique<ToastOverlay>();
  }
  if (!toast_overlay_->Show(hwnd_, dpi_, std::move(toast))) {
    PresentNextToast();
    return;
  }
  if (dur > 0.f) {
    const UINT ms = static_cast<UINT>(std::max(1.f, dur * 1000.f));
    SetTimer(hwnd_, kToastTimerId, ms, nullptr);
  }
}

bool Window::EnsureWindowClass(HINSTANCE instance) {
  static bool registered = false;
  if (registered) {
    return true;
  }

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_DBLCLKS;
  wc.lpfnWndProc = &Window::WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kWindowClassName;
  if (!RegisterClassExW(&wc)) {
    if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return false;
    }
  }
  registered = true;
  return true;
}

bool Window::Create(const wchar_t* title, int w, int h, const WindowOptions& opt) {
  if (hwnd_) {
    return false;
  }

  (void)Theme::Active();

  HINSTANCE instance = GetModuleHandleW(nullptr);
  if (!EnsureWindowClass(instance)) {
    return false;
  }

  options_ = opt;
  options_.Normalize();
  popup_mode_ = false;
  quit_on_close_ = options_.quit_on_close;
  dialog_owner_ = options_.owner;

  dpi_ = QueryMonitorDpiNearCursor();
  const int pw = DipToOuterPx(static_cast<float>(w), dpi_);
  const int ph = DipToOuterPx(static_cast<float>(h), dpi_);

  DWORD ex = 0;
  DWORD style = WS_OVERLAPPEDWINDOW;
  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  HWND parent = (options_.owner && IsWindow(options_.owner)) ? options_.owner : nullptr;
  const wchar_t* wnd_title = title ? title : L"MxUI";
  if (!options_.caption) {
    // Dialogs stay owned WS_POPUP. Resizable frameless windows need min/max
    // boxes and an unowned taskbar button — otherwise SW_MINIMIZE shrinks to
    // a tiny rectangle at the bottom-left of the desktop.
    if (FramelessAppWindow(options_)) {
      // No WS_CLIPCHILDREN: child holes in the DWM surface are alpha-0
      // (NativeHost see-through while resizing). Parent paints the full DIB,
      // then NativeHost::RedrawGuests.
      style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
              WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
      ex |= WS_EX_APPWINDOW;
      parent = nullptr;
    } else {
      style = WS_POPUP;
    }
    if (options_.topmost) {
      ex |= WS_EX_TOPMOST;
    }
    x = 0;
    y = 0;
    if (!title) {
      wnd_title = L"";
    }
  }

  hwnd_ = CreateWindowExW(ex, kWindowClassName, wnd_title, style, x, y, pw, ph,
                          parent, nullptr, instance, this);
  if (!hwnd_) {
    ResetCreateState();
    return false;
  }

  dpi_ = QueryHwndDpi(hwnd_);
  canvas_.SetDpi(dpi_);

  if (!canvas_.Init(hwnd_)) {
    DestroyHostHwnd();
    ResetCreateState();
    return false;
  }

  ImmAssociateContextEx(hwnd_, NULL, 0);

  theme_sink_ = [this] { Invalidate(); };
  Theme::AddInvalidateSink(&theme_sink_);

  layout_dirty_ = true;
  if (uses_custom_chrome()) {
    PlaceWindow(options_.owner, w, h);
    ApplyChromeShape();
    ApplyChromeDwm();
  }
  ApplyAcceptFiles();
  return true;
}

void Window::DestroyHostHwnd() {
  if (!hwnd_) {
    return;
  }
  NativeHost::OrphanTree(root_.get());
  NativeHost::OrphanTree(popup_.get());
  DestroyWindow(hwnd_);
  hwnd_ = nullptr;
}

void Window::ResetCreateState() {
  quit_on_close_ = true;
  dialog_owner_ = nullptr;
  options_ = WindowOptions{};
}

bool Window::CreatePopup(HWND owner, int w, int h) {
  return CreateLayeredTool(owner, w, h,
                           WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED);
}

bool Window::CreateLayeredOverlay(int width, int height) {
  return CreateLayeredTool(
      nullptr, width, height,
      WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
}

void Window::SetPopupChrome(float corner_radius, float border_width) {
  popup_radius_ = std::max(0.f, corner_radius);
  popup_border_ = std::max(0.f, border_width);
}

void Window::StealPopupRootBg() {
  popup_fill_.reset();
  if (!popup_mode_ || !root_ || (popup_radius_ <= 0.f && popup_border_ <= 0.f)) {
    return;
  }
  if (root_->has_bg()) {
    popup_fill_ = root_->bg_color();
    root_->clear_bg();
  }
}

void Window::RestorePopupRootBg() {
  if (root_ && popup_fill_) {
    root_->bg(*popup_fill_);
  }
  popup_fill_.reset();
}

void Window::PaintPopupFill(mx::Canvas& canvas) {
  if (!popup_mode_ || (popup_radius_ <= 0.f && popup_border_ <= 0.f)) {
    return;
  }
  const RectF client = ClientRectF();
  const ColorF fill = popup_fill_.value_or(Theme::Active().surface);
  if (popup_radius_ > 0.f) {
    canvas.FillRoundedRect(client, popup_radius_, popup_radius_, fill);
  } else {
    canvas.FillRect(client, fill);
  }
}

void Window::PaintPopupBorder(mx::Canvas& canvas) {
  if (!popup_mode_ || popup_border_ <= 0.f) {
    return;
  }
  const RectF client = ClientRectF();
  const float inset = popup_border_ * 0.5f;
  const RectF stroke{client.x + inset, client.y + inset,
                     std::max(0.f, client.w - popup_border_),
                     std::max(0.f, client.h - popup_border_)};
  const float rr = std::max(0.f, popup_radius_ - inset);
  if (rr > 0.f) {
    canvas.DrawRoundedRect(stroke, rr, rr, Theme::Active().border, popup_border_);
  } else {
    canvas.DrawRect(stroke, Theme::Active().border, popup_border_);
  }
}

bool Window::CreateLayeredTool(HWND owner, int w, int h, DWORD extra_ex) {
  if (hwnd_) {
    return false;
  }

  (void)Theme::Active();

  HINSTANCE instance = GetModuleHandleW(nullptr);
  if (!EnsureWindowClass(instance)) {
    return false;
  }

  popup_mode_ = true;
  quit_on_close_ = false;

  dpi_ = QueryMonitorDpiNearCursor();
  const int pw = DipToOuterPx(static_cast<float>(w), dpi_);
  const int ph = DipToOuterPx(static_cast<float>(h), dpi_);

  hwnd_ = CreateWindowExW(extra_ex, kWindowClassName, L"",
                          WS_POPUP | WS_CLIPCHILDREN, 0, 0, pw, ph, owner,
                          nullptr, instance, this);
  if (!hwnd_) {
    popup_mode_ = false;
    quit_on_close_ = true;
    return false;
  }

  dpi_ = QueryHwndDpi(hwnd_);
  canvas_.SetDpi(dpi_);

  if (!canvas_.InitLayered(hwnd_)) {
    DestroyHostHwnd();
    popup_mode_ = false;
    quit_on_close_ = true;
    return false;
  }

  ImmAssociateContextEx(hwnd_, NULL, 0);

  theme_sink_ = [this] { Invalidate(); };
  Theme::AddInvalidateSink(&theme_sink_);

  if (Window* owner_ui = FromHwnd(owner)) {
    theme_name_ = owner_ui->theme_name_;
  }

  layout_dirty_ = true;
  return true;
}

void Window::PlaceWindow(HWND owner, int width_dip, int height_dip) {
  if (!hwnd_) {
    return;
  }

  const int pw = DipToOuterPx(static_cast<float>(width_dip), dpi_);
  const int ph = DipToOuterPx(static_cast<float>(height_dip), dpi_);

  int x = 0;
  int y = 0;
  HMONITOR mon = nullptr;
  bool center_owner = options_.center_on_owner && owner && IsWindow(owner);
  if (center_owner) {
    RECT orc = {};
    GetWindowRect(owner, &orc);
    const int ow = orc.right - orc.left;
    const int oh = orc.bottom - orc.top;
    // Degenerate/hidden owners (e.g. 0x0 message anchors) would pin the
    // dialog to a corner after work-area clamp — fall back to work center.
    if (ow < 32 || oh < 32) {
      center_owner = false;
    } else {
      x = orc.left + (ow - pw) / 2;
      y = orc.top + (oh - ph) / 2;
      mon = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
    }
  }
  if (!center_owner) {
    POINT pt = {};
    GetCursorPos(&pt);
    mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  }

  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (mon && GetMonitorInfoW(mon, &mi)) {
    const RECT& work = mi.rcWork;
    if (!center_owner) {
      x = work.left + (work.right - work.left - pw) / 2;
      y = work.top + (work.bottom - work.top - ph) / 2;
    }
    if (x + pw > work.right) {
      x = work.right - pw;
    }
    if (y + ph > work.bottom) {
      y = work.bottom - ph;
    }
    if (x < work.left) {
      x = work.left;
    }
    if (y < work.top) {
      y = work.top;
    }
  }

  SetWindowPos(hwnd_, options_.topmost ? HWND_TOPMOST : HWND_TOP, x, y, pw, ph,
               SWP_FRAMECHANGED);
}

void Window::ApplyChromeShape() {
  if (!hwnd_ || !uses_custom_chrome()) {
    return;
  }
  // Resizable frames must stay rectangular for DWM. SetWindowRgn punches
  // alpha holes; live resize then shows the desktop. Win11 rounding is DWM.
  if (options_.resizable) {
    SetWindowRgn(hwnd_, nullptr, FALSE);
    return;
  }
  RECT wr = {};
  GetWindowRect(hwnd_, &wr);
  const int w = wr.right - wr.left;
  const int h = wr.bottom - wr.top;
  if (w <= 0 || h <= 0) {
    return;
  }
  const float r_dip = options_.corner_radius;
  if (r_dip <= 0.f) {
    SetWindowRgn(hwnd_, nullptr, FALSE);
    return;
  }
  int dia = DipToOuterPx(r_dip, dpi_) * 2;
  const int limit = (std::min)(w, h);
  if (dia > limit) {
    dia = limit;
  }
  if (dia < 2) {
    SetWindowRgn(hwnd_, nullptr, FALSE);
    return;
  }
  HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, dia, dia);
  SetWindowRgn(hwnd_, rgn, FALSE);
}

void Window::ApplyChromeDwm() {
  if (!hwnd_ || !uses_custom_chrome()) {
    return;
  }
  const MARGINS margins = {0, 0, 0, 0};
  DwmExtendFrameIntoClientArea(hwnd_, &margins);
  const BOOL disable_transitions = TRUE;
  DwmSetWindowAttribute(hwnd_, DWMWA_TRANSITIONS_FORCEDISABLED,
                        &disable_transitions, sizeof(disable_transitions));
  DWM_BLURBEHIND bb = {};
  bb.dwFlags = DWM_BB_ENABLE;
  bb.fEnable = FALSE;
  DwmEnableBlurBehindWindow(hwnd_, &bb);
  const DWORD backdrop_none = kDwmsbtNone;
  DwmSetWindowAttribute(hwnd_, kDwmwaSystemBackdropType, &backdrop_none,
                        sizeof(backdrop_none));
  // COLOR_NONE: keep WS_CAPTION for snap/min/max, but do not let DWM paint
  // the default title bar (that was the opaque strip that stopped flicker).
  const COLORREF none = kDwmwaColorNone;
  DwmSetWindowAttribute(hwnd_, kDwmwaCaptionColor, &none, sizeof(none));
  DwmSetWindowAttribute(hwnd_, kDwmwaBorderColor, &none, sizeof(none));
  const DWORD corner =
      options_.corner_radius > 0.f ? kDwmwcpRound : kDwmwcpDoNotRound;
  DwmSetWindowAttribute(hwnd_, kDwmwaWindowCornerPreference, &corner,
                        sizeof(corner));
}

void Window::AdjustCustomChromeClient(RECT* r) const {
  if (!r || !is_maximized()) {
    return;
  }
  const UINT dpi = static_cast<UINT>(std::lround(dpi_));
  const int dx = SystemMetricForDpi(SM_CXFRAME, dpi) +
                 SystemMetricForDpi(SM_CXPADDEDBORDER, dpi);
  const int dy = SystemMetricForDpi(SM_CYFRAME, dpi) +
                 SystemMetricForDpi(SM_CXPADDEDBORDER, dpi);
  r->left += dx;
  r->top += dy;
  r->right -= dx;
  r->bottom -= dy;
}

void Window::PaintChrome(mx::Canvas& canvas) {
  if (!uses_custom_chrome()) {
    return;
  }
  const float bw = options_.border_width;
  const float radius = options_.corner_radius;
  if (bw <= 0.f) {
    return;
  }
  const RectF client = ClientRectF();
  const float inset = bw * 0.5f;
  const RectF stroke{client.x + inset, client.y + inset,
                     std::max(0.f, client.w - bw),
                     std::max(0.f, client.h - bw)};
  const float rr = std::max(0.f, radius - inset);
  canvas.DrawRoundedRect(stroke, rr, rr, Theme::Active().border, bw);
}

void Window::ActivateHwnd() {
  if (!hwnd_) {
    return;
  }
  AllowSetForegroundWindow(ASFW_ANY);
  ShowWindow(hwnd_, SW_SHOWNORMAL);
  HWND fg = GetForegroundWindow();
  DWORD fg_tid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
  const DWORD cur_tid = GetCurrentThreadId();
  const bool attached =
      (fg_tid != 0 && fg_tid != cur_tid) &&
      AttachThreadInput(cur_tid, fg_tid, TRUE);
  SetWindowPos(hwnd_, options_.topmost ? HWND_TOPMOST : HWND_TOP, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  BringWindowToTop(hwnd_);
  SetForegroundWindow(hwnd_);
  SetActiveWindow(hwnd_);
  SetFocus(hwnd_);
  if (attached) {
    AttachThreadInput(cur_tid, fg_tid, FALSE);
  }
}

void Window::RestoreOwner() {
  if (dialog_owner_ && IsWindow(dialog_owner_)) {
    EnableWindow(dialog_owner_, TRUE);
  }
}

int Window::RunModal() {
  if (!hwnd_ || !uses_custom_chrome()) {
    return IDABORT;
  }
  MessageLoop* base_loop = MessageLoop::current();
  if (!base_loop || base_loop->type() != MessageLoop::TYPE_UI) {
    return IDABORT;
  }
  MessageLoopForUI* loop = MessageLoopForUI::current();
  if (!loop) {
    return IDABORT;
  }
  HideTooltip();
  if (dialog_owner_) {
    if (Window* owner_ui = FromHwnd(dialog_owner_)) {
      owner_ui->HideTooltip();
    }
  }
  modal_result_ = IDCANCEL;
  modal_running_ = true;
  if (dialog_owner_ && IsWindow(dialog_owner_)) {
    EnableWindow(dialog_owner_, FALSE);
  }
  ActivateHwnd();
  ModalDispatcher dispatcher(hwnd_);
  loop->Run(&dispatcher);
  modal_running_ = false;
  RestoreOwner();
  if (hwnd_) {
    DestroyHostHwnd();
  }
  return modal_result_;
}

void Window::EndModal(int result) {
  modal_result_ = result;
  HideTooltip();
  if (modal_running_ && MessageLoop::current()) {
    MessageLoop::current()->Quit();
  }
  if (hwnd_ && !modal_running_) {
    DestroyHostHwnd();
  }
}

void Window::Close() {
  if (modal_running_) {
    EndModal(IDCANCEL);
    return;
  }
  HideTooltip();
  if (hwnd_) {
    DestroyHostHwnd();
  }
}

void Window::Minimize() {
  if (hwnd_) {
    // ShowWindow, not nested WM_SYSCOMMAND/SC_MINIMIZE: DefWindowProc ignores
    // SC_MINIMIZE while we are still inside WM_LBUTTONUP (TitleBar min button).
    ShowWindow(hwnd_, SW_MINIMIZE);
  }
}

void Window::ToggleMaximize() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
}

bool Window::is_maximized() const {
  return hwnd_ && IsZoomed(hwnd_) != FALSE;
}

bool Window::set_theme(std::string name) {
  if (!name.empty() && !Theme::Has(name)) {
    return false;
  }
  if (theme_name_ == name) {
    return true;
  }
  theme_name_ = std::move(name);
  Invalidate();
  return true;
}

void Window::SetRoot(std::unique_ptr<Node> root) {
  RestorePopupRootBg();
  ClearPopup();
  SetFocusNode(nullptr);
  root_ = std::move(root);
  if (root_) {
    root_->set_host_window(this);
  }
  StealPopupRootBg();
  mouse_capture_ = nullptr;
  hovered_ = nullptr;
  layout_dirty_ = true;
  Invalidate();
}

std::unique_ptr<Node> Window::ReleaseRoot() {
  RestorePopupRootBg();
  ClearPopup();
  SetFocusNode(nullptr);
  mouse_capture_ = nullptr;
  hovered_ = nullptr;
  if (root_) {
    root_->set_host_window(nullptr);
  }
  layout_dirty_ = true;
  return std::move(root_);
}

void Window::SetPopup(std::unique_ptr<Node> popup,
                      std::function<void()> on_dismiss, Node* anchor,
                      const RectF* fixed_bounds) {
  if (popup_) {
    ClearPopup();
  }
  popup_ = std::move(popup);
  if (popup_) {
    popup_->set_host_window(this);
  }
  popup_dismiss_ = std::move(on_dismiss);
  popup_anchor_ = anchor;
  popup_fixed_bounds_ =
      fixed_bounds ? std::optional<RectF>(*fixed_bounds) : std::nullopt;
  Invalidate();
}

void Window::ClearPopup() {
  clear_popup_pending_ = false;
  deferred_focus_ = nullptr;
  if (popup_) {
    if (focused_ &&
        (focused_ == popup_.get() || IsDescendantOf(focused_, popup_.get()))) {
      Node* restore = popup_anchor_;
      if (!restore || !restore->focusable()) {
        restore = nullptr;
      }
      SetFocusNode(restore);
    }
    if (hovered_ &&
        (hovered_ == popup_.get() || IsDescendantOf(hovered_, popup_.get()))) {
      hovered_ = nullptr;
    }
    if (mouse_capture_ &&
        (mouse_capture_ == popup_.get() ||
         IsDescendantOf(mouse_capture_, popup_.get()))) {
      mouse_capture_ = nullptr;
      if (hwnd_ && GetCapture() == hwnd_) {
        ReleaseCapture();
      }
    }
  }
  popup_.reset();
  popup_anchor_ = nullptr;
  popup_fixed_bounds_.reset();
  if (popup_dismiss_) {
    auto dismiss = std::move(popup_dismiss_);
    popup_dismiss_ = nullptr;
    dismiss();
  }
  Invalidate();
}

void Window::RequestClearPopup() {
  if (popup_ || popup_dismiss_) {
    clear_popup_pending_ = true;
  }
}

void Window::DeferFocusNode(Node* node) {
  deferred_focus_ = node;
}

void Window::Defer(std::function<void()> fn) {
  if (!fn) {
    return;
  }
  if (input_dispatch_depth_ > 0) {
    deferred_fns_.push_back(std::move(fn));
    return;
  }
  fn();
}

void Window::FlushDeferred() {
  std::vector<std::function<void()>> batch;
  batch.swap(deferred_fns_);
  for (std::function<void()>& fn : batch) {
    if (fn) {
      fn();
    }
  }
}

Window::InputDispatchGuard::InputDispatchGuard(Window* win)
    : w(win), alive(win ? win->alive_flag() : nullptr) {
  if (w) {
    ++w->input_dispatch_depth_;
  }
}

Window::InputDispatchGuard::~InputDispatchGuard() {
  if (!w || !alive || !alive->load()) {
    return;
  }
  --w->input_dispatch_depth_;
  w->FlushDeferred();
}

void Window::SyncPopupLayout() {
  if (!popup_) {
    return;
  }
  if (popup_fixed_bounds_) {
    popup_->Layout(*popup_fixed_bounds_);
    return;
  }
  if (!popup_anchor_) {
    return;
  }
  const RectF a = popup_anchor_->bounds();
  const RectF client = ClientRectF();
  // Dismiss when the anchor has scrolled/clipped fully out of the client.
  const bool visible =
      a.x < client.x + client.w && a.x + a.w > client.x &&
      a.y < client.y + client.h && a.y + a.h > client.y;
  if (!visible) {
    RequestClearPopup();
    return;
  }

  float h = popup_->bounds().h;
  if (h <= 0.f) {
    h = popup_->Measure(a.w, 200.f).h;
  }
  if (h <= 0.f) {
    h = 120.f;
  }
  float y = a.y + a.h + 2.f;
  if (y + h > client.y + client.h && a.y - 2.f - h >= client.y) {
    y = a.y - 2.f - h;
  }
  popup_->Layout(RectF{a.x, y, a.w, h});
}

void Window::Invalidate() {
  if (!hwnd_) {
    return;
  }
  // InvalidateRect immediately. Do not PostTask-coalesce: nestable tasks can
  // still be deferred relative to the current WM_* handler, and nested modal
  // MessageLoop runs (password/settings) then show stale UI until the dialog
  // closes — typing works (Enter submits) but bullets never appear.
  invalidate_posted_ = false;
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void Window::InvalidateNode(const Node* node) {
  if (!hwnd_ || !node) {
    return;
  }
  const RectF b = node->bounds();
  if (b.w <= 0.f || b.h <= 0.f) {
    return;
  }
  RECT rc;
  rc.left = static_cast<LONG>(std::floor(mx::PxFromDip(b.x, dpi_))) - 2;
  rc.top = static_cast<LONG>(std::floor(mx::PxFromDip(b.y, dpi_))) - 2;
  rc.right =
      static_cast<LONG>(std::ceil(mx::PxFromDip(b.x + b.w, dpi_))) + 2;
  rc.bottom =
      static_cast<LONG>(std::ceil(mx::PxFromDip(b.y + b.h, dpi_))) + 2;
  invalidate_posted_ = false;
  InvalidateRect(hwnd_, &rc, FALSE);
}

void Window::RequestLayout() {
  layout_dirty_ = true;
  Invalidate();
}

void Window::ReleaseNodePointers(const Node* subtree) {
  if (!subtree) {
    return;
  }
  if (focused_ && (focused_ == subtree || IsDescendantOf(focused_, subtree))) {
    SetFocusNode(nullptr);
  }
  if (hovered_ && (hovered_ == subtree || IsDescendantOf(hovered_, subtree))) {
    DismissTooltip();
    hovered_ = nullptr;
  }
  if (mouse_capture_ &&
      (mouse_capture_ == subtree || IsDescendantOf(mouse_capture_, subtree))) {
    if (hwnd_ && GetCapture() == hwnd_) {
      ReleaseCapture();
    }
    mouse_capture_ = nullptr;
  }
}

void Window::CollectFocusable(Node* node, std::vector<Node*>* out) {
  if (!node || !out || !node->visible()) {
    return;
  }
  if (node->focusable()) {
    out->push_back(node);
  }
  for (const auto& child : node->children()) {
    if (child) {
      CollectFocusable(child.get(), out);
    }
  }
}

Button* Window::FindDefaultButton(Node* node) {
  if (!node || !node->visible()) {
    return nullptr;
  }
  if (auto* btn = dynamic_cast<Button*>(node)) {
    if (btn->is_default() && btn->enabled()) {
      return btn;
    }
  }
  for (const auto& child : node->children()) {
    if (Button* found = FindDefaultButton(child.get())) {
      return found;
    }
  }
  return nullptr;
}

Button* Window::default_button() const {
  return FindDefaultButton(root_.get());
}

bool Window::ActivateDefaultButton() {
  Button* btn = default_button();
  if (!btn) {
    return false;
  }
  return btn->AccInvoke();
}

Button* Window::FindAcceleratorButton(Node* node, const KeyChord& chord) {
  if (!node || !node->visible()) {
    return nullptr;
  }
  if (auto* btn = dynamic_cast<Button*>(node)) {
    if (btn->enabled() && btn->accelerator().vk != 0 &&
        btn->accelerator() == chord) {
      return btn;
    }
  }
  for (const auto& child : node->children()) {
    if (Button* found = FindAcceleratorButton(child.get(), chord)) {
      return found;
    }
  }
  return nullptr;
}

bool Window::AddAccelerator(KeyChord chord, std::function<void()> handler) {
  if (!chord.IsShortcut() || !handler) {
    return false;
  }
  accelerators_.push_back({chord, std::move(handler)});
  return true;
}

bool Window::AddAccelerator(const std::string& spec,
                           std::function<void()> handler) {
  KeyChord chord;
  if (!ParseKeyChord(spec, &chord)) {
    return false;
  }
  return AddAccelerator(chord, std::move(handler));
}

void Window::ClearAccelerators() {
  accelerators_.clear();
}

bool Window::ProcessAccelerator(const KeyEvent& e) {
  if (!e.down) {
    return false;
  }
  for (auto it = accelerators_.rbegin(); it != accelerators_.rend(); ++it) {
    if (it->chord.Matches(e) && it->handler) {
      it->handler();
      return true;
    }
  }
  if (Button* btn = FindAcceleratorButton(root_.get(), KeyChord{
          e.vk, e.ctrl, e.alt, e.shift})) {
    return btn->AccInvoke();
  }
  return false;
}

bool Window::HandleKey(const KeyEvent& e) {
  InputDispatchGuard guard(this);
  if (!e.down) {
    if (focused_) {
      focused_->OnKey(e);
    }
    return false;
  }

  if (popup_mode_ && e.vk == VK_ESCAPE) {
    if (PopupHost* host = PopupHost::Current()) {
      const size_t d = host->depth();
      if (d >= 1) {
        host->RequestDismissFrom(d - 1);
      }
      if (host->FlushPendingDismiss()) {
        return true;
      }
    }
  }

  if ((drag_armed_ || drag_active_) && e.vk == VK_ESCAPE) {
    CancelDrag();
    if (hwnd_ && GetCapture() == hwnd_) {
      ReleaseCapture();
    }
    mouse_capture_ = nullptr;
    Invalidate();
    return true;
  }

  if (modal_running_ && e.vk == VK_ESCAPE) {
    EndModal(IDCANCEL);
    return true;
  }

  if (!popup_mode_ && e.vk == VK_RETURN) {
    const bool keep_enter = focused_ && focused_->ConsumesEnter();
    if (!keep_enter && ActivateDefaultButton()) {
      Invalidate();
      return true;
    }
  }

  if (!popup_mode_ && ProcessAccelerator(e)) {
    Invalidate();
    return true;
  }

  if (!focused_) {
    return false;
  }
  focused_->OnKey(e);
  Invalidate();
  return false;
}

void Window::BeginCaptionDrag() {
  if (!hwnd_) {
    return;
  }
  mouse_capture_ = nullptr;
  if (GetCapture() == hwnd_) {
    ReleaseCapture();
  }
  SendMessageW(hwnd_, WM_SYSCOMMAND, static_cast<WPARAM>(SC_MOVE | HTCAPTION),
               0);
}

int Window::HitTestResizeBorder(float client_x, float client_y) const {
  if (!EdgeResizeEnabled()) {
    return HTNOWHERE;
  }
  const RectF rc = ClientRectF();
  const float edge = std::max(kResizeEdgeDip, options_.border_width);
  return HitTestResizeEdge(client_x, client_y, rc.w, rc.h, edge,
                           kResizeCornerDip);
}

int Window::HitTestResizeEdge(float x, float y, float w, float h,
                              float thickness, float corner) {
  if (w <= 1.f || h <= 1.f || thickness <= 0.f) {
    return HTNOWHERE;
  }
  if (x < 0.f || y < 0.f || x >= w || y >= h) {
    return HTNOWHERE;
  }
  const float e = thickness;
  const float c = std::max(thickness, corner);
  if (x < e) {
    if (y < c) {
      return HTTOPLEFT;
    }
    if (y >= h - e) {
      return HTBOTTOMLEFT;
    }
    return HTLEFT;
  }
  if (x >= w - e) {
    if (y < c) {
      return HTTOPRIGHT;
    }
    if (y >= h - e) {
      return HTBOTTOMRIGHT;
    }
    return HTRIGHT;
  }
  if (y < e) {
    if (x < c) {
      return HTTOPLEFT;
    }
    if (x >= w - c) {
      return HTTOPRIGHT;
    }
    return HTTOP;
  }
  if (y >= h - e) {
    if (x < c) {
      return HTBOTTOMLEFT;
    }
    if (x >= w - c) {
      return HTBOTTOMRIGHT;
    }
    return HTBOTTOM;
  }
  return HTNOWHERE;
}

bool Window::EdgeResizeEnabled() const {
  return hwnd_ && uses_custom_chrome() && options_.resizable &&
         !is_maximized();
}

bool Window::HitBlocksResize(Node* hit) const {
  for (Node* n = hit; n; n = n->parent()) {
    if (n->focusable()) {
      return true;
    }
  }
  return false;
}

void Window::UpdateResizeCursor(float x, float y, Node* hit) {
  if (!EdgeResizeEnabled() || mouse_capture_ || drag_active_) {
    return;
  }
  const RectF rc = ClientRectF();
  const float edge = std::max(kResizeEdgeDip, options_.border_width);
  const int ht =
      HitTestResizeEdge(x, y, rc.w, rc.h, edge, kResizeCornerDip);
  if (ht != HTNOWHERE && !HitBlocksResize(hit)) {
    SetCursor(LoadCursorW(nullptr, CursorForResizeHit(ht)));
  }
}

bool Window::TryBeginEdgeResize(const MouseEvent& ev, Node* hit) {
  if (!EdgeResizeEnabled() || ev.button != MouseButton::Left) {
    return false;
  }
  if (HitBlocksResize(hit)) {
    return false;
  }
  const RectF rc = ClientRectF();
  const float edge = std::max(kResizeEdgeDip, options_.border_width);
  const int ht =
      HitTestResizeEdge(ev.x, ev.y, rc.w, rc.h, edge, kResizeCornerDip);
  if (ht == HTNOWHERE) {
    return false;
  }
  BeginEdgeResize(ht, ev.x, ev.y);
  return true;
}

void Window::BeginEdgeResize(int ht, float client_x, float client_y) {
  if (!hwnd_) {
    return;
  }
  CancelDrag();
  mouse_capture_ = nullptr;
  if (GetCapture() == hwnd_) {
    ReleaseCapture();
  }
  POINT pt = {static_cast<LONG>(mx::PxFromDip(client_x, dpi_)),
              static_cast<LONG>(mx::PxFromDip(client_y, dpi_))};
  ClientToScreen(hwnd_, &pt);
  SendMessageW(hwnd_, WM_NCLBUTTONDOWN, static_cast<WPARAM>(ht),
               MAKELPARAM(pt.x, pt.y));
}

void Window::ApplyMinMaxInfo(MINMAXINFO* info) const {
  if (!info) {
    return;
  }
  int min_w = options_.min_width;
  int min_h = options_.min_height;
  if (uses_custom_chrome() && options_.resizable) {
    if (min_w <= 0) {
      min_w = kDefaultMinWidthDip;
    }
    if (min_h <= 0) {
      min_h = kDefaultMinHeightDip;
    }
  }
  if (min_w > 0) {
    info->ptMinTrackSize.x = DipToOuterPx(static_cast<float>(min_w), dpi_);
  }
  if (min_h > 0) {
    info->ptMinTrackSize.y = DipToOuterPx(static_cast<float>(min_h), dpi_);
  }
  if (uses_custom_chrome() && hwnd_) {
    HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (mon && GetMonitorInfoW(mon, &mi)) {
      const RECT& work = mi.rcWork;
      info->ptMaxPosition.x = work.left;
      info->ptMaxPosition.y = work.top;
      info->ptMaxSize.x = work.right - work.left;
      info->ptMaxSize.y = work.bottom - work.top;
    }
  }
}

void Window::ArmDrag(Node* hit, const MouseEvent& ev) {
  CancelDrag();
  if (!hit || popup_mode_ || ev.button != MouseButton::Left) {
    return;
  }
  if (dynamic_cast<TitleBar*>(hit)) {
    return;
  }
  Node* src = ResolveDraggable(hit);
  if (!src) {
    return;
  }
  drag_source_ = src;
  drag_armed_ = true;
  drag_active_ = false;
  drag_origin_x_ = ev.x;
  drag_origin_y_ = ev.y;
}

void Window::UpdateDrag(const MouseEvent& ev, Node* /*hit*/) {
  if (!drag_armed_ || !drag_source_ || drag_active_) {
    return;
  }
  const float dx = ev.x - drag_origin_x_;
  const float dy = ev.y - drag_origin_y_;
  if (dx * dx + dy * dy < kDragSlopDip * kDragSlopDip) {
    return;
  }
  drag_active_ = true;
  HideTooltip();
  if (mouse_capture_) {
    mouse_capture_->OnMouseLeave(ev);
  }
  SetCursor(LoadCursorW(nullptr, IDC_HAND));
}

void Window::FinishDrag(const MouseEvent& ev, Node* hit) {
  Node* source = drag_source_;
  Node* target = drag_active_ ? ResolveDropTarget(hit, source) : nullptr;
  const bool active = drag_active_;
  CancelDrag();
  if (!active || !source || !target) {
    return;
  }
  DragEvent drop;
  drop.data = source->drag_data();
  drop.source = source;
  drop.target = target;
  drop.x = ev.x;
  drop.y = ev.y;
  target->DeliverDrop(drop);
}

void Window::CancelDrag() {
  drag_source_ = nullptr;
  drag_armed_ = false;
  drag_active_ = false;
  SetCursor(LoadCursorW(nullptr, IDC_ARROW));
}

void Window::set_accept_files(bool on) {
  accept_files_ = on;
  ApplyAcceptFiles();
}

void Window::ApplyAcceptFiles() {
  if (!hwnd_) {
    return;
  }
  DragAcceptFiles(hwnd_, accept_files_ ? TRUE : FALSE);
}

void Window::HandleDropFiles(HANDLE drop_handle) {
  const HDROP drop = static_cast<HDROP>(drop_handle);
  if (!drop) {
    return;
  }
  FileDropEvent ev;
  if (accept_files_) {
    POINT pt = {};
    DragQueryPoint(drop, &pt);
    ev.x = mx::DipFromPx(static_cast<float>(pt.x), dpi_);
    ev.y = mx::DipFromPx(static_cast<float>(pt.y), dpi_);
    const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    ev.paths.reserve(n);
    for (UINT i = 0; i < n; ++i) {
      const UINT len = DragQueryFileW(drop, i, nullptr, 0);
      if (len == 0) {
        continue;
      }
      std::wstring path(static_cast<size_t>(len), L'\0');
      DragQueryFileW(drop, i, path.data(), len + 1);
      while (!path.empty() && path.back() == L'\0') {
        path.pop_back();
      }
      if (!path.empty()) {
        ev.paths.push_back(std::move(path));
      }
    }
  }
  DragFinish(drop);
  if (accept_files_ && on_files_dropped_ && !ev.paths.empty()) {
    on_files_dropped_(ev);
  }
}

void Window::SetFocusNode(Node* node) {
  if (focused_ == node) {
    return;
  }
  if (node && !node->focusable()) {
    return;
  }
  if (focused_) {
    focused_->set_focused(false);
    focused_->OnBlur();
  }
  focused_ = node;
  ime_char_suppress_ = 0;
  if (focused_) {
    focused_->set_focused(true);
    focused_->OnFocus();
    if (hwnd_) {
      ::SetFocus(hwnd_);
    }
  }
  UpdateImeAssociation();
  if (uia_root_) {
    RaiseAccFocusChanged();
  }
  Invalidate();
}

void Window::FocusNext(bool reverse) {
  if (!root_) {
    return;
  }
  std::vector<Node*> list;
  CollectFocusable(root_.get(), &list);
  if (list.empty()) {
    SetFocusNode(nullptr);
    return;
  }

  int index = -1;
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i] == focused_) {
      index = static_cast<int>(i);
      break;
    }
  }

  if (index < 0) {
    SetFocusNode(reverse ? list.back() : list.front());
    return;
  }

  const int n = static_cast<int>(list.size());
  const int next = reverse ? (index - 1 + n) % n : (index + 1) % n;
  SetFocusNode(list[static_cast<size_t>(next)]);
}

void Window::NotifyDeviceLost() {
  if (root_) {
    root_->OnDeviceLost();
  }
  if (popup_) {
    popup_->OnDeviceLost();
  }
}

void Window::ClearHover() {
  DismissTooltip();
  if (!hovered_) {
    return;
  }
  MouseEvent ev;
  hovered_->OnMouseLeave(ev);
  InvalidateNode(hovered_);
  hovered_ = nullptr;
}

void Window::EnsureMouseLeaveTracking() {
  if (tracking_mouse_leave_ || !hwnd_) {
    return;
  }
  TRACKMOUSEEVENT tme = {};
  tme.cbSize = sizeof(tme);
  tme.dwFlags = TME_LEAVE;
  tme.hwndTrack = hwnd_;
  if (TrackMouseEvent(&tme)) {
    tracking_mouse_leave_ = true;
  }
}

void Window::UpdateImeAssociation() {
  if (!hwnd_) {
    return;
  }
  if (focused_ && focused_->WantsIme()) {
    ImmAssociateContextEx(hwnd_, NULL, IACE_DEFAULT);
    UpdateImeCandidatePos();
  } else {
    ImmAssociateContextEx(hwnd_, NULL, 0);
  }
}

void Window::UpdateImeCandidatePos() {
  if (!hwnd_ || !focused_ || !focused_->WantsIme()) {
    return;
  }
  HIMC himc = ImmGetContext(hwnd_);
  if (!himc) {
    return;
  }
  const RectF b = focused_->bounds();
  COMPOSITIONFORM form = {};
  form.dwStyle = CFS_POINT;
  form.ptCurrentPos.x =
      static_cast<LONG>(mx::PxFromDip(b.x + 10.f, dpi_));
  form.ptCurrentPos.y =
      static_cast<LONG>(mx::PxFromDip(b.y + b.h, dpi_));
  ImmSetCompositionWindow(himc, &form);
  ImmReleaseContext(hwnd_, himc);
}

Window* Window::FromHwnd(HWND hwnd) {
  return reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                 LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Window*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  Window* self = FromHwnd(hwnd);
  if (!self) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  Theme::Scope theme_scope(self->theme_name_);
  return self->HandleMessage(msg, wparam, lparam);
}

LRESULT Window::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case kWmDismissToast:
      DismissToast();
      return 0;

    case WM_DROPFILES:
      HandleDropFiles(reinterpret_cast<HDROP>(wparam));
      return 0;

    case WM_GETOBJECT:
      if (IsUiaGetObject(lparam)) {
        return HandleGetObject(wparam, lparam);
      }
      break;

    case WM_SIZE:
      if (wparam != SIZE_MINIMIZED) {
        OnSize(LOWORD(lparam), HIWORD(lparam));
      }
      return 0;

    case WM_ENTERSIZEMOVE:
      size_move_ = true;
      return 0;

    case WM_EXITSIZEMOVE:
      size_move_ = false;
      ApplyChromeShape();
      InvalidateRect(hwnd_, nullptr, FALSE);
      UpdateWindow(hwnd_);
      return 0;

    case WM_ERASEBKGND:
      // Class has no background brush. Default erase leaves newly exposed
      // pixels as DWM-transparent during live resize.
      return 1;

    case WM_NCPAINT:
      if (uses_custom_chrome()) {
        return 0;
      }
      break;

    case WM_NCACTIVATE:
      // FramelessAppWindow keeps WS_CAPTION for snap/min/max but hides the
      // system title via DWMWA_COLOR_NONE. DefWindowProc on deactivate still
      // paints a default caption strip — flash when PopupHost takes focus.
      if (uses_custom_chrome()) {
        return TRUE;
      }
      break;

    case WM_DPICHANGED: {
      const UINT new_dpi = LOWORD(wparam);
      const RECT* rec = reinterpret_cast<const RECT*>(lparam);
      ApplyDpiChange(new_dpi, rec);
      return 0;
    }

    case WM_DISPLAYCHANGE:
    case WM_PAINT: {
      // Layout (and NativeHost SetWindowPos) before BeginPaint so CLIPCHILDREN
      // on the paint DC matches guest HWNDs.
      EnsureLayout();
      PAINTSTRUCT ps = {};
      BeginPaint(hwnd_, &ps);
      OnPaint(ps.hdc, &ps.rcPaint);
      EndPaint(hwnd_, &ps);
      return 0;
    }

    case WM_GETMINMAXINFO:
      DefWindowProcW(hwnd_, msg, wparam, lparam);
      ApplyMinMaxInfo(reinterpret_cast<MINMAXINFO*>(lparam));
      return 0;

    case WM_NCCALCSIZE:
      if (uses_custom_chrome()) {
        if (wparam) {
          auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
          AdjustCustomChromeClient(&p->rgrc[0]);
        } else if (lparam) {
          AdjustCustomChromeClient(reinterpret_cast<RECT*>(lparam));
        }
        return 0;
      }
      break;

    case WM_NCHITTEST: {
      const LRESULT hit = DefWindowProcW(hwnd_, msg, wparam, lparam);
      if (EdgeResizeEnabled()) {
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd_, &pt);
        const float x =
            mx::DipFromPx(static_cast<float>(pt.x), dpi_);
        const float y =
            mx::DipFromPx(static_cast<float>(pt.y), dpi_);
        const int ht = HitTestResizeBorder(x, y);
        if (ht != HTNOWHERE) {
          return ht;
        }
      }
      return hit;
    }

    case WM_SETCURSOR:
      if (LOWORD(lparam) == HTCLIENT) {
        POINT pt = {};
        GetCursorPos(&pt);
        ScreenToClient(hwnd_, &pt);
        const float x =
            mx::DipFromPx(static_cast<float>(pt.x), dpi_);
        const float y =
            mx::DipFromPx(static_cast<float>(pt.y), dpi_);
        Node* hit = nullptr;
        if (popup_) {
          hit = popup_->HitTest(x, y);
        }
        if (!hit && root_) {
          hit = root_->HitTest(x, y);
        }
        if (EdgeResizeEnabled()) {
          const RectF rc = ClientRectF();
          const float edge = std::max(kResizeEdgeDip, options_.border_width);
          const int ht =
              HitTestResizeEdge(x, y, rc.w, rc.h, edge, kResizeCornerDip);
          if (ht != HTNOWHERE && !HitBlocksResize(hit)) {
            SetCursor(LoadCursorW(nullptr, CursorForResizeHit(ht)));
            return TRUE;
          }
        }
        if (hit && ListHeaderResizeCursorAt(hit, x, y)) {
          SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
          return TRUE;
        }
      }
      break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
      DispatchMouse(msg, wparam, lparam);
      return 0;

    case WM_RBUTTONUP: {
      // We consume mouse messages (no DefWindowProc), so WM_CONTEXTMENU is
      // never synthesized — fire it ourselves from the click point.
      DispatchMouse(msg, wparam, lparam);
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      ClientToScreen(hwnd_, &pt);
      DispatchContextMenu(0, MAKELPARAM(pt.x, pt.y));
      return 0;
    }

    case WM_MOUSELEAVE:
      tracking_mouse_leave_ = false;
      ClearHover();
      return 0;

    case WM_CONTEXTMENU:
      DispatchContextMenu(wparam, lparam);
      return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      if (wparam == VK_TAB) {
        const bool reverse = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        FocusNext(reverse);
        return 0;
      }
      DispatchKey(msg, wparam);
      // Esc may have destroyed this Window via PopupHost — do not touch
      // members after DispatchKey when popup_mode_ was set.
      return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
      DispatchKey(msg, wparam);
      return 0;

    case WM_CHAR:
      DispatchChar(wparam);
      return 0;

    case WM_IME_STARTCOMPOSITION:
      UpdateImeCandidatePos();
      return 0;

    case WM_IME_COMPOSITION:
      HandleImeComposition(lparam);
      // Let DefWindowProc also run when we want candidate UI; we consume string.
      return 0;

    case WM_IME_ENDCOMPOSITION:
      if (focused_) {
        focused_->OnImeEnd();
        Invalidate();
      }
      return 0;

    case WM_IME_CHAR:
      DispatchImeChar(wparam);
      return 0;

    case WM_ACTIVATE:
      // PopupHost menus: inactive + activation target outside stack → dismiss
      // entire stack. Nested Push activates another layer in-stack — keep open.
      // Non-popup windows must fall through to DefWindowProc.
      if (popup_mode_) {
        if (LOWORD(wparam) == WA_INACTIVE && on_deactivate_outside_) {
          HWND other = reinterpret_cast<HWND>(lparam);
          auto cb = on_deactivate_outside_;
          cb(other);
          // |this| may be destroyed if the host dismissed the stack.
        }
        return 0;
      }
      if (LOWORD(wparam) == WA_INACTIVE) {
        HWND other = reinterpret_cast<HWND>(lparam);
        if (!(tooltip_ && tooltip_->OwnsHwnd(other))) {
          HideTooltip();
        }
      }
      break;

    case WM_SETFOCUS:
      Invalidate();
      return 0;

    case WM_KILLFOCUS:
      Invalidate();
      return 0;

    case WM_TIMER:
      if (wparam == kAnimTimerId) {
        if (!anim_driver_.empty()) {
          anim_driver_.Tick(NowSec());
          SyncAnimTimer();
        }
        Invalidate();
        return 0;
      }
      if (wparam == kTooltipTimerId) {
        KillTimer(hwnd_, kTooltipTimerId);
        ShowTooltipFor(hovered_);
        return 0;
      }
      if (wparam == kToastTimerId) {
        KillTimer(hwnd_, kToastTimerId);
        DismissToast();
        return 0;
      }
      break;

    case WM_CAPTURECHANGED:
      if (drag_armed_ || drag_active_) {
        if (reinterpret_cast<HWND>(lparam) != hwnd_) {
          CancelDrag();
        }
      }
      break;

    case WM_DESTROY:
      DisconnectUia();
      HideTooltip();
      if (modal_running_ && MessageLoop::current()) {
        MessageLoop::current()->Quit();
      }
      if (anim_clients_ > 0 || !anim_driver_.empty()) {
        KillTimer(hwnd_, kAnimTimerId);
        anim_clients_ = 0;
      }
      anim_driver_.Clear();
      KillTimer(hwnd_, kToastTimerId);
      canvas_.Shutdown();
      CancelDrag();
      hwnd_ = nullptr;
      popup_mode_ = false;
      mouse_capture_ = nullptr;
      hovered_ = nullptr;
      focused_ = nullptr;
      tracking_mouse_leave_ = false;
      // Optional: Application::Run / single-window demos quit here.
      // Embedded hosts (Shell MessageLoop) must set_quit_on_close(false).
      if (quit_on_close_) {
        PostQuitMessage(0);
      }
      return 0;

    default:
      break;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

void Window::HandleImeComposition(LPARAM lparam) {
  if (!focused_ || !focused_->WantsIme()) {
    return;
  }
  HIMC himc = ImmGetContext(hwnd_);
  if (!himc) {
    return;
  }

  if (lparam & GCS_RESULTSTR) {
    const std::wstring result = ImmGetString(himc, GCS_RESULTSTR);
    if (!result.empty()) {
      focused_->OnImeResult(result);
      ime_char_suppress_ += result.size();
    }
  }
  if (lparam & GCS_COMPSTR) {
    focused_->OnImeComposition(ImmGetString(himc, GCS_COMPSTR));
  }

  ImmReleaseContext(hwnd_, himc);
  UpdateImeCandidatePos();
  Invalidate();
}

RectF Window::ClientRectF() const {
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  return RectF{0.f, 0.f,
               mx::DipFromPx(static_cast<float>(rc.right), dpi_),
               mx::DipFromPx(static_cast<float>(rc.bottom), dpi_)};
}

void Window::OnSize(UINT width, UINT height) {
  canvas_.Resize(width, height);
  layout_dirty_ = true;
  ApplyChromeShape();
  if (canvas_.is_valid()) {
    PaintImmediate();
  } else {
    Invalidate();
  }
}

void Window::PaintImmediate() {
  if (!hwnd_ || painting_) {
    return;
  }
  // WM_PAINT + BeginPaint, not GetDC: DWM only composites the redirection
  // surface from BeginPaint's HDC (same as the earlier present-path bug).
  InvalidateRect(hwnd_, nullptr, FALSE);
  UpdateWindow(hwnd_);
}

void Window::EnsureLayout() {
  if (root_ && layout_dirty_) {
    root_->Layout(ClientRectF());
    layout_dirty_ = false;
  }
  if (popup_) {
    SyncPopupLayout();
  }
}

void Window::ApplyDpiChange(UINT new_dpi, const RECT* suggested) {
  dpi_ = new_dpi > 0 ? static_cast<float>(new_dpi) : mx::kDipDpi;
  canvas_.SetDpi(dpi_);
  if (!popup_mode_ && suggested) {
    SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left,
                 suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  } else if (popup_mode_ && root_) {
    const SizeF dip = root_->Measure(400.f, 800.f);
    SetWindowPos(hwnd_, nullptr, 0, 0, DipToOuterPx(dip.w, dpi_),
                 DipToOuterPx(dip.h, dpi_),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  canvas_.Resize(static_cast<UINT>(rc.right > 0 ? rc.right : 1),
                 static_cast<UINT>(rc.bottom > 0 ? rc.bottom : 1));
  ApplyChromeShape();
  RequestLayout();
  Invalidate();
}

void Window::OnPaint(HDC present_dc, const RECT* present_px) {
  if (!hwnd_ || painting_) {
    return;
  }
  painting_ = true;
  struct PaintingGuard {
    bool* flag;
    ~PaintingGuard() { *flag = false; }
  } guard{&painting_};

  const bool need_init = !canvas_.is_valid();
  if (need_init) {
    const bool ok =
        popup_mode_ ? canvas_.InitLayered(hwnd_) : canvas_.Init(hwnd_);
    if (!ok) {
      return;
    }
  }
  if (need_init) {
    NotifyDeviceLost();
  }

  if (!canvas_.BeginDraw()) {
    return;
  }

  if (popup_mode_) {
    canvas_.Clear(ColorF(0.f, 0.f, 0.f, 0.f));
    PaintPopupFill(canvas_);
  } else {
    canvas_.Clear(Theme::Active().window_bg);
  }

  if (root_) {
    const RectF client = ClientRectF();
    if (layout_dirty_) {
      root_->Layout(client);
      layout_dirty_ = false;
    }
    root_->Paint(canvas_);
  }
  if (popup_) {
    SyncPopupLayout();
    popup_->Paint(canvas_);
  }
  PaintPopupBorder(canvas_);
  PaintChrome(canvas_);

  if (!canvas_.EndDraw(present_dc, present_px)) {
    const bool ok =
        popup_mode_ ? canvas_.InitLayered(hwnd_) : canvas_.Init(hwnd_);
    if (ok) {
      NotifyDeviceLost();
    }
    layout_dirty_ = true;
    Invalidate();
    return;
  }
  if (!popup_mode_ && hwnd_ && present_px) {
    NativeHost::RedrawGuests(hwnd_, *present_px);
  } else if (!popup_mode_ && hwnd_) {
    RECT full = {};
    GetClientRect(hwnd_, &full);
    NativeHost::RedrawGuests(hwnd_, full);
  }
}

MouseButton Window::ButtonFromMsg(UINT msg, WPARAM wparam) {
  switch (msg) {
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
      return MouseButton::Right;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
      return MouseButton::Middle;
    case WM_MOUSEWHEEL:
      if (GET_KEYSTATE_WPARAM(wparam) & MK_RBUTTON) {
        return MouseButton::Right;
      }
      if (GET_KEYSTATE_WPARAM(wparam) & MK_MBUTTON) {
        return MouseButton::Middle;
      }
      return MouseButton::Left;
    default:
      return MouseButton::Left;
  }
}

void Window::DispatchMouse(UINT msg, WPARAM wparam, LPARAM lparam) {
  if (!root_ && !popup_) {
    return;
  }
  InputDispatchGuard guard(this);

  if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK ||
      msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_MOUSEWHEEL) {
    HideTooltip();
  }

  if (msg == WM_MOUSEWHEEL && drag_active_) {
    CancelDrag();
  }

  if (msg == WM_MOUSEMOVE) {
    EnsureMouseLeaveTracking();
  }

  MouseEvent ev;
  ev.button = ButtonFromMsg(msg, wparam);
  ev.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
  ev.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

  // Client coords from Win32 are physical pixels; Node tree is DIP.
  if (msg == WM_MOUSEWHEEL) {
    POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    ScreenToClient(hwnd_, &pt);
    ev.x = mx::DipFromPx(static_cast<float>(pt.x), dpi_);
    ev.y = mx::DipFromPx(static_cast<float>(pt.y), dpi_);
    ev.wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
  } else {
    ev.x = mx::DipFromPx(static_cast<float>(GET_X_LPARAM(lparam)), dpi_);
    ev.y = mx::DipFromPx(static_cast<float>(GET_Y_LPARAM(lparam)), dpi_);
  }

  SyncPopupLayout();

  Node* popup_hit = popup_ ? popup_->HitTest(ev.x, ev.y) : nullptr;
  Node* root_hit = root_ ? root_->HitTest(ev.x, ev.y) : nullptr;

  if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK ||
       msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN) &&
      popup_ && !popup_hit) {
    if (!(popup_anchor_ && root_hit == popup_anchor_)) {
      ClearPopup();
      root_hit = root_ ? root_->HitTest(ev.x, ev.y) : nullptr;
    }
  }

  Node* hit = popup_hit ? popup_hit : root_hit;

  Node* prev_hovered = hovered_;
  bool hover_changed = false;
  if (msg == WM_MOUSEMOVE && !mouse_capture_ && !drag_active_) {
    if (hovered_ != hit) {
      if (hovered_) {
        hovered_->OnMouseLeave(ev);
      }
      hovered_ = hit;
      if (hovered_) {
        hovered_->OnMouseEnter(ev);
      }
      RestartTooltipTimer();
      hover_changed = true;
    }
  }

  if (msg == WM_MOUSEMOVE) {
    UpdateResizeCursor(ev.x, ev.y, hit);
  }
  if (msg == WM_LBUTTONDOWN && TryBeginEdgeResize(ev, hit)) {
    return;
  }

  // PopupHost §4.3: hover/click a non-opener sibling while a child submenu
  // is open → dismiss child layers (before enter so Submenu can re-Push).
  if (popup_mode_ && hit &&
      (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
       msg == WM_MBUTTONDOWN)) {
    if (PopupHost* host = PopupHost::Current()) {
      host->OnPopupHit(this, hit);
    }
  }

  Node* target = mouse_capture_ ? mouse_capture_ : hit;
  if (!target) {
    if (hover_changed) {
      InvalidateNode(prev_hovered);
      InvalidateNode(hovered_);
    }
    return;
  }

  switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
      mouse_capture_ = target;
      SetCapture(hwnd_);
      if (target->focusable()) {
        SetFocusNode(target);
      }
      if (msg == WM_LBUTTONDBLCLK) {
        target->OnMouseDoubleClick(ev);
      } else {
        target->OnMouseDown(ev);
        if (msg == WM_LBUTTONDOWN) {
          ArmDrag(target, ev);
        }
      }
      break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
      if (msg == WM_LBUTTONUP && drag_active_) {
        FinishDrag(ev, hit);
        if (GetCapture() == hwnd_) {
          ReleaseCapture();
        }
        mouse_capture_ = nullptr;
        break;
      }
      CancelDrag();
      target->OnMouseUp(ev);
      if (GetCapture() == hwnd_) {
        ReleaseCapture();
      }
      mouse_capture_ = nullptr;
      // Refresh hover after release (cursor may still be over a control).
      if (hovered_ != hit) {
        if (hovered_) {
          hovered_->OnMouseLeave(ev);
        }
        hovered_ = hit;
        if (hovered_) {
          hovered_->OnMouseEnter(ev);
        }
      }
      break;
    case WM_MOUSEMOVE:
      UpdateDrag(ev, hit);
      if (!drag_active_) {
        target->OnMouseMove(ev);
      }
      break;
    case WM_MOUSEWHEEL: {
      // Prefer first scrollable ancestor from the hit node (ScrollView).
      Node* wheel_target = target;
      while (wheel_target && !wheel_target->WantsMouseWheel()) {
        wheel_target = wheel_target->parent();
      }
      if (wheel_target) {
        wheel_target->OnMouseWheel(ev);
      }
      break;
    }
    default:
      break;
  }

  if (clear_popup_pending_) {
    ClearPopup();
  }

  if (deferred_focus_) {
    Node* node = deferred_focus_;
    deferred_focus_ = nullptr;
    SetFocusNode(node);
  }

  // PopupHost menu dismiss after click/Esc — must run before Invalidate so we
  // do not touch |this| if Flush destroys this Window.
  if (popup_mode_) {
    if (PopupHost* host = PopupHost::Current()) {
      if (host->has_pending_dismiss()) {
        host->FlushPendingDismiss();
        return;
      }
    }
  }

  if (msg == WM_MOUSEMOVE && !drag_active_) {
    if (hover_changed) {
      InvalidateNode(prev_hovered);
      InvalidateNode(hovered_);
    } else if (mouse_capture_) {
      InvalidateNode(mouse_capture_);
    }
    return;
  }

  Invalidate();
}

void Window::DispatchContextMenu(WPARAM /*wparam*/, LPARAM lparam) {
  if (!root_) {
    return;
  }

  POINT screen = {};
  float hit_x = 0.f;
  float hit_y = 0.f;
  if (lparam == static_cast<LPARAM>(-1)) {
    // Keyboard invocation (Shift+F10 / VK_APPS): use focused node or client
    // center. bounds_ / ClientRectF are DIP.
    if (focused_) {
      const RectF b = focused_->bounds();
      hit_x = b.x + b.w * 0.5f;
      hit_y = b.y + b.h * 0.5f;
    } else {
      const RectF c = ClientRectF();
      hit_x = c.w * 0.5f;
      hit_y = c.h * 0.5f;
    }
    POINT client_px{static_cast<LONG>(mx::PxFromDip(hit_x, dpi_)),
                    static_cast<LONG>(mx::PxFromDip(hit_y, dpi_))};
    screen = client_px;
    ClientToScreen(hwnd_, &screen);
  } else {
    screen.x = GET_X_LPARAM(lparam);
    screen.y = GET_Y_LPARAM(lparam);
    POINT client_px = screen;
    ScreenToClient(hwnd_, &client_px);
    hit_x = mx::DipFromPx(static_cast<float>(client_px.x), dpi_);
    hit_y = mx::DipFromPx(static_cast<float>(client_px.y), dpi_);
  }

  Node* hit = root_->HitTest(hit_x, hit_y);
  if (!hit && focused_) {
    hit = focused_;
  }
  if (hit) {
    hit->OnContextMenu(screen.x, screen.y);
  }
}

void Window::DispatchKey(UINT msg, WPARAM wparam) {
  KeyEvent ev;
  ev.vk = static_cast<UINT>(wparam);
  ev.down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
  ev.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
  ev.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
  ev.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
  HandleKey(ev);
}

void Window::DispatchChar(WPARAM wparam) {
  if (!focused_) {
    return;
  }
  // Skip Tab / Return / Esc / Backspace (handled via WM_KEYDOWN).
  if (wparam == VK_TAB || wparam == VK_RETURN || wparam == VK_ESCAPE ||
      wparam == VK_BACK) {
    return;
  }
  // Swallow WM_CHAR duplicates after Imm GCS_RESULTSTR.
  if (ime_char_suppress_ > 0) {
    --ime_char_suppress_;
    return;
  }
  focused_->OnChar(static_cast<wchar_t>(wparam));
  Invalidate();
}

void Window::DispatchImeChar(WPARAM wparam) {
  if (!focused_ || !focused_->WantsIme()) {
    return;
  }
  // Swallow duplicates already committed via WM_IME_COMPOSITION / GCS_RESULTSTR.
  if (ime_char_suppress_ > 0) {
    --ime_char_suppress_;
    return;
  }
  focused_->OnChar(static_cast<wchar_t>(wparam));
  Invalidate();
}

void Window::SyncTooltip() {
  if (hwnd_) {
    KillTimer(hwnd_, kTooltipTimerId);
  }
  if (!hwnd_ || popup_mode_ || modal_running_ || drag_active_) {
    HideTooltip();
    return;
  }
  if (ResolveTooltipText(hovered_)) {
    ShowTooltipFor(hovered_);
    return;
  }
  HideTooltip();
}

void Window::RestartTooltipTimer() {
  if (hwnd_) {
    KillTimer(hwnd_, kTooltipTimerId);
  }
  if (!hwnd_ || popup_mode_ || modal_running_ || drag_active_) {
    HideTooltip();
    return;
  }
  if (!ResolveTooltipText(hovered_)) {
    HideTooltip();
    return;
  }
  const UINT delay = ResolveTooltipShowDelayMs(this, hovered_);
  if (delay == 0) {
    ShowTooltipFor(hovered_);
    return;
  }
  HideTooltip();
  SetTimer(hwnd_, kTooltipTimerId, delay, nullptr);
}

void Window::HideTooltip() {
  if (hwnd_) {
    KillTimer(hwnd_, kTooltipTimerId);
  }
  if (tooltip_) {
    tooltip_->Hide();
  }
}

void Window::DismissTooltip() { HideTooltip(); }

void Window::ShowTooltipFor(const Node* hit) {
  const std::wstring* text = ResolveTooltipText(hit);
  if (!text || !hwnd_) {
    return;
  }
  if (!tooltip_) {
    tooltip_ = std::make_unique<TooltipOverlay>();
  }
  tooltip_->Show(hwnd_, dpi_, *text, false);
}

}  // namespace mx::ui
