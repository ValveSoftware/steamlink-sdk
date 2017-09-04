// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_PUBLIC_CPP_VIEW_DELEGATE_H_
#define SERVICES_NAVIGATION_PUBLIC_CPP_VIEW_DELEGATE_H_

#include <memory>

namespace gfx {
class Rect;
}

namespace navigation {

class View;

class ViewDelegate {
 public:
  // Called when an action within the page has caused a new View to be created.
  // The delegate must take action to either create a new host for this View, or
  // close it.
  // |source| is the View originating the request.
  // |new_view| is the newly created View.
  // |is_popup| is true if the View should be shown in a popup window instead of
  // a new tab.
  // |requested_bounds| are the requested bounds of the new popup.
  // |user_gesture| is true if the View was created as the result of an explicit
  // user input event.
  virtual void ViewCreated(View* source,
                           std::unique_ptr<View> new_view,
                           bool is_popup,
                           const gfx::Rect& requested_bounds,
                           bool user_gesture) = 0;

  // Called when an action within the page has requested that the View be
  // closed.
  virtual void Close(View* source) = 0;

  // Called when the application in the source view wants the delegate to
  // navigate the view according to |params|.
  virtual void OpenURL(View* source, mojom::OpenURLParamsPtr params) = 0;
};

}  // namespace navigation

#endif  // SERVICES_NAVIGATION_PUBLIC_CPP_VIEW_DELEGATE_H_
