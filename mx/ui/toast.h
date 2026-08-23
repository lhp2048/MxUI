#pragma once

#include "mx/ui/node.h"

#include <functional>
#include <optional>
#include <string>

namespace mx::ui {

enum class ToastVariant { Info, Success, Danger };

// Overlay placement relative to the owner Window client area.
enum class ToastAnchor {
  BottomCenter,
  BottomStart,
  BottomEnd,
  TopCenter,
  TopStart,
  TopEnd,
  Center
};

// In-tree banner or payload for Window::ShowToast (overlay). Same Node class.
class Toast : public Node {
 public:
  using DismissHandler = std::function<void()>;

  Toast();

  Toast& text(const std::wstring& t);
  Toast& variant(ToastVariant v);
  // Seconds on overlay; <=0 stays until DismissToast() or click (if enabled).
  Toast& duration_sec(float s);
  Toast& animate(bool on);
  Toast& fade_sec(float s);
  Toast& font_size(float size);
  // Default false: overlay is display-only and ignores mouse (click-through).
  Toast& dismiss_on_click(bool on);
  // Placement within owner client area (DIP). Default bottom-center, 16 margin.
  Toast& anchor(ToastAnchor a);
  Toast& margin(float dip);
  Toast& offset(float x_dip, float y_dip);
  Toast& on_dismiss(DismissHandler handler);

  const std::wstring& text() const { return text_; }
  ToastVariant variant() const { return variant_; }
  float duration_sec() const { return duration_sec_; }
  bool dismiss_on_click() const { return dismiss_on_click_; }
  ToastAnchor anchor() const { return anchor_; }
  float margin() const { return margin_; }
  float offset_x() const { return offset_x_; }
  float offset_y() const { return offset_y_; }
  bool animate() const { return Node::animate(); }
  float fade_sec() const { return fade_sec_; }
  float effective_fade_sec() const {
    return Node::animate() && fade_sec_ > 0.f ? fade_sec_ : 0.f;
  }

  AccRole acc_role() const override;
  SizeF Measure(float max_w, float max_h) override;
  void Paint(mx::Canvas& canvas) override;
  void OnMouseDown(const MouseEvent& e) override;
  bool AccInvoke() override;
  std::wstring AccDefaultName() const override;

  DismissHandler release_on_dismiss();

 private:
  std::wstring text_;
  ToastVariant variant_ = ToastVariant::Info;
  float duration_sec_ = 2.5f;
  float fade_sec_ = 0.2f;
  bool dismiss_on_click_ = false;
  ToastAnchor anchor_ = ToastAnchor::BottomCenter;
  float margin_ = 16.f;
  float offset_x_ = 0.f;
  float offset_y_ = 0.f;
  std::optional<float> font_size_;
  DismissHandler on_dismiss_;
};

}  // namespace mx::ui
