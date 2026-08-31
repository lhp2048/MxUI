#pragma once

#include "mx/canvas.h"
#include "mx/ui/anim.h"
#include "mx/ui/node.h"
#include "mx/ui/locale.h"
#include "mx/ui/theme.h"

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mx::ui {

class PopupHost;
class Toast;
class ToastOverlay;
class TooltipOverlay;
class UiaProvider;
class Button;

struct FileDropEvent {
  std::vector<std::wstring> paths;
  float x = 0.f;
  float y = 0.f;
};

class Window {
 public:
  Window();
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  struct WindowOptions {
    HWND owner = nullptr;
    bool caption = true;
    bool quit_on_close = true;
    bool topmost = false;
    bool center_on_owner = true;
    // Custom chrome (caption=false) only. Captioned windows ignore these.
    // Resizable frameless: no SetWindowRgn (DWM treats RGN as glass on resize);
    // Win11 rounding is DWMWA_WINDOW_CORNER_PREFERENCE. Dialogs (!resizable)
    // still use SetWindowRgn for a punched round outline.
    float corner_radius = 0.f;
    float border_width = 0.f;
    // caption=false only. Drag window edges/corners to resize (like HTLEFT).
    // Dialog() turns this off. Captioned windows use the OS frame.
    // Frameless + resizable: independent app HWND (taskbar, min/max).
    // Frameless + !resizable: owned popup (dialogs). Do not minimize those —
    // they have no taskbar button and shrink to a desktop corner.
    bool resizable = true;
    // DIP; 0 = 160×80 when resizable custom chrome.
    int min_width = 0;
    int min_height = 0;

    // Captioned windows drop radius/border. Clamps negatives to 0.
    void Normalize() {
      if (corner_radius < 0.f) {
        corner_radius = 0.f;
      }
      if (border_width < 0.f) {
        border_width = 0.f;
      }
      if (caption) {
        corner_radius = 0.f;
        border_width = 0.f;
      }
    }

    static WindowOptions Dialog(HWND owner = nullptr, float corner_radius = 8.f) {
      WindowOptions o;
      o.owner = owner;
      o.caption = false;
      o.quit_on_close = false;
      o.topmost = true;
      o.center_on_owner = true;
      o.corner_radius = corner_radius;
      o.border_width = 1.f;
      o.resizable = false;
      o.Normalize();
      return o;
    }
  };

  bool Create(const wchar_t* title, int w, int h,
              const WindowOptions& opt = {});

  // Full-screen layered overlay (no owner). Transparent background; topmost.
  bool CreateLayeredOverlay(int width, int height);

  const WindowOptions& options() const { return options_; }

  int RunModal();
  void EndModal(int result);
  // Destroy this HWND. If RunModal is nested, same as EndModal(IDCANCEL).
  void Close();
  int modal_result() const { return modal_result_; }
  // True while a caption=false Window HWND exists (Dialog preset or modeless).
  bool is_dialog() const {
    return hwnd_ && !popup_mode_ && !options_.caption;
  }
  bool is_popup() const { return popup_mode_; }
  bool is_dragging() const { return drag_active_; }

  // When true (default), WM_DESTROY posts WM_QUIT. Host apps that own the
  // message loop (e.g. Family Shell) should set false so closing a UI window
  // does not tear down the process.
  void set_quit_on_close(bool quit) {
    quit_on_close_ = quit;
    options_.quit_on_close = quit;
  }
  bool quit_on_close() const { return quit_on_close_; }

  // Opt-in Explorer / shell file drop (WM_DROPFILES). Default off.
  // Independent of Node::draggable / on_drop.
  void set_accept_files(bool on);
  bool accept_files() const { return accept_files_; }
  using FilesDroppedHandler = std::function<void(const FileDropEvent&)>;
  void set_on_files_dropped(FilesDroppedHandler handler) {
    on_files_dropped_ = std::move(handler);
  }
  void SetRoot(std::unique_ptr<Node> root);
  Node* root() const { return root_.get(); }

  // Empty name inherits process Theme::SetActive. Unknown name → false.
  bool set_theme(std::string name);
  void clear_theme() { set_theme({}); }
  const std::string& theme_name() const { return theme_name_; }
  bool has_theme() const { return !theme_name_.empty(); }
  // Detach root without destroying the window (Submenu return / PopupHost).
  std::unique_ptr<Node> ReleaseRoot();
  // Floating layer above root (Combo dropdown). |on_dismiss| runs on ClearPopup.
  // |anchor| click while open is left to the control (toggle), not dismissed here.
  // |fixed_bounds| when set: SyncPopupLayout keeps that rect (cell editors).
  void SetPopup(std::unique_ptr<Node> popup,
                std::function<void()> on_dismiss = {},
                Node* anchor = nullptr,
                const RectF* fixed_bounds = nullptr);
  void ClearPopup();
  // Safe while handling popup mouse events: runs ClearPopup after dispatch returns.
  void RequestClearPopup();
  // Focus after the current mouse message finishes (popup child editors).
  void DeferFocusNode(Node* node);
  // Run after the current mouse/key dispatch returns (safe to rebuild UI that
  // would destroy the Node still handling on_click / AccInvoke).
  void Defer(std::function<void()> fn);
  Node* popup() const { return popup_.get(); }

  // PopupHost: invoked on WM_ACTIVATE(WA_INACTIVE) with the HWND gaining
  // activation. Host dismisses only when that HWND is outside the stack.
  void set_on_deactivate_outside(std::function<void(HWND)> cb) {
    on_deactivate_outside_ = std::move(cb);
  }

  void Invalidate();
  // Invalidate |node| bounds in pixels (hover / local motion). Full Invalidate
  // still used for layout, theme, and focus.
  void InvalidateNode(const Node* node);
  // Mark layout dirty and repaint (e.g. after visible toggles).
  void RequestLayout();
  // Drop focus/hover/capture if they point into |subtree| (before destroying it).
  void ReleaseNodePointers(const Node* subtree);
  // Shared alive flag for async/coroutines; cleared in destructor.
  std::shared_ptr<std::atomic_bool> alive_flag() const { return alive_; }
  HWND hwnd() const { return hwnd_; }
  float dpi() const { return dpi_; }
  bool PeekDibBgra(int x, int y, uint8_t* b, uint8_t* g, uint8_t* r,
                   uint8_t* a) const {
    return canvas_.PeekDibBgra(x, y, b, g, r, a);
  }

  // Ref-counted ~30fps Invalidate for indeterminate ProgressBar etc.
  void RegisterAnimation();
  void UnregisterAnimation();
  // t in on_tick is eased [0,1]. duration_sec<=0 runs tick(1)+done immediately.
  uint64_t Animate(float duration_sec, Easing easing, AnimationDriver::TickFn on_tick,
                   AnimationDriver::DoneFn on_done = {});
  void CancelAnimation(uint64_t id);
  void set_layered_opacity(float a);
  float layered_opacity() const;

  // Overlay a Toast Node (does not insert into the window tree). Queues if one
  // is already visible. Placement is relative to this window's client area
  // (anchor/margin/offset on Toast). Auto-hide when duration_sec()>0.
  // dismiss_on_click(true) enables click-to-dismiss; default is display-only.
  void ShowToast(std::unique_ptr<Toast> toast);
  void ShowToast(std::unique_ptr<Node> toast);
  void DismissToast();
  bool has_toast() const;
  Toast* toast() const;
  // If the showing toast turned animate off mid-fade, finish dismiss now.
  void SyncToastFade();

  void SetFocusNode(Node* node);
  Node* focused_node() const { return focused_; }
  void FocusNext(bool reverse);

  void RaiseAccToggleChanged(Node* node);
  void RaiseAccValueChanged(Node* node);
  void RaiseAccRangeChanged(Node* node);
  void RaiseAccExpandCollapseChanged(Node* node);
  void RaiseAccStructureChanged();

  // First visible enabled Button with is_default(); null if none.
  Button* default_button() const;
  // Invoke that button. Returns true if one fired.
  bool ActivateDefaultButton();

  // Window shortcut table. Later registrations win. Chord must IsShortcut().
  bool AddAccelerator(KeyChord chord, std::function<void()> handler);
  bool AddAccelerator(const std::string& spec, std::function<void()> handler);
  void ClearAccelerators();
  // Key path used by WndProc; tests can call with explicit modifiers.
  bool HandleKey(const KeyEvent& e);

  // Tooltip defaults for this window (ms). Per-node override via Node::tooltip_show_delay_ms.
  static constexpr UINT kDefaultTooltipShowDelayMs = 400;
  void set_tooltip_show_delay_ms(UINT ms) { tooltip_show_delay_ms_ = ms; }
  UINT tooltip_show_delay_ms() const { return tooltip_show_delay_ms_; }
  // Update tooltip when text changes without hover target change (shows immediately).
  void SyncTooltip();

  void Minimize();
  void ToggleMaximize();
  // Named to avoid windows.h `IsMaximized` → `IsZoomed` macro.
  bool is_maximized() const;

  // TitleBar: after drag slop, release capture and let DefWindowProc move.
  void BeginCaptionDrag();

  // Client-DIP hit test for frameless resize. HTLEFT… / HTNOWHERE.
  static int HitTestResizeEdge(float x, float y, float w, float h,
                               float thickness, float corner);
  // Active frameless window edge (caption=false, resizable). HTNOWHERE if off.
  int HitTestResizeBorder(float client_x, float client_y) const;

 private:
  friend class PopupHost;
  friend class ToastOverlay;
  friend class TooltipOverlay;
  friend class UiaProvider;

  // Layered tool HWND for menus. Not a WindowOptions style — use PopupHost.
  bool CreatePopup(HWND owner, int width, int height);
  void SetPopupChrome(float corner_radius, float border_width);
  bool CreateLayeredTool(HWND owner, int width_dip, int height_dip,
                         DWORD extra_ex);

  static constexpr UINT_PTR kAnimTimerId = 1;
  static constexpr UINT_PTR kTooltipTimerId = 2;
  static constexpr UINT_PTR kToastTimerId = 3;
  static constexpr UINT kWmDismissToast = WM_APP + 54;

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                  LPARAM lparam);
  static Window* FromHwnd(HWND hwnd);
  static bool EnsureWindowClass(HINSTANCE instance);
  static void CollectFocusable(Node* node, std::vector<Node*>* out);
  static Button* FindDefaultButton(Node* node);
  static Button* FindAcceleratorButton(Node* node, const KeyChord& chord);
  bool ProcessAccelerator(const KeyEvent& e);

  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);
  void OnPaint(HDC present_dc = nullptr, const RECT* present_px = nullptr);
  void OnSize(UINT width, UINT height);
  // Blit the current DIB now (live resize). Avoids an empty client flashing
  // through to the desktop before WM_PAINT.
  void PaintImmediate();
  void EnsureLayout();
  void NotifyDeviceLost();
  void ClearHover();
  void EnsureMouseLeaveTracking();
  void DispatchMouse(UINT msg, WPARAM wparam, LPARAM lparam);
  void DispatchContextMenu(WPARAM wparam, LPARAM lparam);
  void DispatchKey(UINT msg, WPARAM wparam);
  void DispatchChar(WPARAM wparam);
  void HandleImeComposition(LPARAM lparam);
  void DispatchImeChar(WPARAM wparam);
  void FlushDeferred();
  struct InputDispatchGuard {
    Window* w = nullptr;
    std::shared_ptr<std::atomic_bool> alive;
    explicit InputDispatchGuard(Window* win);
    ~InputDispatchGuard();
  };
  void UpdateImeAssociation();
  void UpdateImeCandidatePos();
  void SyncPopupLayout();
  void ApplyDpiChange(UINT new_dpi, const RECT* suggested);

  void RestartTooltipTimer();
  void HideTooltip();
  void DismissTooltip();
  void ShowTooltipFor(const Node* hit);
  void ArmDrag(Node* hit, const MouseEvent& ev);
  void UpdateDrag(const MouseEvent& ev, Node* hit);
  void FinishDrag(const MouseEvent& ev, Node* hit);
  void CancelDrag();
  void ApplyAcceptFiles();
  void HandleDropFiles(HANDLE drop);
  void PresentNextToast();
  void SyncAnimTimer();
  static double NowSec();

  bool uses_custom_chrome() const {
    return !popup_mode_ && !options_.caption;
  }
  bool EdgeResizeEnabled() const;
  bool HitBlocksResize(Node* hit) const;
  void UpdateResizeCursor(float x, float y, Node* hit);
  bool TryBeginEdgeResize(const MouseEvent& ev, Node* hit);
  void BeginEdgeResize(int ht, float client_x, float client_y);
  void ApplyMinMaxInfo(MINMAXINFO* info) const;
  void PlaceWindow(HWND owner, int width_dip, int height_dip);
  void ApplyChromeShape();
  void ApplyChromeDwm();
  void AdjustCustomChromeClient(RECT* r) const;
  void PaintChrome(mx::Canvas& canvas);
  void PaintPopupFill(mx::Canvas& canvas);
  void PaintPopupBorder(mx::Canvas& canvas);
  void StealPopupRootBg();
  void RestorePopupRootBg();
  void ActivateHwnd();
  void RestoreOwner();
  void ResetCreateState();
  void DestroyHostHwnd();

  LRESULT HandleGetObject(WPARAM wparam, LPARAM lparam);
  void DisconnectUia();
  void RaiseAccFocusChanged();

  static MouseButton ButtonFromMsg(UINT msg, WPARAM wparam);
  RectF ClientRectF() const;

  HWND hwnd_ = nullptr;
  float dpi_ = mx::kDipDpi;
  mx::Canvas canvas_;
  std::unique_ptr<Node> root_;
  std::unique_ptr<Node> popup_;
  std::function<void()> popup_dismiss_;
  Node* popup_anchor_ = nullptr;
  std::optional<RectF> popup_fixed_bounds_;
  Node* deferred_focus_ = nullptr;
  std::vector<std::function<void()>> deferred_fns_;
  int input_dispatch_depth_ = 0;
  bool clear_popup_pending_ = false;
  std::string theme_name_;
  bool layout_dirty_ = true;
  bool painting_ = false;
  bool size_move_ = false;
  bool quit_on_close_ = true;
  bool accept_files_ = false;
  FilesDroppedHandler on_files_dropped_;
  bool popup_mode_ = false;
  bool modal_running_ = false;
  Node* drag_source_ = nullptr;
  bool drag_armed_ = false;
  bool drag_active_ = false;
  float drag_origin_x_ = 0.f;
  float drag_origin_y_ = 0.f;
  HWND dialog_owner_ = nullptr;
  int modal_result_ = IDCANCEL;
  WindowOptions options_{};
  float popup_radius_ = 0.f;
  float popup_border_ = 0.f;
  std::optional<ColorF> popup_fill_;
  std::function<void(HWND)> on_deactivate_outside_;
  Node* mouse_capture_ = nullptr;
  Node* hovered_ = nullptr;
  Node* focused_ = nullptr;
  bool tracking_mouse_leave_ = false;
  size_t ime_char_suppress_ = 0;
  int anim_clients_ = 0;
  AnimationDriver anim_driver_;
  bool toast_fading_ = false;
  bool invalidate_posted_ = false;
  Theme::InvalidateSink theme_sink_;
  LocaleInvalidateSink locale_sink_;
  std::shared_ptr<std::atomic_bool> alive_ =
      std::make_shared<std::atomic_bool>(true);

  UINT tooltip_show_delay_ms_ = kDefaultTooltipShowDelayMs;
  std::unique_ptr<TooltipOverlay> tooltip_;
  std::unique_ptr<ToastOverlay> toast_overlay_;
  std::deque<std::unique_ptr<Toast>> toast_queue_;
  UiaProvider* uia_root_ = nullptr;
  struct AcceleratorBinding {
    KeyChord chord;
    std::function<void()> handler;
  };
  std::vector<AcceleratorBinding> accelerators_;
};

}  // namespace mx::ui
