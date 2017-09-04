// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/demo/mus_demo.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/demo/bitmap_uploader.h"
#include "services/ui/public/cpp/gpu_service.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace demo {

namespace {

// Milliseconds between frames.
const int64_t kFrameDelay = 33;

// Size of square in pixels to draw.
const int kSquareSize = 300;

const SkColor kBgColor = SK_ColorRED;
const SkColor kFgColor = SK_ColorYELLOW;

void DrawSquare(const gfx::Rect& bounds, double angle, SkCanvas* canvas) {
  // Create SkRect to draw centered inside the bounds.
  gfx::Point top_left = bounds.CenterPoint();
  top_left.Offset(-kSquareSize / 2, -kSquareSize / 2);
  SkRect rect =
      SkRect::MakeXYWH(top_left.x(), top_left.y(), kSquareSize, kSquareSize);

  // Set SkPaint to fill solid color.
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kFgColor);

  // Rotate the canvas.
  const gfx::Size canvas_size = bounds.size();
  if (angle != 0.0) {
    canvas->translate(SkFloatToScalar(canvas_size.width() * 0.5f),
                      SkFloatToScalar(canvas_size.height() * 0.5f));
    canvas->rotate(angle);
    canvas->translate(-SkFloatToScalar(canvas_size.width() * 0.5f),
                      -SkFloatToScalar(canvas_size.height() * 0.5f));
  }

  canvas->drawRect(rect, paint);
}

}  // namespace

MusDemo::MusDemo() {}

MusDemo::~MusDemo() {
  display::Screen::SetScreenInstance(nullptr);
}

void MusDemo::OnStart() {
  screen_ = base::MakeUnique<display::ScreenBase>();
  display::Screen::SetScreenInstance(screen_.get());
  gpu_service_ = GpuService::Create(context()->connector());
  window_tree_client_ = base::MakeUnique<WindowTreeClient>(this, this);
  window_tree_client_->ConnectAsWindowManager(context()->connector());
}

bool MusDemo::OnConnect(const service_manager::ServiceInfo& remote_info,
                        service_manager::InterfaceRegistry* registry) {
  return true;
}

void MusDemo::OnEmbed(Window* window) {
  // Not called for the WindowManager.
  NOTREACHED();
}

void MusDemo::OnEmbedRootDestroyed(Window* root) {
  // Not called for the WindowManager.
  NOTREACHED();
}

void MusDemo::OnLostConnection(WindowTreeClient* client) {
  window_ = nullptr;
  window_tree_client_.reset();
  timer_.Stop();
}

void MusDemo::OnPointerEventObserved(const PointerEvent& event,
                                     Window* target) {}

void MusDemo::SetWindowManagerClient(WindowManagerClient* client) {}

bool MusDemo::OnWmSetBounds(Window* window, gfx::Rect* bounds) {
  return true;
}

bool MusDemo::OnWmSetProperty(Window* window,
                              const std::string& name,
                              std::unique_ptr<std::vector<uint8_t>>* new_data) {
  return true;
}

Window* MusDemo::OnWmCreateTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  return nullptr;
}

void MusDemo::OnWmClientJankinessChanged(
    const std::set<Window*>& client_windows,
    bool janky) {
  // Don't care
}

void MusDemo::OnWmNewDisplay(Window* window, const display::Display& display) {
  DCHECK(!window_);  // Only support one display.
  window_ = window;

  // Initialize bitmap uploader for sending frames to MUS.
  uploader_.reset(new BitmapUploader(window_));
  uploader_->Init(gpu_service_.get());

  // Draw initial frame and start the timer to regularly draw frames.
  DrawFrame();
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kFrameDelay),
               base::Bind(&MusDemo::DrawFrame, base::Unretained(this)));
}

void MusDemo::OnWmDisplayRemoved(ui::Window* window) {
  window->Destroy();
}

void MusDemo::OnWmDisplayModified(const display::Display& display) {}

void MusDemo::OnWmPerformMoveLoop(Window* window,
                                  mojom::MoveLoopSource source,
                                  const gfx::Point& cursor_location,
                                  const base::Callback<void(bool)>& on_done) {
  // Don't care
}

void MusDemo::OnWmCancelMoveLoop(Window* window) {}

void MusDemo::AllocBitmap() {
  const gfx::Rect bounds = window_->GetBoundsInRoot();

  // Allocate bitmap the same size as the window for drawing.
  bitmap_.reset();
  SkImageInfo image_info = SkImageInfo::MakeN32(bounds.width(), bounds.height(),
                                                kPremul_SkAlphaType);
  bitmap_.allocPixels(image_info);
}

void MusDemo::DrawFrame() {
  base::TimeTicks now = base::TimeTicks::Now();

  VLOG(1) << (now - last_draw_frame_time_).InMilliseconds()
          << "ms since the last frame was drawn.";
  last_draw_frame_time_ = now;

  angle_ += 2.0;
  if (angle_ >= 360.0)
    angle_ = 0.0;

  const gfx::Rect bounds = window_->GetBoundsInRoot();

  // Check that bitmap and window sizes match, otherwise reallocate bitmap.
  const SkImageInfo info = bitmap_.info();
  if (info.width() != bounds.width() || info.height() != bounds.height()) {
    AllocBitmap();
  }

  // Draw the rotated square on background in bitmap.
  SkCanvas canvas(bitmap_);
  canvas.clear(kBgColor);
  // TODO(kylechar): Add GL drawing instead of software rasterization in future.
  DrawSquare(bounds, angle_, &canvas);
  canvas.flush();

  // Copy pixels data into vector that will be passed to BitmapUploader.
  // TODO(rjkroege): Make a 1/0-copy bitmap uploader for the contents of a
  // SkBitmap.
  bitmap_.lockPixels();
  const unsigned char* addr =
      static_cast<const unsigned char*>(bitmap_.getPixels());
  const int bytes = bounds.width() * bounds.height() * 4;
  std::unique_ptr<std::vector<unsigned char>> data(
      new std::vector<unsigned char>(addr, addr + bytes));
  bitmap_.unlockPixels();

#if defined(OS_ANDROID)
  // TODO(jcivelli): find a way to not have an ifdef here.
  BitmapUploader::Format bitmap_format = BitmapUploader::RGBA;
#else
  BitmapUploader::Format bitmap_format = BitmapUploader::BGRA;
#endif

  // Send frame to MUS via BitmapUploader.
  uploader_->SetBitmap(bounds.width(), bounds.height(), std::move(data),
                       bitmap_format);
}

}  // namespace demo
}  // namespace ui
