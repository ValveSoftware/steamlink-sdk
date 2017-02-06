/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef UI_OZONE_PLATFORM_EGLFS_OZONE_PLATFORM_EGLFS_H_
#define UI_OZONE_PLATFORM_EGLFS_OZONE_PLATFORM_EGLFS_H_

#if defined(USE_OZONE)

#include "ui/ozone/public/ozone_platform.h"

#include "surface_factory_qt.h"

namespace ui {

class DeviceManager;
class EventFactoryEvdev;
class CursorFactoryOzone;

class OzonePlatformEglfs : public OzonePlatform {
 public:
  OzonePlatformEglfs();
  virtual ~OzonePlatformEglfs();

  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override;
  virtual ui::CursorFactoryOzone* GetCursorFactoryOzone() override;
  virtual GpuPlatformSupport* GetGpuPlatformSupport() override;
  virtual GpuPlatformSupportHost* GetGpuPlatformSupportHost() override;
  virtual std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override;
  virtual std::unique_ptr<ui::NativeDisplayDelegate> CreateNativeDisplayDelegate() override;
  virtual ui::InputController* GetInputController() override;
  virtual std::unique_ptr<ui::SystemInputInjector> CreateSystemInputInjector() override;
  virtual ui::OverlayManagerOzone* GetOverlayManager() override;

 private:
  virtual void InitializeUI() override;
  virtual void InitializeGPU() override;
  std::unique_ptr<DeviceManager> device_manager_;

  std::unique_ptr<QtWebEngineCore::SurfaceFactoryQt> surface_factory_ozone_;
  std::unique_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<EventFactoryEvdev> event_factory_ozone_;

  std::unique_ptr<GpuPlatformSupport> gpu_platform_support_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformEglfs);
};

// Constructor hook for use in ozone_platform_list.cc
OzonePlatform* CreateOzonePlatformEglfs();

}  // namespace ui

#endif // defined(USE_OZONE)
#endif // UI_OZONE_PLATFORM_EGLFS_OZONE_PLATFORM_EGLFS_H_
