// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include "base/logging.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

NativeViewHostAura::NativeViewHostAura(NativeViewHost* host)
    : host_(host),
      installed_clip_(false) {
}

NativeViewHostAura::~NativeViewHostAura() {
  if (host_->native_view()) {
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->RemoveObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostAura, NativeViewHostWrapper implementation:
void NativeViewHostAura::NativeViewWillAttach() {
  host_->native_view()->AddObserver(this);
  host_->native_view()->SetProperty(views::kHostViewKey,
      static_cast<View*>(host_));
}

void NativeViewHostAura::NativeViewDetaching(bool destroyed) {
  if (!destroyed) {
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->Hide();
    if (host_->native_view()->parent())
      Widget::ReparentNativeView(host_->native_view(), NULL);
  }
}

void NativeViewHostAura::AddedToWidget() {
  if (!host_->native_view())
    return;

  aura::Window* widget_window = host_->GetWidget()->GetNativeView();
  if (host_->native_view()->parent() != widget_window)
    widget_window->AddChild(host_->native_view());
  if (host_->IsDrawn())
    host_->native_view()->Show();
  else
    host_->native_view()->Hide();
  host_->Layout();
}

void NativeViewHostAura::RemovedFromWidget() {
  if (host_->native_view()) {
    host_->native_view()->Hide();
    if (host_->native_view()->parent())
      host_->native_view()->parent()->RemoveChild(host_->native_view());
  }
}

void NativeViewHostAura::InstallClip(int x, int y, int w, int h) {
  // Note that this does not pose a problem functionality wise - it might
  // however pose a speed degradation if not implemented.
  LOG(WARNING) << "NativeViewHostAura::InstallClip is not implemented yet.";
}

bool NativeViewHostAura::HasInstalledClip() {
  return installed_clip_;
}

void NativeViewHostAura::UninstallClip() {
  installed_clip_ = false;
}

void NativeViewHostAura::ShowWidget(int x, int y, int w, int h) {
  // TODO: need to support fast resize.
  host_->native_view()->SetBounds(gfx::Rect(x, y, w, h));
  host_->native_view()->Show();
}

void NativeViewHostAura::HideWidget() {
  host_->native_view()->Hide();
}

void NativeViewHostAura::SetFocus() {
  aura::Window* window = host_->native_view();
  aura::client::FocusClient* client = aura::client::GetFocusClient(window);
  if (client)
    client->FocusWindow(window);
}

gfx::NativeViewAccessible NativeViewHostAura::GetNativeViewAccessible() {
  return NULL;
}

gfx::NativeCursor NativeViewHostAura::GetCursor(int x, int y) {
  if (host_->native_view())
    return host_->native_view()->GetCursor(gfx::Point(x, y));
  return gfx::kNullCursor;
}

void NativeViewHostAura::OnWindowDestroyed(aura::Window* window) {
  DCHECK(window == host_->native_view());
  host_->NativeViewDestroyed();
}

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostAura(host);
}

}  // namespace views
