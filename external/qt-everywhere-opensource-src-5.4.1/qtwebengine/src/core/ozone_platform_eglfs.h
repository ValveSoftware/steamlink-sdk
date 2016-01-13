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

#ifndef UI_OZONE_PLATFORM_EGLFS_OZONE_PLATFORM_EGLFS_H_
#define UI_OZONE_PLATFORM_EGLFS_OZONE_PLATFORM_EGLFS_H_

#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/ozone/ozone_platform.h"

#include "surface_factory_qt.h"

#if defined(USE_OZONE)

namespace ui {

class OzonePlatformEglfs : public OzonePlatform {
 public:
  OzonePlatformEglfs();
  virtual ~OzonePlatformEglfs();

  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE;
  virtual ui::EventFactoryOzone* GetEventFactoryOzone() OVERRIDE;
  virtual ui::CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE;
  virtual GpuPlatformSupport* GetGpuPlatformSupport() OVERRIDE;
  virtual GpuPlatformSupportHost* GetGpuPlatformSupportHost() OVERRIDE;
  virtual void InitializeUI() OVERRIDE;
  virtual void InitializeGPU() OVERRIDE;

 private:
  scoped_ptr<DeviceManager> device_manager_;

  scoped_ptr<SurfaceFactoryQt> surface_factory_ozone_;
  scoped_ptr<ui::CursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<ui::EventFactoryEvdev> event_factory_ozone_;

  scoped_ptr<GpuPlatformSupport> gpu_platform_support_;
  scoped_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformEglfs);
};

// Constructor hook for use in ozone_platform_list.cc
OzonePlatform* CreateOzonePlatformEglfs();

}  // namespace ui

#endif // defined(USE_OZONE)
#endif // UI_OZONE_PLATFORM_EGLFS_OZONE_PLATFORM_EGLFS_H_
