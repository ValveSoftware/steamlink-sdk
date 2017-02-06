// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/caca_window.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/caca/caca_event_source.h"
#include "ui/ozone/platform/caca/caca_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

namespace {

// Size of initial cnavas (in characters).
const int kDefaultCanvasWidth = 160;
const int kDefaultCanvasHeight = 48;

}  // namespace

// TODO(dnicoara) As an optimization, |bitmap_size_| should be scaled based on
// |physical_size_| and font size.
CacaWindow::CacaWindow(PlatformWindowDelegate* delegate,
                       CacaWindowManager* manager,
                       CacaEventSource* event_source,
                       const gfx::Rect& bounds)
    : delegate_(delegate),
      manager_(manager),
      event_source_(event_source),
      weak_ptr_factory_(this) {
  widget_ = manager_->AddWindow(this);
  ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(widget_, 1.f);
}

CacaWindow::~CacaWindow() {
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  manager_->RemoveWindow(widget_, this);
}

bool CacaWindow::Initialize() {
  if (display_)
    return true;

  canvas_.reset(caca_create_canvas(kDefaultCanvasWidth, kDefaultCanvasHeight));
  if (!canvas_) {
    PLOG(ERROR) << "failed to create libcaca canvas";
    return false;
  }

  display_.reset(caca_create_display(canvas_.get()));
  if (!display_) {
    PLOG(ERROR) << "failed to initialize libcaca display";
    return false;
  }

  caca_set_cursor(display_.get(), 1);
  caca_set_display_title(display_.get(), "Ozone Content Shell");

  UpdateDisplaySize();

  TryProcessingEvent();

  return true;
}

void CacaWindow::TryProcessingEvent() {
  event_source_->TryProcessingEvent(this);

  // Caca uses a poll based event retrieval. Since we don't want to block we'd
  // either need to spin up a new thread or just poll. For simplicity just poll
  // for messages.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&CacaWindow::TryProcessingEvent,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(10));
}

void CacaWindow::UpdateDisplaySize() {
  physical_size_.SetSize(caca_get_canvas_width(canvas_.get()),
                         caca_get_canvas_height(canvas_.get()));

  bitmap_size_.SetSize(caca_get_display_width(display_.get()),
                       caca_get_display_height(display_.get()));
}

void CacaWindow::OnCacaResize() {
  UpdateDisplaySize();

  delegate_->OnBoundsChanged(gfx::Rect(bitmap_size_));
}

void CacaWindow::OnCacaQuit() {
  delegate_->OnCloseRequest();
}


void CacaWindow::OnCacaEvent(ui::Event* event) {
  DispatchEventFromNativeUiEvent(
      event, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                        base::Unretained(delegate_)));
}

gfx::Rect CacaWindow::GetBounds() { return gfx::Rect(bitmap_size_); }

void CacaWindow::SetBounds(const gfx::Rect& bounds) {}

void CacaWindow::Show() {}

void CacaWindow::Hide() {}

void CacaWindow::Close() {}

void CacaWindow::SetCapture() {}

void CacaWindow::ReleaseCapture() {}

void CacaWindow::ToggleFullscreen() {}

void CacaWindow::Maximize() {}

void CacaWindow::Minimize() {}

void CacaWindow::Restore() {}

void CacaWindow::SetCursor(PlatformCursor cursor) {}

void CacaWindow::MoveCursorTo(const gfx::Point& location) {}

void CacaWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {}

PlatformImeController* CacaWindow::GetPlatformImeController() {
  return nullptr;
}

void CacaWindow::SetTitle(const base::string16& title) {
  caca_set_display_title(display_.get(), UTF16ToUTF8(title).c_str());
}

bool CacaWindow::CanDispatchEvent(const PlatformEvent& event) { return true; }

uint32_t CacaWindow::DispatchEvent(const PlatformEvent& ne) {
  // We don't dispatch events via PlatformEventSource.
  NOTREACHED();
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
