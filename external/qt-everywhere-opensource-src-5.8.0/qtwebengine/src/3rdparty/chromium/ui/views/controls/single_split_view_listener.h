// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_LISTENER_H_
#define UI_VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_LISTENER_H_

#include "ui/views/views_export.h"

namespace views {

class SingleSplitView;

// An interface implemented by objects that want to be notified when the
// splitter moves.
class VIEWS_EXPORT SingleSplitViewListener {
 public:
  // Invoked when split handle is moved by the user. |sender|'s divider_offset
  // is already set to the new value, but Layout has not happened yet.
  // Returns false if the layout has been handled by the listener, returns
  // true if |sender| should do it by itself.
  virtual bool SplitHandleMoved(SingleSplitView* sender) = 0;

 protected:
  virtual ~SingleSplitViewListener() {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_LISTENER_H_
