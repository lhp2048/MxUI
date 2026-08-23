#pragma once

#include "mx/ui/button.h"
#include "mx/ui/checkbox.h"
#include "mx/ui/column.h"
#include "mx/ui/absolute.h"
#include "mx/ui/color_picker.h"
#include "mx/ui/combo.h"
#include "mx/ui/data_grid.h"
#include "mx/ui/date_picker.h"
#include "mx/ui/image_button.h"
#include "mx/ui/image_view.h"
#include "mx/ui/label.h"
#include "mx/ui/list_view.h"
#include "mx/ui/item_list.h"
#include "mx/ui/menu_bar.h"
#include "mx/ui/menu_item.h"
#include "mx/ui/virtual_list.h"
#include "mx/ui/tree_view.h"
#include "mx/ui/native_host.h"
#include "mx/ui/progress_bar.h"
#include "mx/ui/radio.h"
#include "mx/ui/row.h"
#include "mx/ui/scroll_view.h"
#include "mx/ui/slider.h"
#include "mx/ui/spin_box.h"
#include "mx/ui/split_view.h"
#include "mx/ui/status_bar.h"
#include "mx/ui/submenu.h"
#include "mx/ui/switch_control.h"
#include "mx/ui/tab.h"
#include "mx/ui/text_area.h"
#include "mx/ui/text_field.h"
#include "mx/ui/tile.h"
#include "mx/ui/title_bar.h"
#include "mx/ui/toast.h"
#include "mx/ui/user_control.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mx::ui::dsl {

namespace detail {

template <typename T>
class BuilderBase {
 public:
  BuilderBase() : node_(std::make_unique<T>()) {}

  std::unique_ptr<Node> Build() { return std::move(node_); }

  T* get() { return node_.get(); }
  const T* get() const { return node_.get(); }

 protected:
  std::unique_ptr<T> node_;
};

template <typename Self, typename T>
class ChildHost : public BuilderBase<T> {
 public:
  Self& bg(const ColorF& c) {
    this->get()->bg(c);
    return static_cast<Self&>(*this);
  }

  Self& child(std::unique_ptr<Node> n) {
    this->node_->AddChild(std::move(n));
    return static_cast<Self&>(*this);
  }

  template <typename B>
  Self& child(B&& builder) {
    return child(std::forward<B>(builder).Build());
  }
};

}  // namespace detail

class ColumnBuilder : public detail::ChildHost<ColumnBuilder, Column> {
 public:
  ColumnBuilder& padding(float all) {
    get()->padding(all);
    return *this;
  }
  ColumnBuilder& padding(float l, float t, float r, float b) {
    get()->padding(l, t, r, b);
    return *this;
  }
  ColumnBuilder& spacing(float s) {
    get()->spacing(s);
    return *this;
  }
  ColumnBuilder& h_align(Align a) {
    get()->h_align(a);
    return *this;
  }
  ColumnBuilder& v_align(Align a) {
    get()->v_align(a);
    return *this;
  }
  ColumnBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  ColumnBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
  ColumnBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

class RowBuilder : public detail::ChildHost<RowBuilder, Row> {
 public:
  RowBuilder& padding(float all) {
    get()->padding(all);
    return *this;
  }
  RowBuilder& padding(float l, float t, float r, float b) {
    get()->padding(l, t, r, b);
    return *this;
  }
  RowBuilder& spacing(float s) {
    get()->spacing(s);
    return *this;
  }
  RowBuilder& h_align(Align a) {
    get()->h_align(a);
    return *this;
  }
  RowBuilder& v_align(Align a) {
    get()->v_align(a);
    return *this;
  }
  RowBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  RowBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

class TitleBarBuilder : public detail::ChildHost<TitleBarBuilder, TitleBar> {
 public:
  TitleBarBuilder& padding(float all) {
    get()->padding(all);
    return *this;
  }
  TitleBarBuilder& padding(float l, float t, float r, float b) {
    get()->padding(l, t, r, b);
    return *this;
  }
  TitleBarBuilder& spacing(float s) {
    get()->spacing(s);
    return *this;
  }
  TitleBarBuilder& h_align(Align a) {
    get()->h_align(a);
    return *this;
  }
  TitleBarBuilder& v_align(Align a) {
    get()->v_align(a);
    return *this;
  }
  TitleBarBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  TitleBarBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
  TitleBarBuilder& title(std::wstring text) {
    get()->title(std::move(text));
    return *this;
  }
  TitleBarBuilder& icon(std::wstring path) {
    get()->icon(std::move(path));
    return *this;
  }
  TitleBarBuilder& close(bool on) {
    get()->close(on);
    return *this;
  }
  TitleBarBuilder& minimize(bool on) {
    get()->minimize(on);
    return *this;
  }
  TitleBarBuilder& maximize(bool on) {
    get()->maximize(on);
    return *this;
  }
};

class TileBuilder : public detail::ChildHost<TileBuilder, Tile> {
 public:
  TileBuilder& padding(float all) {
    get()->padding(all);
    return *this;
  }
  TileBuilder& spacing(float s) {
    get()->spacing(s);
    return *this;
  }
  TileBuilder& spacing(float h, float v) {
    get()->spacing(h, v);
    return *this;
  }
  TileBuilder& columns(int cols) {
    get()->columns(cols);
    return *this;
  }
  TileBuilder& item_size(float w, float h) {
    get()->item_size(w, h);
    return *this;
  }
  TileBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  TileBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

class TabBuilder : public detail::ChildHost<TabBuilder, Tab> {
 public:
  TabBuilder& selected(int index) {
    get()->set_selected(index);
    return *this;
  }
  TabBuilder& headers(std::vector<std::wstring> titles) {
    get()->set_headers(std::move(titles));
    return *this;
  }
  TabBuilder& header_height(float h) {
    get()->header_height(h);
    return *this;
  }
  TabBuilder& on_selected(Tab::SelectedHandler handler) {
    get()->on_selected(std::move(handler));
    return *this;
  }
  TabBuilder& name(std::string n) {
    get()->set_name(std::move(n));
    return *this;
  }
  TabBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  TabBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
  TabBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

class AbsoluteBuilder : public detail::ChildHost<AbsoluteBuilder, Absolute> {
 public:
  AbsoluteBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  AbsoluteBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
  AbsoluteBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

class LabelBuilder : public detail::BuilderBase<Label> {
 public:
  LabelBuilder& name(std::string n) {
    get()->set_name(std::move(n));
    return *this;
  }
  LabelBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  LabelBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  LabelBuilder& color(const ColorF& c) {
    get()->color(c);
    return *this;
  }
  LabelBuilder& align(TextAlign a) {
    get()->align(a);
    return *this;
  }
  LabelBuilder& wrap(bool enable) {
    get()->wrap(enable);
    return *this;
  }
  LabelBuilder& trim(TextTrim t) {
    get()->trim(t);
    return *this;
  }
  LabelBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  LabelBuilder& fixed_width(float w) {
    get()->fixed_width(w);
    return *this;
  }
  LabelBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
  LabelBuilder& h_align(Align a) {
    get()->h_align(a);
    return *this;
  }
  LabelBuilder& v_align(Align a) {
    get()->v_align(a);
    return *this;
  }
  LabelBuilder& weight(float w) {
    get()->weight(w);
    return *this;
  }
  LabelBuilder& left(float v) {
    get()->left(v);
    return *this;
  }
  LabelBuilder& top(float v) {
    get()->top(v);
    return *this;
  }
  LabelBuilder& right(float v) {
    get()->right(v);
    return *this;
  }
  LabelBuilder& bottom(float v) {
    get()->bottom(v);
    return *this;
  }
  LabelBuilder& preferred_height(float h) {
    get()->preferred_height(h);
    return *this;
  }
};

class ButtonBuilder : public detail::BuilderBase<Button> {
 public:
  ButtonBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  ButtonBuilder& name(std::string n) {
    get()->set_name(std::move(n));
    return *this;
  }
  ButtonBuilder& acc_name(std::wstring n) {
    get()->acc_name(std::move(n));
    return *this;
  }
  ButtonBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  ButtonBuilder& preferred_size(float w, float h) {
    get()->preferred_size(w, h);
    return *this;
  }
  ButtonBuilder& pos(float x, float y) {
    get()->set_pos(x, y);
    return *this;
  }
  ButtonBuilder& left(float v) {
    get()->left(v);
    return *this;
  }
  ButtonBuilder& top(float v) {
    get()->top(v);
    return *this;
  }
  ButtonBuilder& right(float v) {
    get()->right(v);
    return *this;
  }
  ButtonBuilder& bottom(float v) {
    get()->bottom(v);
    return *this;
  }
  ButtonBuilder& weight(float w) {
    get()->weight(w);
    return *this;
  }
  ButtonBuilder& h_align(Align a) {
    get()->h_align(a);
    return *this;
  }
  ButtonBuilder& v_align(Align a) {
    get()->v_align(a);
    return *this;
  }
  ButtonBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
  ButtonBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
  ButtonBuilder& hug_width() {
    get()->hug_width();
    return *this;
  }
  ButtonBuilder& fixed_width(float w) {
    get()->fixed_width(w);
    return *this;
  }
  ButtonBuilder& on_click(Button::ClickHandler handler) {
    get()->on_click(std::move(handler));
    return *this;
  }
  ButtonBuilder& variant(ButtonVariant v) {
    get()->variant(v);
    return *this;
  }
  ButtonBuilder& bg(const ColorF& c) {
    get()->bg(c);
    return *this;
  }
  ButtonBuilder& bg_hover(const ColorF& c) {
    get()->bg_hover(c);
    return *this;
  }
  ButtonBuilder& bg_pressed(const ColorF& c) {
    get()->bg_pressed(c);
    return *this;
  }
  ButtonBuilder& text_color(const ColorF& c) {
    get()->text_color(c);
    return *this;
  }
  ButtonBuilder& text_align(mx::TextHAlign a) {
    get()->text_align(a);
    return *this;
  }
  ButtonBuilder& corner_radius(float r) {
    get()->corner_radius(r);
    return *this;
  }
  ButtonBuilder& enabled(bool e) {
    get()->set_enabled(e);
    return *this;
  }
  ButtonBuilder& is_default(bool v) {
    get()->is_default(v);
    return *this;
  }
  ButtonBuilder& accelerator(KeyChord chord) {
    get()->accelerator(chord);
    return *this;
  }
  ButtonBuilder& accelerator(const std::string& spec) {
    get()->accelerator(spec);
    return *this;
  }
};

class TextFieldBuilder : public detail::BuilderBase<TextField> {
 public:
  TextFieldBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  TextFieldBuilder& placeholder(const std::wstring& t) {
    get()->placeholder(t);
    return *this;
  }
  TextFieldBuilder& password(bool enabled) {
    get()->password(enabled);
    return *this;
  }
  TextFieldBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  TextFieldBuilder& preferred_size(float w, float h) {
    get()->preferred_size(w, h);
    return *this;
  }
  TextFieldBuilder& on_change(TextField::ChangeHandler handler) {
    get()->on_change(std::move(handler));
    return *this;
  }
  TextFieldBuilder& on_submit(std::function<void()> handler) {
    get()->on_submit(std::move(handler));
    return *this;
  }
};

class ImageViewBuilder : public detail::BuilderBase<ImageView> {
 public:
  ImageViewBuilder& path(const std::wstring& p) {
    get()->LoadFromFile(p);
    return *this;
  }
  ImageViewBuilder& preferred_size(float w, float h) {
    get()->preferred_size(w, h);
    return *this;
  }
};

class ImageButtonBuilder : public detail::BuilderBase<ImageButton> {
 public:
  ImageButtonBuilder& name(std::string n) {
    get()->set_name(std::move(n));
    return *this;
  }
  ImageButtonBuilder& preferred_size(float w, float h) {
    get()->preferred_size(w, h);
    return *this;
  }
  ImageButtonBuilder& enabled(bool e) {
    get()->set_enabled(e);
    return *this;
  }
  ImageButtonBuilder& on_click(ImageButton::ClickHandler handler) {
    get()->on_click(std::move(handler));
    return *this;
  }
  ImageButtonBuilder& acc_name(std::wstring n) {
    get()->acc_name(std::move(n));
    return *this;
  }
};

class CheckboxBuilder : public detail::BuilderBase<Checkbox> {
 public:
  CheckboxBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  CheckboxBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  CheckboxBuilder& checked(bool v) {
    get()->checked(v);
    return *this;
  }
  CheckboxBuilder& on_changed(Checkbox::ChangedHandler handler) {
    get()->on_changed(std::move(handler));
    return *this;
  }
};

class RadioBuilder : public detail::BuilderBase<Radio> {
 public:
  RadioBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  RadioBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  RadioBuilder& group_id(int id) {
    get()->group_id(id);
    return *this;
  }
  RadioBuilder& checked(bool v) {
    get()->checked(v);
    return *this;
  }
  RadioBuilder& on_changed(Radio::ChangedHandler handler) {
    get()->on_changed(std::move(handler));
    return *this;
  }
};

class SwitchBuilder : public detail::BuilderBase<Switch> {
 public:
  SwitchBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  SwitchBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  SwitchBuilder& on(bool v) {
    get()->on(v);
    return *this;
  }
  SwitchBuilder& on_changed(Switch::ChangedHandler handler) {
    get()->on_changed(std::move(handler));
    return *this;
  }
};

class ScrollViewBuilder : public detail::BuilderBase<ScrollView> {
 public:
  ScrollViewBuilder& preferred_size(float w, float h) {
    get()->preferred_size(w, h);
    return *this;
  }
  ScrollViewBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  ScrollViewBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
  ScrollViewBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
  ScrollViewBuilder& content(std::unique_ptr<Node> n) {
    get()->set_content(std::move(n));
    return *this;
  }
  template <typename B>
  ScrollViewBuilder& content(B&& builder) {
    return content(std::forward<B>(builder).Build());
  }
};

class SubmenuBuilder : public detail::BuilderBase<Submenu> {
 public:
  SubmenuBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  SubmenuBuilder& content(std::unique_ptr<Node> n) {
    get()->content(std::move(n));
    return *this;
  }
  template <typename B>
  SubmenuBuilder& content(B&& builder) {
    return content(std::forward<B>(builder).Build());
  }
  SubmenuBuilder& open_on_hover(bool v) {
    get()->open_on_hover(v);
    return *this;
  }
  SubmenuBuilder& bg(const ColorF& c) {
    get()->bg(c);
    return *this;
  }
  SubmenuBuilder& bg_hover(const ColorF& c) {
    get()->bg_hover(c);
    return *this;
  }
  SubmenuBuilder& text_color(const ColorF& c) {
    get()->text_color(c);
    return *this;
  }
  SubmenuBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  SubmenuBuilder& corner_radius(float r) {
    get()->corner_radius(r);
    return *this;
  }
};

class ListViewBuilder : public detail::BuilderBase<ListView> {
 public:
  ListViewBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  ListViewBuilder& item(const std::wstring& text) {
    get()->AddItem(text);
    return *this;
  }
  ListViewBuilder& on_selection_changed(ListView::SelectionHandler handler) {
    get()->on_selection_changed(std::move(handler));
    return *this;
  }
  ListViewBuilder& checkable(bool v) {
    get()->checkable(v);
    return *this;
  }
  ListViewBuilder& selected(int index) {
    get()->set_selected_index(index);
    return *this;
  }
  ListViewBuilder& checked(std::vector<int> indices) {
    get()->set_checked_indices(std::move(indices));
    return *this;
  }
};

class SplitViewBuilder : public detail::BuilderBase<SplitView> {
 public:
  SplitViewBuilder& preferred_size(float w, float h) {
    get()->preferred_size(w, h);
    return *this;
  }
  SplitViewBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  SplitViewBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
  SplitViewBuilder& ratio(float r) {
    get()->set_ratio(r);
    return *this;
  }
  SplitViewBuilder& leading(std::unique_ptr<Node> n) {
    get()->set_leading(std::move(n));
    return *this;
  }
  SplitViewBuilder& trailing(std::unique_ptr<Node> n) {
    get()->set_trailing(std::move(n));
    return *this;
  }
  template <typename B>
  SplitViewBuilder& leading(B&& builder) {
    return leading(std::forward<B>(builder).Build());
  }
  template <typename B>
  SplitViewBuilder& trailing(B&& builder) {
    return trailing(std::forward<B>(builder).Build());
  }
};

inline ColumnBuilder Column() { return ColumnBuilder(); }
inline RowBuilder Row() { return RowBuilder(); }
inline TitleBarBuilder TitleBar() { return TitleBarBuilder(); }
inline TileBuilder Tile() { return TileBuilder(); }
inline TabBuilder Tab() { return TabBuilder(); }
inline AbsoluteBuilder Absolute() { return AbsoluteBuilder(); }
inline LabelBuilder Label() { return LabelBuilder(); }
inline ButtonBuilder Button() { return ButtonBuilder(); }

class ToastBuilder : public detail::BuilderBase<Toast> {
 public:
  ToastBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  ToastBuilder& variant(ToastVariant v) {
    get()->variant(v);
    return *this;
  }
  ToastBuilder& duration_sec(float s) {
    get()->duration_sec(s);
    return *this;
  }
  ToastBuilder& animate(bool on) {
    get()->animate(on);
    return *this;
  }
  ToastBuilder& fade_sec(float s) {
    get()->fade_sec(s);
    return *this;
  }
  ToastBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  ToastBuilder& dismiss_on_click(bool on) {
    get()->dismiss_on_click(on);
    return *this;
  }
  ToastBuilder& anchor(ToastAnchor a) {
    get()->anchor(a);
    return *this;
  }
  ToastBuilder& margin(float dip) {
    get()->margin(dip);
    return *this;
  }
  ToastBuilder& offset(float x_dip, float y_dip) {
    get()->offset(x_dip, y_dip);
    return *this;
  }
  ToastBuilder& hug_width() {
    get()->hug_width();
    return *this;
  }
  ToastBuilder& hug_height() {
    get()->hug_height();
    return *this;
  }
  ToastBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  ToastBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

inline ToastBuilder Toast() { return ToastBuilder(); }

class UserControlBuilder : public detail::BuilderBase<UserControl> {
 public:
  UserControlBuilder& on_paint(UserControl::PaintHandler handler) {
    get()->on_paint(std::move(handler));
    return *this;
  }
  UserControlBuilder& on_mouse_down(UserControl::MouseHandler handler) {
    get()->on_mouse_down(std::move(handler));
    return *this;
  }
  UserControlBuilder& on_mouse_up(UserControl::MouseHandler handler) {
    get()->on_mouse_up(std::move(handler));
    return *this;
  }
  UserControlBuilder& on_mouse_move(UserControl::MouseHandler handler) {
    get()->on_mouse_move(std::move(handler));
    return *this;
  }
  UserControlBuilder& on_mouse_wheel(UserControl::MouseHandler handler) {
    get()->on_mouse_wheel(std::move(handler));
    return *this;
  }
  UserControlBuilder& on_key(UserControl::KeyHandler handler) {
    get()->on_key(std::move(handler));
    return *this;
  }
  UserControlBuilder& wants_mouse_wheel(bool want) {
    get()->wants_mouse_wheel(want);
    return *this;
  }
  UserControlBuilder& name(std::string n) {
    get()->set_name(std::move(n));
    return *this;
  }
  UserControlBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  UserControlBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
  UserControlBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
  UserControlBuilder& weight(float w) {
    get()->weight(w);
    return *this;
  }
};

inline UserControlBuilder UserControl() { return UserControlBuilder(); }

class NativeHostBuilder : public detail::BuilderBase<NativeHost> {
 public:
  NativeHostBuilder& attach(HWND hwnd) {
    get()->Attach(hwnd);
    return *this;
  }
  NativeHostBuilder& attach(HWND hwnd, NativeLifetime life) {
    get()->Attach(hwnd, life);
    return *this;
  }
  NativeHostBuilder& attach_borrowed(HWND hwnd) {
    get()->AttachBorrowed(hwnd);
    return *this;
  }
  NativeHostBuilder& name(std::string n) {
    get()->set_name(std::move(n));
    return *this;
  }
  NativeHostBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  NativeHostBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
  NativeHostBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
  NativeHostBuilder& weight(float w) {
    get()->weight(w);
    return *this;
  }
};

inline NativeHostBuilder NativeHost() { return NativeHostBuilder(); }
inline TextFieldBuilder TextField() { return TextFieldBuilder(); }
inline ImageViewBuilder ImageView() { return ImageViewBuilder(); }
inline ImageButtonBuilder ImageButton() { return ImageButtonBuilder(); }
inline CheckboxBuilder Checkbox() { return CheckboxBuilder(); }
inline RadioBuilder Radio() { return RadioBuilder(); }
inline SwitchBuilder Switch() { return SwitchBuilder(); }
inline ScrollViewBuilder ScrollView() { return ScrollViewBuilder(); }
inline SubmenuBuilder Submenu() { return SubmenuBuilder(); }
inline ListViewBuilder ListView() { return ListViewBuilder(); }
inline SplitViewBuilder SplitView() { return SplitViewBuilder(); }

class ProgressBarBuilder : public detail::BuilderBase<ProgressBar> {
 public:
  ProgressBarBuilder& value(float v) {
    get()->value(v);
    return *this;
  }
  ProgressBarBuilder& indeterminate(bool enable) {
    get()->indeterminate(enable);
    return *this;
  }
  ProgressBarBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  ProgressBarBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

class SliderBuilder : public detail::BuilderBase<Slider> {
 public:
  SliderBuilder& value(float v) {
    get()->value(v);
    return *this;
  }
  SliderBuilder& step(float s) {
    get()->step(s);
    return *this;
  }
  SliderBuilder& tick_count(int n) {
    get()->tick_count(n);
    return *this;
  }
  SliderBuilder& orientation(SliderOrientation o) {
    get()->orientation(o);
    return *this;
  }
  SliderBuilder& on_changed(Slider::ChangeHandler handler) {
    get()->on_changed(std::move(handler));
    return *this;
  }
  SliderBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  SliderBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
  SliderBuilder& fixed_width(float w) {
    get()->fixed_width(w);
    return *this;
  }
  SliderBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

class ComboBuilder : public detail::BuilderBase<Combo> {
 public:
  ComboBuilder& items(std::vector<std::wstring> values) {
    get()->items(std::move(values));
    return *this;
  }
  ComboBuilder& selected(int index) {
    get()->selected(index);
    return *this;
  }
  ComboBuilder& selected_indices(std::vector<int> indices) {
    get()->selected_indices(std::move(indices));
    return *this;
  }
  ComboBuilder& editable(bool enable) {
    get()->editable(enable);
    return *this;
  }
  ComboBuilder& multi(bool enable) {
    get()->multi(enable);
    return *this;
  }
  ComboBuilder& on_changed(Combo::ChangeHandler handler) {
    get()->on_changed(std::move(handler));
    return *this;
  }
  ComboBuilder& on_multi_changed(Combo::MultiChangeHandler handler) {
    get()->on_multi_changed(std::move(handler));
    return *this;
  }
  ComboBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
};

class SpinBoxBuilder : public detail::BuilderBase<SpinBox> {
 public:
  SpinBoxBuilder& value(double v) {
    get()->value(v);
    return *this;
  }
  SpinBoxBuilder& min_value(double v) {
    get()->min_value(v);
    return *this;
  }
  SpinBoxBuilder& max_value(double v) {
    get()->max_value(v);
    return *this;
  }
  SpinBoxBuilder& step(double s) {
    get()->step(s);
    return *this;
  }
  SpinBoxBuilder& decimals(int n) {
    get()->decimals(n);
    return *this;
  }
  SpinBoxBuilder& wrap(bool enable) {
    get()->wrap(enable);
    return *this;
  }
  SpinBoxBuilder& on_changed(SpinBox::ChangeHandler handler) {
    get()->on_changed(std::move(handler));
    return *this;
  }
  SpinBoxBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
};

class DatePickerBuilder : public detail::BuilderBase<DatePicker> {
 public:
  DatePickerBuilder& date(CivilDate d) {
    get()->date(d);
    return *this;
  }
  DatePickerBuilder& year(int y) {
    get()->year(y);
    return *this;
  }
  DatePickerBuilder& month(int m) {
    get()->month(m);
    return *this;
  }
  DatePickerBuilder& day(int d) {
    get()->day(d);
    return *this;
  }
  DatePickerBuilder& time(bool enable) {
    get()->time(enable);
    return *this;
  }
  DatePickerBuilder& hour(int h) {
    get()->hour(h);
    return *this;
  }
  DatePickerBuilder& minute(int m) {
    get()->minute(m);
    return *this;
  }
  DatePickerBuilder& seconds(bool enable) {
    get()->seconds(enable);
    return *this;
  }
  DatePickerBuilder& second(int s) {
    get()->second(s);
    return *this;
  }
  DatePickerBuilder& on_changed(DatePicker::ChangeHandler handler) {
    get()->on_changed(std::move(handler));
    return *this;
  }
  DatePickerBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
};

class ColorPickerBuilder : public detail::BuilderBase<ColorPicker> {
 public:
  ColorPickerBuilder& color(const ColorF& c) {
    get()->color(c);
    return *this;
  }
  ColorPickerBuilder& mode(ColorPickerMode m) {
    get()->mode(m);
    return *this;
  }
  ColorPickerBuilder& alpha(bool enable) {
    get()->alpha(enable);
    return *this;
  }
  ColorPickerBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  ColorPickerBuilder& on_changed(ColorPicker::ChangeHandler handler) {
    get()->on_changed(std::move(handler));
    return *this;
  }
  ColorPickerBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
};

class MenuItemBuilder : public detail::BuilderBase<MenuItem> {
 public:
  MenuItemBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  MenuItemBuilder& icon(std::wstring name) {
    get()->icon(std::move(name));
    return *this;
  }
  MenuItemBuilder& separator(bool v = true) {
    get()->separator(v);
    return *this;
  }
  MenuItemBuilder& checkable(bool v = true) {
    get()->checkable(v);
    return *this;
  }
  MenuItemBuilder& checked(bool v) {
    get()->checked(v);
    return *this;
  }
  MenuItemBuilder& radio_group(int id) {
    get()->radio_group(id);
    return *this;
  }
  MenuItemBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  MenuItemBuilder& text_color(const ColorF& c) {
    get()->text_color(c);
    return *this;
  }
  MenuItemBuilder& bg_hover(const ColorF& c) {
    get()->bg_hover(c);
    return *this;
  }
  MenuItemBuilder& on_click(MenuItem::ClickHandler handler) {
    get()->on_click(std::move(handler));
    return *this;
  }
  MenuItemBuilder& on_changed(MenuItem::ChangedHandler handler) {
    get()->on_changed(std::move(handler));
    return *this;
  }
  MenuItemBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
};

class MenuBarBuilder : public detail::BuilderBase<MenuBar> {
 public:
  MenuBarBuilder& add_menu(std::wstring title, std::vector<MenuCommand> commands) {
    get()->add_menu(std::move(title), std::move(commands));
    return *this;
  }
  MenuBarBuilder& on_command(MenuBar::CommandHandler handler) {
    get()->on_command(std::move(handler));
    return *this;
  }
  MenuBarBuilder& corner_radius(float r) {
    get()->corner_radius(r);
    return *this;
  }
  MenuBarBuilder& border_width(float w) {
    get()->border_width(w);
    return *this;
  }
  MenuBarBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
};

class StatusBarBuilder : public detail::BuilderBase<StatusBar> {
 public:
  StatusBarBuilder& items(std::vector<std::wstring> panes) {
    get()->items(std::move(panes));
    return *this;
  }
  StatusBarBuilder& add_item(std::wstring text) {
    get()->add_item(std::move(text));
    return *this;
  }
  StatusBarBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
};

class TextAreaBuilder : public detail::BuilderBase<TextArea> {
 public:
  TextAreaBuilder& text(const std::wstring& t) {
    get()->text(t);
    return *this;
  }
  TextAreaBuilder& placeholder(const std::wstring& t) {
    get()->placeholder(t);
    return *this;
  }
  TextAreaBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  TextAreaBuilder& wrap(bool enable) {
    get()->wrap(enable);
    return *this;
  }
  TextAreaBuilder& on_change(TextArea::ChangeHandler handler) {
    get()->on_change(std::move(handler));
    return *this;
  }
  TextAreaBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  TextAreaBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

class VirtualListBuilder : public detail::BuilderBase<VirtualList> {
 public:
  VirtualListBuilder& item_count(VirtualList::ItemCountFn fn) {
    get()->item_count(std::move(fn));
    return *this;
  }
  VirtualListBuilder& item_kind(VirtualList::ItemKindFn fn) {
    get()->item_kind(std::move(fn));
    return *this;
  }
  VirtualListBuilder& item_text(VirtualList::ItemTextFn fn) {
    get()->item_text(std::move(fn));
    return *this;
  }
  VirtualListBuilder& item_sub_text(VirtualList::ItemSubTextFn fn) {
    get()->item_sub_text(std::move(fn));
    return *this;
  }
  VirtualListBuilder& item_cell_text(VirtualList::ItemCellTextFn fn) {
    get()->item_cell_text(std::move(fn));
    return *this;
  }
  VirtualListBuilder& item_checked(VirtualList::ItemCheckedFn fn) {
    get()->item_checked(std::move(fn));
    return *this;
  }
  VirtualListBuilder& item_set_checked(VirtualList::ItemCheckSetFn fn) {
    get()->item_set_checked(std::move(fn));
    return *this;
  }
  VirtualListBuilder& on_paint_item(VirtualList::PaintItemFn fn) {
    get()->on_paint_item(std::move(fn));
    return *this;
  }
  VirtualListBuilder& on_selection_changed(VirtualList::SelectionHandler h) {
    get()->on_selection_changed(std::move(h));
    return *this;
  }
  VirtualListBuilder& on_check_changed(VirtualList::CheckHandler h) {
    get()->on_check_changed(std::move(h));
    return *this;
  }
  VirtualListBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  VirtualListBuilder& overscan(int rows) {
    get()->overscan(rows);
    return *this;
  }
  VirtualListBuilder& columns(std::vector<ListColumn> cols) {
    get()->columns(std::move(cols));
    return *this;
  }
  VirtualListBuilder& show_header(bool v) {
    get()->show_header(v);
    return *this;
  }
  VirtualListBuilder& header_height(float h) {
    get()->header_height(h);
    return *this;
  }
  VirtualListBuilder& frozen_count(int n) {
    get()->frozen_count(n);
    return *this;
  }
  VirtualListBuilder& on_sort_changed(VirtualList::SortHandler h) {
    get()->on_sort_changed(std::move(h));
    return *this;
  }
  VirtualListBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  VirtualListBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
  VirtualListBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
};

class DataGridBuilder : public detail::BuilderBase<DataGrid> {
 public:
  DataGridBuilder& editable(bool on) {
    get()->editable(on);
    return *this;
  }
  DataGridBuilder& set_row_count(int rows) {
    get()->set_row_count(rows);
    return *this;
  }
  DataGridBuilder& set_cell(int row, int col, std::wstring value) {
    get()->set_cell(row, col, std::move(value));
    return *this;
  }
  DataGridBuilder& on_cell_changed(DataGrid::CellChangedHandler handler) {
    get()->on_cell_changed(std::move(handler));
    return *this;
  }
  DataGridBuilder& on_selection_changed(VirtualList::SelectionHandler h) {
    get()->on_selection_changed(std::move(h));
    return *this;
  }
  DataGridBuilder& on_sort_changed(VirtualList::SortHandler h) {
    get()->on_sort_changed(std::move(h));
    return *this;
  }
  DataGridBuilder& auto_sort(bool on) {
    get()->auto_sort(on);
    return *this;
  }
  DataGridBuilder& sort_compare(DataGrid::SortCompareHandler h) {
    get()->sort_compare(std::move(h));
    return *this;
  }
  DataGridBuilder& columns(std::vector<ListColumn> cols) {
    get()->columns(std::move(cols));
    return *this;
  }
  DataGridBuilder& show_header(bool v) {
    get()->show_header(v);
    return *this;
  }
  DataGridBuilder& header_height(float h) {
    get()->header_height(h);
    return *this;
  }
  DataGridBuilder& frozen_count(int n) {
    get()->frozen_count(n);
    return *this;
  }
  DataGridBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  DataGridBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  DataGridBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
  DataGridBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
};

class TreeViewBuilder : public detail::BuilderBase<TreeView> {
 public:
  TreeViewBuilder& font_size(float size) {
    get()->font_size(size);
    return *this;
  }
  TreeViewBuilder& row_height(float h) {
    get()->row_height(h);
    return *this;
  }
  TreeViewBuilder& indent(float px) {
    get()->indent(px);
    return *this;
  }
  TreeViewBuilder& on_selection_changed(TreeView::SelectionHandler h) {
    get()->on_selection_changed(std::move(h));
    return *this;
  }
  TreeViewBuilder& on_expanded_changed(TreeView::ExpandHandler h) {
    get()->on_expanded_changed(std::move(h));
    return *this;
  }
  TreeViewBuilder& checkable(bool v) {
    get()->checkable(v);
    return *this;
  }
  TreeViewBuilder& check_cascade(bool v) {
    get()->check_cascade(v);
    return *this;
  }
  TreeViewBuilder& on_check_changed(TreeView::CheckHandler h) {
    get()->on_check_changed(std::move(h));
    return *this;
  }
  TreeViewBuilder& on_load_children(TreeView::LoadChildrenHandler h) {
    get()->on_load_children(std::move(h));
    return *this;
  }
  TreeViewBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  TreeViewBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

class ItemListBuilder : public detail::BuilderBase<ItemList> {
 public:
  ItemListBuilder& add_item(ItemList::PaintItemFn paint = {}) {
    get()->AddItem(std::move(paint));
    return *this;
  }
  ItemListBuilder& item_count(int n) {
    get()->set_item_count(n);
    return *this;
  }
  ItemListBuilder& on_paint_item(ItemList::PaintItemFn fn) {
    get()->on_paint_item(std::move(fn));
    return *this;
  }
  ItemListBuilder& on_bind_item(ItemList::BindItemFn fn) {
    get()->on_bind_item(std::move(fn));
    return *this;
  }
  ItemListBuilder& item_template_factory(ItemList::ItemTemplateFactory fn) {
    get()->item_template_factory(std::move(fn));
    return *this;
  }
  ItemListBuilder& on_selection_changed(ItemList::SelectionHandler h) {
    get()->on_selection_changed(std::move(h));
    return *this;
  }
  ItemListBuilder& row_height(float h) {
    get()->row_height(h);
    return *this;
  }
  ItemListBuilder& overscan(int rows) {
    get()->overscan(rows);
    return *this;
  }
  ItemListBuilder& row_padding(float pad) {
    get()->row_padding(pad);
    return *this;
  }
  ItemListBuilder& header_height(float h) {
    get()->header_height(h);
    return *this;
  }
  ItemListBuilder& columns(std::vector<ListColumn> cols) {
    get()->columns(std::move(cols));
    return *this;
  }
  ItemListBuilder& show_header(bool v) {
    get()->show_header(v);
    return *this;
  }
  ItemListBuilder& frozen_count(int n) {
    get()->frozen_count(n);
    return *this;
  }
  ItemListBuilder& on_sort_changed(ItemList::SortHandler h) {
    get()->on_sort_changed(std::move(h));
    return *this;
  }
  ItemListBuilder& selected(int index) {
    get()->set_selected_index(index, false);
    return *this;
  }
  ItemListBuilder& fill_width() {
    get()->fill_width();
    return *this;
  }
  ItemListBuilder& fill_height() {
    get()->fill_height();
    return *this;
  }
  ItemListBuilder& fixed_height(float h) {
    get()->fixed_height(h);
    return *this;
  }
};

inline ProgressBarBuilder ProgressBar() { return ProgressBarBuilder(); }
inline SliderBuilder Slider() { return SliderBuilder(); }
inline ComboBuilder Combo() { return ComboBuilder(); }
inline SpinBoxBuilder SpinBox() { return SpinBoxBuilder(); }
inline DatePickerBuilder DatePicker() { return DatePickerBuilder(); }
inline ColorPickerBuilder ColorPicker() { return ColorPickerBuilder(); }
inline MenuItemBuilder MenuItem() { return MenuItemBuilder(); }
inline MenuBarBuilder MenuBar() { return MenuBarBuilder(); }
inline StatusBarBuilder StatusBar() { return StatusBarBuilder(); }
inline TextAreaBuilder TextArea() { return TextAreaBuilder(); }
inline DataGridBuilder DataGrid() { return DataGridBuilder(); }
inline VirtualListBuilder VirtualList() { return VirtualListBuilder(); }
inline TreeViewBuilder TreeView() { return TreeViewBuilder(); }
inline ItemListBuilder ItemList() { return ItemListBuilder(); }

}  // namespace mx::ui::dsl
