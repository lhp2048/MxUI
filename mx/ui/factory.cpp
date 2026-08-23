#include "mx/ui/factory.h"

#include "mx/ui/absolute.h"
#include "mx/ui/button.h"
#include "mx/ui/checkbox.h"
#include "mx/ui/column.h"
#include "mx/ui/color_picker.h"
#include "mx/ui/combo.h"
#include "mx/ui/data_grid.h"
#include "mx/ui/date_picker.h"
#include "mx/ui/image_button.h"
#include "mx/ui/image_view.h"
#include "mx/ui/label.h"
#include "mx/ui/list_view.h"
#include "mx/ui/list_columns.h"
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
#include "mx/ui/yaml_loader.h"
#include "mx/ui/theme_yaml.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mx::ui {
namespace {

std::string WideToUtf8(const std::wstring& wide) {
  if (wide.empty()) {
    return {};
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                    static_cast<int>(wide.size()), nullptr, 0,
                                    nullptr, nullptr);
  if (n <= 0) {
    return {};
  }
  std::string out(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                      out.data(), n, nullptr, nullptr);
  return out;
}

TextAlign ParseTextAlign(const std::string& s) {
  if (s == "center" || s == "Center") {
    return TextAlign::Center;
  }
  if (s == "right" || s == "Right") {
    return TextAlign::Right;
  }
  return TextAlign::Left;
}

Align ParseAlign(const std::string& s) {
  if (s == "center" || s == "Center") {
    return Align::Center;
  }
  if (s == "end" || s == "End" || s == "right" || s == "Right") {
    return Align::End;
  }
  return Align::Start;
}

ToastAnchor ParseToastAnchor(const std::string& s) {
  if (s == "bottom_start" || s == "BottomStart") {
    return ToastAnchor::BottomStart;
  }
  if (s == "bottom_end" || s == "BottomEnd") {
    return ToastAnchor::BottomEnd;
  }
  if (s == "top_center" || s == "TopCenter") {
    return ToastAnchor::TopCenter;
  }
  if (s == "top_start" || s == "TopStart") {
    return ToastAnchor::TopStart;
  }
  if (s == "top_end" || s == "TopEnd") {
    return ToastAnchor::TopEnd;
  }
  if (s == "center" || s == "Center") {
    return ToastAnchor::Center;
  }
  return ToastAnchor::BottomCenter;
}

void ApplyPaddingColumn(Column* col, const YAML::Node& props) {
  if (!col || !props["padding"]) {
    return;
  }
  const YAML::Node& p = props["padding"];
  if (p.IsSequence() && p.size() >= 4) {
    col->padding(p[0].as<float>(), p[1].as<float>(), p[2].as<float>(),
                 p[3].as<float>());
  } else {
    col->padding(p.as<float>());
  }
}

void ApplyPaddingRow(Row* row, const YAML::Node& props) {
  if (!row || !props["padding"]) {
    return;
  }
  const YAML::Node& p = props["padding"];
  if (p.IsSequence() && p.size() >= 4) {
    row->padding(p[0].as<float>(), p[1].as<float>(), p[2].as<float>(),
                 p[3].as<float>());
  } else {
    row->padding(p.as<float>());
  }
}

void BindOnClick(Button* btn, const YAML::Node& props,
                 const HandlerMap& handlers) {
  if (!btn || !props["on_click"]) {
    return;
  }
  const std::string name = props["on_click"].as<std::string>();
  const auto it = handlers.find(name);
  if (it != handlers.end()) {
    btn->on_click(it->second);
  }
}

void BindOnClick(MenuItem* item, const YAML::Node& props,
                 const HandlerMap& handlers) {
  if (!item || !props["on_click"]) {
    return;
  }
  const std::string name = props["on_click"].as<std::string>();
  const auto it = handlers.find(name);
  if (it != handlers.end()) {
    item->on_click(it->second);
  }
}

void ApplyMenuCommandMap(MenuCommand* cmd, const YAML::Node& c,
                         const HandlerMap& handlers) {
  if (!cmd || !c || !c.IsMap()) {
    return;
  }
  if (c["separator"] && c["separator"].as<bool>()) {
    cmd->separator = true;
  }
  if (c["text"]) {
    cmd->text = Utf8ToWide(c["text"].as<std::string>());
  }
  if (cmd->text == L"-") {
    cmd->separator = true;
  }
  if (c["icon"]) {
    cmd->icon = Utf8ToWide(c["icon"].as<std::string>());
  }
  if (c["checkable"]) {
    cmd->checkable = c["checkable"].as<bool>();
  }
  if (c["checked"]) {
    cmd->checked = c["checked"].as<bool>();
  }
  if (c["radio_group"]) {
    cmd->radio_group = c["radio_group"].as<int>();
  } else if (c["radio"]) {
    cmd->radio_group = c["radio"].as<int>();
  }
  if (c["on_click"]) {
    const std::string name = c["on_click"].as<std::string>();
    const auto hit = handlers.find(name);
    if (hit != handlers.end()) {
      cmd->on_click = hit->second;
    }
  }
}

template <typename T>
void ApplyOptionalColor(T* ctrl, const YAML::Node& props, const char* key,
                        T& (T::*setter)(const ColorF&)) {
  if (!ctrl || !props[key]) {
    return;
  }
  ColorF c;
  if (ParseColorHex(props[key].as<std::string>(), &c)) {
    (ctrl->*setter)(c);
  }
}

void ApplyButtonChrome(Button* btn, const YAML::Node& props) {
  if (!btn) {
    return;
  }
  if (props["variant"]) {
    const std::string v = props["variant"].as<std::string>();
    if (v == "secondary" || v == "Secondary") {
      btn->variant(ButtonVariant::Secondary);
    } else if (v == "danger" || v == "Danger") {
      btn->variant(ButtonVariant::Danger);
    } else if (v == "primary" || v == "Primary") {
      btn->variant(ButtonVariant::Primary);
    }
  }
  ApplyOptionalColor(btn, props, "bg", &Button::bg);
  ApplyOptionalColor(btn, props, "bg_hover", &Button::bg_hover);
  ApplyOptionalColor(btn, props, "bg_pressed", &Button::bg_pressed);
  ApplyOptionalColor(btn, props, "text_color", &Button::text_color);
  if (props["corner_radius"]) {
    btn->corner_radius(props["corner_radius"].as<float>());
  }
  if (props["text_align"]) {
    const std::string a = props["text_align"].as<std::string>();
    if (a == "left" || a == "Left") {
      btn->text_align(mx::TextHAlign::Left);
    } else if (a == "right" || a == "Right") {
      btn->text_align(mx::TextHAlign::Right);
    } else {
      btn->text_align(mx::TextHAlign::Center);
    }
  }
  if (props["enabled"]) {
    btn->set_enabled(props["enabled"].as<bool>());
  }
}

void ApplySubmenuChrome(Submenu* sm, const YAML::Node& props) {
  if (!sm) {
    return;
  }
  ApplyOptionalColor(sm, props, "bg", &Submenu::bg);
  ApplyOptionalColor(sm, props, "bg_hover", &Submenu::bg_hover);
  ApplyOptionalColor(sm, props, "text_color", &Submenu::text_color);
  if (props["corner_radius"]) {
    sm->corner_radius(props["corner_radius"].as<float>());
  }
  if (props["font_size"]) {
    sm->font_size(props["font_size"].as<float>());
  }
}

// width/height: number → Fixed; "fill"/"hug" → policy. Omitting keeps control defaults.
void ApplySizeAxis(Node* node, const YAML::Node& props, const char* key,
                   bool is_width) {
  if (!node || !props[key]) {
    return;
  }
  const YAML::Node& n = props[key];
  if (!n.IsScalar()) {
    return;
  }
  try {
    const float v = n.as<float>();
    if (is_width) {
      node->fixed_width(v);
    } else {
      node->fixed_height(v);
    }
    return;
  } catch (const YAML::Exception&) {
  }
  const std::string s = n.as<std::string>();
  if (s == "fill" || s == "Fill") {
    if (is_width) {
      node->fill_width();
    } else {
      node->fill_height();
    }
  } else if (s == "hug" || s == "Hug") {
    if (is_width) {
      node->hug_width();
    } else {
      node->hug_height();
    }
  }
}

void ApplyWidthHeight(Node* node, const YAML::Node& props) {
  ApplySizeAxis(node, props, "width", true);
  ApplySizeAxis(node, props, "height", false);
}

std::vector<ListColumn> ParseListColumns(const YAML::Node& props) {
  std::vector<ListColumn> cols;
  if (!props["columns"] || !props["columns"].IsSequence()) {
    return cols;
  }
  for (const auto& c : props["columns"]) {
    ListColumn col;
    if (c.IsScalar()) {
      col.title = Utf8ToWide(c.as<std::string>());
    } else if (c.IsMap()) {
      if (c["title"]) {
        col.title = Utf8ToWide(c["title"].as<std::string>());
      }
      if (c["width"]) {
        col.width = c["width"].as<float>();
      }
      if (c["weight"]) {
        col.weight = c["weight"].as<float>();
      }
      if (c["align"]) {
        const std::string a = c["align"].as<std::string>();
        if (a == "center") {
          col.align = TextAlign::Center;
        } else if (a == "right") {
          col.align = TextAlign::Right;
        }
      }
      if (c["sort_kind"]) {
        const std::string k = c["sort_kind"].as<std::string>();
        if (k == "text" || k == "string") {
          col.sort_kind = ColumnSortKind::Text;
        } else if (k == "number" || k == "numeric") {
          col.sort_kind = ColumnSortKind::Number;
        } else if (k == "natural") {
          col.sort_kind = ColumnSortKind::Natural;
        } else if (k == "auto") {
          col.sort_kind = ColumnSortKind::Auto;
        }
      } else if (col.align == TextAlign::Right) {
        col.sort_kind = ColumnSortKind::Number;
      }
      if (c["sortable"]) {
        col.sortable = c["sortable"].as<bool>();
      }
      if (c["resizable"]) {
        col.resizable = c["resizable"].as<bool>();
      }
      if (c["editable"]) {
        col.editable = c["editable"].as<bool>();
      }
    }
    cols.push_back(std::move(col));
  }
  return cols;
}

void ApplyListColumnsHeader(auto* list, const YAML::Node& props) {
  auto cols = ParseListColumns(props);
  if (!cols.empty()) {
    list->columns(std::move(cols));
  }
  if (props["show_header"]) {
    list->show_header(props["show_header"].as<bool>());
  }
  if (props["header_height"]) {
    list->header_height(props["header_height"].as<float>());
  }
  if (props["frozen_count"]) {
    list->frozen_count(props["frozen_count"].as<int>());
  }
}

void ApplyWeightHVAlign(Node* node, const YAML::Node& props) {
  if (!node) {
    return;
  }
  if (props["weight"]) {
    node->weight(props["weight"].as<float>());
  }
  if (props["h_align"]) {
    node->h_align(ParseAlign(props["h_align"].as<std::string>()));
  }
  if (props["v_align"]) {
    node->v_align(ParseAlign(props["v_align"].as<std::string>()));
  }
}

void ApplyPos(Node* node, const YAML::Node& props) {
  if (!node) {
    return;
  }
  if (props["x"] || props["y"]) {
    const float x = props["x"] ? props["x"].as<float>() : 0.f;
    const float y = props["y"] ? props["y"].as<float>() : 0.f;
    node->set_pos(x, y);
  }
}

void ApplyAnchors(Node* node, const YAML::Node& props) {
  if (!node) {
    return;
  }
  if (props["left"]) {
    node->left(props["left"].as<float>());
  }
  if (props["top"]) {
    node->top(props["top"].as<float>());
  }
  if (props["right"]) {
    node->right(props["right"].as<float>());
  }
  if (props["bottom"]) {
    node->bottom(props["bottom"].as<float>());
  }
}

void BindOnClick(ImageButton* btn, const YAML::Node& props,
                 const HandlerMap& handlers) {
  if (!btn || !props["on_click"]) {
    return;
  }
  const std::string name = props["on_click"].as<std::string>();
  const auto it = handlers.find(name);
  if (it != handlers.end()) {
    btn->on_click(it->second);
  }
}

std::string NodeTypeName(const Node* n) {
  if (!n) {
    return "null";
  }
  if (dynamic_cast<const Column*>(n)) {
    return "Column";
  }
  if (dynamic_cast<const TitleBar*>(n)) {
    return "TitleBar";
  }
  if (dynamic_cast<const Row*>(n)) {
    return "Row";
  }
  if (dynamic_cast<const Tile*>(n)) {
    return "Tile";
  }
  if (dynamic_cast<const Tab*>(n)) {
    return "Tab";
  }
  if (dynamic_cast<const Absolute*>(n)) {
    return "Absolute";
  }
  if (dynamic_cast<const Label*>(n)) {
    return "Label";
  }
  if (dynamic_cast<const Toast*>(n)) {
    return "Toast";
  }
  if (dynamic_cast<const NativeHost*>(n)) {
    return "NativeHost";
  }
  if (dynamic_cast<const UserControl*>(n)) {
    return "UserControl";
  }
  if (dynamic_cast<const Button*>(n)) {
    return "Button";
  }
  if (dynamic_cast<const TextField*>(n)) {
    return "TextField";
  }
  if (dynamic_cast<const ImageView*>(n)) {
    return "ImageView";
  }
  if (dynamic_cast<const ImageButton*>(n)) {
    return "ImageButton";
  }
  if (dynamic_cast<const MenuItem*>(n)) {
    return "MenuItem";
  }
  if (dynamic_cast<const Checkbox*>(n)) {
    return "Checkbox";
  }
  if (dynamic_cast<const Radio*>(n)) {
    return "Radio";
  }
  if (dynamic_cast<const Switch*>(n)) {
    return "Switch";
  }
  if (dynamic_cast<const ScrollView*>(n)) {
    return "ScrollView";
  }
  if (dynamic_cast<const Submenu*>(n)) {
    return "Submenu";
  }
  if (dynamic_cast<const ListView*>(n)) {
    return "ListView";
  }
  if (dynamic_cast<const ItemList*>(n)) {
    return "ItemList";
  }
  if (dynamic_cast<const DataGrid*>(n)) {
    return "DataGrid";
  }
  if (dynamic_cast<const VirtualList*>(n)) {
    return "VirtualList";
  }
  if (dynamic_cast<const TreeView*>(n)) {
    return "TreeView";
  }
  if (dynamic_cast<const ProgressBar*>(n)) {
    return "ProgressBar";
  }
  if (dynamic_cast<const Slider*>(n)) {
    return "Slider";
  }
  if (dynamic_cast<const Combo*>(n)) {
    return "Combo";
  }
  if (dynamic_cast<const SpinBox*>(n)) {
    return "SpinBox";
  }
  if (dynamic_cast<const DatePicker*>(n)) {
    return "DatePicker";
  }
  if (dynamic_cast<const ColorPicker*>(n)) {
    return "ColorPicker";
  }
  if (dynamic_cast<const MenuBar*>(n)) {
    return "MenuBar";
  }
  if (dynamic_cast<const StatusBar*>(n)) {
    return "StatusBar";
  }
  if (dynamic_cast<const TextArea*>(n)) {
    return "TextArea";
  }
  if (dynamic_cast<const SplitView*>(n)) {
    return "SplitView";
  }
  return "Node";
}

std::string NodeDetail(const Node* n) {
  if (const auto* label = dynamic_cast<const Label*>(n)) {
    return " text=\"" + WideToUtf8(label->text()) + "\"";
  }
  if (const auto* toast = dynamic_cast<const Toast*>(n)) {
    return " text=\"" + WideToUtf8(toast->text()) + "\"";
  }
  if (const auto* btn = dynamic_cast<const Button*>(n)) {
    return " text=\"" + WideToUtf8(btn->text()) + "\"";
  }
  if (const auto* mi = dynamic_cast<const MenuItem*>(n)) {
    if (mi->separator()) {
      return " separator";
    }
    return " text=\"" + WideToUtf8(mi->text()) + "\"";
  }
  if (const auto* cb = dynamic_cast<const Checkbox*>(n)) {
    return " text=\"" + WideToUtf8(cb->text()) + "\"";
  }
  if (const auto* radio = dynamic_cast<const Radio*>(n)) {
    return " text=\"" + WideToUtf8(radio->text()) + "\"";
  }
  if (const auto* sw = dynamic_cast<const Switch*>(n)) {
    return " text=\"" + WideToUtf8(sw->text()) + "\"";
  }
  if (const auto* col = dynamic_cast<const Column*>(n)) {
    return " spacing=" + std::to_string(static_cast<int>(col->spacing()));
  }
  if (const auto* row = dynamic_cast<const Row*>(n)) {
    return " spacing=" + std::to_string(static_cast<int>(row->spacing()));
  }
  if (const auto* tile = dynamic_cast<const Tile*>(n)) {
    return " columns=" + std::to_string(tile->columns());
  }
  if (const auto* tab = dynamic_cast<const Tab*>(n)) {
    return " selected=" + std::to_string(tab->selected());
  }
  if (const auto* list = dynamic_cast<const ListView*>(n)) {
    return " items=" + std::to_string(list->item_count());
  }
  if (const auto* split = dynamic_cast<const SplitView*>(n)) {
    return " ratio=" + std::to_string(split->ratio());
  }
  if (const auto* spin = dynamic_cast<const SpinBox*>(n)) {
    return " value=" + std::to_string(static_cast<int>(spin->value()));
  }
  if (const auto* dp = dynamic_cast<const DatePicker*>(n)) {
    return " date=\"" + WideToUtf8(FormatYmd(dp->date())) + "\"";
  }
  if (const auto* dg = dynamic_cast<const DataGrid*>(n)) {
    return " rows=" + std::to_string(dg->row_count());
  }
  if (const auto* cp = dynamic_cast<const ColorPicker*>(n)) {
    return " color=\"" + WideToUtf8(cp->hex(cp->alpha())) + "\"";
  }
  if (const auto* mb = dynamic_cast<const MenuBar*>(n)) {
    return " items=" + std::to_string(mb->menu_count());
  }
  if (const auto* sb = dynamic_cast<const StatusBar*>(n)) {
    return " panes=" + std::to_string(sb->item_count());
  }
  return {};
}

void DumpTreeRec(const Node* n, int depth, std::ostringstream& out) {
  if (!n) {
    return;
  }
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
  out << NodeTypeName(n) << NodeDetail(n) << '\n';
  for (const auto& child : n->children()) {
    DumpTreeRec(child.get(), depth + 1, out);
  }
}

}  // namespace

ViewFactory::ViewFactory() { RegisterBuiltinTypes(); }

void ViewFactory::Register(const std::string& type, NodeBuilder builder) {
  builders_[type] = std::move(builder);
}

bool ViewFactory::HasType(const std::string& type) const {
  return builders_.find(type) != builders_.end();
}

std::unique_ptr<Node> ViewFactory::Build(const std::string& type,
                                         const YAML::Node& props,
                                         const HandlerMap& handlers) const {
  const auto it = builders_.find(type);
  if (it == builders_.end() || !it->second) {
    return nullptr;
  }
  return it->second(props, handlers);
}

void ViewFactory::RegisterBuiltinTypes() {
  Register("Column", [](const YAML::Node& props, const HandlerMap&) {
    auto col = std::make_unique<Column>();
    ApplyPaddingColumn(col.get(), props);
    if (props["spacing"]) {
      col->spacing(props["spacing"].as<float>());
    }
    ApplyWidthHeight(col.get(), props);
    ApplyWeightHVAlign(col.get(), props);
    return col;
  });

  Register("Row", [](const YAML::Node& props, const HandlerMap&) {
    auto row = std::make_unique<Row>();
    ApplyPaddingRow(row.get(), props);
    if (props["spacing"]) {
      row->spacing(props["spacing"].as<float>());
    }
    ApplyWidthHeight(row.get(), props);
    ApplyWeightHVAlign(row.get(), props);
    return row;
  });

  Register("TitleBar", [](const YAML::Node& props, const HandlerMap&) {
    auto bar = std::make_unique<TitleBar>();
    ApplyPaddingRow(bar.get(), props);
    if (props["spacing"]) {
      bar->spacing(props["spacing"].as<float>());
    }
    if (props["title"]) {
      bar->title(Utf8ToWide(props["title"].as<std::string>()));
    }
    if (props["icon"] && props["icon"].IsScalar()) {
      const std::string raw = props["icon"].as<std::string>();
      if (raw != "true" && raw != "false" && raw != "True" && raw != "False") {
        bar->icon(Utf8ToWide(raw));
      }
    }
    if (props["close"]) {
      bar->close(props["close"].as<bool>());
    }
    if (props["minimize"]) {
      bar->minimize(props["minimize"].as<bool>());
    }
    if (props["maximize"]) {
      bar->maximize(props["maximize"].as<bool>());
    }
    ApplyWidthHeight(bar.get(), props);
    ApplyWeightHVAlign(bar.get(), props);
    return bar;
  });

  Register("Tile", [](const YAML::Node& props, const HandlerMap&) {
    auto tile = std::make_unique<Tile>();
    if (props["padding"]) {
      const YAML::Node& p = props["padding"];
      if (p.IsSequence() && p.size() >= 4) {
        tile->padding(p[0].as<float>(), p[1].as<float>(), p[2].as<float>(),
                      p[3].as<float>());
      } else {
        tile->padding(p.as<float>());
      }
    }
    if (props["spacing"]) {
      const YAML::Node& s = props["spacing"];
      if (s.IsSequence() && s.size() >= 2) {
        tile->spacing(s[0].as<float>(), s[1].as<float>());
      } else {
        tile->spacing(s.as<float>());
      }
    }
    if (props["columns"]) {
      tile->columns(props["columns"].as<int>());
    }
    if (props["item_width"] || props["item_height"] || props["item_size"]) {
      float iw = 80.f;
      float ih = 80.f;
      if (props["item_size"] && props["item_size"].IsSequence() &&
          props["item_size"].size() >= 2) {
        iw = props["item_size"][0].as<float>();
        ih = props["item_size"][1].as<float>();
      }
      if (props["item_width"]) {
        iw = props["item_width"].as<float>();
      }
      if (props["item_height"]) {
        ih = props["item_height"].as<float>();
      }
      tile->item_size(iw, ih);
    }
    ApplyWidthHeight(tile.get(), props);
    ApplyWeightHVAlign(tile.get(), props);
    return tile;
  });

  Register("Tab", [](const YAML::Node& props, const HandlerMap&) {
    auto tab = std::make_unique<Tab>();
    if (props["selected"]) {
      tab->set_selected(props["selected"].as<int>());
    }
    if (props["header_height"]) {
      tab->header_height(props["header_height"].as<float>());
    }
    if (props["headers"] && props["headers"].IsSequence()) {
      std::vector<std::wstring> titles;
      for (const auto& h : props["headers"]) {
        titles.push_back(Utf8ToWide(h.as<std::string>()));
      }
      tab->set_headers(std::move(titles));
    }
    ApplyWidthHeight(tab.get(), props);
    ApplyWeightHVAlign(tab.get(), props);
    return tab;
  });

  Register("Absolute", [](const YAML::Node& props, const HandlerMap&) {
    auto abs = std::make_unique<Absolute>();
    ApplyWidthHeight(abs.get(), props);
    ApplyWeightHVAlign(abs.get(), props);
    return abs;
  });

  Register("Label", [](const YAML::Node& props, const HandlerMap&) {
    auto label = std::make_unique<Label>();
    if (props["text"]) {
      label->text(Utf8ToWide(props["text"].as<std::string>()));
    }
    if (props["font_size"]) {
      label->font_size(props["font_size"].as<float>());
    }
    if (props["align"]) {
      label->align(ParseTextAlign(props["align"].as<std::string>()));
    }
    if (props["wrap"]) {
      label->wrap(props["wrap"].as<bool>());
    }
    if (props["trim"]) {
      const std::string t = props["trim"].as<std::string>();
      if (t == "start" || t == "Start") {
        label->trim(TextTrim::Start);
      } else if (t == "middle" || t == "Middle") {
        label->trim(TextTrim::Middle);
      } else if (t == "end" || t == "End") {
        label->trim(TextTrim::End);
      } else {
        label->trim(TextTrim::Clip);
      }
    }
    if (props["preferred_height"]) {
      label->preferred_height(props["preferred_height"].as<float>());
    }
    ApplyWidthHeight(label.get(), props);
    ApplyWeightHVAlign(label.get(), props);
    return label;
  });

  Register("Toast", [](const YAML::Node& props, const HandlerMap&) {
    auto toast = std::make_unique<Toast>();
    if (props["text"]) {
      toast->text(Utf8ToWide(props["text"].as<std::string>()));
    }
    if (props["font_size"]) {
      toast->font_size(props["font_size"].as<float>());
    }
    if (props["variant"]) {
      const std::string v = props["variant"].as<std::string>();
      if (v == "success" || v == "Success") {
        toast->variant(ToastVariant::Success);
      } else if (v == "danger" || v == "Danger") {
        toast->variant(ToastVariant::Danger);
      } else {
        toast->variant(ToastVariant::Info);
      }
    }
    if (props["duration"]) {
      toast->duration_sec(props["duration"].as<float>());
    }
    if (props["animate"]) {
      toast->animate(props["animate"].as<bool>());
    }
    if (props["fade"]) {
      toast->fade_sec(props["fade"].as<float>());
    }
    if (props["dismiss_on_click"]) {
      toast->dismiss_on_click(props["dismiss_on_click"].as<bool>());
    }
    if (props["anchor"]) {
      toast->anchor(ParseToastAnchor(props["anchor"].as<std::string>()));
    }
    if (props["margin"]) {
      toast->margin(props["margin"].as<float>());
    }
    if (props["offset_x"]) {
      toast->offset(props["offset_x"].as<float>(), toast->offset_y());
    }
    if (props["offset_y"]) {
      toast->offset(toast->offset_x(), props["offset_y"].as<float>());
    }
    if (props["offset"] && props["offset"].IsSequence() &&
        props["offset"].size() >= 2) {
      toast->offset(props["offset"][0].as<float>(),
                    props["offset"][1].as<float>());
    }
    ApplyWidthHeight(toast.get(), props);
    ApplyWeightHVAlign(toast.get(), props);
    return toast;
  });

  Register("Button", [](const YAML::Node& props, const HandlerMap& handlers) {
    auto btn = std::make_unique<Button>();
    if (props["text"]) {
      btn->text(Utf8ToWide(props["text"].as<std::string>()));
    }
    if (props["font_size"]) {
      btn->font_size(props["font_size"].as<float>());
    }
    ApplyButtonChrome(btn.get(), props);
    if (props["default"]) {
      btn->is_default(props["default"].as<bool>());
    }
    if (props["accelerator"]) {
      btn->accelerator(props["accelerator"].as<std::string>());
    }
    ApplyWidthHeight(btn.get(), props);
    ApplyWeightHVAlign(btn.get(), props);
    BindOnClick(btn.get(), props, handlers);
    return btn;
  });

  Register("NativeHost", [](const YAML::Node& props, const HandlerMap&) {
    auto host = std::make_unique<NativeHost>();
    ApplyWidthHeight(host.get(), props);
    ApplyWeightHVAlign(host.get(), props);
    return host;
  });

  Register("UserControl", [](const YAML::Node& props, const HandlerMap&) {
    auto view = std::make_unique<UserControl>();
    ApplyWidthHeight(view.get(), props);
    ApplyWeightHVAlign(view.get(), props);
    return view;
  });

  Register("TextField", [](const YAML::Node& props, const HandlerMap&) {
    auto field = std::make_unique<TextField>();
    if (props["text"]) {
      field->text(Utf8ToWide(props["text"].as<std::string>()));
    }
    if (props["placeholder"]) {
      field->placeholder(Utf8ToWide(props["placeholder"].as<std::string>()));
    }
    if (props["password"]) {
      field->password(props["password"].as<bool>());
    }
    if (props["font_size"]) {
      field->font_size(props["font_size"].as<float>());
    }
    ApplyWidthHeight(field.get(), props);
    ApplyWeightHVAlign(field.get(), props);
    return field;
  });

  Register("ImageView", [](const YAML::Node& props, const HandlerMap&) {
    auto image = std::make_unique<ImageView>();
    if (props["path"]) {
      image->LoadFromFile(Utf8ToWide(props["path"].as<std::string>()));
    }
    ApplyWidthHeight(image.get(), props);
    ApplyWeightHVAlign(image.get(), props);
    return image;
  });

  Register("ImageButton",
           [](const YAML::Node& props, const HandlerMap& handlers) {
             auto btn = std::make_unique<ImageButton>();
             ApplyWidthHeight(btn.get(), props);
             ApplyWeightHVAlign(btn.get(), props);
             if (props["enabled"]) {
               btn->set_enabled(props["enabled"].as<bool>());
             }
             BindOnClick(btn.get(), props, handlers);
             return btn;
           });

  Register("Checkbox", [](const YAML::Node& props, const HandlerMap&) {
    auto cb = std::make_unique<Checkbox>();
    if (props["text"]) {
      cb->text(Utf8ToWide(props["text"].as<std::string>()));
    }
    if (props["font_size"]) {
      cb->font_size(props["font_size"].as<float>());
    }
    if (props["checked"]) {
      cb->checked(props["checked"].as<bool>());
    }
    ApplyWidthHeight(cb.get(), props);
    ApplyWeightHVAlign(cb.get(), props);
    return cb;
  });

  Register("Radio", [](const YAML::Node& props, const HandlerMap&) {
    auto radio = std::make_unique<Radio>();
    if (props["text"]) {
      radio->text(Utf8ToWide(props["text"].as<std::string>()));
    }
    if (props["font_size"]) {
      radio->font_size(props["font_size"].as<float>());
    }
    if (props["group_id"]) {
      radio->group_id(props["group_id"].as<int>());
    }
    if (props["checked"]) {
      radio->checked(props["checked"].as<bool>());
    }
    ApplyWidthHeight(radio.get(), props);
    ApplyWeightHVAlign(radio.get(), props);
    return radio;
  });

  Register("Switch", [](const YAML::Node& props, const HandlerMap&) {
    auto sw = std::make_unique<Switch>();
    if (props["text"]) {
      sw->text(Utf8ToWide(props["text"].as<std::string>()));
    }
    if (props["font_size"]) {
      sw->font_size(props["font_size"].as<float>());
    }
    // Prefer "value" / "is_on"; also accept "on" (YAML bool key).
    if (props["value"]) {
      sw->on(props["value"].as<bool>());
    } else if (props["is_on"]) {
      sw->on(props["is_on"].as<bool>());
    } else if (props["on"] && props["on"].IsScalar()) {
      sw->on(props["on"].as<bool>());
    }
    ApplyWidthHeight(sw.get(), props);
    ApplyWeightHVAlign(sw.get(), props);
    return sw;
  });

  Register("ScrollView", [](const YAML::Node& props, const HandlerMap&) {
    auto scroll = std::make_unique<ScrollView>();
    scroll->fill_width();
    ApplyWidthHeight(scroll.get(), props);
    ApplyWeightHVAlign(scroll.get(), props);
    return scroll;
  });

  Register("Submenu", [](const YAML::Node& props, const HandlerMap&) {
    auto sm = std::make_unique<Submenu>();
    if (props["text"]) {
      sm->text(Utf8ToWide(props["text"].as<std::string>()));
    }
    if (props["open_on_hover"]) {
      sm->open_on_hover(props["open_on_hover"].as<bool>());
    }
    ApplySubmenuChrome(sm.get(), props);
    ApplyWidthHeight(sm.get(), props);
    ApplyWeightHVAlign(sm.get(), props);
    return sm;
  });

  Register("ListView", [](const YAML::Node& props, const HandlerMap&) {
    auto list = std::make_unique<ListView>();
    if (props["font_size"]) {
      list->font_size(props["font_size"].as<float>());
    }
    if (props["items"] && props["items"].IsSequence()) {
      for (const auto& item : props["items"]) {
        list->AddItem(Utf8ToWide(item.as<std::string>()));
      }
    }
    if (props["checkable"]) {
      list->checkable(props["checkable"].as<bool>());
    }
    if (props["selected"]) {
      list->set_selected_index(props["selected"].as<int>());
    }
    if (props["checked"] && props["checked"].IsSequence()) {
      std::vector<int> idxs;
      for (const auto& c : props["checked"]) {
        idxs.push_back(c.as<int>());
      }
      list->set_checked_indices(idxs);
    }
    ApplyWidthHeight(list.get(), props);
    ApplyWeightHVAlign(list.get(), props);
    return list;
  });

  Register("SplitView", [](const YAML::Node& props, const HandlerMap&) {
    auto split = std::make_unique<SplitView>();
    split->fill_width();
    ApplyWidthHeight(split.get(), props);
    ApplyWeightHVAlign(split.get(), props);
    if (props["ratio"]) {
      split->set_ratio(props["ratio"].as<float>());
    }
    return split;
  });

  Register("ProgressBar", [](const YAML::Node& props, const HandlerMap&) {
    auto bar = std::make_unique<ProgressBar>();
    if (props["value"]) {
      bar->value(props["value"].as<float>());
    }
    if (props["indeterminate"]) {
      bar->indeterminate(props["indeterminate"].as<bool>());
    }
    ApplyWidthHeight(bar.get(), props);
    ApplyWeightHVAlign(bar.get(), props);
    return bar;
  });

  Register("Slider", [](const YAML::Node& props, const HandlerMap&) {
    auto slider = std::make_unique<Slider>();
    if (props["value"]) {
      slider->value(props["value"].as<float>());
    }
    if (props["step"]) {
      slider->step(props["step"].as<float>());
    }
    if (props["tick_count"]) {
      slider->tick_count(props["tick_count"].as<int>());
    }
    if (props["orientation"]) {
      const auto o = props["orientation"].as<std::string>();
      if (o == "vertical" || o == "v") {
        slider->orientation(SliderOrientation::Vertical);
      } else {
        slider->orientation(SliderOrientation::Horizontal);
      }
    }
    ApplyWidthHeight(slider.get(), props);
    ApplyWeightHVAlign(slider.get(), props);
    return slider;
  });

  Register("Combo", [](const YAML::Node& props, const HandlerMap&) {
    auto combo = std::make_unique<Combo>();
    if (props["font_size"]) {
      combo->font_size(props["font_size"].as<float>());
    }
    if (props["editable"]) {
      combo->editable(props["editable"].as<bool>());
    }
    if (props["multi"]) {
      combo->multi(props["multi"].as<bool>());
    }
    if (props["items"] && props["items"].IsSequence()) {
      std::vector<std::wstring> items;
      for (const auto& it : props["items"]) {
        items.push_back(Utf8ToWide(it.as<std::string>()));
      }
      combo->items(std::move(items));
    }
    if (props["selected"]) {
      if (props["selected"].IsSequence()) {
        std::vector<int> idxs;
        for (const auto& it : props["selected"]) {
          idxs.push_back(it.as<int>());
        }
        combo->selected_indices(std::move(idxs));
      } else {
        combo->selected(props["selected"].as<int>());
      }
    }
    ApplyWidthHeight(combo.get(), props);
    ApplyWeightHVAlign(combo.get(), props);
    return combo;
  });

  Register("SpinBox", [](const YAML::Node& props, const HandlerMap&) {
    auto spin = std::make_unique<SpinBox>();
    if (props["min"]) {
      spin->min_value(props["min"].as<double>());
    }
    if (props["max"]) {
      spin->max_value(props["max"].as<double>());
    }
    if (props["step"]) {
      spin->step(props["step"].as<double>());
    }
    if (props["decimals"]) {
      spin->decimals(props["decimals"].as<int>());
    }
    if (props["wrap"]) {
      spin->wrap(props["wrap"].as<bool>());
    }
    if (props["value"]) {
      spin->value(props["value"].as<double>());
    }
    if (props["font_size"]) {
      spin->font_size(props["font_size"].as<float>());
    }
    ApplyWidthHeight(spin.get(), props);
    ApplyWeightHVAlign(spin.get(), props);
    return spin;
  });

  Register("DatePicker", [](const YAML::Node& props, const HandlerMap&) {
    auto dp = std::make_unique<DatePicker>();
    CivilDate d = dp->date();
    if (props["year"]) {
      d.year = props["year"].as<int>();
    }
    if (props["month"]) {
      d.month = props["month"].as<int>();
    }
    if (props["day"]) {
      d.day = props["day"].as<int>();
    }
    dp->date(d);
    if (props["time"]) {
      dp->time(props["time"].as<bool>());
    }
    if (props["hour"]) {
      dp->hour(props["hour"].as<int>());
    }
    if (props["minute"]) {
      dp->minute(props["minute"].as<int>());
    }
    if (props["seconds"]) {
      dp->seconds(props["seconds"].as<bool>());
    }
    if (props["second"]) {
      dp->second(props["second"].as<int>());
      if (!props["seconds"]) {
        dp->seconds(true);
      }
    }
    if (props["font_size"]) {
      dp->font_size(props["font_size"].as<float>());
    }
    ApplyWidthHeight(dp.get(), props);
    ApplyWeightHVAlign(dp.get(), props);
    return dp;
  });

  Register("ColorPicker", [](const YAML::Node& props, const HandlerMap&) {
    auto cp = std::make_unique<ColorPicker>();
    if (props["mode"]) {
      const std::string m = props["mode"].as<std::string>();
      if (m == "full" || m == "Full") {
        cp->mode(ColorPickerMode::Full);
      } else {
        cp->mode(ColorPickerMode::Simple);
      }
    }
    if (props["alpha"]) {
      cp->alpha(props["alpha"].as<bool>());
    }
    ColorF c = cp->color();
    if (props["color"]) {
      ParseColorHex(props["color"].as<std::string>(), &c);
    }
    if (props["r"]) {
      c.r = props["r"].as<float>();
      if (c.r > 1.f) {
        c.r /= 255.f;
      }
    }
    if (props["g"]) {
      c.g = props["g"].as<float>();
      if (c.g > 1.f) {
        c.g /= 255.f;
      }
    }
    if (props["b"]) {
      c.b = props["b"].as<float>();
      if (c.b > 1.f) {
        c.b /= 255.f;
      }
    }
    if (props["a"]) {
      c.a = props["a"].as<float>();
      if (c.a > 1.f) {
        c.a /= 255.f;
      }
      if (!props["alpha"]) {
        cp->alpha(true);
      }
    }
    cp->color(c);
    if (props["font_size"]) {
      cp->font_size(props["font_size"].as<float>());
    }
    ApplyWidthHeight(cp.get(), props);
    ApplyWeightHVAlign(cp.get(), props);
    return cp;
  });

  Register("MenuItem", [](const YAML::Node& props, const HandlerMap& handlers) {
    auto item = std::make_unique<MenuItem>();
    if (props["separator"] && props["separator"].as<bool>()) {
      item->separator(true);
    }
    if (props["text"]) {
      item->text(Utf8ToWide(props["text"].as<std::string>()));
      if (item->text() == L"-") {
        item->separator(true);
      }
    }
    if (props["icon"]) {
      item->icon(Utf8ToWide(props["icon"].as<std::string>()));
    }
    if (props["checkable"]) {
      item->checkable(props["checkable"].as<bool>());
    }
    if (props["checked"]) {
      item->checked(props["checked"].as<bool>());
    }
    if (props["radio_group"]) {
      item->radio_group(props["radio_group"].as<int>());
    } else if (props["radio"]) {
      item->radio_group(props["radio"].as<int>());
    }
    if (props["font_size"]) {
      item->font_size(props["font_size"].as<float>());
    }
    ApplyOptionalColor(item.get(), props, "text_color", &MenuItem::text_color);
    ApplyOptionalColor(item.get(), props, "bg_hover", &MenuItem::bg_hover);
    ApplyWidthHeight(item.get(), props);
    ApplyWeightHVAlign(item.get(), props);
    BindOnClick(item.get(), props, handlers);
    return item;
  });

  Register("MenuBar", [](const YAML::Node& props, const HandlerMap& handlers) {
    auto bar = std::make_unique<MenuBar>();
    if (props["items"] && props["items"].IsSequence()) {
      for (const auto& it : props["items"]) {
        if (!it || !it.IsMap()) {
          continue;
        }
        std::wstring title;
        if (it["text"]) {
          title = Utf8ToWide(it["text"].as<std::string>());
        }
        std::vector<MenuCommand> cmds;
        if (it["items"] && it["items"].IsSequence()) {
          for (const auto& c : it["items"]) {
            MenuCommand cmd;
            if (c.IsScalar()) {
              cmd.text = Utf8ToWide(c.as<std::string>());
              cmd.separator = (cmd.text == L"-");
            } else if (c.IsMap()) {
              ApplyMenuCommandMap(&cmd, c, handlers);
            }
            cmds.push_back(std::move(cmd));
          }
        }
        bar->add_menu(std::move(title), std::move(cmds));
      }
    }
    ApplyWidthHeight(bar.get(), props);
    ApplyWeightHVAlign(bar.get(), props);
    if (props["corner_radius"]) {
      bar->corner_radius(props["corner_radius"].as<float>());
    }
    if (props["border_width"]) {
      bar->border_width(props["border_width"].as<float>());
    }
    return bar;
  });

  Register("StatusBar", [](const YAML::Node& props, const HandlerMap&) {
    auto bar = std::make_unique<StatusBar>();
    if (props["items"] && props["items"].IsSequence()) {
      std::vector<std::wstring> panes;
      for (const auto& it : props["items"]) {
        if (it.IsScalar()) {
          panes.push_back(Utf8ToWide(it.as<std::string>()));
        } else if (it.IsMap() && it["text"]) {
          panes.push_back(Utf8ToWide(it["text"].as<std::string>()));
        }
      }
      bar->items(std::move(panes));
    }
    ApplyWidthHeight(bar.get(), props);
    ApplyWeightHVAlign(bar.get(), props);
    return bar;
  });

  Register("TextArea", [](const YAML::Node& props, const HandlerMap&) {
    auto area = std::make_unique<TextArea>();
    if (props["text"]) {
      area->text(Utf8ToWide(props["text"].as<std::string>()));
    }
    if (props["placeholder"]) {
      area->placeholder(Utf8ToWide(props["placeholder"].as<std::string>()));
    }
    if (props["font_size"]) {
      area->font_size(props["font_size"].as<float>());
    }
    if (props["wrap"]) {
      area->wrap(props["wrap"].as<bool>());
    }
    ApplyWidthHeight(area.get(), props);
    ApplyWeightHVAlign(area.get(), props);
    return area;
  });

  Register("DataGrid", [](const YAML::Node& props, const HandlerMap&) {
    auto grid = std::make_unique<DataGrid>();
    int rows = 10;
    if (props["rows"]) {
      rows = std::max(0, props["rows"].as<int>());
    }
    if (props["editable"]) {
      grid->editable(props["editable"].as<bool>());
    }
    if (props["auto_sort"]) {
      grid->auto_sort(props["auto_sort"].as<bool>());
    }
    if (props["font_size"]) {
      grid->font_size(props["font_size"].as<float>());
    }
    ApplyListColumnsHeader(grid.get(), props);
    grid->set_row_count(rows);
    if (props["data"] && props["data"].IsSequence()) {
      int r = 0;
      for (const auto& row_node : props["data"]) {
        if (row_node.IsSequence()) {
          int c = 0;
          for (const auto& cell : row_node) {
            grid->set_cell(r, c, Utf8ToWide(cell.as<std::string>()));
            ++c;
          }
        }
        ++r;
      }
    } else {
      const int col_count = static_cast<int>(grid->columns().size());
      for (int i = 0; i < rows; ++i) {
        if (col_count > 0) {
          grid->set_cell(i, 0, L"行 " + std::to_wstring(i + 1));
        }
        if (col_count > 1) {
          grid->set_cell(i, 1, std::to_wstring(i * 10));
        }
        if (col_count > 2) {
          grid->set_cell(i, 2, L"备注 " + std::to_wstring(i));
        }
        for (int c = 3; c < col_count; ++c) {
          grid->set_cell(i, c, L"#" + std::to_wstring(i));
        }
      }
    }
    ApplyWidthHeight(grid.get(), props);
    ApplyWeightHVAlign(grid.get(), props);
    return grid;
  });

  // Thin YAML: count of Text rows for smoke demos. Real apps use fluent callbacks.
  Register("VirtualList", [](const YAML::Node& props, const HandlerMap&) {
    auto list = std::make_unique<VirtualList>();
    int count = 100;
    if (props["count"]) {
      count = std::max(0, props["count"].as<int>());
    }
    if (props["font_size"]) {
      list->font_size(props["font_size"].as<float>());
    }
    ApplyListColumnsHeader(list.get(), props);
    list->item_count([count]() { return count; });
    if (!list->columns().empty()) {
      list->item_cell_text([](int i, int col) {
        if (col == 0) {
          return L"行 " + std::to_wstring(i + 1);
        }
        if (col == 1) {
          return L"状态-" + std::to_wstring(i % 5);
        }
        if (col == 2) {
          return L"详情-" + std::to_wstring(i);
        }
        return L"#" + std::to_wstring(i);
      });
    } else {
      list->item_text([](int i) {
        return L"Virtual item " + std::to_wstring(i);
      });
    }
    ApplyWidthHeight(list.get(), props);
    ApplyWeightHVAlign(list.get(), props);
    return list;
  });

  // TreeView: nested YAML `nodes:` with text / expanded / children / lazy / checked.
  Register("TreeView", [](const YAML::Node& props, const HandlerMap&) {
    auto tree = std::make_unique<TreeView>();
    if (props["font_size"]) {
      tree->font_size(props["font_size"].as<float>());
    }
    if (props["row_height"]) {
      tree->row_height(props["row_height"].as<float>());
    }
    if (props["indent"]) {
      tree->indent(props["indent"].as<float>());
    }
    if (props["checkable"]) {
      tree->checkable(props["checkable"].as<bool>());
    }
    if (props["check_cascade"]) {
      tree->check_cascade(props["check_cascade"].as<bool>());
    }
    std::function<void(int parent, const YAML::Node& seq)> load;
    load = [&](int parent, const YAML::Node& seq) {
      if (!seq || !seq.IsSequence()) {
        return;
      }
      for (const auto& item : seq) {
        std::wstring text = L"node";
        bool expanded = false;
        bool lazy = false;
        bool checked = false;
        if (item.IsMap()) {
          if (item["text"]) {
            text = Utf8ToWide(item["text"].as<std::string>());
          }
          if (item["expanded"]) {
            expanded = item["expanded"].as<bool>();
          }
          if (item["lazy"]) {
            lazy = item["lazy"].as<bool>();
          }
          if (item["checked"]) {
            checked = item["checked"].as<bool>();
          }
          const int id = tree->AddNode(parent, std::move(text), expanded);
          if (lazy) {
            tree->set_lazy(id, true);
          }
          if (checked) {
            tree->set_checked(id, TreeCheckState::Checked, false);
          }
          if (item["children"]) {
            load(id, item["children"]);
          }
        } else {
          tree->AddNode(parent, Utf8ToWide(item.as<std::string>()), false);
        }
      }
    };
    if (props["nodes"]) {
      load(-1, props["nodes"]);
    }
    ApplyWidthHeight(tree.get(), props);
    ApplyWeightHVAlign(tree.get(), props);
    return tree;
  });

  // ItemList: count + optional item_template (row widget tree); bind/paint in code.
  Register("ItemList", [this](const YAML::Node& props, const HandlerMap& handlers) {
    auto list = std::make_unique<ItemList>();
    int count = 0;
    if (props["count"]) {
      count = std::max(0, props["count"].as<int>());
    }
    if (props["row_height"]) {
      list->row_height(props["row_height"].as<float>());
    }
    if (props["overscan"]) {
      list->overscan(props["overscan"].as<int>());
    }
    if (props["row_padding"]) {
      list->row_padding(props["row_padding"].as<float>());
    }
    ApplyListColumnsHeader(list.get(), props);
    if (props["item_template"]) {
      // Clone: YAML::Node is a view into the Load() document; that document is
      // destroyed when CreateFromYaml* returns. Deferring LoadYamlNode without
      // Clone (and without keeping ViewFactory alive) AVs on first SyncVisibleRows.
      const YAML::Node tmpl = YAML::Clone(props["item_template"]);
      list->item_template_factory([tmpl, handlers]() {
        ViewFactory factory;
        return LoadYamlNode(tmpl, factory, handlers);
      });
    }
    for (int i = 0; i < count; ++i) {
      list->AddItem();
    }
    if (props["selected"]) {
      list->set_selected_index(props["selected"].as<int>(), false);
    }
    ApplyWidthHeight(list.get(), props);
    ApplyWeightHVAlign(list.get(), props);
    return list;
  });
}

std::unique_ptr<Node> ViewFactory::CreateFromYamlFile(
    const std::string& path, const HandlerMap& handlers,
    WindowYaml* window_out) const {
  return LoadYamlFile(path, *this, handlers, window_out);
}

std::unique_ptr<Node> ViewFactory::CreateFromYamlString(
    const std::string& yaml, const HandlerMap& handlers,
    WindowYaml* window_out) const {
  return LoadYamlString(yaml, *this, handlers, window_out);
}

std::unique_ptr<Node> ViewFactory::CreateFromYaml(
    const std::string& path_or_yaml, const HandlerMap& handlers,
    WindowYaml* window_out) const {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path as_path(path_or_yaml);
  if (fs::exists(as_path, ec) && fs::is_regular_file(as_path, ec)) {
    return CreateFromYamlFile(path_or_yaml, handlers, window_out);
  }
  return CreateFromYamlString(path_or_yaml, handlers, window_out);
}

std::string ViewFactory::DumpTree(const Node* root) {
  std::ostringstream out;
  DumpTreeRec(root, 0, out);
  return out.str();
}

}  // namespace mx::ui
