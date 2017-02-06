// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_QUEUE_H_
#define CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_QUEUE_H_

#include <deque>
#include <memory>

namespace content {

enum class WebInputEventQueueState { ITEM_PENDING, ITEM_NOT_PENDING };

// WebInputEventQueue is a coalescing queue with the addition of a state
// variable that represents whether an item is pending to be processed.
// The desired usage sending with this queue is:
//   if (queue.state() == WebInputEventQueueState::ITEM_PENDING) {
//     queue.Queue(T);
//   } else {
//     send T
//     queue.set_state(WebInputEventQueueState::ITEM_PENDING);
//   }
//
// Processing the event response:
//  if (!queue.empty()) {
//    T = queue.Pop();
//    send T now
//  } else {
//    queue.set_state(WebInputEventQueueState::ITEM_NOT_PENDING);
//  }
//
template <typename T>
class WebInputEventQueue {
 public:
  WebInputEventQueue() : state_(WebInputEventQueueState::ITEM_NOT_PENDING) {}

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events.
  void Queue(const T& event) {
    if (!queue_.empty()) {
      std::unique_ptr<T>& last_event = queue_.back();
      if (last_event->CanCoalesceWith(event)) {
        last_event->CoalesceWith(event);
        return;
      }
    }
    queue_.emplace_back(std::unique_ptr<T>(new T(event)));
  }

  std::unique_ptr<T> Pop() {
    std::unique_ptr<T> result;
    if (!queue_.empty()) {
      result.reset(queue_.front().release());
      queue_.pop_front();
    }
    return result;
  }

  bool empty() const { return queue_.empty(); }

  size_t size() const { return queue_.size(); }

  void set_state(WebInputEventQueueState state) { state_ = state; }

  WebInputEventQueueState state() const WARN_UNUSED_RESULT { return state_; }

 private:
  typedef std::deque<std::unique_ptr<T>> EventQueue;
  EventQueue queue_;
  WebInputEventQueueState state_;

  DISALLOW_COPY_AND_ASSIGN(WebInputEventQueue);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_QUEUE_H_
