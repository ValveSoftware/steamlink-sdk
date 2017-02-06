// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/demo/mus_demo.h"

#include "base/time/time.h"
#include "components/bitmap_uploader/bitmap_uploader.h"
#include "components/mus/common/gpu_service.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "services/shell/public/cpp/connector.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect.h"

namespace mus_demo {

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
  delete window_tree_client_;
}

void MusDemo::Initialize(shell::Connector* connector,
                         const shell::Identity& identity,
                         uint32_t id) {
  connector_ = connector;
  mus::GpuService::Initialize(connector_);
  window_tree_client_ = new mus::WindowTreeClient(this, this, nullptr);
  window_tree_client_->ConnectAsWindowManager(connector);
}

bool MusDemo::AcceptConnection(shell::Connection* connection) {
  return true;
}

void MusDemo::OnEmbed(mus::Window* window) {
  // Not called for the WindowManager.
  NOTREACHED();
}

void MusDemo::OnWindowTreeClientDestroyed(mus::WindowTreeClient* client) {
  window_tree_client_ = nullptr;
  timer_.Stop();
}

void MusDemo::OnEventObserved(const ui::Event& event, mus::Window* target) {}

void MusDemo::SetWindowManagerClient(mus::WindowManagerClient* client) {}

bool MusDemo::OnWmSetBounds(mus::Window* window, gfx::Rect* bounds) {
  return true;
}

bool MusDemo::OnWmSetProperty(mus::Window* window,
                              const std::string& name,
                              std::unique_ptr<std::vector<uint8_t>>* new_data) {
  return true;
}

mus::Window* MusDemo::OnWmCreateTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  return nullptr;
}

void MusDemo::OnWmClientJankinessChanged(
    const std::set<mus::Window*>& client_windows,
    bool janky) {
  // Don't care
}

void MusDemo::OnWmNewDisplay(mus::Window* window,
                             const display::Display& display) {
  DCHECK(!window_);  // Only support one display.
  window_ = window;

  // Initialize bitmap uploader for sending frames to MUS.
  uploader_.reset(new bitmap_uploader::BitmapUploader(window_));
  uploader_->Init(connector_);

  // Draw initial frame and start the timer to regularly draw frames.
  DrawFrame();
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kFrameDelay),
               base::Bind(&MusDemo::DrawFrame, base::Unretained(this)));
}

void MusDemo::OnAccelerator(uint32_t id, const ui::Event& event) {
  // Don't care
}

void MusDemo::AllocBitmap() {
  const gfx::Rect bounds = window_->GetBoundsInRoot();

  // Allocate bitmap the same size as the window for drawing.
  bitmap_.reset();
  SkImageInfo image_info = SkImageInfo::MakeN32(bounds.width(), bounds.height(),
                                                kPremul_SkAlphaType);
  bitmap_.allocPixels(image_info);
}

void MusDemo::DrawFrame() {
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

  // Send frame to MUS via BitmapUploader.
  uploader_->SetBitmap(bounds.width(), bounds.height(), std::move(data),
                       bitmap_uploader::BitmapUploader::BGRA);
}

}  // namespace mus_demo
