// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_CLIENT_NATIVE_PIXMAP_FACTORY_X11_H_
#define UI_OZONE_PLATFORM_X11_CLIENT_NATIVE_PIXMAP_FACTORY_X11_H_

namespace ui {

class ClientNativePixmapFactory;

// Constructor hook for use in constructor_list.cc
ClientNativePixmapFactory* CreateClientNativePixmapFactoryX11();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_CLIENT_NATIVE_PIXMAP_FACTORY_X11_H_
