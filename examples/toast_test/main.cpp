#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "mx/ui/factory.h"
#include "mx/ui/toast.h"
#include "mx/ui/theme.h"
#include "mx/ui/window.h"
#include "mx/ui/yaml_loader.h"

namespace {

int g_failures = 0;

void Expect(const char* name, bool cond) {
  if (!cond) {
    std::printf("FAIL %s\n", name);
    ++g_failures;
  } else {
    std::printf("ok   %s\n", name);
  }
}

void TestToastNode() {
  using namespace mx::ui;
  Theme::RegisterBuiltInLight();
  Theme::SetActive("light");

  Toast toast;
  toast.text(L"已保存");
  Expect("role text", toast.acc_role() == AccRole::Text);
  Expect("name from text", toast.AccName() == L"已保存");
  Expect("default info", toast.variant() == ToastVariant::Info);
  Expect("default duration", toast.duration_sec() > 0.f);
  Expect("default no click dismiss", !toast.dismiss_on_click());
  Expect("default anchor", toast.anchor() == ToastAnchor::BottomCenter);
  Expect("default margin", toast.margin() == 16.f);
  Expect("default animate on", toast.animate());
  Expect("default fade", toast.fade_sec() > 0.f);
  Expect("effective fade on", toast.effective_fade_sec() > 0.f);

  toast.animate(false);
  Expect("animate off keeps fade_sec", toast.fade_sec() > 0.f);
  Expect("effective fade off", toast.effective_fade_sec() == 0.f);
  toast.animate(true);
  Expect("animate back on", toast.animate() && toast.effective_fade_sec() > 0.f);
  toast.fade_sec(0.f);
  Expect("fade 0 effective 0", toast.effective_fade_sec() == 0.f);

  const SizeF sz = toast.Measure(400.f, 200.f);
  Expect("hug wider than padding", sz.w > 32.f);
  Expect("hug taller than font", sz.h > 16.f);

  int dismiss = 0;
  toast.dismiss_on_click(true);
  toast.on_dismiss([&] { ++dismiss; });
  toast.OnMouseDown(MouseEvent{});
  Expect("click dismiss", dismiss == 1);
  Expect("invoke dismiss", toast.AccInvoke() && dismiss == 2);

  toast.variant(ToastVariant::Danger).duration_sec(0.f);
  Expect("danger variant", toast.variant() == ToastVariant::Danger);
  Expect("zero duration", toast.duration_sec() == 0.f);
}

void TestYamlToast() {
  using namespace mx::ui;
  ViewFactory factory;
  auto n = LoadYamlString(
      "Toast:\n  text: 已保存\n  variant: success\n  duration: 1.5\n"
      "  animate: false\n  fade: 0.15\n",
      factory, {});
  auto* t = dynamic_cast<Toast*>(n.get());
  Expect("yaml built", t != nullptr);
  Expect("yaml text", t && t->text() == L"已保存");
  Expect("yaml success", t && t->variant() == ToastVariant::Success);
  Expect("yaml duration", t && t->duration_sec() == 1.5f);
  Expect("yaml animate", t && !t->animate());
  Expect("yaml fade", t && t->fade_sec() == 0.15f);
  Expect("dump type", factory.DumpTree(n.get()).find("Toast") == 0);
}

void TestShowToastQueue() {
  using namespace mx::ui;
  Window w;
  Window::WindowOptions opt;
  opt.quit_on_close = false;
  if (!w.Create(L"toast_test", 320, 240, opt)) {
    Expect("Create", false);
    return;
  }
  Expect("Create", w.hwnd() != nullptr);
  Expect("no toast yet", !w.has_toast());

  auto a = std::make_unique<Toast>();
  a->text(L"A").duration_sec(0.f).animate(false);
  w.ShowToast(std::move(a));
  Expect("showing A", w.has_toast());

  auto b = std::make_unique<Toast>();
  b->text(L"B").duration_sec(0.f).animate(false);
  w.ShowToast(std::move(b));
  Expect("still showing after queue", w.has_toast());

  w.DismissToast();
  Expect("showing queued B", w.has_toast());
  w.DismissToast();
  Expect("queue empty", !w.has_toast());
  w.DismissToast();
  Expect("dismiss empty ok", !w.has_toast());

  auto d = std::make_unique<Toast>();
  d->text(L"D").duration_sec(0.f);
  w.ShowToast(std::move(d));
  Expect("toast()", w.toast() != nullptr);
  if (Toast* cur = w.toast()) {
    cur->animate(false);
    Expect("dynamic off", !cur->animate());
  }
  w.DismissToast();
  Expect("dismiss after dynamic off", !w.has_toast());
}

}  // namespace

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  TestToastNode();
  TestYamlToast();
  TestShowToastQueue();
  if (g_failures) {
    std::printf("%d failed\n", g_failures);
    return EXIT_FAILURE;
  }
  std::printf("all ok\n");
  return EXIT_SUCCESS;
}
