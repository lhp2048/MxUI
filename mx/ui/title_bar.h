#pragma once

#include "mx/ui/row.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace mx::ui {

// Horizontal caption strip. Child layout is Row.
// Clicking empty space or non-focusable children (Label / icon) drags the
// host HWND after a small slop (so a double-click can maximize). Focusable
// children keep their own mouse handling. Double-click maximizes when the
// host is resizable.
//
// No `children`: [icon?] [title] [min] [max/restore] [close].
// Icon is omitted until `icon` path is set. min/max/close default on.
// With `children`: that list is the layout (nothing is auto-inserted).
// Reserved names `icon` / `title` / `minimize` / `maximize` / `close`
// overlay the default slot widget with the child's own props.
class TitleBar : public Row {
 public:
  TitleBar();

  TitleBar& title(std::wstring text);
  const std::wstring& title() const { return title_; }

  TitleBar& icon(std::wstring path);
  const std::wstring& icon() const { return icon_path_; }

  TitleBar& close(bool on);
  bool close() const { return close_; }
  TitleBar& minimize(bool on);
  bool minimize() const { return minimize_; }
  TitleBar& maximize(bool on);
  bool maximize() const { return maximize_; }

  Node* HitTest(float x, float y) override;
  void OnMouseDown(const MouseEvent& e) override;
  void OnMouseUp(const MouseEvent& e) override;
  void OnMouseMove(const MouseEvent& e) override;
  void OnMouseDoubleClick(const MouseEvent& e) override;
  SizeF Measure(float max_w, float max_h) override;
  void Layout(const RectF& final_rect) override;

 protected:
  void OnHostWindowChanged() override;

 private:
  void ResolveChrome();
  void BuildStandardChrome();
  void ApplyNamedSlotDefaults(Node* child);
  void SyncMaximizeGlyph();
  void SyncLocalizedAcc();
  std::unique_ptr<Node> MakeCaptionButton(const char* slot_name,
                                          const wchar_t* text,
                                          const wchar_t* acc_name,
                                          std::function<void()> on_click);
  std::unique_ptr<Node> MakeIconView();

  std::wstring title_;
  std::wstring icon_path_;
  bool close_ = true;
  bool minimize_ = true;
  bool maximize_ = true;
  bool resolved_ = false;
  bool maximize_glyph_locked_ = false;
  uint32_t acc_gen_ = 0;
  bool caption_drag_armed_ = false;
  float caption_drag_x_ = 0.f;
  float caption_drag_y_ = 0.f;
};

}  // namespace mx::ui
