// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_QUEUE_H_
#define CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_QUEUE_H_

#include <deque>
#include <memory>

namespace content {

// WebInputEventQueue is a coalescing queue. It will examine
// the current events in the queue and will attempt to coalesce with
// the last event of the same class type.
template <typename T>
class WebInputEventQueue {
 public:
  WebInputEventQueue() {}

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events.
  void Queue(std::unique_ptr<T> event) {
    for (auto last_event_iter = queue_.rbegin();
         last_event_iter != queue_.rend(); ++last_event_iter) {
      if (!(*last_event_iter)->event().isSameEventClass(event->event())) {
        continue;
      }

      if ((*last_event_iter)->CanCoalesceWith(*event.get())) {
        (*last_event_iter)->CoalesceWith(*event.get());
        return;
      }
      break;
    }
    queue_.emplace_back(std::move(event));
  }

  const std::unique_ptr<T>& front() const { return queue_.front(); }
  const std::unique_ptr<T>& at(size_t pos) const { return queue_.at(pos); }

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

 private:
  typedef std::deque<std::unique_ptr<T>> EventQueue;
  EventQueue queue_;

  DISALLOW_COPY_AND_ASSIGN(WebInputEventQueue);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_QUEUE_H_
