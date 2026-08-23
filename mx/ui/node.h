#pragma once

#include "mx/reactive/observe.h"
#include "mx/ui/acc.h"
#include "mx/ui/types.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mx::ui {

class Window;
class Node;

// In-process Node drag. Not OLE / WM_DROPFILES.
struct DragEvent {
  std::wstring data;
  Node* source = nullptr;
  Node* target = nullptr;
  float x = 0.f;
  float y = 0.f;
};

class Node {
 public:
  virtual ~Node() = default;

  void AddChild(std::unique_ptr<Node> child);
  const std::vector<std::unique_ptr<Node>>& children() const { return children_; }

  RectF bounds() const { return bounds_; }
  void set_bounds(const RectF& r) { bounds_ = r; }

  Node* parent() const { return parent_; }
  // Event-routing parent only (not added to children_). Used by virtualized hosts.
  void set_event_parent(Node* parent) { parent_ = parent; }

  SizePolicy width_policy() const { return width_policy_; }
  SizePolicy height_policy() const { return height_policy_; }
  float preferred_width() const { return preferred_w_; }
  float preferred_height() const { return preferred_h_; }

  Node& set_width_policy(SizePolicy p);
  Node& set_height_policy(SizePolicy p);
  Node& set_preferred_width(float w);
  Node& set_preferred_height(float h);

  // Convenience: Fixed + value, or Fill / Hug.
  Node& fixed_width(float w);
  Node& fixed_height(float h);
  Node& fill_width();
  Node& fill_height();
  Node& hug_width();
  Node& hug_height();

  // Column/Row main-axis flex share (0 = not weighted; Fill still flexes as weight 1).
  Node& weight(float w);
  float weight() const { return weight_; }

  // Screen-axis placement in leftover space (not Label::align text).
  // Unset → parent Column uses h_align default, parent Row uses v_align default.
  // Absolute: only on an axis with no anchor and no x/y.
  // Column/Row override also sets container child-default / packing on that axis.
  virtual Node& h_align(Align a);
  virtual Node& v_align(Align a);
  Align h_align() const { return h_align_; }
  Align v_align() const { return v_align_; }
  bool has_h_align() const { return has_h_align_; }
  bool has_v_align() const { return has_v_align_; }

  // Absolute parent: child origin relative to parent content (ignored by Column/Row).
  Node& set_pos(float x, float y);
  float pos_x() const { return pos_x_; }
  float pos_y() const { return pos_y_; }
  bool has_pos() const { return has_pos_; }

  // Absolute edge anchors (distance to parent content edges).
  // Per axis: dual-edge wins over set_pos and over width/height (incl. defaults).
  // No compiler diagnostic if both are set. Ignored by Column/Row/Tile/Tab.
  Node& left(float v);
  Node& top(float v);
  Node& right(float v);
  Node& bottom(float v);
  float left() const { return left_; }
  float top() const { return top_; }
  float right() const { return right_; }
  float bottom() const { return bottom_; }
  bool has_left() const { return has_left_; }
  bool has_top() const { return has_top_; }
  bool has_right() const { return has_right_; }
  bool has_bottom() const { return has_bottom_; }

  // Default: claim the full available size (Fill/Fill).
  virtual SizeF Measure(float max_w, float max_h);
  // Default: set bounds and give each child the same rect.
  virtual void Layout(const RectF& final_rect);
  // Default: paint children in order.
  virtual void Paint(mx::Canvas& canvas);
  // Default: deepest child that contains (x,y), else this if inside bounds.
  virtual Node* HitTest(float x, float y);

  virtual void OnMouseDown(const MouseEvent&) {}
  virtual void OnMouseUp(const MouseEvent&) {}
  virtual void OnMouseDoubleClick(const MouseEvent&) {}
  virtual void OnMouseMove(const MouseEvent&) {}
  virtual void OnMouseEnter(const MouseEvent&) {}
  virtual void OnMouseLeave(const MouseEvent&) {}
  virtual void OnMouseWheel(const MouseEvent&) {}
  virtual void OnKey(const KeyEvent&) {}
  virtual void OnChar(wchar_t /*ch*/) {}
  virtual void OnFocus() {}
  virtual void OnBlur() {}

  // Right-click / WM_CONTEXTMENU. |screen_x/y| are screen coordinates for
  // TrackPopupMenu. Default invokes the optional handler (walks parents if
  // unset).
  using ContextMenuHandler = std::function<void(int screen_x, int screen_y)>;
  void set_on_context_menu(ContextMenuHandler handler);
  virtual void OnContextMenu(int screen_x, int screen_y);

  // Wheel: Window walks from hit node up to first ancestor that returns true.
  virtual bool WantsMouseWheel() const { return false; }

  // Enter: Window activates the default Button unless this returns true.
  virtual bool ConsumesEnter() const { return false; }

  // IME: composition is temporary underline text; result commits into the control.
  virtual bool WantsIme() const { return false; }
  virtual void OnImeComposition(const std::wstring& /*composition*/) {}
  virtual void OnImeResult(const std::wstring& /*result*/) {}
  virtual void OnImeEnd() {}

  // Drop GPU resources tied to the previous Canvas/device; default walks children.
  virtual void OnDeviceLost();

  bool focusable() const { return focusable_; }
  void set_focusable(bool v) { focusable_ = v; }
  bool focused() const { return focused_; }

  bool visible() const { return visible_; }
  Node& set_visible(bool v);

  Node& clip_children(bool v);
  bool clip_children() const { return clip_children_; }

  Node& tooltip(std::wstring text);
  const std::wstring& tooltip() const { return tooltip_; }
  // Per-control hover delay before tooltip shows. Unset → Window default.
  // 0 = show immediately (tooltip_instant(true)).
  Node& tooltip_show_delay_ms(UINT ms);
  Node& tooltip_instant(bool on = true);
  bool has_tooltip_show_delay_ms() const { return tooltip_show_delay_ms_.has_value(); }
  UINT tooltip_show_delay_ms_value() const { return tooltip_show_delay_ms_.value_or(0); }
  void clear_tooltip_show_delay_ms() { tooltip_show_delay_ms_.reset(); }

  // Decorative motion (Switch thumb, Toast fade, …). Default on. Not caret blink.
  Node& animate(bool on);
  bool animate() const { return animate_; }

  // Opt-in in-process drag. Default off; not every control is draggable.
  Node& draggable(bool on);
  bool draggable() const { return draggable_; }
  Node& drag_data(std::wstring data);
  const std::wstring& drag_data() const { return drag_data_; }
  Node& drop_target(bool on);
  bool drop_target() const { return drop_target_; }
  using DropHandler = std::function<void(const DragEvent&)>;
  Node& on_drop(DropHandler handler);

  // Accessibility read API. UIA glue queries these; no mirrored tree.
  // acc_name overrides control text; tooltip is last-resort name.
  Node& acc_name(std::wstring name);
  const std::wstring& acc_name() const { return acc_name_; }
  // Sparse override; unset → control default (Button, Edit, …).
  Node& set_acc_role(AccRole role);
  virtual AccRole acc_role() const;
  std::wstring AccName() const;
  virtual std::wstring AccDefaultName() const;
  virtual std::wstring AccValue() const;
  virtual AccState acc_state() const;
  virtual bool AccInvoke();
  virtual bool AccToggle();
  virtual bool AccSetValue(const std::wstring& value);
  virtual double AccRangeValue() const;
  virtual double AccRangeMinimum() const;
  virtual double AccRangeMaximum() const;
  virtual double AccRangeSmallChange() const;
  virtual double AccRangeLargeChange() const;
  virtual bool AccRangeReadOnly() const;
  virtual bool AccSetRangeValue(double value);
  virtual bool AccIsExpanded() const;
  virtual bool AccExpand();
  virtual bool AccCollapse();
  bool AccIncluded() const;
  int acc_id() const { return acc_id_; }
  int EnsureAccId() const;

  Node& bg(const ColorF& c);
  Node& clear_bg();
  bool has_bg() const { return bg_.has_value(); }
  ColorF bg_color() const { return bg_.value_or(ColorF{}); }

  // Keep subscription alive for this node's lifetime (unbind on destroy).
  void OwnSubscription(mx::reactive::Subscription sub);

  Window* host_window() const { return host_window_; }
  void set_host_window(Window* w);
  void Invalidate();

  // Optional id for FindByName (YAML `name`, bind templates).
  Node& set_name(std::string name);
  const std::string& name() const { return name_; }
  Node* FindByName(const std::string& name);
  const Node* FindByName(const std::string& name) const;

 protected:
  friend class Window;
  void set_focused(bool v) { focused_ = v; }
  virtual void OnAnimateChanged() {}
  virtual void OnHostWindowChanged() {}
  bool CanTween() const;
  void NotifyAccToggleChanged();
  void NotifyAccValueChanged();
  void NotifyAccRangeChanged();
  void NotifyAccExpandCollapseChanged();
  void NotifyAccStructureChanged();
  void DeliverDrop(const DragEvent& e);

  static bool ContainsPoint(const RectF& r, float x, float y);

  // Combine policy + preferred + intrinsic (hug) into a Measure result.
  SizeF ResolveSize(float max_w, float max_h, float hug_w, float hug_h) const;

  RectF bounds_{};
  std::vector<std::unique_ptr<Node>> children_;
  Node* parent_ = nullptr;
  bool focusable_ = false;
  bool focused_ = false;
  bool visible_ = true;
  bool clip_children_ = false;
  bool animate_ = true;
  bool draggable_ = false;
  bool drop_target_ = false;
  std::wstring drag_data_;
  DropHandler on_drop_;
  std::wstring tooltip_;
  std::optional<UINT> tooltip_show_delay_ms_;
  std::wstring acc_name_;
  std::optional<AccRole> acc_role_override_;
  mutable int acc_id_ = 0;
  std::vector<mx::reactive::Subscription> owned_subs_;
  Window* host_window_ = nullptr;
  std::string name_;
  ContextMenuHandler on_context_menu_;

  SizePolicy width_policy_ = SizePolicy::Hug;
  SizePolicy height_policy_ = SizePolicy::Hug;
  float preferred_w_ = 0.f;
  float preferred_h_ = 0.f;
  float weight_ = 0.f;
  Align h_align_ = Align::Start;
  bool has_h_align_ = false;
  Align v_align_ = Align::Start;
  bool has_v_align_ = false;
  float pos_x_ = 0.f;
  float pos_y_ = 0.f;
  bool has_pos_ = false;
  float left_ = 0.f;
  float top_ = 0.f;
  float right_ = 0.f;
  float bottom_ = 0.f;
  bool has_left_ = false;
  bool has_top_ = false;
  bool has_right_ = false;
  bool has_bottom_ = false;
  std::optional<ColorF> bg_;
};

const std::wstring* ResolveTooltipText(const Node* hit);
UINT ResolveTooltipShowDelayMs(const Window* w, const Node* hit);
Node* ResolveDraggable(Node* hit);
Node* ResolveDropTarget(Node* hit, const Node* source);

}  // namespace mx::ui
