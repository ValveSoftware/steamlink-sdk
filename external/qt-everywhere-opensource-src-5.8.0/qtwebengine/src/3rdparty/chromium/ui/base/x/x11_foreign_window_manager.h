// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_FOREIGN_WINDOW_MANAGER_H_
#define UI_BASE_X_X11_FOREIGN_WINDOW_MANAGER_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/x/ui_base_x_export.h"
#include "ui/gfx/x/x11_types.h"

// A process wide singleton for selecting events on X windows which were not
// created by Chrome.
namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace ui {

// Manages the events that Chrome has selected on X windows which were not
// created by Chrome. This class allows several clients to select events
// on the same X window.
class UI_BASE_X_EXPORT XForeignWindowManager {
 public:
  static XForeignWindowManager* GetInstance();

  // Requests that events associated with |event_mask| on |xid| be reported to
  // Chrome. Returns an id to use for canceling the request.
  int RequestEvents(XID xid, long event_mask) WARN_UNUSED_RESULT;

  // Cancels the request with |request_id|. Unless there is another request for
  // events on the X window associated with |request_id|, this stops Chrome from
  // getting events for the X window.
  void CancelRequest(int request_id);

  // Called by X11DesktopHandler when |xid| is destroyed.
  void OnWindowDestroyed(XID xid);

 private:
  friend struct base::DefaultSingletonTraits<XForeignWindowManager>;

  struct Request {
    Request(int request_id, long entry_event_mask);
    ~Request();

    int request_id;
    long event_mask;
  };

  XForeignWindowManager();
  ~XForeignWindowManager();

  // Updates which events are selected on |xid|.
  void UpdateSelectedEvents(XID xid);

  // The id of the next request.
  int next_request_id_;

  typedef std::vector<Request> RequestVector;
  std::map<XID, RequestVector> request_map_;

  DISALLOW_COPY_AND_ASSIGN(XForeignWindowManager);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_FOREIGN_WINDOW_MANAGER_H_
