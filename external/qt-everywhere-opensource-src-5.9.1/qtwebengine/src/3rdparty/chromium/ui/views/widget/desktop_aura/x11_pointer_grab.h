// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/x11_types.h"

typedef unsigned long Cursor;

namespace views {

// Grabs the pointer. It is unnecessary to ungrab the pointer prior to grabbing
// it.
int GrabPointer(XID window, bool owner_events, ::Cursor cursor);

// Sets the cursor to use for the duration of the active pointer grab.
void ChangeActivePointerGrabCursor(::Cursor cursor);

// Ungrabs the pointer.
void UngrabPointer();

}  // namespace views
