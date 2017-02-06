// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_foreign_window_manager.h"

#include <stddef.h>
#include <X11/Xlib.h>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"

namespace ui {

// static
XForeignWindowManager* XForeignWindowManager::GetInstance() {
  return base::Singleton<XForeignWindowManager>::get();
}

int XForeignWindowManager::RequestEvents(XID xid, long event_mask) {
  request_map_[xid].push_back(Request(next_request_id_, event_mask));
  UpdateSelectedEvents(xid);
  return next_request_id_++;
}

void XForeignWindowManager::CancelRequest(int request_id) {
  for (std::map<XID, RequestVector>::iterator map_it = request_map_.begin();
       map_it != request_map_.end(); ++map_it) {
    RequestVector* vector = &map_it->second;
    for (RequestVector::iterator vector_it = vector->begin();
         vector_it != vector->end(); ++vector_it) {
      if (vector_it->request_id == request_id) {
        vector->erase(vector_it);
        UpdateSelectedEvents(map_it->first);
        if (vector->empty())
          request_map_.erase(map_it);
        return;
      }
    }
  }
}

void XForeignWindowManager::OnWindowDestroyed(XID xid) {
  request_map_.erase(xid);
}

XForeignWindowManager::XForeignWindowManager() : next_request_id_(0) {
}

XForeignWindowManager::~XForeignWindowManager() {
}

void XForeignWindowManager::UpdateSelectedEvents(XID xid) {
  std::map<XID, RequestVector>::iterator it = request_map_.find(xid);
  if (it == request_map_.end())
    return;

  long event_mask = NoEventMask;
  const RequestVector& list = it->second;
  for (size_t i = 0; i < list.size(); ++i)
    event_mask |= list[i].event_mask;

  XSelectInput(gfx::GetXDisplay(), xid, event_mask);
}

XForeignWindowManager::Request::Request(int request_id, long event_mask)
    : request_id(request_id),
      event_mask(event_mask) {
}

XForeignWindowManager::Request::~Request() {
}

}  // namespace ui
