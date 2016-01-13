// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_CONSTANTS_H_
#define UI_COMPOSITOR_COMPOSITOR_CONSTANTS_H_

// This flag is used to work around a rendering bug in Windows where the
// software surface becomes inconsistent with the hardware surface. This
// happens in particular circumstances (Classic mode, Chrome using hardware
// rendering, and the window on a secondary monitor) and when moving the mouse
// over the window, Windows restores the GDI software surface underneath the
// mouse, rather than the hardware surface we've painted. As a result, for
// small or infrequently painting windows, we force back to the software
// compositor to avoid this bug. See http://crbug.com/333380.
const wchar_t kForceSoftwareCompositor[] = L"Chrome.ForceSoftwareCompositor";

#endif  // UI_COMPOSITOR_COMPOSITOR_CONSTANTS_H_
