// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventDispatchResult_h
#define EventDispatchResult_h

namespace blink {

enum class DispatchEventResult {
  // Event was not canceled by event handler or default event handler.
  NotCanceled,
  // Event was canceled by event handler; i.e. a script handler calling
  // preventDefault.
  CanceledByEventHandler,
  // Event was canceled by the default event handler; i.e. executing the default
  // action.  This result should be used sparingly as it deviates from the DOM
  // Event Dispatch model. Default event handlers really shouldn't be invoked
  // inside of dispatch.
  CanceledByDefaultEventHandler,
  // Event was canceled but suppressed before dispatched to event handler.  This
  // result should be used sparingly; and its usage likely indicates there is
  // potential for a bug. Trusted events may return this code; but untrusted
  // events likely should always execute the event handler the developer intends
  // to execute.
  CanceledBeforeDispatch,
};

}  // namespace blink

#endif  // EventDispatchResult_h
