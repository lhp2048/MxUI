#include "mx/ui/combo.h"

#include "mx/ui/list_view.h"
#include "mx/ui/locale.h"
#include "mx/ui/theme.h"
#include "mx/ui/window.h"

#include <algorithm>

namespace mx::ui {
namespace {

wchar_t ToLowerW(wchar_t c) {
  if (c >= L'A' && c <= L'Z') {
    return static_cast<wchar_t>(c - L'A' + L'a');
  }
  return c;
}

bool ContainsIgnoreCase(const std::wstring& hay, const std::wstring& needle) {
  if (needle.empty()) {
    return true;
  }
  if (needle.size() > hay.size()) {
    return false;
  }
  for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
    bool ok = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      if (ToLowerW(hay[i + j]) != ToLowerW(needle[j])) {
        ok = false;
        break;
      }
    }
    if (ok) {
      return true;
    }
  }
  return false;
}

}  // namespace

Combo::Combo() {
  set_focusable(true);
  fill_width();
  fixed_height(36.f);
}

void Combo::BindWindow(Window* window) { window_ = window; }

Combo& Combo::items(std::vector<std::wstring> values) {
  items_ = std::move(values);
  if (selected_ >= static_cast<int>(items_.size())) {
    selected_ = items_.empty() ? -1 : 0;
  }
  selected_indices_.erase(
      std::remove_if(selected_indices_.begin(), selected_indices_.end(),
                     [this](int i) {
                       return i < 0 || i >= static_cast<int>(items_.size());
                     }),
      selected_indices_.end());
  return *this;
}

Combo& Combo::add_item(std::wstring text) {
  items_.push_back(std::move(text));
  if (!multi_ && selected_ < 0 && !items_.empty()) {
    selected_ = 0;
  }
  return *this;
}

Combo& Combo::selected(int index) {
  SelectIndex(index, false);
  return *this;
}

Combo& Combo::selected_indices(std::vector<int> indices) {
  selected_indices_.clear();
  for (int i : indices) {
    if (i >= 0 && i < static_cast<int>(items_.size())) {
      if (std::find(selected_indices_.begin(), selected_indices_.end(), i) ==
          selected_indices_.end()) {
        selected_indices_.push_back(i);
      }
    }
  }
  std::sort(selected_indices_.begin(), selected_indices_.end());
  if (!selected_indices_.empty()) {
    selected_ = selected_indices_.front();
  }
  return *this;
}

Combo& Combo::on_changed(ChangeHandler handler) {
  on_changed_ = std::move(handler);
  return *this;
}

Combo& Combo::on_multi_changed(MultiChangeHandler handler) {
  on_multi_changed_ = std::move(handler);
  return *this;
}

Combo& Combo::font_size(float size) {
  font_size_ = size;
  return *this;
}

Combo& Combo::editable(bool enable) {
  editable_ = enable;
  if (!editable_) {
    filter_.clear();
  }
  return *this;
}

Combo& Combo::multi(bool enable) {
  multi_ = enable;
  if (multi_ && selected_ >= 0 && selected_indices_.empty()) {
    selected_indices_.push_back(selected_);
  }
  return *this;
}

bool Combo::IsIndexSelected(int index) const {
  return std::find(selected_indices_.begin(), selected_indices_.end(),
                   index) != selected_indices_.end();
}

void Combo::NotifyMulti() {
  if (on_multi_changed_) {
    on_multi_changed_(selected_indices_);
  }
}

void Combo::SelectIndex(int index, bool notify) {
  if (items_.empty()) {
    selected_ = -1;
    selected_indices_.clear();
    return;
  }
  const int next = std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
  const bool changed = next != selected_;
  selected_ = next;
  filter_.clear();
  if (changed) {
    NotifyAccValueChanged();
  }
  if (multi_) {
    selected_indices_.assign(1, selected_);
    if (notify) {
      NotifyMulti();
    }
  } else if (notify && on_changed_) {
    on_changed_(selected_);
  }
}

void Combo::SetMultiFromPopup() {
  ListView* list = PopupList();
  if (!list) {
    return;
  }
  selected_indices_.clear();
  for (int popup_i : list->checked_indices()) {
    if (popup_i >= 0 && popup_i < static_cast<int>(popup_index_map_.size())) {
      selected_indices_.push_back(
          popup_index_map_[static_cast<size_t>(popup_i)]);
    }
  }
  std::sort(selected_indices_.begin(), selected_indices_.end());
  selected_ = selected_indices_.empty() ? -1 : selected_indices_.front();
  NotifyMulti();
  if (window_) {
    window_->Invalidate();
  }
}

bool Combo::ItemMatchesFilter(const std::wstring& text) const {
  if (!editable_ || filter_.empty()) {
    return true;
  }
  return ContainsIgnoreCase(text, filter_);
}

ListView* Combo::PopupList() const {
  if (!window_) {
    return nullptr;
  }
  return dynamic_cast<ListView*>(window_->popup());
}

void Combo::ClosePopup() {
  const bool was_open = open_;
  if (multi_ && open_) {
    SetMultiFromPopup();
  }
  open_ = false;
  popup_index_map_.clear();
  if (window_ && window_->popup()) {
    window_->ClearPopup();
  }
  if (was_open) {
    NotifyAccExpandCollapseChanged();
  }
}

void Combo::NavigatePopup(int delta) {
  ListView* list = PopupList();
  if (!list || list->item_count() <= 0) {
    return;
  }
  int i = list->selected_index();
  if (i < 0) {
    i = (delta >= 0) ? 0 : list->item_count() - 1;
  } else {
    i = std::clamp(i + delta, 0, list->item_count() - 1);
  }
  list->set_selected_index(i, false);
  if (window_) {
    window_->Invalidate();
  }
}

void Combo::CommitPopupSelection() {
  ListView* list = PopupList();
  if (!list) {
    return;
  }
  if (multi_) {
    if (list->selected_index() >= 0) {
      list->ToggleChecked(list->selected_index());
      SetMultiFromPopup();
    }
    return;
  }
  const int popup_idx = list->selected_index();
  if (popup_idx < 0 ||
      popup_idx >= static_cast<int>(popup_index_map_.size())) {
    return;
  }
  SelectIndex(popup_index_map_[static_cast<size_t>(popup_idx)], true);
  const bool was_open = open_;
  open_ = false;
  popup_index_map_.clear();
  if (window_) {
    window_->RequestClearPopup();
  }
  if (was_open) {
    NotifyAccExpandCollapseChanged();
  }
}

void Combo::OpenPopup() {
  if (!window_ || items_.empty()) {
    return;
  }
  if (open_) {
    ClosePopup();
    return;
  }

  auto list = std::make_unique<ListView>();
  if (font_size_) {
    list->font_size(*font_size_);
  }
  if (multi_) {
    list->checkable(true);
  }
  popup_index_map_.clear();
  int select_popup = -1;
  std::vector<int> checked_popup;
  for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
    if (!ItemMatchesFilter(items_[static_cast<size_t>(i)])) {
      continue;
    }
    const int popup_i = list->AddItem(items_[static_cast<size_t>(i)]);
    popup_index_map_.push_back(i);
    if (multi_) {
      if (IsIndexSelected(i)) {
        checked_popup.push_back(popup_i);
        if (select_popup < 0) {
          select_popup = popup_i;
        }
      }
    } else if (i == selected_) {
      select_popup = popup_i;
    }
  }
  if (list->item_count() == 0) {
    list->AddItem(Locale::Tr(L"No matches", L"Combo"));
    popup_index_map_.clear();
  }
  if (multi_) {
    list->set_checked_indices(checked_popup);
    list->on_check_changed([this](int, bool) { SetMultiFromPopup(); });
  } else {
    list->on_selection_changed([this](int popup_idx) {
      if (popup_idx < 0 ||
          popup_idx >= static_cast<int>(popup_index_map_.size())) {
        const bool was_open = open_;
        open_ = false;
        if (window_) {
          window_->RequestClearPopup();
        }
        if (was_open) {
          NotifyAccExpandCollapseChanged();
        }
        return;
      }
      SelectIndex(popup_index_map_[static_cast<size_t>(popup_idx)], true);
      const bool was_open = open_;
      open_ = false;
      popup_index_map_.clear();
      if (window_) {
        window_->RequestClearPopup();
      }
      if (was_open) {
        NotifyAccExpandCollapseChanged();
      }
    });
  }
  if (select_popup >= 0) {
    list->set_selected_index(select_popup, false);
  } else if (list->item_count() > 0 && !popup_index_map_.empty()) {
    list->set_selected_index(0, false);
  }

  const SizeF want = list->Measure(bounds_.w, 200.f);
  const float h = std::min(want.h, 180.f);
  list->fixed_height(h);
  list->Layout(RectF{bounds_.x, bounds_.y + bounds_.h + 2.f, bounds_.w, h});

  open_ = true;
  window_->SetPopup(std::move(list), [this]() {
    if (multi_ && open_) {
      // Dismiss already ran ClearPopup; sync from last known state is done
      // in SetMultiFromPopup during checks. Just clear flags.
    }
    const bool was_open = open_;
    open_ = false;
    popup_index_map_.clear();
    if (was_open) {
      NotifyAccExpandCollapseChanged();
    }
  }, this);
  window_->SetFocusNode(this);
  NotifyAccExpandCollapseChanged();
}

std::wstring Combo::SummaryLabel() const {
  if (multi_) {
    if (selected_indices_.empty()) {
      return Locale::Tr(L"None", L"Combo");
    }
    if (selected_indices_.size() == 1) {
      const int i = selected_indices_.front();
      if (i >= 0 && i < static_cast<int>(items_.size())) {
        return items_[static_cast<size_t>(i)];
      }
    }
    return Locale::Format(L"%1 selected", L"Combo",
                          std::to_wstring(selected_indices_.size()));
  }
  if (selected_ >= 0 && selected_ < static_cast<int>(items_.size())) {
    return items_[static_cast<size_t>(selected_)];
  }
  return Locale::Tr(L"None", L"Combo");
}

SizeF Combo::Measure(float max_w, float max_h) {
  return ResolveSize(max_w, max_h,
                     preferred_width() > 0.f ? preferred_width() : max_w, 36.f);
}

void Combo::Paint(mx::Canvas& canvas) {
  if (!visible()) {
    return;
  }
  const ThemeTokens& th = Theme::Active();
  canvas.FillRoundedRect(bounds_, 6.f, 6.f, th.surface);
  canvas.DrawRect(bounds_, focused() ? th.border_focus : th.border,
                  focused() ? 1.5f : 1.f);

  std::wstring label;
  ColorF color = th.text;
  if (editable_ && (focused() || open_) && !filter_.empty()) {
    label = filter_;
  } else {
    label = SummaryLabel();
    const std::wstring none = Locale::Tr(L"None", L"Combo");
    const std::wstring filter_ph = Locale::Tr(L"Filter…", L"Combo");
    if (label == none || label == filter_ph) {
      color = th.text_muted;
    }
    if (editable_ && selected_ < 0 && selected_indices_.empty() &&
        filter_.empty()) {
      label = filter_ph;
      color = th.text_muted;
    }
  }

  const RectF text_rect{bounds_.x + 10.f, bounds_.y,
                        std::max(0.f, bounds_.w - 36.f), bounds_.h};
  canvas.DrawText(label, text_rect, color, ResolveFontSize(font_size_),
                  th.font_ui.c_str(), mx::TextHAlign::Left);

  const float cx = bounds_.x + bounds_.w - 18.f;
  const float cy = bounds_.y + bounds_.h * 0.5f;
  if (open_) {
    canvas.DrawLine(cx - 5.f, cy + 2.f, cx, cy - 3.f, th.glyph, 1.5f);
    canvas.DrawLine(cx, cy - 3.f, cx + 5.f, cy + 2.f, th.glyph, 1.5f);
  } else {
    canvas.DrawLine(cx - 5.f, cy - 2.f, cx, cy + 3.f, th.glyph, 1.5f);
    canvas.DrawLine(cx, cy + 3.f, cx + 5.f, cy - 2.f, th.glyph, 1.5f);
  }
}

void Combo::OnMouseDown(const MouseEvent& e) {
  if (e.button != MouseButton::Left) {
    return;
  }
  OpenPopup();
}

void Combo::OnKey(const KeyEvent& e) {
  if (!e.down || items_.empty()) {
    return;
  }
  if (e.vk == VK_ESCAPE && open_) {
    filter_.clear();
    ClosePopup();
    return;
  }
  if (open_) {
    if (e.vk == VK_RETURN) {
      if (multi_) {
        ClosePopup();
      } else {
        CommitPopupSelection();
      }
      return;
    }
    if (e.vk == VK_SPACE && multi_) {
      CommitPopupSelection();
      return;
    }
    if (e.vk == VK_DOWN) {
      NavigatePopup(1);
      return;
    }
    if (e.vk == VK_UP) {
      NavigatePopup(-1);
      return;
    }
    if (e.vk == VK_HOME) {
      if (ListView* list = PopupList()) {
        list->set_selected_index(0, false);
        if (window_) {
          window_->Invalidate();
        }
      }
      return;
    }
    if (e.vk == VK_END) {
      if (ListView* list = PopupList()) {
        list->set_selected_index(list->item_count() - 1, false);
        if (window_) {
          window_->Invalidate();
        }
      }
      return;
    }
    if (editable_ && e.vk == VK_BACK && !filter_.empty()) {
      filter_.pop_back();
      ClosePopup();
      OpenPopup();
      return;
    }
    return;
  }

  if (e.vk == VK_RETURN || e.vk == VK_SPACE) {
    OpenPopup();
    return;
  }
  if (editable_ && e.vk == VK_BACK && !filter_.empty()) {
    filter_.pop_back();
    if (window_) {
      window_->Invalidate();
    }
    return;
  }
  if (!multi_) {
    if (e.vk == VK_DOWN) {
      SelectIndex(selected_ < 0 ? 0 : selected_ + 1, true);
    } else if (e.vk == VK_UP) {
      SelectIndex(selected_ < 0 ? 0 : selected_ - 1, true);
    }
  }
}

void Combo::OnChar(wchar_t ch) {
  if (!editable_ || items_.empty()) {
    return;
  }
  if (ch < 32 && ch != L'\t') {
    return;
  }
  if (ch == L'\t') {
    return;
  }
  filter_.push_back(ch);
  if (open_) {
    // Rebuild without committing multi selection wipe.
    open_ = false;
    popup_index_map_.clear();
    if (window_ && window_->popup()) {
      window_->ClearPopup();
    }
  }
  OpenPopup();
  if (window_) {
    window_->Invalidate();
  }
}

AccRole Combo::acc_role() const {
  if (acc_role_override_) {
    return *acc_role_override_;
  }
  return AccRole::ComboBox;
}

std::wstring Combo::AccDefaultName() const {
  if (multi_) {
    return SummaryLabel();
  }
  if (selected_ >= 0 && selected_ < static_cast<int>(items_.size())) {
    return items_[static_cast<size_t>(selected_)];
  }
  return {};
}

std::wstring Combo::AccValue() const {
  return AccDefaultName();
}

bool Combo::AccSetValue(const std::wstring& value) {
  for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
    if (items_[static_cast<size_t>(i)] == value) {
      SelectIndex(i, true);
      return true;
    }
  }
  if (value.empty()) {
    return false;
  }
  int idx = 0;
  for (wchar_t c : value) {
    if (c < L'0' || c > L'9') {
      return false;
    }
    idx = idx * 10 + static_cast<int>(c - L'0');
  }
  if (idx < 0 || idx >= static_cast<int>(items_.size())) {
    return false;
  }
  SelectIndex(idx, true);
  return true;
}

bool Combo::AccIsExpanded() const {
  return open_;
}

bool Combo::AccExpand() {
  if (open_) {
    return true;
  }
  OpenPopup();
  return open_;
}

bool Combo::AccCollapse() {
  if (!open_) {
    return true;
  }
  ClosePopup();
  return true;
}

}  // namespace mx::ui
