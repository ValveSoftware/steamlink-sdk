// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requestor.h"

#include "base/run_loop.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";

const char* kAtomsToCache[] = {
  kChromeSelection,
  NULL
};

}  // namespace

SelectionRequestor::SelectionRequestor(Display* x_display,
                                       Window x_window,
                                       Atom selection_name,
                                       PlatformEventDispatcher* dispatcher)
    : x_display_(x_display),
      x_window_(x_window),
      selection_name_(selection_name),
      dispatcher_(dispatcher),
      atom_cache_(x_display_, kAtomsToCache) {
}

SelectionRequestor::~SelectionRequestor() {}

bool SelectionRequestor::PerformBlockingConvertSelection(
    Atom target,
    scoped_refptr<base::RefCountedMemory>* out_data,
    size_t* out_data_bytes,
    size_t* out_data_items,
    Atom* out_type) {
  // The name of the property that we are either:
  // - Passing as a parameter with the XConvertSelection() request.
  // OR
  // - Asking the selection owner to set on |x_window_|.
  Atom property = atom_cache_.GetAtom(kChromeSelection);

  XConvertSelection(x_display_,
                    selection_name_,
                    target,
                    property,
                    x_window_,
                    CurrentTime);

  // Now that we've thrown our message off to the X11 server, we block waiting
  // for a response.
  PendingRequest pending_request(target);
  BlockTillSelectionNotifyForRequest(&pending_request);

  bool success = false;
  if (pending_request.returned_property == property) {
    success =  ui::GetRawBytesOfProperty(x_window_,
                                         pending_request.returned_property,
                                         out_data, out_data_bytes,
                                         out_data_items, out_type);
  }
  if (pending_request.returned_property != None)
    XDeleteProperty(x_display_, x_window_, pending_request.returned_property);
  return success;
}

void SelectionRequestor::PerformBlockingConvertSelectionWithParameter(
    Atom target,
    const std::vector< ::Atom>& parameter) {
  SetAtomArrayProperty(x_window_, kChromeSelection, "ATOM", parameter);
  PerformBlockingConvertSelection(target, NULL, NULL, NULL, NULL);
}

SelectionData SelectionRequestor::RequestAndWaitForTypes(
    const std::vector< ::Atom>& types) {
  for (std::vector< ::Atom>::const_iterator it = types.begin();
       it != types.end(); ++it) {
    scoped_refptr<base::RefCountedMemory> data;
    size_t data_bytes = 0;
    ::Atom type = None;
    if (PerformBlockingConvertSelection(*it,
                                        &data,
                                        &data_bytes,
                                        NULL,
                                        &type) &&
        type == *it) {
      return SelectionData(type, data);
    }
  }

  return SelectionData();
}

void SelectionRequestor::OnSelectionNotify(const XSelectionEvent& event) {
  // Find the PendingRequest for the corresponding XConvertSelection call. If
  // there are multiple pending requests on the same target, satisfy them in
  // FIFO order.
  PendingRequest* request_notified = NULL;
  if (selection_name_ == event.selection) {
    for (std::list<PendingRequest*>::iterator iter = pending_requests_.begin();
         iter != pending_requests_.end(); ++iter) {
      PendingRequest* request = *iter;
      if (request->returned)
        continue;
      if (request->target != event.target)
        continue;
      request_notified = request;
      break;
    }
  }

  // This event doesn't correspond to any XConvertSelection calls that we
  // issued in PerformBlockingConvertSelection. This shouldn't happen, but any
  // client can send any message, so it can happen.
  if (!request_notified) {
    // ICCCM requires us to delete the property passed into SelectionNotify. If
    // |request_notified| is true, the property will be deleted when the run
    // loop has quit.
    if (event.property != None)
      XDeleteProperty(x_display_, x_window_, event.property);
    return;
  }

  request_notified->returned_property = event.property;
  request_notified->returned = true;

  if (!request_notified->quit_closure.is_null())
    request_notified->quit_closure.Run();
}

void SelectionRequestor::BlockTillSelectionNotifyForRequest(
    PendingRequest* request) {
  pending_requests_.push_back(request);

  const int kMaxWaitTimeForClipboardResponse = 300;
  if (PlatformEventSource::GetInstance()) {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
    base::RunLoop run_loop;

    request->quit_closure = run_loop.QuitClosure();
    loop->PostDelayedTask(
        FROM_HERE,
        request->quit_closure,
        base::TimeDelta::FromMilliseconds(kMaxWaitTimeForClipboardResponse));

    run_loop.Run();
  } else {
    // This occurs if PerformBlockingConvertSelection() is called during
    // shutdown and the PlatformEventSource has already been destroyed.
    base::TimeTicks start = base::TimeTicks::Now();
    while (!request->returned) {
      if (XPending(x_display_)) {
        XEvent event;
        XNextEvent(x_display_, &event);
        dispatcher_->DispatchEvent(&event);
      }
      base::TimeDelta wait_time = base::TimeTicks::Now() - start;
      if (wait_time.InMilliseconds() > kMaxWaitTimeForClipboardResponse)
        break;
    }
  }

  DCHECK(!pending_requests_.empty());
  DCHECK_EQ(request, pending_requests_.back());
  pending_requests_.pop_back();
}

SelectionRequestor::PendingRequest::PendingRequest(Atom target)
    : target(target),
      returned_property(None),
      returned(false) {
}

SelectionRequestor::PendingRequest::~PendingRequest() {
}

}  // namespace ui
