#include "mx/ui/date_picker.h"

#include "mx/ui/locale.h"
#include "mx/ui/theme.h"
#include "mx/ui/window.h"

#include <algorithm>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mx::ui {
namespace {

constexpr float kCalW = 280.f;
constexpr float kHeaderH = 36.f;
constexpr float kDowH = 24.f;
constexpr float kCellH = 32.f;
constexpr float kTimeBoxW = 62.f;
constexpr float kTimeBoxH = 30.f;
constexpr float kTimeChevW = 18.f;
constexpr float kTimeColonW = 14.f;
constexpr float kTimeH = 38.f;
constexpr float kPad = 8.f;
constexpr int kYearMin = 1900;
constexpr int kYearMax = 2100;

const wchar_t* kDowEn[] = {L"Mon", L"Tue", L"Wed", L"Thu",
                           L"Fri", L"Sat", L"Sun"};

int MondayIndex(int sunday_based) {
  return (sunday_based + 6) % 7;
}

}  // namespace

class CalendarPopup : public Node {
  enum class Panel { Days, Months, Years, Hours, Minutes, Seconds };

 public:
  CalendarPopup(DatePicker* owner, CivilDate selected, bool show_time,
                bool show_seconds, int hour, int minute, int second)
      : owner_(owner),
        selected_(NormalizeDate(selected)),
        view_(NormalizeDate(CivilDate{selected.year, selected.month, 1})),
        show_time_(show_time),
        show_seconds_(show_time && show_seconds),
        hour_(std::clamp(hour, 0, 23)),
        minute_(std::clamp(minute, 0, 59)),
        second_(std::clamp(second, 0, 59)) {
    hug_width();
    hug_height();
  }

  SizeF Measure(float max_w, float max_h) override {
    const float h = kPad + kHeaderH + kDowH + kCellH * 6.f +
                    (show_time_ ? kTimeH : kPad);
    return ResolveSize(max_w, max_h, kCalW, h);
  }

  void Paint(mx::Canvas& canvas) override {
    if (!visible()) {
      return;
    }
    const ThemeTokens& th = Theme::Active();
    canvas.FillRoundedRect(bounds_, 8.f, 8.f, th.surface);
    canvas.DrawRect(bounds_, th.border, 1.f);

    DrawChevron(canvas, PrevRect(), true, th.glyph);
    DrawChevron(canvas, NextRect(), false, th.glyph);
    DrawHeaderChip(canvas, YearChip(), YearLabel(), hover_head_ == 1,
                   panel_ == Panel::Years, th);
    DrawHeaderChip(canvas, MonthChip(), MonthLabel(), hover_head_ == 2,
                   panel_ == Panel::Months, th);

    if (panel_ == Panel::Days) {
      PaintDayGrid(canvas, th);
    } else if (panel_ == Panel::Months || panel_ == Panel::Years) {
      PaintYmGrid(canvas, th);
    } else {
      PaintTimeGrid(canvas, th);
    }

    if (show_time_) {
      const RectF band = TimeBand();
      canvas.FillRect(RectF{bounds_.x + kPad, band.y, bounds_.w - kPad * 2.f, 1.f},
                      th.divider);
      DrawTimeBox(canvas, TimeBoxAt(0), hour_, hover_spin_ == 1, hover_spin_ == 2,
                  panel_ == Panel::Hours, hover_face_ == 1, th);
      canvas.DrawText(L":", ColonAt(0), th.text_muted, 14.f, th.font_ui.c_str(),
                      mx::TextHAlign::Center);
      DrawTimeBox(canvas, TimeBoxAt(1), minute_, hover_spin_ == 3, hover_spin_ == 4,
                  panel_ == Panel::Minutes, hover_face_ == 2, th);
      if (show_seconds_) {
        canvas.DrawText(L":", ColonAt(1), th.text_muted, 14.f, th.font_ui.c_str(),
                        mx::TextHAlign::Center);
        DrawTimeBox(canvas, TimeBoxAt(2), second_, hover_spin_ == 5,
                    hover_spin_ == 6, panel_ == Panel::Seconds, hover_face_ == 3,
                    th);
      }
    }
  }

  void OnMouseMove(const MouseEvent& e) override {
    const int day = panel_ == Panel::Days ? DayAt(e.x, e.y) : 0;
    const int spin = TimeHit(e.x, e.y);
    int head = 0;
    if (ContainsPoint(YearChip(), e.x, e.y)) {
      head = 1;
    } else if (ContainsPoint(MonthChip(), e.x, e.y)) {
      head = 2;
    }
    int ym = 0;
    if (panel_ == Panel::Months || panel_ == Panel::Years) {
      ym = YmIndexAt(e.x, e.y) + 1;
    } else if (IsTimePanel()) {
      const int i = TimeGridAt(e.x, e.y);
      ym = i >= 0 ? i + 1 : 0;
    }
    const int face = TimeFaceHit(e.x, e.y);
    if (day != hover_day_ || spin != hover_spin_ || head != hover_head_ ||
        ym != hover_ym_ || face != hover_face_) {
      hover_day_ = day;
      hover_spin_ = spin;
      hover_head_ = head;
      hover_ym_ = ym;
      hover_face_ = face;
      Invalidate();
    }
  }

  void OnMouseLeave(const MouseEvent&) override {
    if (hover_day_ != 0 || hover_spin_ != 0 || hover_head_ != 0 ||
        hover_ym_ != 0 || hover_face_ != 0) {
      hover_day_ = 0;
      hover_spin_ = 0;
      hover_head_ = 0;
      hover_ym_ = 0;
      hover_face_ = 0;
      Invalidate();
    }
  }

  void OnMouseDown(const MouseEvent& e) override {
    if (e.button != MouseButton::Left || !owner_) {
      return;
    }
    if (ContainsPoint(PrevRect(), e.x, e.y)) {
      StepHeader(-1);
      Invalidate();
      return;
    }
    if (ContainsPoint(NextRect(), e.x, e.y)) {
      StepHeader(1);
      Invalidate();
      return;
    }
    if (ContainsPoint(YearChip(), e.x, e.y)) {
      if (panel_ == Panel::Years) {
        panel_ = Panel::Days;
      } else {
        panel_ = Panel::Years;
        year_page_ = std::clamp(view_.year - 4, kYearMin, kYearMax - 11);
      }
      Invalidate();
      return;
    }
    if (ContainsPoint(MonthChip(), e.x, e.y)) {
      panel_ = (panel_ == Panel::Months) ? Panel::Days : Panel::Months;
      Invalidate();
      return;
    }
    if (panel_ == Panel::Months) {
      const int i = YmIndexAt(e.x, e.y);
      if (i >= 0) {
        ApplyView(view_.year, i + 1);
        panel_ = Panel::Days;
        Invalidate();
      }
      return;
    }
    if (panel_ == Panel::Years) {
      const int i = YmIndexAt(e.x, e.y);
      if (i >= 0) {
        ApplyView(year_page_ + i, view_.month);
        panel_ = Panel::Days;
        Invalidate();
      }
      return;
    }
    if (IsTimePanel()) {
      const int v = TimeGridAt(e.x, e.y);
      if (v >= 0) {
        ApplyTimeValue(v);
        panel_ = Panel::Days;
        Invalidate();
        return;
      }
    }
    if (show_time_) {
      const int hit = TimeHit(e.x, e.y);
      if (hit != 0) {
        NudgeTimeHit(hit);
        owner_->Commit(selected_, hour_, minute_, second_, false);
        Invalidate();
        return;
      }
      const int face = TimeFaceHit(e.x, e.y);
      if (face != 0) {
        const Panel next = FacePanel(face);
        panel_ = (panel_ == next) ? Panel::Days : next;
        Invalidate();
        return;
      }
    }
    if (panel_ != Panel::Days) {
      return;
    }
    const int day = DayAt(e.x, e.y);
    if (day > 0) {
      selected_ = NormalizeDate(CivilDate{view_.year, view_.month, day});
      owner_->Commit(selected_, hour_, minute_, second_, true);
    }
  }

  bool WantsMouseWheel() const override { return show_time_; }

  void OnMouseWheel(const MouseEvent& e) override {
    if (!show_time_ || e.wheel_delta == 0) {
      return;
    }
    int field = TimeFaceHit(e.x, e.y);
    if (field == 0) {
      const int hit = TimeHit(e.x, e.y);
      if (hit != 0) {
        field = (hit + 1) / 2;
      }
    }
    if (field == 0 && IsTimePanel()) {
      if (panel_ == Panel::Hours) {
        field = 1;
      } else if (panel_ == Panel::Minutes) {
        field = 2;
      } else {
        field = 3;
      }
    }
    if (field == 0) {
      return;
    }
    NudgeTimeHit(field * 2 - (e.wheel_delta > 0 ? 1 : 0));
    if (owner_) {
      owner_->Commit(selected_, hour_, minute_, second_, false);
    }
    Invalidate();
  }

 private:
  static RectF Inset(const RectF& r, float p) {
    return RectF{r.x + p, r.y + p, std::max(0.f, r.w - p * 2.f),
                 std::max(0.f, r.h - p * 2.f)};
  }

  std::wstring YearLabel() const {
    return Locale::Format(L"%1 Year", L"DatePicker",
                          std::to_wstring(view_.year));
  }
  std::wstring MonthLabel() const {
    return Locale::Format(L"%1 Month", L"DatePicker",
                          std::to_wstring(view_.month));
  }

  RectF PrevRect() const {
    return RectF{bounds_.x + kPad, bounds_.y + kPad, 28.f, kHeaderH};
  }
  RectF NextRect() const {
    return RectF{bounds_.x + bounds_.w - kPad - 28.f, bounds_.y + kPad, 28.f,
                 kHeaderH};
  }
  RectF HeaderInner() const {
    return RectF{bounds_.x + kPad + 30.f, bounds_.y + kPad,
                 std::max(0.f, bounds_.w - kPad * 2.f - 60.f), kHeaderH};
  }
  RectF YearChip() const {
    const RectF h = HeaderInner();
    return RectF{h.x, h.y + 4.f, h.w * 0.55f - 3.f, h.h - 8.f};
  }
  RectF MonthChip() const {
    const RectF h = HeaderInner();
    const float x = h.x + h.w * 0.55f + 3.f;
    return RectF{x, h.y + 4.f, std::max(0.f, h.x + h.w - x), h.h - 8.f};
  }
  RectF YmBody() const {
    return RectF{bounds_.x + kPad, bounds_.y + kPad + kHeaderH,
                 bounds_.w - kPad * 2.f, kDowH + kCellH * 6.f};
  }
  RectF YmCell(int i) const {
    const RectF b = YmBody();
    const float cw = b.w / 3.f;
    const float ch = b.h / 4.f;
    return RectF{b.x + cw * static_cast<float>(i % 3),
                 b.y + ch * static_cast<float>(i / 3), cw, ch};
  }
  int YmIndexAt(float x, float y) const {
    const RectF b = YmBody();
    if (!ContainsPoint(b, x, y) || b.w <= 0.f || b.h <= 0.f) {
      return -1;
    }
    const int col =
        std::clamp(static_cast<int>((x - b.x) / (b.w / 3.f)), 0, 2);
    const int row =
        std::clamp(static_cast<int>((y - b.y) / (b.h / 4.f)), 0, 3);
    return row * 3 + col;
  }

  void StepHeader(int dir) {
    if (panel_ == Panel::Years) {
      year_page_ = std::clamp(year_page_ + dir * 12, kYearMin, kYearMax - 11);
      return;
    }
    view_ = AddMonths(view_, dir * (panel_ == Panel::Months ? 12 : 1));
    view_.day = 1;
  }

  void ApplyView(int year, int month) {
    year = std::clamp(year, kYearMin, kYearMax);
    month = std::clamp(month, 1, 12);
    view_ = NormalizeDate(CivilDate{year, month, 1});
    selected_ = NormalizeDate(CivilDate{year, month, selected_.day});
    if (owner_) {
      owner_->Commit(selected_, hour_, minute_, second_, false);
    }
  }

  bool IsTimePanel() const {
    return panel_ == Panel::Hours || panel_ == Panel::Minutes ||
           panel_ == Panel::Seconds;
  }
  static Panel FacePanel(int face) {
    if (face == 1) {
      return Panel::Hours;
    }
    if (face == 2) {
      return Panel::Minutes;
    }
    return Panel::Seconds;
  }
  void ApplyTimeValue(int v) {
    if (panel_ == Panel::Hours) {
      hour_ = std::clamp(v, 0, 23);
    } else if (panel_ == Panel::Minutes) {
      minute_ = std::clamp(v, 0, 59);
    } else {
      second_ = std::clamp(v, 0, 59);
    }
    if (owner_) {
      owner_->Commit(selected_, hour_, minute_, second_, false);
    }
  }
  void NudgeTimeHit(int hit) {
    switch (hit) {
      case 1:
        hour_ = (hour_ + 1) % 24;
        break;
      case 2:
        hour_ = (hour_ + 23) % 24;
        break;
      case 3:
        minute_ = (minute_ + 1) % 60;
        break;
      case 4:
        minute_ = (minute_ + 59) % 60;
        break;
      case 5:
        second_ = (second_ + 1) % 60;
        break;
      case 6:
        second_ = (second_ + 59) % 60;
        break;
      default:
        break;
    }
  }
  int TimeGridCount() const { return panel_ == Panel::Hours ? 24 : 60; }
  int TimeGridRows() const { return panel_ == Panel::Hours ? 4 : 10; }
  RectF TimeGridCell(int i) const {
    const RectF b = YmBody();
    const int rows = TimeGridRows();
    const float cw = b.w / 6.f;
    const float ch = b.h / static_cast<float>(rows);
    return RectF{b.x + cw * static_cast<float>(i % 6),
                 b.y + ch * static_cast<float>(i / 6), cw, ch};
  }
  int TimeGridAt(float x, float y) const {
    const RectF b = YmBody();
    const int rows = TimeGridRows();
    const int count = TimeGridCount();
    if (!ContainsPoint(b, x, y) || b.w <= 0.f || b.h <= 0.f || rows <= 0) {
      return -1;
    }
    const int col =
        std::clamp(static_cast<int>((x - b.x) / (b.w / 6.f)), 0, 5);
    const int row = std::clamp(
        static_cast<int>((y - b.y) / (b.h / static_cast<float>(rows))), 0,
        rows - 1);
    const int i = row * 6 + col;
    return i < count ? i : -1;
  }
  RectF TimeFaceRect(int i) const {
    const RectF box = TimeBoxAt(i);
    return RectF{box.x, box.y, std::max(0.f, box.w - kTimeChevW), box.h};
  }
  int TimeFaceHit(float x, float y) const {
    if (!show_time_) {
      return 0;
    }
    const int n = TimeFieldCount();
    for (int i = 0; i < n; ++i) {
      if (ContainsPoint(TimeFaceRect(i), x, y)) {
        return i + 1;
      }
    }
    return 0;
  }

  void PaintTimeGrid(mx::Canvas& canvas, const ThemeTokens& th) {
    const int count = TimeGridCount();
    const int sel = (panel_ == Panel::Hours)
                        ? hour_
                        : (panel_ == Panel::Minutes ? minute_ : second_);
    const float fs = panel_ == Panel::Hours ? 13.f : 11.f;
    for (int i = 0; i < count; ++i) {
      const RectF cell = TimeGridCell(i);
      if (i == sel) {
        canvas.FillRoundedRect(Inset(cell, 2.f), 4.f, 4.f, th.accent);
      } else if (hover_ym_ == i + 1) {
        canvas.FillRoundedRect(Inset(cell, 2.f), 4.f, 4.f, th.surface_alt);
      }
      wchar_t buf[8] = {};
      swprintf_s(buf, L"%02d", i);
      const ColorF color = i == sel ? th.text_on_accent : th.text;
      canvas.DrawText(buf, cell, color, fs, th.font_ui.c_str(),
                      mx::TextHAlign::Center);
    }
  }

  void PaintDayGrid(mx::Canvas& canvas, const ThemeTokens& th) {
    const float grid_x = bounds_.x + kPad;
    const float grid_y = bounds_.y + kPad + kHeaderH;
    const float cell_w = (bounds_.w - kPad * 2.f) / 7.f;
    for (int i = 0; i < 7; ++i) {
      const RectF cell{grid_x + cell_w * static_cast<float>(i), grid_y, cell_w,
                       kDowH};
      canvas.DrawText(Locale::Tr(kDowEn[i], L"DatePicker"), cell, th.text_muted,
                      12.f, th.font_ui.c_str(),
                      mx::TextHAlign::Center);
    }
    const CivilDate today = TodayLocal();
    const int first_wd = MondayIndex(Weekday(view_));
    const int dim = DaysInMonth(view_.year, view_.month);
    for (int i = 0; i < 42; ++i) {
      const int day = i - first_wd + 1;
      const int row = i / 7;
      const int col = i % 7;
      const RectF cell{grid_x + cell_w * static_cast<float>(col),
                       grid_y + kDowH + kCellH * static_cast<float>(row),
                       cell_w, kCellH};
      if (day < 1 || day > dim) {
        continue;
      }
      const CivilDate cur{view_.year, view_.month, day};
      if (cur == selected_) {
        canvas.FillRoundedRect(Inset(cell, 3.f), 4.f, 4.f, th.accent);
      } else if (cur == today) {
        canvas.FillRoundedRect(Inset(cell, 3.f), 4.f, 4.f, th.accent_soft);
      } else if (hover_day_ == day) {
        canvas.FillRoundedRect(Inset(cell, 3.f), 4.f, 4.f, th.surface_alt);
      }
      const ColorF color = cur == selected_ ? th.text_on_accent : th.text;
      canvas.DrawText(std::to_wstring(day), cell, color, 13.f, th.font_ui.c_str(),
                      mx::TextHAlign::Center);
    }
  }

  void PaintYmGrid(mx::Canvas& canvas, const ThemeTokens& th) {
    const CivilDate today = TodayLocal();
    for (int i = 0; i < 12; ++i) {
      const RectF cell = YmCell(i);
      const bool sel = (panel_ == Panel::Months)
                           ? (i + 1 == view_.month)
                           : (year_page_ + i == view_.year);
      const bool now = (panel_ == Panel::Months)
                           ? (view_.year == today.year && i + 1 == today.month)
                           : (year_page_ + i == today.year);
      if (sel) {
        canvas.FillRoundedRect(Inset(cell, 4.f), 6.f, 6.f, th.accent);
      } else if (now) {
        canvas.FillRoundedRect(Inset(cell, 4.f), 6.f, 6.f, th.accent_soft);
      } else if (hover_ym_ == i + 1) {
        canvas.FillRoundedRect(Inset(cell, 4.f), 6.f, 6.f, th.surface_alt);
      }
      std::wstring text;
      if (panel_ == Panel::Months) {
        text = Locale::Format(L"%1 Month", L"DatePicker",
                              std::to_wstring(i + 1));
      } else {
        text = std::to_wstring(year_page_ + i);
      }
      const ColorF color = sel ? th.text_on_accent : th.text;
      canvas.DrawText(text, cell, color, 13.f, th.font_ui.c_str(),
                      mx::TextHAlign::Center);
    }
  }

  static void DrawHeaderChip(mx::Canvas& canvas, const RectF& r,
                             const std::wstring& text, bool hover, bool active,
                             const ThemeTokens& th) {
    if (active) {
      canvas.FillRoundedRect(r, 6.f, 6.f, th.accent_soft);
    } else if (hover) {
      canvas.FillRoundedRect(r, 6.f, 6.f, th.surface_alt);
    }
    const RectF label{r.x + 4.f, r.y, std::max(0.f, r.w - 16.f), r.h};
    canvas.DrawText(text, label, th.text, 13.f, th.font_ui.c_str(),
                    mx::TextHAlign::Center);
    const float cx = r.x + r.w - 8.f;
    const float cy = r.y + r.h * 0.5f;
    canvas.DrawLine(cx - 3.5f, cy - 1.5f, cx, cy + 2.f, th.glyph, 1.2f);
    canvas.DrawLine(cx, cy + 2.f, cx + 3.5f, cy - 1.5f, th.glyph, 1.2f);
  }
  RectF TimeBand() const {
    return RectF{bounds_.x, bounds_.y + bounds_.h - kTimeH, bounds_.w, kTimeH};
  }
  int TimeFieldCount() const { return show_seconds_ ? 3 : 2; }
  RectF TimeCluster() const {
    const RectF band = TimeBand();
    const float n = static_cast<float>(TimeFieldCount());
    const float w = kTimeBoxW * n + kTimeColonW * (n - 1.f);
    return RectF{band.x + (band.w - w) * 0.5f,
                 band.y + (band.h - kTimeBoxH) * 0.5f, w, kTimeBoxH};
  }
  RectF TimeBoxAt(int i) const {
    const RectF c = TimeCluster();
    return RectF{c.x + static_cast<float>(i) * (kTimeBoxW + kTimeColonW), c.y,
                 kTimeBoxW, c.h};
  }
  RectF ColonAt(int i) const {
    const RectF c = TimeCluster();
    return RectF{c.x + kTimeBoxW + static_cast<float>(i) * (kTimeBoxW + kTimeColonW),
                 c.y, kTimeColonW, c.h};
  }
  static RectF ChevRect(const RectF& box) {
    return RectF{box.x + box.w - kTimeChevW, box.y, kTimeChevW, box.h};
  }
  RectF SpinHalf(int field, bool up) const {
    const RectF c = ChevRect(TimeBoxAt(field));
    return RectF{c.x, up ? c.y : c.y + c.h * 0.5f, c.w, c.h * 0.5f};
  }

  int TimeHit(float x, float y) const {
    if (!show_time_) {
      return 0;
    }
    const int n = TimeFieldCount();
    for (int i = 0; i < n; ++i) {
      if (ContainsPoint(SpinHalf(i, true), x, y)) {
        return i * 2 + 1;
      }
      if (ContainsPoint(SpinHalf(i, false), x, y)) {
        return i * 2 + 2;
      }
    }
    return 0;
  }

  static void DrawChevron(mx::Canvas& canvas, const RectF& r, bool left,
                          const ColorF& color) {
    const float cx = r.x + r.w * 0.5f;
    const float cy = r.y + r.h * 0.5f;
    const float dx = left ? -4.f : 4.f;
    canvas.DrawLine(cx + dx, cy, cx - dx, cy - 5.f, color, 1.5f);
    canvas.DrawLine(cx + dx, cy, cx - dx, cy + 5.f, color, 1.5f);
  }

  static void DrawTimeBox(mx::Canvas& canvas, const RectF& box, int value,
                          bool hover_up, bool hover_down, bool active,
                          bool hover_face, const ThemeTokens& th) {
    if (active) {
      canvas.FillRoundedRect(box, 6.f, 6.f, th.accent_soft);
      canvas.DrawRect(box, th.border_focus, 1.5f);
    } else {
      canvas.FillRoundedRect(box, 6.f, 6.f,
                             hover_face ? th.surface_alt : th.surface);
      canvas.DrawRect(box, th.border, 1.f);
    }
    const RectF chev = ChevRect(box);
    canvas.FillRect(RectF{chev.x, box.y + 1.f, 1.f, box.h - 2.f}, th.divider);
    if (hover_up) {
      canvas.FillRect(RectF{chev.x + 1.f, chev.y, chev.w - 1.f, chev.h * 0.5f},
                      th.accent_soft);
    } else if (hover_down) {
      canvas.FillRect(RectF{chev.x + 1.f, chev.y + chev.h * 0.5f, chev.w - 1.f,
                            chev.h * 0.5f},
                      th.accent_soft);
    }
    const float cx = chev.x + chev.w * 0.5f;
    const float up_y = chev.y + chev.h * 0.25f;
    const float dn_y = chev.y + chev.h * 0.75f;
    canvas.DrawLine(cx - 4.f, up_y + 2.f, cx, up_y - 2.f, th.glyph, 1.4f);
    canvas.DrawLine(cx, up_y - 2.f, cx + 4.f, up_y + 2.f, th.glyph, 1.4f);
    canvas.DrawLine(cx - 4.f, dn_y - 2.f, cx, dn_y + 2.f, th.glyph, 1.4f);
    canvas.DrawLine(cx, dn_y + 2.f, cx + 4.f, dn_y - 2.f, th.glyph, 1.4f);
    wchar_t buf[8] = {};
    swprintf_s(buf, L"%02d", value);
    const RectF text{box.x + 8.f, box.y,
                     std::max(0.f, box.w - kTimeChevW - 10.f), box.h};
    canvas.DrawText(buf, text, th.text, 14.f, th.font_ui.c_str(),
                    mx::TextHAlign::Center);
  }

  int DayAt(float x, float y) const {
    const float grid_x = bounds_.x + kPad;
    const float grid_y = bounds_.y + kPad + kHeaderH + kDowH;
    const float cell_w = (bounds_.w - kPad * 2.f) / 7.f;
    const RectF grid{grid_x, grid_y, bounds_.w - kPad * 2.f, kCellH * 6.f};
    if (!ContainsPoint(grid, x, y) || cell_w <= 0.f) {
      return 0;
    }
    const int col =
        std::clamp(static_cast<int>((x - grid_x) / cell_w), 0, 6);
    const int row =
        std::clamp(static_cast<int>((y - grid_y) / kCellH), 0, 5);
    const int i = row * 7 + col;
    const int first_wd = MondayIndex(Weekday(view_));
    const int day = i - first_wd + 1;
    const int dim = DaysInMonth(view_.year, view_.month);
    if (day < 1 || day > dim) {
      return 0;
    }
    return day;
  }

  DatePicker* owner_ = nullptr;
  CivilDate selected_{};
  CivilDate view_{};
  Panel panel_ = Panel::Days;
  int year_page_ = 2020;
  bool show_time_ = false;
  bool show_seconds_ = false;
  int hour_ = 0;
  int minute_ = 0;
  int second_ = 0;
  int hover_day_ = 0;
  int hover_spin_ = 0;
  int hover_head_ = 0;
  int hover_ym_ = 0;
  int hover_face_ = 0;
};

DatePicker::DatePicker() {
  set_focusable(true);
  fill_width();
  fixed_height(36.f);
  date_ = TodayLocal();
}

void DatePicker::BindWindow(Window* window) { window_ = window; }

DatePicker& DatePicker::date(CivilDate d) {
  d = NormalizeDate(d);
  if (d == date_) {
    return *this;
  }
  date_ = d;
  NotifyAccValueChanged();
  Invalidate();
  return *this;
}

DatePicker& DatePicker::year(int y) {
  return date(CivilDate{y, date_.month, date_.day});
}

DatePicker& DatePicker::month(int m) {
  return date(CivilDate{date_.year, m, date_.day});
}

DatePicker& DatePicker::day(int d) {
  return date(CivilDate{date_.year, date_.month, d});
}

DatePicker& DatePicker::time(bool enable) {
  time_ = enable;
  Invalidate();
  return *this;
}

DatePicker& DatePicker::seconds(bool enable) {
  seconds_ = enable;
  Invalidate();
  return *this;
}

DatePicker& DatePicker::hour(int h) {
  h = std::clamp(h, 0, 23);
  if (h == hour_) {
    return *this;
  }
  hour_ = h;
  NotifyAccValueChanged();
  Invalidate();
  return *this;
}

DatePicker& DatePicker::minute(int m) {
  m = std::clamp(m, 0, 59);
  if (m == minute_) {
    return *this;
  }
  minute_ = m;
  NotifyAccValueChanged();
  Invalidate();
  return *this;
}

DatePicker& DatePicker::second(int s) {
  s = std::clamp(s, 0, 59);
  if (s == second_) {
    return *this;
  }
  second_ = s;
  NotifyAccValueChanged();
  Invalidate();
  return *this;
}

DatePicker& DatePicker::font_size(float size) {
  font_size_ = size;
  return *this;
}

DatePicker& DatePicker::on_changed(ChangeHandler handler) {
  on_changed_ = std::move(handler);
  return *this;
}

void DatePicker::Notify() {
  if (on_changed_) {
    on_changed_(date_, hour_, minute_, second_);
  }
}

void DatePicker::Commit(CivilDate d, int hour, int minute, int second, bool close) {
  date_ = NormalizeDate(d);
  hour_ = std::clamp(hour, 0, 23);
  minute_ = std::clamp(minute, 0, 59);
  second_ = std::clamp(second, 0, 59);
  NotifyAccValueChanged();
  Notify();
  Invalidate();
  if (close) {
    // CalendarPopup lives in Window::popup_; destroying it from OnMouseDown
    // UAFs the dispatch `target`. Combo uses the same deferral.
    const bool was = open_;
    open_ = false;
    if (window_) {
      window_->RequestClearPopup();
    }
    if (was) {
      NotifyAccExpandCollapseChanged();
    }
  }
}

std::wstring DatePicker::DisplayText() const {
  if (time_) {
    if (seconds_) {
      return FormatYmdHms(date_, hour_, minute_, second_);
    }
    return FormatYmdHm(date_, hour_, minute_);
  }
  return FormatYmd(date_);
}

void DatePicker::ClosePopup() {
  const bool was = open_;
  open_ = false;
  if (window_ && window_->popup()) {
    window_->ClearPopup();
  }
  if (was) {
    NotifyAccExpandCollapseChanged();
  }
}

void DatePicker::OpenPopup() {
  if (!window_) {
    return;
  }
  if (open_) {
    ClosePopup();
    return;
  }
  auto cal = std::make_unique<CalendarPopup>(this, date_, time_, seconds_, hour_,
                                             minute_, second_);
  const SizeF want = cal->Measure(kCalW, 400.f);
  const float w = std::max(bounds_.w, want.w);
  cal->Layout(RectF{bounds_.x, bounds_.y + bounds_.h + 2.f, w, want.h});
  open_ = true;
  window_->SetPopup(
      std::move(cal),
      [this]() {
        const bool was = open_;
        open_ = false;
        if (was) {
          NotifyAccExpandCollapseChanged();
        }
      },
      this);
  window_->SetFocusNode(this);
  NotifyAccExpandCollapseChanged();
}

AccRole DatePicker::acc_role() const {
  if (acc_role_override_) {
    return *acc_role_override_;
  }
  return AccRole::ComboBox;
}

std::wstring DatePicker::AccDefaultName() const {
  return {};
}

std::wstring DatePicker::AccValue() const {
  return DisplayText();
}

bool DatePicker::AccSetValue(const std::wstring& value) {
  CivilDate d;
  if (!ParseYmd(value, &d)) {
    return false;
  }
  int h = hour_;
  int m = minute_;
  int s = second_;
  if (value.size() >= 16) {
    swscanf_s(value.c_str() + 11, L"%d:%d:%d", &h, &m, &s);
  }
  Commit(d, h, m, s, false);
  return true;
}

bool DatePicker::AccIsExpanded() const {
  return open_;
}

bool DatePicker::AccExpand() {
  if (open_) {
    return true;
  }
  OpenPopup();
  return open_;
}

bool DatePicker::AccCollapse() {
  if (!open_) {
    return true;
  }
  ClosePopup();
  return true;
}

SizeF DatePicker::Measure(float max_w, float max_h) {
  return ResolveSize(max_w, max_h,
                     preferred_width() > 0.f ? preferred_width() : max_w, 36.f);
}

void DatePicker::Paint(mx::Canvas& canvas) {
  if (!visible()) {
    return;
  }
  const ThemeTokens& th = Theme::Active();
  canvas.FillRoundedRect(bounds_, 6.f, 6.f, th.surface);
  canvas.DrawRect(bounds_, focused() || open_ ? th.border_focus : th.border,
                  focused() || open_ ? 1.5f : 1.f);
  const RectF text{bounds_.x + 10.f, bounds_.y,
                   std::max(0.f, bounds_.w - 36.f), bounds_.h};
  canvas.DrawText(DisplayText(), text, th.text, ResolveFontSize(font_size_),
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

void DatePicker::OnMouseDown(const MouseEvent& e) {
  if (e.button != MouseButton::Left) {
    return;
  }
  OpenPopup();
}

void DatePicker::OnKey(const KeyEvent& e) {
  if (!e.down) {
    return;
  }
  if (e.vk == VK_ESCAPE && open_) {
    ClosePopup();
    return;
  }
  if (e.vk == VK_SPACE || e.vk == VK_DOWN || e.vk == VK_RETURN) {
    OpenPopup();
    return;
  }
  if (e.vk == VK_LEFT) {
    Commit(AddDays(date_, -1), hour_, minute_, second_, false);
  } else if (e.vk == VK_RIGHT) {
    Commit(AddDays(date_, 1), hour_, minute_, second_, false);
  } else if (e.vk == VK_UP) {
    Commit(AddDays(date_, -7), hour_, minute_, second_, false);
  } else if (e.vk == VK_DOWN && !open_) {
    Commit(AddDays(date_, 7), hour_, minute_, second_, false);
  }
}

}  // namespace mx::ui
