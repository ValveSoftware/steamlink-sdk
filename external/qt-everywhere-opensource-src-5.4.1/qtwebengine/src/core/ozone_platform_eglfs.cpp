/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "ozone_platform_eglfs.h"

#include "media/ozone/media_ozone_platform.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

#if defined(USE_OZONE)

namespace media {

MediaOzonePlatform* CreateMediaOzonePlatformEglfs() {
  return new MediaOzonePlatform;
}

}

namespace ui {

OzonePlatformEglfs::OzonePlatformEglfs() {}

OzonePlatformEglfs::~OzonePlatformEglfs() {}

ui::SurfaceFactoryOzone* OzonePlatformEglfs::GetSurfaceFactoryOzone() {
  return surface_factory_ozone_.get();
}

ui::EventFactoryOzone* OzonePlatformEglfs::GetEventFactoryOzone() {
  return event_factory_ozone_.get();
}

ui::CursorFactoryOzone* OzonePlatformEglfs::GetCursorFactoryOzone() {
  return cursor_factory_ozone_.get();
}

GpuPlatformSupport* OzonePlatformEglfs::GetGpuPlatformSupport() {
  return gpu_platform_support_.get();
}

GpuPlatformSupportHost* OzonePlatformEglfs::GetGpuPlatformSupportHost() {
  return gpu_platform_support_host_.get();
}

OzonePlatform* CreateOzonePlatformEglfs() { return new OzonePlatformEglfs; }

void OzonePlatformEglfs::InitializeUI() {
  device_manager_ = CreateDeviceManager();
  cursor_factory_ozone_.reset(new CursorFactoryOzone());
  event_factory_ozone_.reset(new EventFactoryEvdev(NULL, device_manager_.get()));
  gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
}

void OzonePlatformEglfs::InitializeGPU() {
  surface_factory_ozone_.reset(new SurfaceFactoryQt());
  gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
}

}  // namespace ui

#endif

