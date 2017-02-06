// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/platform_screen_impl_ozone.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/public/ozone_platform.h"

namespace mus {

namespace ws {
namespace {

// TODO(rjkroege): Remove this code once ozone oxygen has the same
// display creation semantics as ozone drm.
// Some ozone platforms do not configure physical displays and so do not
// callback into this class via the implementation of NativeDisplayObserver.
// FixedSizeScreenConfiguration() short-circuits the implementation of display
// configuration in this case by calling the |callback| provided to
// ConfigurePhysicalDisplay() with a hard-coded |id| and |bounds|.
void FixedSizeScreenConfiguration(
    const PlatformScreen::ConfiguredDisplayCallback& callback) {
  callback.Run(1, gfx::Rect(1024, 768));
}

void GetDisplaysFinished(const std::vector<ui::DisplaySnapshot*>& displays) {
  // We don't really care about list of displays, we just want the snapshots
  // held by DrmDisplayManager to be updated. This only only happens when we
  // call NativeDisplayDelegate::GetDisplays(). Although, this would be a good
  // place to have PlatformScreen cache the snapshots if need be.
}

}  // namespace

// static
std::unique_ptr<PlatformScreen> PlatformScreen::Create() {
  return base::WrapUnique(new PlatformScreenImplOzone);
}

PlatformScreenImplOzone::PlatformScreenImplOzone() : weak_ptr_factory_(this) {}

PlatformScreenImplOzone::~PlatformScreenImplOzone() {}

void PlatformScreenImplOzone::Init() {
  native_display_delegate_ =
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();
  native_display_delegate_->AddObserver(this);
  native_display_delegate_->Initialize();
}

void PlatformScreenImplOzone::ConfigurePhysicalDisplay(
    const PlatformScreen::ConfiguredDisplayCallback& callback) {
#if defined(OS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // Kick off the configuration of the physical displays comprising the
    // |PlatformScreenImplOzone|

    DCHECK(native_display_delegate_) << "DefaultDisplayManager::"
                                        "OnConfigurationChanged requires a "
                                        "native_display_delegate_ to work.";

    native_display_delegate_->GetDisplays(
        base::Bind(&PlatformScreenImplOzone::OnDisplaysAquired,
                   weak_ptr_factory_.GetWeakPtr(), callback));

    return;
  }
#endif  // defined(OS_CHROMEOS)
  // PostTask()ed to maximize control flow similarity with the ChromeOS case.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FixedSizeScreenConfiguration, callback));
}

void PlatformScreenImplOzone::OnConfigurationChanged() {}

// The display subsystem calls |OnDisplaysAquired| to deliver |displays|
// describing the attached displays.
void PlatformScreenImplOzone::OnDisplaysAquired(
    const ConfiguredDisplayCallback& callback,
    const std::vector<ui::DisplaySnapshot*>& displays) {
  DCHECK(native_display_delegate_) << "DefaultDisplayManager::"
                                      "OnConfigurationChanged requires a "
                                      "native_display_delegate_ to work.";
  CHECK(displays.size() == 1) << "Mus only supports one 1 display\n";
  gfx::Point origin;
  for (auto display : displays) {
    if (!display->native_mode()) {
      LOG(ERROR) << "Display " << display->display_id()
                 << " doesn't have a native mode";
      continue;
    }
    // Setup each native display. This places a task on the DRM thread's
    // runqueue that configures the window size correctly before the call to
    // Configure.
    native_display_delegate_->Configure(
        *display, display->native_mode(), origin,
        base::Bind(&PlatformScreenImplOzone::OnDisplayConfigured,
                   weak_ptr_factory_.GetWeakPtr(), callback,
                   display->display_id(),
                   gfx::Rect(origin, display->native_mode()->size())));
    origin.Offset(display->native_mode()->size().width(), 0);
  }
}

void PlatformScreenImplOzone::OnDisplayConfigured(
    const ConfiguredDisplayCallback& callback,
    int64_t id,
    const gfx::Rect& bounds,
    bool success) {
  if (success) {
    native_display_delegate_->GetDisplays(base::Bind(&GetDisplaysFinished));
    callback.Run(id, bounds);
  } else {
    LOG(FATAL) << "Failed to configure display at " << bounds.ToString();
  }
}

}  // namespace ws
}  // namespace mus
