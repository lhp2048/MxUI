#include "mx/ui/status_bar.h"

#include "mx/ui/locale.h"
#include "mx/ui/theme.h"

#include <algorithm>

namespace mx::ui {

StatusBar::StatusBar() {
  fill_width();
  hug_height();
}

StatusBar& StatusBar::items(std::vector<std::wstring> panes) {
  panes_ = std::move(panes);
  Invalidate();
  return *this;
}

StatusBar& StatusBar::add_item(std::wstring text) {
  panes_.push_back(std::move(text));
  Invalidate();
  return *this;
}

StatusBar& StatusBar::set_item(int index, std::wstring text) {
  if (index >= 0 && index < static_cast<int>(panes_.size())) {
    panes_[static_cast<size_t>(index)] = std::move(text);
    Invalidate();
  }
  return *this;
}

AccRole StatusBar::acc_role() const {
  if (acc_role_override_) {
    return *acc_role_override_;
  }
  return AccRole::StatusBar;
}

std::wstring StatusBar::AccDefaultName() const {
  return Locale::Tr(L"Status bar", L"StatusBar");
}

std::wstring StatusBar::AccValue() const {
  if (panes_.empty()) {
    return {};
  }
  return panes_.front();
}

SizeF StatusBar::Measure(float max_w, float max_h) {
  return ResolveSize(max_w, max_h,
                     preferred_width() > 0.f ? preferred_width() : max_w, kBarH);
}

void StatusBar::Paint(mx::Canvas& canvas) {
  if (!visible()) {
    return;
  }
  const ThemeTokens& th = Theme::Active();
  canvas.FillRect(bounds_, th.surface_alt);
  canvas.FillRect(RectF{bounds_.x, bounds_.y, bounds_.w, 1.f}, th.divider);
  if (panes_.empty()) {
    return;
  }

  float right = bounds_.x + bounds_.w - kPad;
  std::vector<float> widths(panes_.size(), 0.f);
  for (int i = static_cast<int>(panes_.size()) - 1; i >= 1; --i) {
    const float tw = mx::MeasureUiTextWidth(
        panes_[static_cast<size_t>(i)], th.font_size_sm, th.font_ui.c_str());
    widths[static_cast<size_t>(i)] = tw + kPad * 2.f;
    right -= widths[static_cast<size_t>(i)];
  }
  const float first_w = std::max(0.f, right - (bounds_.x + kPad));
  widths[0] = first_w;

  float x = bounds_.x + kPad;
  for (int i = 0; i < static_cast<int>(panes_.size()); ++i) {
    const RectF cell{x, bounds_.y, widths[static_cast<size_t>(i)], bounds_.h};
    canvas.DrawText(panes_[static_cast<size_t>(i)], cell, th.text_muted,
                    th.font_size_sm, th.font_ui.c_str(),
                    mx::TextHAlign::Left);
    x += widths[static_cast<size_t>(i)];
    if (i + 1 < static_cast<int>(panes_.size())) {
      canvas.FillRect(RectF{x, bounds_.y + 4.f, 1.f, bounds_.h - 8.f},
                      th.divider);
    }
  }
}

}  // namespace mx::ui
