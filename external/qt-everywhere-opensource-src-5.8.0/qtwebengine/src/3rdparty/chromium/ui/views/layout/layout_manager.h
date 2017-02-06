// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_MANAGER_H_
#define UI_VIEWS_LAYOUT_LAYOUT_MANAGER_H_

#include "ui/views/views_export.h"

namespace gfx {
class Size;
}

namespace views {

class View;

/////////////////////////////////////////////////////////////////////////////
//
// LayoutManager interface
//
//   The LayoutManager interface provides methods to handle the sizing of
//   the children of a View according to implementation-specific heuristics.
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT LayoutManager {
 public:
  virtual ~LayoutManager();

  // Notification that this LayoutManager has been installed on a particular
  // host.
  virtual void Installed(View* host);

  // Notification that this LayoutManager has been uninstalled on a particular
  // host.
  virtual void Uninstalled(View* host);

  // Lay out the children of |host| according to implementation-specific
  // heuristics. The graphics used during painting is provided to allow for
  // string sizing.
  virtual void Layout(View* host) = 0;

  // Return the preferred size which is the size required to give each
  // children their respective preferred size.
  virtual gfx::Size GetPreferredSize(const View* host) const = 0;

  // Returns the preferred height for the specified width. The default
  // implementation returns the value from GetPreferredSize.
  virtual int GetPreferredHeightForWidth(const View* host, int width) const;

  // Notification that a view has been added.
  virtual void ViewAdded(View* host, View* view);

  // Notification that a view has been removed.
  virtual void ViewRemoved(View* host, View* view);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_MANAGER_H_
