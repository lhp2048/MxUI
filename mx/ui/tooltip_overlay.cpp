#include "mx/ui/tooltip_overlay.h"

#include "mx/canvas.h"
#include "mx/ui/column.h"
#include "mx/ui/label.h"
#include "mx/ui/theme.h"
#include "mx/ui/window.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mx::ui {
namespace {

// Longer than kUiAnimSec (0.15): opacity fades need more frames to read.
constexpr float kTooltipFadeSec = 0.3f;

std::vector<std::wstring> SplitTooltipLines(const std::wstring& text) {
  std::vector<std::wstring> lines;
  if (text.empty()) {
    return lines;
  }
  size_t start = 0;
  while (start <= text.size()) {
    const size_t nl = text.find(L'\n', start);
    if (nl == std::wstring::npos) {
      lines.push_back(text.substr(start));
      break;
    }
    lines.push_back(text.substr(start, nl - start));
    start = nl + 1;
  }
  return lines;
}


class TooltipBubble : public Column {
 public:
  TooltipBubble(ColorF fill, ColorF stroke, float radius)
      : fill_(fill), stroke_(stroke), radius_(std::max(0.f, radius)) {}

  void Paint(mx::Canvas& canvas) override {
    if (!visible()) {
      return;
    }
    canvas.FillRoundedRect(bounds_, radius_, radius_, stroke_);
    const float inset = 1.f;
    const RectF inner{
        bounds_.x + inset, bounds_.y + inset,
        std::max(0.f, bounds_.w - inset * 2.f),
        std::max(0.f, bounds_.h - inset * 2.f)};
    const float ir = std::max(0.f, radius_ - inset);
    canvas.FillRoundedRect(inner, ir, ir, fill_);
    for (auto& child : children_) {
      if (child && child->visible()) {
        child->Paint(canvas);
      }
    }
  }

 private:
  ColorF fill_;
  ColorF stroke_;
  float radius_;
};

}  // namespace

TooltipOverlay::~TooltipOverlay() { Hide(); }

void TooltipOverlay::CancelFade() {
  if (fade_id_ && window_) {
    window_->CancelAnimation(fade_id_);
    fade_id_ = 0;
  }
}

void TooltipOverlay::Hide() {
  CancelFade();
  if (window_ && window_->hwnd()) {
    ShowWindow(window_->hwnd(), SW_HIDE);
    window_->set_layered_opacity(1.f);
  }
}

void TooltipOverlay::Dismiss() {
  if (!window_ || !window_->hwnd() || !IsWindowVisible(window_->hwnd())) {
    Hide();
    return;
  }
  if (!animate_) {
    Hide();
    return;
  }
  FadeTo(0.f, [this]() { Hide(); });
}

void TooltipOverlay::FadeTo(float to_opacity, std::function<void()> done) {
  if (!window_ || !window_->hwnd()) {
    if (done) {
      done();
    }
    return;
  }
  CancelFade();
  const float from = window_->layered_opacity();
  fade_id_ = window_->Animate(
      kTooltipFadeSec, Easing::EaseOutCubic,
      [this, from, to_opacity](float t) {
        window_->set_layered_opacity(from + (to_opacity - from) * t);
      },
      [this, done = std::move(done)]() {
        fade_id_ = 0;
        if (done) {
          done();
        }
      });
}

bool TooltipOverlay::OwnsHwnd(HWND hwnd) const {
  return window_ && window_->hwnd() && hwnd == window_->hwnd();
}

bool TooltipOverlay::Ensure(HWND owner) {
  if (window_ && window_->hwnd()) {
    return true;
  }
  window_ = std::make_unique<Window>();
  const DWORD ex = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST |
                   WS_EX_LAYERED | WS_EX_TRANSPARENT;
  if (!window_->CreateLayeredTool(owner, 1, 1, ex)) {
    window_.reset();
    return false;
  }
  return true;
}

void TooltipOverlay::Place(float owner_dpi, float dip_w, float dip_h) {
  if (!window_ || !window_->hwnd()) {
    return;
  }

  POINT pt = {};
  GetCursorPos(&pt);
  const int offset_y = static_cast<int>(mx::PxFromDip(16.f, owner_dpi));
  int x = pt.x;
  int y = pt.y + offset_y;
  const int pw =
      static_cast<int>(std::ceil(mx::PxFromDip(dip_w, owner_dpi)));
  const int ph =
      static_cast<int>(std::ceil(mx::PxFromDip(dip_h, owner_dpi)));

  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
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

void TooltipOverlay::Show(HWND owner, float owner_dpi, const std::wstring& text,
                          bool animate) {
  if (text.empty() || !owner) {
    return;
  }
  const std::wstring owned = text;
  if (!Ensure(owner)) {
    return;
  }

  const ThemeTokens& t = Theme::Active();
  const std::vector<std::wstring> lines = SplitTooltipLines(owned);
  if (lines.empty()) {
    return;
  }

  const float fs = t.font_size_sm;
  const wchar_t* font = t.font_ui.c_str();
  const float line_h = fs + 4.f;
  float max_line_w = 0.f;
  for (const auto& line : lines) {
    if (line.empty()) {
      continue;
    }
    max_line_w =
        (std::max)(max_line_w, mx::MeasureUiTextWidth(line, fs, font));
  }

  const float pad_x = 8.f;
  const float pad_y = 4.f;
  const float border = 1.f;
  const float radius = 6.f;
  const float inner_w = (std::min)(max_line_w + pad_x * 2.f, 320.f);
  const float tw = inner_w + border * 2.f;
  const float th = pad_y * 2.f + border * 2.f + line_h * static_cast<float>(lines.size());
  const TextAlign align = lines.size() > 1 ? TextAlign::Left : TextAlign::Center;

  auto root = std::make_unique<TooltipBubble>(t.surface, t.border, radius);
  root->padding(pad_x + border, pad_y + border, pad_x + border, pad_y + border);
  root->h_align(Align::Center);
  root->v_align(Align::Center);
  root->clip_children(false);
  root->fixed_width(tw).fixed_height(th);
  for (const auto& line : lines) {
    auto label = std::make_unique<Label>();
    label->text(line)
        .font_size(fs)
        .color(t.text)
        .align(align)
        .fill_width()
        .fixed_height(line_h);
    root->AddChild(std::move(label));
  }
  window_->SetRoot(std::move(root));
  Place(owner_dpi, tw, th);
  animate_ = animate;
  CancelFade();
  if (animate_) {
    window_->set_layered_opacity(0.f);
    ShowWindow(window_->hwnd(), SW_SHOWNOACTIVATE);
    window_->OnPaint();
    FadeTo(1.f, {});
  } else {
    window_->set_layered_opacity(1.f);
    ShowWindow(window_->hwnd(), SW_SHOWNOACTIVATE);
    window_->OnPaint();
  }
}

}  // namespace mx::ui
