#include "mx/ui/node.h"
#include "mx/ui/window.h"

#include <algorithm>
#include <atomic>

namespace mx::ui {

bool Node::ContainsPoint(const RectF& r, float x, float y) {
  return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

void Node::AddChild(std::unique_ptr<Node> child) {
  if (!child) {
    return;
  }
  child->parent_ = this;
  child->set_host_window(host_window_);
  children_.push_back(std::move(child));
}

void Node::set_host_window(Window* w) {
  const bool changed = host_window_ != w;
  host_window_ = w;
  if (changed) {
    OnHostWindowChanged();
  }
  for (auto& child : children_) {
    if (child) {
      child->set_host_window(w);
    }
  }
}

void Node::Invalidate() {
  if (host_window_) {
    host_window_->InvalidateNode(this);
  }
}

Node& Node::set_width_policy(SizePolicy p) {
  width_policy_ = p;
  return *this;
}

Node& Node::set_height_policy(SizePolicy p) {
  height_policy_ = p;
  return *this;
}

Node& Node::set_preferred_width(float w) {
  preferred_w_ = w;
  return *this;
}

Node& Node::set_preferred_height(float h) {
  preferred_h_ = h;
  return *this;
}

Node& Node::fixed_width(float w) {
  width_policy_ = SizePolicy::Fixed;
  preferred_w_ = w;
  return *this;
}

Node& Node::fixed_height(float h) {
  height_policy_ = SizePolicy::Fixed;
  preferred_h_ = h;
  return *this;
}

Node& Node::fill_width() {
  width_policy_ = SizePolicy::Fill;
  return *this;
}

Node& Node::fill_height() {
  height_policy_ = SizePolicy::Fill;
  return *this;
}

Node& Node::hug_width() {
  width_policy_ = SizePolicy::Hug;
  return *this;
}

Node& Node::hug_height() {
  height_policy_ = SizePolicy::Hug;
  return *this;
}

Node& Node::weight(float w) {
  weight_ = std::max(0.f, w);
  return *this;
}

Node& Node::h_align(Align a) {
  h_align_ = a;
  has_h_align_ = true;
  return *this;
}

Node& Node::v_align(Align a) {
  v_align_ = a;
  has_v_align_ = true;
  return *this;
}

Node& Node::set_pos(float x, float y) {
  pos_x_ = x;
  pos_y_ = y;
  has_pos_ = true;
  return *this;
}

Node& Node::left(float v) {
  left_ = v;
  has_left_ = true;
  return *this;
}

Node& Node::top(float v) {
  top_ = v;
  has_top_ = true;
  return *this;
}

Node& Node::right(float v) {
  right_ = v;
  has_right_ = true;
  return *this;
}

Node& Node::bottom(float v) {
  bottom_ = v;
  has_bottom_ = true;
  return *this;
}

SizeF Node::ResolveSize(float max_w, float max_h, float hug_w,
                        float hug_h) const {
  float w = hug_w;
  float h = hug_h;

  switch (width_policy_) {
    case SizePolicy::Fixed:
      w = preferred_w_ > 0.f ? preferred_w_ : hug_w;
      break;
    case SizePolicy::Hug:
      w = hug_w;
      break;
    case SizePolicy::Fill:
      w = max_w > 0.f ? max_w : (preferred_w_ > 0.f ? preferred_w_ : hug_w);
      break;
  }

  switch (height_policy_) {
    case SizePolicy::Fixed:
      h = preferred_h_ > 0.f ? preferred_h_ : hug_h;
      break;
    case SizePolicy::Hug:
      h = hug_h;
      break;
    case SizePolicy::Fill:
      h = max_h > 0.f ? max_h : (preferred_h_ > 0.f ? preferred_h_ : hug_h);
      break;
  }

  if (max_w > 0.f) {
    w = std::min(w, max_w);
  }
  if (max_h > 0.f) {
    h = std::min(h, max_h);
  }
  return SizeF{std::max(0.f, w), std::max(0.f, h)};
}

SizeF Node::Measure(float max_w, float max_h) {
  return ResolveSize(max_w, max_h, max_w, max_h);
}

void Node::Layout(const RectF& final_rect) {
  bounds_ = final_rect;
  for (auto& child : children_) {
    if (child) {
      child->Layout(final_rect);
    }
  }
}

void Node::Paint(mx::Canvas& canvas) {
  if (!visible_) {
    return;
  }
  if (bg_) {
    canvas.FillRect(bounds_, *bg_);
  }
  if (clip_children_) {
    canvas.PushAxisAlignedClip(bounds_);
  }
  for (auto& child : children_) {
    if (child && child->visible()) {
      child->Paint(canvas);
    }
  }
  if (clip_children_) {
    canvas.PopAxisAlignedClip();
  }
}

Node* Node::HitTest(float x, float y) {
  if (!visible_) {
    return nullptr;
  }
  if (!ContainsPoint(bounds_, x, y)) {
    return nullptr;
  }
  // Front-most (last painted) child first.
  for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
    if (!*it) {
      continue;
    }
    if (Node* hit = (*it)->HitTest(x, y)) {
      return hit;
    }
  }
  return this;
}

Node& Node::bg(const ColorF& c) {
  bg_ = c;
  return *this;
}

Node& Node::clear_bg() {
  bg_.reset();
  return *this;
}

Node& Node::set_visible(bool v) {
  if (visible_ == v) {
    return *this;
  }
  visible_ = v;
  if (host_window_) {
    host_window_->RequestLayout();
  }
  return *this;
}

Node& Node::clip_children(bool v) {
  clip_children_ = v;
  return *this;
}

Node& Node::tooltip(std::wstring text) {
  tooltip_ = std::move(text);
  return *this;
}

Node& Node::tooltip_show_delay_ms(UINT ms) {
  tooltip_show_delay_ms_ = ms;
  return *this;
}

Node& Node::tooltip_instant(bool on) {
  if (on) {
    tooltip_show_delay_ms_ = 0;
  } else {
    tooltip_show_delay_ms_.reset();
  }
  return *this;
}

Node& Node::animate(bool on) {
  if (animate_ == on) {
    return *this;
  }
  animate_ = on;
  OnAnimateChanged();
  return *this;
}

bool Node::CanTween() const {
  return animate_ && host_window_ && host_window_->hwnd();
}

Node& Node::draggable(bool on) {
  draggable_ = on;
  return *this;
}

Node& Node::drag_data(std::wstring data) {
  drag_data_ = std::move(data);
  return *this;
}

Node& Node::drop_target(bool on) {
  drop_target_ = on;
  return *this;
}

Node& Node::on_drop(DropHandler handler) {
  on_drop_ = std::move(handler);
  if (on_drop_) {
    drop_target_ = true;
  }
  return *this;
}

void Node::DeliverDrop(const DragEvent& e) {
  if (on_drop_) {
    on_drop_(e);
  }
}

Node& Node::acc_name(std::wstring name) {
  acc_name_ = std::move(name);
  return *this;
}

Node& Node::set_acc_role(AccRole role) {
  acc_role_override_ = role;
  return *this;
}

AccRole Node::acc_role() const {
  if (acc_role_override_) {
    return *acc_role_override_;
  }
  return acc_name_.empty() ? AccRole::Ignore : AccRole::Group;
}

std::wstring Node::AccName() const {
  if (!acc_name_.empty()) {
    return acc_name_;
  }
  std::wstring def = AccDefaultName();
  if (!def.empty()) {
    return def;
  }
  return tooltip_;
}

std::wstring Node::AccDefaultName() const {
  return {};
}

std::wstring Node::AccValue() const {
  return {};
}

AccState Node::acc_state() const {
  AccState s;
  s.focused = focused_;
  return s;
}

bool Node::AccInvoke() {
  return false;
}

bool Node::AccToggle() {
  return false;
}

bool Node::AccSetValue(const std::wstring&) {
  return false;
}

double Node::AccRangeValue() const {
  return 0.0;
}

double Node::AccRangeMinimum() const {
  return 0.0;
}

double Node::AccRangeMaximum() const {
  return 1.0;
}

double Node::AccRangeSmallChange() const {
  return 0.05;
}

double Node::AccRangeLargeChange() const {
  return 0.1;
}

bool Node::AccRangeReadOnly() const {
  return true;
}

bool Node::AccSetRangeValue(double) {
  return false;
}

bool Node::AccIsExpanded() const {
  return false;
}

bool Node::AccExpand() {
  return false;
}

bool Node::AccCollapse() {
  return false;
}

void Node::NotifyAccToggleChanged() {
  if (host_window_) {
    host_window_->RaiseAccToggleChanged(this);
  }
}

void Node::NotifyAccValueChanged() {
  if (host_window_) {
    host_window_->RaiseAccValueChanged(this);
  }
}

void Node::NotifyAccRangeChanged() {
  if (host_window_) {
    host_window_->RaiseAccRangeChanged(this);
  }
}

void Node::NotifyAccExpandCollapseChanged() {
  if (host_window_) {
    host_window_->RaiseAccExpandCollapseChanged(this);
  }
}

void Node::NotifyAccStructureChanged() {
  if (host_window_) {
    host_window_->RaiseAccStructureChanged();
  }
}

bool Node::AccIncluded() const {
  return visible_ && acc_role() != AccRole::Ignore;
}

int Node::EnsureAccId() const {
  static std::atomic<int> next{1};
  if (acc_id_ == 0) {
    acc_id_ = next.fetch_add(1, std::memory_order_relaxed);
  }
  return acc_id_;
}

const std::wstring* ResolveTooltipText(const Node* hit) {
  for (const Node* n = hit; n; n = n->parent()) {
    if (!n->tooltip().empty()) {
      return &n->tooltip();
    }
  }
  return nullptr;
}

UINT ResolveTooltipShowDelayMs(const Window* w, const Node* hit) {
  const Node* tooltip_node = nullptr;
  for (const Node* n = hit; n; n = n->parent()) {
    if (!n->tooltip().empty()) {
      tooltip_node = n;
      break;
    }
  }
  for (const Node* n = hit; n; n = n->parent()) {
    if (n->has_tooltip_show_delay_ms()) {
      return n->tooltip_show_delay_ms_value();
    }
    if (n == tooltip_node) {
      break;
    }
  }
  return w ? w->tooltip_show_delay_ms() : Window::kDefaultTooltipShowDelayMs;
}

Node* ResolveDraggable(Node* hit) {
  for (Node* n = hit; n; n = n->parent()) {
    if (n->draggable()) {
      return n;
    }
  }
  return nullptr;
}

Node* ResolveDropTarget(Node* hit, const Node* source) {
  for (Node* n = hit; n; n = n->parent()) {
    if (n != source && n->drop_target()) {
      return n;
    }
  }
  return nullptr;
}

void Node::OwnSubscription(mx::reactive::Subscription sub) {
  owned_subs_.push_back(std::move(sub));
}

Node& Node::set_name(std::string name) {
  name_ = std::move(name);
  return *this;
}

Node* Node::FindByName(const std::string& name) {
  if (name.empty()) {
    return nullptr;
  }
  if (name_ == name) {
    return this;
  }
  for (auto& child : children_) {
    if (!child) {
      continue;
    }
    if (Node* hit = child->FindByName(name)) {
      return hit;
    }
  }
  return nullptr;
}

const Node* Node::FindByName(const std::string& name) const {
  return const_cast<Node*>(this)->FindByName(name);
}

void Node::set_on_context_menu(ContextMenuHandler handler) {
  on_context_menu_ = std::move(handler);
}

void Node::OnContextMenu(int screen_x, int screen_y) {
  if (on_context_menu_) {
    on_context_menu_(screen_x, screen_y);
    return;
  }
  if (parent_) {
    parent_->OnContextMenu(screen_x, screen_y);
  }
}

void Node::OnDeviceLost() {
  for (auto& child : children_) {
    if (child) {
      child->OnDeviceLost();
    }
  }
}

}  // namespace mx::ui
