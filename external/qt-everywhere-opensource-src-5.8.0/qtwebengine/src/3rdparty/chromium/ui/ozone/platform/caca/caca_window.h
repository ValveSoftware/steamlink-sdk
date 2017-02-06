// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CACA_CACA_WINDOW_H_
#define UI_OZONE_PLATFORM_CACA_CACA_WINDOW_H_

#include <caca.h>
#include <stdint.h>

#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/caca/scoped_caca_types.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class CacaEventSource;
class CacaWindowManager;
class PlatformWindowDelegate;

// Basic initialization of Libcaca. This needs to be shared between SFO and EFO
// since both need the |display_| to draw and process events respectively.
// Note, |canvas_| needs to be here since it is needed for creating a
// |display_|.
class CacaWindow : public PlatformWindow, public PlatformEventDispatcher {
 public:
  CacaWindow(PlatformWindowDelegate* delegate,
             CacaWindowManager* manager,
             CacaEventSource* event_source,
             const gfx::Rect& bounds);
  ~CacaWindow() override;

  bool Initialize();

  // Handlers for events received from libcaca.
  void OnCacaQuit();
  void OnCacaResize();
  void OnCacaEvent(ui::Event* event);

  // This is the Caca canvas size.
  gfx::Size physical_size() const { return physical_size_; }
  gfx::Size bitmap_size() const { return bitmap_size_; }
  caca_display_t* display() const { return display_.get(); }

  // PlatformWindow:
  gfx::Rect GetBounds() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;
  void SetTitle(const base::string16& title) override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

 private:
  // Event polling.
  void TryProcessingEvent();

  // Sync sizes with libcaca.
  void UpdateDisplaySize();

  PlatformWindowDelegate* delegate_;
  CacaWindowManager* manager_;
  CacaEventSource* event_source_;
  gfx::AcceleratedWidget widget_;

  ScopedCacaCanvas canvas_;
  ScopedCacaDisplay display_;

  // Size of the console in characters. This can be changed by setting
  // CACA_GEOMETRY environment variable.
  gfx::Size physical_size_;

  // Size of the backing buffer we draw into. Size in pixels.
  gfx::Size bitmap_size_;

  base::WeakPtrFactory<CacaWindow> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacaWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CACA_CACA_WINDOW_H_
