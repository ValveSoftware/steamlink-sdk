// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_UNDERLAY_OPENGL_HOSTING_WINDOW_H_
#define UI_BASE_COCOA_UNDERLAY_OPENGL_HOSTING_WINDOW_H_

#import <Cocoa/Cocoa.h>

#include "ui/base/ui_base_export.h"

// Common base class for windows that host a OpenGL surface that renders under
// the window. Contains methods relating to hole punching so that the OpenGL
// surface is visible through the window.
UI_BASE_EXPORT
@interface UnderlayOpenGLHostingWindow : NSWindow
@end

#endif  // UI_BASE_COCOA_UNDERLAY_OPENGL_HOSTING_WINDOW_H_
