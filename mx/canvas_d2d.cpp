#include "mx/canvas.h"

#include <algorithm>
#include <cmath>
#include <wincodec.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "msimg32.lib")

namespace mx {
namespace {

template <typename T>
void SafeRelease(T*& ptr) {
  if (ptr) {
    ptr->Release();
    ptr = nullptr;
  }
}

D2D1_COLOR_F ToD2D(const ColorF& c) {
  return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

D2D1_RECT_F ToD2D(const RectF& r) {
  return D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h);
}

HRESULT CreateDCRenderTargetSeh(ID2D1Factory* factory,
                                const D2D1_RENDER_TARGET_PROPERTIES& props,
                                ID2D1DCRenderTarget** target) {
  if (!factory || !target) {
    return E_INVALIDARG;
  }
  *target = nullptr;
  __try {
    return factory->CreateDCRenderTarget(&props, target);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return E_FAIL;
  }
}

}  // namespace

Image::Image() = default;

Image::~Image() {
  Reset();
}

void Image::Reset() {
  SafeRelease(bitmap_);
  width_ = 0;
  height_ = 0;
}

bool Image::CreateFromBgra(Canvas& canvas, UINT width, UINT height,
                           const uint8_t* bgra, UINT stride) {
  Reset();
  if (!canvas.EnsureRenderTarget() || !bgra || width == 0 || height == 0) {
    return false;
  }
  const D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED));
  HRESULT hr = canvas.render_target()->CreateBitmap(
      D2D1::SizeU(width, height), bgra, stride, props, &bitmap_);
  if (FAILED(hr)) {
    return false;
  }
  width_ = width;
  height_ = height;
  return true;
}

bool Image::LoadFromFile(Canvas& canvas, const std::wstring& path) {
  Reset();
  if (!canvas.EnsureRenderTarget() || path.empty()) {
    return false;
  }

  IWICImagingFactory* wic = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
  if (FAILED(hr)) {
    return false;
  }

  IWICBitmapDecoder* decoder = nullptr;
  hr = wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                      WICDecodeMetadataCacheOnLoad, &decoder);
  if (FAILED(hr)) {
    SafeRelease(wic);
    return false;
  }

  IWICBitmapFrameDecode* frame = nullptr;
  hr = decoder->GetFrame(0, &frame);
  if (FAILED(hr)) {
    SafeRelease(decoder);
    SafeRelease(wic);
    return false;
  }

  IWICFormatConverter* converter = nullptr;
  hr = wic->CreateFormatConverter(&converter);
  if (FAILED(hr)) {
    SafeRelease(frame);
    SafeRelease(decoder);
    SafeRelease(wic);
    return false;
  }

  hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                             WICBitmapDitherTypeNone, nullptr, 0.0,
                             WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) {
    SafeRelease(converter);
    SafeRelease(frame);
    SafeRelease(decoder);
    SafeRelease(wic);
    return false;
  }

  hr = canvas.render_target()->CreateBitmapFromWicBitmap(converter, nullptr,
                                                         &bitmap_);
  if (SUCCEEDED(hr) && bitmap_) {
    const D2D1_SIZE_U size = bitmap_->GetPixelSize();
    width_ = size.width;
    height_ = size.height;
  }

  SafeRelease(converter);
  SafeRelease(frame);
  SafeRelease(decoder);
  SafeRelease(wic);
  return bitmap_ != nullptr;
}

Canvas::Canvas() = default;

void Canvas::SetDpi(float dpi) {
  dpi_ = EffectiveDpi(dpi);
  if (render_target_) {
    render_target_->SetDpi(dpi_, dpi_);
  }
}

bool Canvas::PeekDibBgra(int x, int y, uint8_t* b, uint8_t* g, uint8_t* r,
                         uint8_t* a) const {
  if (!dib_bits_ || x < 0 || y < 0 ||
      static_cast<UINT>(x) >= dib_w_ || static_cast<UINT>(y) >= dib_h_) {
    return false;
  }
  const uint8_t* p = static_cast<const uint8_t*>(dib_bits_) +
                     (static_cast<size_t>(y) * dib_w_ + static_cast<size_t>(x)) * 4u;
  if (b) {
    *b = p[0];
  }
  if (g) {
    *g = p[1];
  }
  if (r) {
    *r = p[2];
  }
  if (a) {
    *a = p[3];
  }
  return true;
}

Canvas::~Canvas() {
  Shutdown();
}

bool Canvas::Init(HWND hwnd) {
  if (!hwnd) {
    return false;
  }
  hwnd_ = hwnd;

  if (!d2d_factory_) {
    const HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                         &d2d_factory_);
    if (FAILED(hr)) {
      return false;
    }
  }

  if (!dwrite_factory_) {
    const HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&dwrite_factory_));
    if (FAILED(hr)) {
      return false;
    }
  }

  DiscardDeviceResources();
  layered_ = false;
  return CreateDeviceResources();
}

bool Canvas::InitLayered(HWND hwnd) {
  if (!hwnd) {
    return false;
  }
  Shutdown();
  hwnd_ = hwnd;
  layered_ = true;

  if (!d2d_factory_) {
    const HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                         &d2d_factory_);
    if (FAILED(hr)) {
      return false;
    }
  }

  if (!dwrite_factory_) {
    const HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&dwrite_factory_));
    if (FAILED(hr)) {
      return false;
    }
  }

  return CreateDeviceResources();
}

void Canvas::Shutdown() {
  DiscardDeviceResources();
  SafeRelease(dwrite_factory_);
  SafeRelease(d2d_factory_);
  hwnd_ = nullptr;
  layered_ = false;
  layered_opacity_ = 1.f;
}

void Canvas::set_layered_opacity(float a) {
  layered_opacity_ = std::clamp(a, 0.f, 1.f);
}

void Canvas::DestroyDibSurface() {
  if (dib_dc_ && dib_old_) {
    SelectObject(dib_dc_, dib_old_);
    dib_old_ = nullptr;
  }
  if (dib_bitmap_) {
    DeleteObject(dib_bitmap_);
    dib_bitmap_ = nullptr;
  }
  if (dib_dc_) {
    DeleteDC(dib_dc_);
    dib_dc_ = nullptr;
  }
  dib_bits_ = nullptr;
  dib_w_ = 0;
  dib_h_ = 0;
}

bool Canvas::CreateDibSurface(UINT w, UINT h) {
  w = w ? w : 1;
  h = h ? h : 1;

  HDC old_dc = dib_dc_;
  HBITMAP old_bitmap = dib_bitmap_;
  HGDIOBJ old_sel = dib_old_;
  const UINT old_w = dib_w_;
  const UINT old_h = dib_h_;
  dib_dc_ = nullptr;
  dib_bitmap_ = nullptr;
  dib_old_ = nullptr;
  dib_bits_ = nullptr;
  dib_w_ = 0;
  dib_h_ = 0;

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = static_cast<LONG>(w);
  bmi.bmiHeader.biHeight = -static_cast<LONG>(h);
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  HDC screen = GetDC(nullptr);
  dib_dc_ = CreateCompatibleDC(screen);
  ReleaseDC(nullptr, screen);
  dib_bitmap_ = CreateDIBSection(dib_dc_, &bmi, DIB_RGB_COLORS, &dib_bits_,
                                 nullptr, 0);
  if (!dib_dc_ || !dib_bitmap_ || !dib_bits_) {
    if (old_dc) {
      dib_dc_ = old_dc;
      dib_bitmap_ = old_bitmap;
      dib_old_ = static_cast<HBITMAP>(old_sel);
      dib_w_ = old_w;
      dib_h_ = old_h;
    } else {
      DestroyDibSurface();
    }
    return dib_dc_ != nullptr;
  }
  dib_old_ = static_cast<HBITMAP>(SelectObject(dib_dc_, dib_bitmap_));
  dib_w_ = w;
  dib_h_ = h;
  if (old_dc && old_w > 0 && old_h > 0) {
    SetStretchBltMode(dib_dc_, COLORONCOLOR);
    StretchBlt(dib_dc_, 0, 0, static_cast<int>(w), static_cast<int>(h), old_dc,
               0, 0, static_cast<int>(old_w), static_cast<int>(old_h),
               SRCCOPY);
  }
  // GDI stretch/create leaves A=0. DWM treats 0-alpha as glass/see-through
  // during live resize until the next D2D Clear.
  ForceDibOpaque();
  if (old_dc) {
    if (old_sel) {
      SelectObject(old_dc, old_sel);
    }
    if (old_bitmap) {
      DeleteObject(old_bitmap);
    }
    DeleteDC(old_dc);
  }
  return true;
}

void Canvas::ForceDibOpaque() {
  if (!dib_bits_ || dib_w_ == 0 || dib_h_ == 0) {
    return;
  }
  auto* p = static_cast<uint8_t*>(dib_bits_);
  const size_t n = static_cast<size_t>(dib_w_) * dib_h_;
  for (size_t i = 0; i < n; ++i) {
    p[i * 4 + 3] = 255;
  }
}

bool Canvas::EnsureRenderTarget() {
  if (render_target_) {
    return true;
  }
  return CreateDeviceResources();
}

bool Canvas::CreateDeviceResources() {
  if (!d2d_factory_ || !hwnd_) {
    return false;
  }
  if (render_target_) {
    return true;
  }

  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const UINT w = static_cast<UINT>(rc.right > 0 ? rc.right : 1);
  const UINT h = static_cast<UINT>(rc.bottom > 0 ? rc.bottom : 1);
  if (!CreateDibSurface(w, h)) {
    return false;
  }

  // Layout/hit-test/paint are DIP. Pixel buffer size is GetClientRect.
  // RT DPI is dpi_ (set by Window) so D2D maps DIP drawing to physical pixels.
  // DC+DIB (not HWND RT): EndDraw of an HWND RT copies the full client over
  // WS_CHILD guests and they appear to blink on every parent Invalidate.
  const D2D1_PIXEL_FORMAT pixel_format =
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED);
  const D2D1_RENDER_TARGET_PROPERTIES hw_props = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT, pixel_format, dpi_, dpi_);
  ID2D1DCRenderTarget* dc_render_target = nullptr;
  HRESULT hr = CreateDCRenderTargetSeh(d2d_factory_, hw_props, &dc_render_target);
  if (FAILED(hr)) {
    const D2D1_RENDER_TARGET_PROPERTIES sw_props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE, pixel_format, dpi_, dpi_);
    hr = CreateDCRenderTargetSeh(d2d_factory_, sw_props, &dc_render_target);
  }
  if (FAILED(hr)) {
    DestroyDibSurface();
    return false;
  }
  render_target_ = dc_render_target;
  if (!BindDib()) {
    DiscardDeviceResources();
    return false;
  }
  render_target_->SetTextAntialiasMode(
      layered_ ? D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE
               : D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

  hr = render_target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                             &brush_);
  if (FAILED(hr)) {
    DiscardDeviceResources();
    return false;
  }
  return true;
}

void Canvas::DiscardDeviceResources() {
  SafeRelease(brush_);
  SafeRelease(render_target_);
  DestroyDibSurface();
}

bool Canvas::BindDib() {
  auto* dc_render_target = static_cast<ID2D1DCRenderTarget*>(render_target_);
  if (!dc_render_target || !dib_dc_) {
    return false;
  }
  const RECT bind = {0, 0, static_cast<LONG>(dib_w_),
                     static_cast<LONG>(dib_h_)};
  const HRESULT hr = dc_render_target->BindDC(dib_dc_, &bind);
  if (FAILED(hr)) {
    return false;
  }
  // BindDC resets the DC target to 96 DPI. Layout/paint are DIP; restore
  // Window DPI so 96 DIP maps to physical pixels (otherwise the bitmap is
  // only filled in the top-left at 100% scale).
  render_target_->SetDpi(dpi_, dpi_);
  return true;
}

void Canvas::Resize(UINT width, UINT height) {
  width = width > 0 ? width : 1;
  height = height > 0 ? height : 1;

  if (!EnsureRenderTarget()) {
    return;
  }
  if (width == dib_w_ && height == dib_h_) {
    return;
  }
  if (!CreateDibSurface(width, height)) {
    return;
  }
  BindDib();
}

bool Canvas::BeginDraw() {
  if (!EnsureRenderTarget()) {
    return false;
  }
  if (!BindDib()) {
    return false;
  }
  render_target_->BeginDraw();
  return true;
}

void Canvas::PresentDib(HDC present_dc, const RECT* present_px) {
  if (!hwnd_ || !dib_dc_ || dib_w_ == 0 || dib_h_ == 0) {
    return;
  }
  RECT dest = {0, 0, static_cast<LONG>(dib_w_), static_cast<LONG>(dib_h_)};
  if (present_px) {
    if (!IntersectRect(&dest, &dest, present_px)) {
      return;
    }
  }
  const int w = dest.right - dest.left;
  const int h = dest.bottom - dest.top;
  if (w <= 0 || h <= 0) {
    return;
  }
  // BeginPaint's HDC is required (a second GetDCEx clip was wrong). BitBlt of
  // a 32-bit DIB onto a DWM surface leaves destination alpha at 0, so newly
  // exposed pixels during live resize are glass. AlphaBlend copies A=255.
  bool own_dc = false;
  HDC hdc = present_dc;
  if (!hdc) {
    hdc = GetDCEx(hwnd_, nullptr, DCX_CACHE);
    own_dc = true;
  }
  if (!hdc) {
    return;
  }
  SelectClipRgn(hdc, nullptr);
  BLENDFUNCTION blend = {};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;
  if (!AlphaBlend(hdc, dest.left, dest.top, w, h, dib_dc_, dest.left,
                  dest.top, w, h, blend)) {
    BitBlt(hdc, dest.left, dest.top, w, h, dib_dc_, dest.left, dest.top,
           SRCCOPY);
  }
  GdiFlush();
  if (own_dc) {
    ReleaseDC(hwnd_, hdc);
  }
}

bool Canvas::EndDraw(HDC present_dc, const RECT* present_px) {
  if (!render_target_) {
    return false;
  }
  const HRESULT hr = render_target_->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    DiscardDeviceResources();
    return false;
  }
  if (FAILED(hr) || !hwnd_) {
    return SUCCEEDED(hr);
  }
  if (layered_) {
    POINT pt_src = {0, 0};
    SIZE size = {static_cast<LONG>(dib_w_), static_cast<LONG>(dib_h_)};
    POINT pt_dst = {};
    RECT wr = {};
    GetWindowRect(hwnd_, &wr);
    pt_dst.x = wr.left;
    pt_dst.y = wr.top;
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha =
        static_cast<BYTE>(std::lround(layered_opacity_ * 255.f));
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd_, nullptr, &pt_dst, &size, dib_dc_, &pt_src, 0,
                        &blend, ULW_ALPHA);
  } else {
    ForceDibOpaque();
    PresentDib(present_dc, present_px);
  }
  return true;
}

void Canvas::Clear(const ColorF& color) {
  if (!render_target_) {
    return;
  }
  render_target_->Clear(ToD2D(color));
}

ID2D1SolidColorBrush* Canvas::BrushFor(const ColorF& color) {
  if (!brush_) {
    return nullptr;
  }
  brush_->SetColor(ToD2D(color));
  return brush_;
}

void Canvas::FillRect(const RectF& rect, const ColorF& color) {
  if (!render_target_) {
    return;
  }
  ID2D1SolidColorBrush* brush = BrushFor(color);
  if (!brush) {
    return;
  }
  render_target_->FillRectangle(ToD2D(rect), brush);
}

void Canvas::FillRoundedRect(const RectF& rect, float radius_x, float radius_y,
                             const ColorF& color) {
  if (!render_target_) {
    return;
  }
  ID2D1SolidColorBrush* brush = BrushFor(color);
  if (!brush) {
    return;
  }
  const D2D1_ROUNDED_RECT rr =
      D2D1::RoundedRect(ToD2D(rect), radius_x, radius_y);
  render_target_->FillRoundedRectangle(rr, brush);
}

void Canvas::DrawRect(const RectF& rect, const ColorF& color,
                      float stroke_width) {
  if (!render_target_) {
    return;
  }
  ID2D1SolidColorBrush* brush = BrushFor(color);
  if (!brush) {
    return;
  }
  render_target_->DrawRectangle(ToD2D(rect), brush, stroke_width);
}

void Canvas::DrawRoundedRect(const RectF& rect, float radius_x, float radius_y,
                             const ColorF& color, float stroke_width) {
  if (!render_target_ || stroke_width <= 0.f) {
    return;
  }
  ID2D1SolidColorBrush* brush = BrushFor(color);
  if (!brush) {
    return;
  }
  const D2D1_ROUNDED_RECT rr =
      D2D1::RoundedRect(ToD2D(rect), radius_x, radius_y);
  render_target_->DrawRoundedRectangle(rr, brush, stroke_width);
}

void Canvas::DrawDashedRect(const RectF& rect, const ColorF& color,
                            float stroke_width) {
  if (!render_target_ || !d2d_factory_) {
    return;
  }
  ID2D1SolidColorBrush* brush = BrushFor(color);
  if (!brush) {
    return;
  }
  ID2D1StrokeStyle* style = nullptr;
  const HRESULT hr = d2d_factory_->CreateStrokeStyle(
      D2D1::StrokeStyleProperties(
          D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT,
          D2D1_LINE_JOIN_MITER, 10.f, D2D1_DASH_STYLE_DASH, 0.f),
      nullptr, 0, &style);
  if (FAILED(hr) || !style) {
    render_target_->DrawRectangle(ToD2D(rect), brush, stroke_width);
    return;
  }
  render_target_->DrawRectangle(ToD2D(rect), brush, stroke_width, style);
  style->Release();
}

void Canvas::FillEllipse(const RectF& rect, const ColorF& color) {
  if (!render_target_ || rect.w <= 0.f || rect.h <= 0.f) {
    return;
  }
  ID2D1SolidColorBrush* brush = BrushFor(color);
  if (!brush) {
    return;
  }
  const D2D1_ELLIPSE ellipse = D2D1::Ellipse(
      D2D1::Point2F(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f),
      rect.w * 0.5f, rect.h * 0.5f);
  render_target_->FillEllipse(ellipse, brush);
}

void Canvas::DrawEllipse(const RectF& rect, const ColorF& color,
                         float stroke_width) {
  if (!render_target_ || rect.w <= 0.f || rect.h <= 0.f) {
    return;
  }
  ID2D1SolidColorBrush* brush = BrushFor(color);
  if (!brush) {
    return;
  }
  const D2D1_ELLIPSE ellipse = D2D1::Ellipse(
      D2D1::Point2F(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f),
      rect.w * 0.5f, rect.h * 0.5f);
  render_target_->DrawEllipse(ellipse, brush, stroke_width);
}

void Canvas::DrawLine(float x0, float y0, float x1, float y1,
                      const ColorF& color, float stroke_width) {
  if (!render_target_) {
    return;
  }
  ID2D1SolidColorBrush* brush = BrushFor(color);
  if (!brush) {
    return;
  }
  render_target_->DrawLine(D2D1::Point2F(x0, y0), D2D1::Point2F(x1, y1), brush,
                           stroke_width);
}

void Canvas::PushAxisAlignedClip(const RectF& rect) {
  if (!render_target_) {
    return;
  }
  render_target_->PushAxisAlignedClip(ToD2D(rect),
                                      D2D1_ANTIALIAS_MODE_ALIASED);
}

void Canvas::PopAxisAlignedClip() {
  if (!render_target_) {
    return;
  }
  render_target_->PopAxisAlignedClip();
}

void Canvas::DrawText(const std::wstring& text, const RectF& layout_rect,
                      const ColorF& color, float font_size,
                      const wchar_t* font_family, TextHAlign align) {
  if (!render_target_ || !dwrite_factory_ || text.empty()) {
    return;
  }

  IDWriteTextFormat* format = nullptr;
  HRESULT hr = dwrite_factory_->CreateTextFormat(
      font_family ? font_family : L"Microsoft YaHei UI", nullptr,
      DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, font_size, L"zh-cn", &format);
  if (FAILED(hr) || !format) {
    return;
  }

  DWRITE_TEXT_ALIGNMENT dwrite_align = DWRITE_TEXT_ALIGNMENT_LEADING;
  switch (align) {
    case TextHAlign::Center:
      dwrite_align = DWRITE_TEXT_ALIGNMENT_CENTER;
      break;
    case TextHAlign::Right:
      dwrite_align = DWRITE_TEXT_ALIGNMENT_TRAILING;
      break;
    case TextHAlign::Left:
    default:
      dwrite_align = DWRITE_TEXT_ALIGNMENT_LEADING;
      break;
  }
  format->SetTextAlignment(dwrite_align);
  format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

  ID2D1SolidColorBrush* brush = BrushFor(color);
  if (brush) {
    render_target_->DrawText(text.c_str(), static_cast<UINT32>(text.size()),
                             format, ToD2D(layout_rect), brush,
                             D2D1_DRAW_TEXT_OPTIONS_CLIP);
  }
  format->Release();
}

float Canvas::MeasureTextWidth(const std::wstring& text, float font_size,
                               const wchar_t* font_family) {
  return MeasureUiTextWidth(text, font_size, font_family);
}

float MeasureUiTextWidth(const std::wstring& text, float font_size,
                         const wchar_t* font_family) {
  if (text.empty()) {
    return 0.f;
  }

  static IDWriteFactory* factory = nullptr;
  if (!factory) {
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(&factory));
  }
  if (!factory) {
    // Fallback: CJK ≈ em square; Latin ≈ 0.55em (old Latin-only heuristic).
    float w = 0.f;
    for (wchar_t ch : text) {
      w += (ch > 0x7F) ? font_size : (font_size * 0.55f);
    }
    return w;
  }

  IDWriteTextFormat* format = nullptr;
  HRESULT hr = factory->CreateTextFormat(
      font_family ? font_family : L"Microsoft YaHei UI", nullptr,
      DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, font_size, L"zh-cn", &format);
  if (FAILED(hr) || !format) {
    float w = 0.f;
    for (wchar_t ch : text) {
      w += (ch > 0x7F) ? font_size : (font_size * 0.55f);
    }
    return w;
  }

  IDWriteTextLayout* layout = nullptr;
  hr = factory->CreateTextLayout(text.c_str(),
                                 static_cast<UINT32>(text.size()), format,
                                 100000.f, font_size * 2.f, &layout);
  format->Release();
  if (FAILED(hr) || !layout) {
    float w = 0.f;
    for (wchar_t ch : text) {
      w += (ch > 0x7F) ? font_size : (font_size * 0.55f);
    }
    return w;
  }

  DWRITE_TEXT_METRICS metrics = {};
  hr = layout->GetMetrics(&metrics);
  layout->Release();
  if (FAILED(hr)) {
    float w = 0.f;
    for (wchar_t ch : text) {
      w += (ch > 0x7F) ? font_size : (font_size * 0.55f);
    }
    return w;
  }
  return metrics.widthIncludingTrailingWhitespace;
}

void Canvas::DrawImage(const Image& image, const RectF& dest) {
  if (!render_target_ || image.empty()) {
    return;
  }
  render_target_->DrawBitmap(image.bitmap(), ToD2D(dest));
}

void Canvas::DrawImage(const Image& image, const RectF& src,
                       const RectF& dest) {
  if (!render_target_ || image.empty()) {
    return;
  }
  render_target_->DrawBitmap(image.bitmap(), ToD2D(dest), 1.f,
                             D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                             ToD2D(src));
}

}  // namespace mx
