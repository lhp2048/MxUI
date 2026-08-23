#include "mx/ui/toast_overlay.h"

#include "mx/ui/toast.h"
#include "mx/ui/window.h"

#include <algorithm>
#include <cmath>

namespace mx::ui {

ToastOverlay::~ToastOverlay() { Hide(); }

void ToastOverlay::Hide() {
  CancelFade();
  if (window_ && window_->hwnd()) {
    ShowWindow(window_->hwnd(), SW_HIDE);
    window_->SetRoot(std::unique_ptr<Node>{});
    window_->set_layered_opacity(1.f);
  }
}

void ToastOverlay::CancelFade() {
  if (fade_id_ && window_) {
    window_->CancelAnimation(fade_id_);
    fade_id_ = 0;
  }
}

void ToastOverlay::FadeOut(float duration_sec, std::function<void()> done) {
  if (!window_ || !window_->hwnd() || duration_sec <= 0.f) {
    if (done) {
      done();
    }
    return;
  }
  CancelFade();
  fade_id_ = window_->Animate(
      duration_sec, Easing::EaseOutCubic,
      [this](float t) { window_->set_layered_opacity(1.f - t); },
      [this, done = std::move(done)]() {
        fade_id_ = 0;
        if (done) {
          done();
        }
      });
}

bool ToastOverlay::showing() const {
  return window_ && window_->hwnd() && IsWindowVisible(window_->hwnd());
}

Toast* ToastOverlay::toast() const {
  return window_ ? dynamic_cast<Toast*>(window_->root()) : nullptr;
}

bool ToastOverlay::OwnsHwnd(HWND hwnd) const {
  return window_ && window_->hwnd() && hwnd == window_->hwnd();
}

bool ToastOverlay::Ensure(HWND owner) {
  if (window_ && window_->hwnd()) {
    return true;
  }
  window_ = std::make_unique<Window>();
  const DWORD ex =
      WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED;
  if (!window_->CreateLayeredTool(owner, 1, 1, ex)) {
    window_.reset();
    return false;
  }
  return true;
}

void ToastOverlay::Place(HWND owner, float owner_dpi, ToastAnchor anchor,
                         float margin_dip, float offset_x_dip, float offset_y_dip,
                         float dip_w, float dip_h) {
  if (!window_ || !window_->hwnd() || !owner) {
    return;
  }
  const int pw =
      static_cast<int>(std::ceil(mx::PxFromDip(dip_w, owner_dpi)));
  const int ph =
      static_cast<int>(std::ceil(mx::PxFromDip(dip_h, owner_dpi)));
  const int margin =
      static_cast<int>(std::ceil(mx::PxFromDip(std::max(0.f, margin_dip), owner_dpi)));
  const int off_x =
      static_cast<int>(std::round(mx::PxFromDip(offset_x_dip, owner_dpi)));
  const int off_y =
      static_cast<int>(std::round(mx::PxFromDip(offset_y_dip, owner_dpi)));

  RECT cr{};
  GetClientRect(owner, &cr);
  POINT tl{cr.left, cr.top};
  POINT br{cr.right, cr.bottom};
  ClientToScreen(owner, &tl);
  ClientToScreen(owner, &br);
  const int cw = br.x - tl.x;
  const int ch = br.y - tl.y;

  int x = tl.x;
  int y = tl.y;
  switch (anchor) {
    case ToastAnchor::BottomCenter:
      x = tl.x + (cw - pw) / 2;
      y = br.y - ph - margin;
      break;
    case ToastAnchor::BottomStart:
      x = tl.x + margin;
      y = br.y - ph - margin;
      break;
    case ToastAnchor::BottomEnd:
      x = br.x - pw - margin;
      y = br.y - ph - margin;
      break;
    case ToastAnchor::TopCenter:
      x = tl.x + (cw - pw) / 2;
      y = tl.y + margin;
      break;
    case ToastAnchor::TopStart:
      x = tl.x + margin;
      y = tl.y + margin;
      break;
    case ToastAnchor::TopEnd:
      x = br.x - pw - margin;
      y = tl.y + margin;
      break;
    case ToastAnchor::Center:
      x = tl.x + (cw - pw) / 2;
      y = tl.y + (ch - ph) / 2;
      break;
  }
  x += off_x;
  y += off_y;

  HMONITOR mon = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (mon && GetMonitorInfoW(mon, &mi)) {
    const RECT& work = mi.rcWork;
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

  SetWindowPos(window_->hwnd(), HWND_TOPMOST, x, y, pw, ph, SWP_NOACTIVATE);
}

bool ToastOverlay::Show(HWND owner, float owner_dpi,
                        std::unique_ptr<Toast> toast) {
  if (!toast || !owner) {
    return false;
  }
  if (!Ensure(owner)) {
    return false;
  }
  CancelFade();
  const bool click_through = !toast->dismiss_on_click();
  const ToastAnchor anchor = toast->anchor();
  const float margin = toast->margin();
  const float offset_x = toast->offset_x();
  const float offset_y = toast->offset_y();
  const SizeF sz = toast->Measure(360.f, 120.f);
  const float tw = std::max(sz.w, 80.f);
  const float th = std::max(sz.h, 32.f);
  window_->set_layered_opacity(1.f);
  window_->SetRoot(std::move(toast));
  if (HWND thwnd = window_->hwnd()) {
    LONG ex = GetWindowLongW(thwnd, GWL_EXSTYLE);
    if (click_through) {
      ex |= WS_EX_TRANSPARENT;
    } else {
      ex &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongW(thwnd, GWL_EXSTYLE, ex);
  }
  Place(owner, owner_dpi, anchor, margin, offset_x, offset_y, tw, th);
  ShowWindow(window_->hwnd(), SW_SHOWNOACTIVATE);
  window_->OnPaint();
  return true;
}

}  // namespace mx::ui
