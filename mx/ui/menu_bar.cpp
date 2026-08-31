#include "mx/ui/menu_bar.h"

#include "mx/ui/column.h"
#include "mx/ui/locale.h"
#include "mx/ui/menu_item.h"
#include "mx/ui/popup_host.h"
#include "mx/ui/theme.h"
#include "mx/ui/window.h"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mx::ui {

namespace {
constexpr float kBarH = 28.f;
constexpr float kPadX = 12.f;
constexpr float kMenuPad = 4.f;
}  // namespace

MenuBar::MenuBar() {
  fill_width();
  hug_height();
  host_ = std::make_unique<PopupHost>();
}

MenuBar::~MenuBar() {
  if (host_) {
    host_->Dismiss();
  }
}

void MenuBar::BindWindow(Window* window) { window_ = window; }

MenuBar& MenuBar::add_menu(std::wstring title, std::vector<MenuCommand> commands) {
  Menu m;
  m.title = std::move(title);
  m.commands = std::move(commands);
  menus_.push_back(std::move(m));
  Invalidate();
  return *this;
}

const std::wstring& MenuBar::menu_title(int index) const {
  static const std::wstring kEmpty;
  if (index < 0 || index >= static_cast<int>(menus_.size())) {
    return kEmpty;
  }
  return menus_[static_cast<size_t>(index)].title;
}

MenuBar& MenuBar::on_command(CommandHandler handler) {
  on_command_ = std::move(handler);
  return *this;
}

MenuBar& MenuBar::corner_radius(float r) {
  corner_radius_ = std::max(0.f, r);
  return *this;
}

MenuBar& MenuBar::border_width(float w) {
  border_width_ = std::max(0.f, w);
  return *this;
}

AccRole MenuBar::acc_role() const {
  if (acc_role_override_) {
    return *acc_role_override_;
  }
  return AccRole::MenuBar;
}

std::wstring MenuBar::AccDefaultName() const {
  return Locale::Tr(L"Menu bar", L"MenuBar");
}

SizeF MenuBar::Measure(float max_w, float max_h) {
  RebuildLayout(max_w);
  float hug_w = kPadX;
  if (!menus_.empty()) {
    hug_w = menus_.back().x + menus_.back().w + kPadX;
  }
  return ResolveSize(max_w, max_h, hug_w, kBarH);
}

void MenuBar::Layout(const RectF& final_rect) {
  bounds_ = final_rect;
  RebuildLayout(final_rect.w);
}

void MenuBar::RebuildLayout(float width) {
  (void)width;
  const ThemeTokens& th = Theme::Active();
  float x = kPadX;
  for (auto& m : menus_) {
    const float tw =
        mx::MeasureUiTextWidth(m.title, th.font_size_sm, th.font_ui.c_str());
    m.x = x;
    m.w = tw + 16.f;
    x += m.w;
  }
}

int MenuBar::IndexAt(float x, float y) const {
  if (!ContainsPoint(bounds_, x, y)) {
    return -1;
  }
  const float lx = x - bounds_.x;
  for (int i = 0; i < static_cast<int>(menus_.size()); ++i) {
    const Menu& m = menus_[static_cast<size_t>(i)];
    if (lx >= m.x && lx < m.x + m.w) {
      return i;
    }
  }
  return -1;
}

POINT MenuBar::ItemScreenPoint(int index) const {
  POINT pt{0, 0};
  if (!window_ || !window_->hwnd() || index < 0 ||
      index >= static_cast<int>(menus_.size())) {
    return pt;
  }
  const Menu& m = menus_[static_cast<size_t>(index)];
  const float dpi = window_->dpi();
  pt.x = static_cast<LONG>(mx::PxFromDip(bounds_.x + m.x, dpi));
  pt.y = static_cast<LONG>(
      mx::PxFromDip(bounds_.y + bounds_.h, dpi));
  ClientToScreen(window_->hwnd(), &pt);
  return pt;
}

void MenuBar::OpenMenu(int index) {
  if (!window_ || !window_->hwnd() || !host_ || index < 0 ||
      index >= static_cast<int>(menus_.size())) {
    return;
  }
  if (open_ == index && host_->is_open()) {
    host_->Dismiss();
    open_ = -1;
    Invalidate();
    return;
  }
  Menu& menu = menus_[static_cast<size_t>(index)];
  const ThemeTokens& th = Theme::Active();
  auto col = std::make_unique<Column>();
  col->padding(kMenuPad);
  col->spacing(0.f);
  col->hug_width();
  col->hug_height();
  col->bg(th.surface);
  const std::wstring title = menu.title;
  for (int cmd_i = 0; cmd_i < static_cast<int>(menu.commands.size()); ++cmd_i) {
    MenuCommand& cmd = menu.commands[static_cast<size_t>(cmd_i)];
    auto item = std::make_unique<MenuItem>();
    if (cmd.separator || cmd.text == L"-") {
      item->separator(true);
      col->AddChild(std::move(item));
      continue;
    }
    item->text(cmd.text);
    item->icon(cmd.icon);
    item->checkable(cmd.checkable);
    item->checked(cmd.checked);
    item->radio_group(cmd.radio_group);
    item->font_size(th.font_size_sm);
    item->on_changed([this, index, cmd_i](bool on) {
      if (index < 0 || index >= static_cast<int>(menus_.size())) {
        return;
      }
      auto& cmds = menus_[static_cast<size_t>(index)].commands;
      if (cmd_i < 0 || cmd_i >= static_cast<int>(cmds.size())) {
        return;
      }
      cmds[static_cast<size_t>(cmd_i)].checked = on;
      const int group = cmds[static_cast<size_t>(cmd_i)].radio_group;
      if (group > 0) {
        for (int i = 0; i < static_cast<int>(cmds.size()); ++i) {
          if (i != cmd_i && cmds[static_cast<size_t>(i)].radio_group == group) {
            cmds[static_cast<size_t>(i)].checked = false;
          }
        }
      }
    });
    std::function<void()> click = cmd.on_click;
    const std::wstring item_text = cmd.text;
    item->on_click(host_->WrapDismiss([this, title, item_text, click]() {
      if (click) {
        click();
      }
      if (on_command_) {
        on_command_(title, item_text);
      }
    }));
    col->AddChild(std::move(item));
  }
  open_ = index;
  const POINT at = ItemScreenPoint(index);
  PopupShowOptions opt;
  opt.corner_radius = corner_radius_;
  opt.border_width = border_width_;
  if (host_->is_open()) {
    host_->Replace(std::move(col), at);
  } else {
    host_->Show(window_->hwnd(), at, std::move(col), opt);
  }
  Invalidate();
}

void MenuBar::Paint(mx::Canvas& canvas) {
  if (!visible()) {
    return;
  }
  const ThemeTokens& th = Theme::Active();
  canvas.FillRect(bounds_, th.surface_alt);
  canvas.FillRect(RectF{bounds_.x, bounds_.y + bounds_.h - 1.f, bounds_.w, 1.f},
                  th.divider);
  for (int i = 0; i < static_cast<int>(menus_.size()); ++i) {
    const Menu& m = menus_[static_cast<size_t>(i)];
    const RectF cell{bounds_.x + m.x, bounds_.y, m.w, bounds_.h};
    if (i == hover_ || i == open_) {
      canvas.FillRect(cell, th.accent_soft);
    }
    canvas.DrawText(m.title, cell, th.text, th.font_size_sm, th.font_ui.c_str(),
                    mx::TextHAlign::Center);
  }
}

void MenuBar::OnMouseMove(const MouseEvent& e) {
  const int idx = IndexAt(e.x, e.y);
  if (idx != hover_) {
    hover_ = idx;
    Invalidate();
  }
  if (host_ && host_->is_open() && idx >= 0 && idx != open_) {
    OpenMenu(idx);
  }
}

void MenuBar::OnMouseLeave(const MouseEvent&) {
  if (hover_ != -1) {
    hover_ = -1;
    Invalidate();
  }
}

void MenuBar::OnMouseDown(const MouseEvent& e) {
  if (e.button != MouseButton::Left) {
    return;
  }
  const int idx = IndexAt(e.x, e.y);
  if (idx >= 0) {
    OpenMenu(idx);
  }
}

}  // namespace mx::ui
