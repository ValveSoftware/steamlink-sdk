// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/resource_scheduling_filter.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "content/child/resource_dispatcher.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_start.h"
#include "third_party/WebKit/public/platform/WebTaskRunner.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace content {

namespace {
class DispatchMessageTask : public blink::WebTaskRunner::Task {
 public:
  DispatchMessageTask(
      base::WeakPtr<ResourceSchedulingFilter> resource_scheduling_filter,
      const IPC::Message& message)
    : resource_scheduling_filter_(resource_scheduling_filter),
      message_(message) {}

  void run() override {
    if (!resource_scheduling_filter_.get())
      return;
    resource_scheduling_filter_->DispatchMessage(message_);
  }

 private:
  base::WeakPtr<ResourceSchedulingFilter> resource_scheduling_filter_;
  const IPC::Message message_;
};
} // namespace

ResourceSchedulingFilter::ResourceSchedulingFilter(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    ResourceDispatcher* resource_dispatcher)
    : main_thread_task_runner_(main_thread_task_runner),
      resource_dispatcher_(resource_dispatcher),
      weak_ptr_factory_(this) {
  DCHECK(main_thread_task_runner_.get());
  DCHECK(resource_dispatcher_);
}

ResourceSchedulingFilter::~ResourceSchedulingFilter() {
}

bool ResourceSchedulingFilter::OnMessageReceived(const IPC::Message& message) {
  base::AutoLock lock(request_id_to_task_runner_map_lock_);
  int request_id;

  base::PickleIterator pickle_iterator(message);
  if (!pickle_iterator.ReadInt(&request_id)) {
    NOTREACHED() << "malformed resource message";
    return true;
  }
  // Dispatch the message on the request id specific task runner, if there is
  // one, or on the general main_thread_task_runner if there isn't.
  RequestIdToTaskRunnerMap::const_iterator iter =
      request_id_to_task_runner_map_.find(request_id);
  if (iter != request_id_to_task_runner_map_.end()) {
    // TODO(alexclarke): Find a way to let blink and chromium FROM_HERE coexist.
    iter->second->postTask(
        blink::WebTraceLocation(__FUNCTION__, __FILE__),
        new DispatchMessageTask(weak_ptr_factory_.GetWeakPtr(), message));
  } else {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ResourceSchedulingFilter::DispatchMessage,
                              weak_ptr_factory_.GetWeakPtr(), message));
  }
  return true;
}

void ResourceSchedulingFilter::SetRequestIdTaskRunner(
    int id,
    std::unique_ptr<blink::WebTaskRunner> web_task_runner) {
  base::AutoLock lock(request_id_to_task_runner_map_lock_);
  request_id_to_task_runner_map_.insert(
      std::make_pair(id, std::move(web_task_runner)));
}

void ResourceSchedulingFilter::ClearRequestIdTaskRunner(int id) {
  base::AutoLock lock(request_id_to_task_runner_map_lock_);
  request_id_to_task_runner_map_.erase(id);
}

bool ResourceSchedulingFilter::GetSupportedMessageClasses(
    std::vector<uint32_t>* supported_message_classes) const {
  supported_message_classes->push_back(ResourceMsgStart);
  return true;
}

void ResourceSchedulingFilter::DispatchMessage(const IPC::Message& message) {
  resource_dispatcher_->OnMessageReceived(message);
}

}  // namespace content
