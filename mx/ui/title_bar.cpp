#include "mx/ui/title_bar.h"

#include "mx/ui/button.h"
#include "mx/ui/image_view.h"
#include "mx/ui/label.h"
#include "mx/ui/locale.h"
#include "mx/ui/window.h"

#include <utility>

namespace mx::ui {
namespace {

constexpr const char kSlotIcon[] = "icon";
constexpr const char kSlotTitle[] = "title";
constexpr const char kSlotMinimize[] = "minimize";
constexpr const char kSlotMaximize[] = "maximize";
constexpr const char kSlotClose[] = "close";

constexpr const wchar_t kGlyphMaximize[] = L"\u25A1";
constexpr const wchar_t kGlyphRestore[] = L"\u29C9";

bool IsCaptionSlot(const std::string& name) {
  return name == kSlotMinimize || name == kSlotMaximize || name == kSlotClose;
}

}  // namespace

TitleBar::TitleBar() {
  v_align(Align::Center);
  fill_width();
  fixed_height(36.f);
  padding(12.f, 0.f, 4.f, 0.f);
  spacing(4.f);
}

TitleBar& TitleBar::title(std::wstring text) {
  title_ = std::move(text);
  if (resolved_) {
    if (auto* lab = dynamic_cast<Label*>(FindByName(kSlotTitle))) {
      lab->text(title_);
    }
  }
  return *this;
}

TitleBar& TitleBar::icon(std::wstring path) {
  icon_path_ = std::move(path);
  return *this;
}

TitleBar& TitleBar::close(bool on) {
  close_ = on;
  return *this;
}

TitleBar& TitleBar::minimize(bool on) {
  minimize_ = on;
  return *this;
}

TitleBar& TitleBar::maximize(bool on) {
  maximize_ = on;
  return *this;
}

void TitleBar::OnHostWindowChanged() {
  ResolveChrome();
}

SizeF TitleBar::Measure(float max_w, float max_h) {
  ResolveChrome();
  SyncLocalizedAcc();
  return Row::Measure(max_w, max_h);
}

void TitleBar::Layout(const RectF& final_rect) {
  ResolveChrome();
  SyncMaximizeGlyph();
  SyncLocalizedAcc();
  Row::Layout(final_rect);
}

void TitleBar::SyncLocalizedAcc() {
  const uint32_t gen = Locale::Generation();
  if (acc_gen_ == gen) {
    return;
  }
  acc_gen_ = gen;
  if (auto* b = dynamic_cast<Button*>(FindByName(kSlotMinimize))) {
    b->acc_name(Locale::Tr(L"Minimize", L"TitleBar"));
  }
  if (auto* b = dynamic_cast<Button*>(FindByName(kSlotMaximize))) {
    Window* w = host_window();
    const bool maxed = w && w->is_maximized();
    b->acc_name(maxed ? Locale::Tr(L"Restore", L"TitleBar")
                      : Locale::Tr(L"Maximize", L"TitleBar"));
  }
  if (auto* b = dynamic_cast<Button*>(FindByName(kSlotClose))) {
    b->acc_name(Locale::Tr(L"Close", L"TitleBar"));
  }
}

std::unique_ptr<Node> TitleBar::MakeIconView() {
  auto img = std::make_unique<ImageView>();
  img->set_name(kSlotIcon);
  img->preferred_size(16.f, 16.f);
  if (!icon_path_.empty()) {
    img->LoadFromFile(icon_path_);
  }
  return img;
}

std::unique_ptr<Node> TitleBar::MakeCaptionButton(
    const char* slot_name, const wchar_t* text, const wchar_t* accessible_name,
    std::function<void()> on_click) {
  auto btn = std::make_unique<Button>();
  if (slot_name && slot_name[0]) {
    btn->set_name(slot_name);
  }
  btn->text(text ? text : L"");
  btn->variant(ButtonVariant::Secondary);
  btn->on_click(std::move(on_click));
  btn->fixed_width(32.f);
  btn->fixed_height(28.f);
  if (accessible_name && accessible_name[0]) {
    btn->acc_name(accessible_name);
  }
  return btn;
}

void TitleBar::ApplyNamedSlotDefaults(Node* child) {
  if (!child) {
    return;
  }
  const std::string& slot = child->name();
  if (slot == kSlotIcon) {
    if (auto* img = dynamic_cast<ImageView*>(child)) {
      if (img->preferred_width() == 64.f && img->preferred_height() == 64.f) {
        img->preferred_size(16.f, 16.f);
      }
      if (img->path().empty() && !icon_path_.empty()) {
        img->LoadFromFile(icon_path_);
      }
    }
    return;
  }
  if (slot == kSlotTitle) {
    if (auto* lab = dynamic_cast<Label*>(child)) {
      if (lab->text().empty()) {
        lab->text(title_);
      }
      if (!lab->has_font_size()) {
        lab->font_size(13.f);
      }
    }
    return;
  }
  if (!IsCaptionSlot(slot)) {
    return;
  }
  auto* btn = dynamic_cast<Button*>(child);
  if (!btn) {
    return;
  }
  const wchar_t* glyph = L"";
  std::wstring acc;
  std::function<void()> action;
  if (slot == kSlotMinimize) {
    glyph = L"\u2013";
    acc = Locale::Tr(L"Minimize", L"TitleBar");
    action = [this] {
      if (Window* w = host_window()) {
        w->Minimize();
      }
    };
  } else if (slot == kSlotMaximize) {
    glyph = kGlyphMaximize;
    acc = Locale::Tr(L"Maximize", L"TitleBar");
    action = [this] {
      if (Window* w = host_window()) {
        w->ToggleMaximize();
      }
    };
    if (!btn->text().empty()) {
      maximize_glyph_locked_ = true;
    }
  } else {
    glyph = L"\u00D7";
    acc = Locale::Tr(L"Close", L"TitleBar");
    action = [this] {
      if (Window* w = host_window()) {
        w->Close();
      }
    };
  }
  if (btn->text().empty()) {
    btn->text(glyph);
  }
  if (!btn->has_on_click()) {
    btn->on_click(std::move(action));
  }
  if (btn->variant() == ButtonVariant::Primary) {
    btn->variant(ButtonVariant::Secondary);
  }
  if (btn->acc_name().empty()) {
    btn->acc_name(std::move(acc));
  }
  // Button ctor: fill width + fixed height 40. Treat as "size not set".
  if (btn->width_policy() == SizePolicy::Fill &&
      btn->height_policy() == SizePolicy::Fixed &&
      btn->preferred_height() == 40.f) {
    btn->fixed_width(32.f);
    btn->fixed_height(28.f);
  }
}

void TitleBar::SyncMaximizeGlyph() {
  if (maximize_glyph_locked_) {
    return;
  }
  auto* btn = dynamic_cast<Button*>(FindByName(kSlotMaximize));
  if (!btn) {
    return;
  }
  Window* w = host_window();
  const bool maxed = w && w->is_maximized();
  btn->text(maxed ? kGlyphRestore : kGlyphMaximize);
  btn->acc_name(maxed ? Locale::Tr(L"Restore", L"TitleBar")
                      : Locale::Tr(L"Maximize", L"TitleBar"));
}

void TitleBar::BuildStandardChrome() {
  if (!icon_path_.empty()) {
    AddChild(MakeIconView());
  }

  auto lab = std::make_unique<Label>();
  lab->set_name(kSlotTitle);
  lab->text(title_).font_size(13.f).fill_width();
  AddChild(std::move(lab));

  if (minimize_) {
    AddChild(MakeCaptionButton(kSlotMinimize, L"\u2013",
                               Locale::Tr(L"Minimize", L"TitleBar").c_str(),
                               [this] {
                                 if (Window* w = host_window()) {
                                   w->Minimize();
                                 }
                               }));
  }
  if (maximize_) {
    AddChild(MakeCaptionButton(kSlotMaximize, kGlyphMaximize,
                               Locale::Tr(L"Maximize", L"TitleBar").c_str(),
                               [this] {
                                 if (Window* w = host_window()) {
                                   w->ToggleMaximize();
                                 }
                               }));
  }
  if (close_) {
    AddChild(MakeCaptionButton(kSlotClose, L"\u00D7",
                               Locale::Tr(L"Close", L"TitleBar").c_str(),
                               [this] {
                                 if (Window* w = host_window()) {
                                   w->Close();
                                 }
                               }));
  }
}

void TitleBar::ResolveChrome() {
  if (resolved_) {
    return;
  }
  resolved_ = true;
  if (children_.empty()) {
    BuildStandardChrome();
    return;
  }
  for (auto& child : children_) {
    ApplyNamedSlotDefaults(child.get());
  }
}

Node* TitleBar::HitTest(float x, float y) {
  ResolveChrome();
  Node* hit = Row::HitTest(x, y);
  if (!hit) {
    return nullptr;
  }
  for (Node* n = hit; n && n != this; n = n->parent()) {
    if (n->focusable()) {
      return hit;
    }
  }
  return this;
}

void TitleBar::OnMouseDown(const MouseEvent& e) {
  if (e.button != MouseButton::Left) {
    return;
  }
  // Wait for slop before SC_MOVE so a second click can become a double-click.
  caption_drag_armed_ = true;
  caption_drag_x_ = e.x;
  caption_drag_y_ = e.y;
}

void TitleBar::OnMouseUp(const MouseEvent&) {
  caption_drag_armed_ = false;
}

void TitleBar::OnMouseMove(const MouseEvent& e) {
  if (!caption_drag_armed_) {
    return;
  }
  const float dx = e.x - caption_drag_x_;
  const float dy = e.y - caption_drag_y_;
  if (dx * dx + dy * dy < 16.f) {
    return;
  }
  caption_drag_armed_ = false;
  if (Window* w = host_window()) {
    w->BeginCaptionDrag();
  }
}

void TitleBar::OnMouseDoubleClick(const MouseEvent& e) {
  if (e.button != MouseButton::Left) {
    return;
  }
  caption_drag_armed_ = false;
  Window* w = host_window();
  if (!w || !w->options().resizable) {
    return;
  }
  w->ToggleMaximize();
}

}  // namespace mx::ui
