// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_RESOURCE_SCHEDULING_FILTER_H_
#define CONTENT_CHILD_RESOURCE_SCHEDULING_FILTER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"

namespace content {
class ResourceDispatcher;

// This filter is used to dispatch resource messages on a specific
// SingleThreadTaskRunner to facilitate task scheduling.
class CONTENT_EXPORT ResourceSchedulingFilter : public IPC::MessageFilter {
 public:
  ResourceSchedulingFilter(const scoped_refptr<base::SingleThreadTaskRunner>&
                               main_thread_task_runner,
                           ResourceDispatcher* resource_dispatcher);

  // IPC::MessageFilter overrides:
  bool OnMessageReceived(const IPC::Message& message) override;

  bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const override;

  // Sets the task runner associated with request messages with |id|.
  void SetRequestIdTaskRunner(
      int id,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Removes the task runner associated with |id|.
  void ClearRequestIdTaskRunner(int id);

  void DispatchMessage(const IPC::Message& message);

 private:
  ~ResourceSchedulingFilter() override;

  using RequestIdToTaskRunnerMap =
      std::map<int, scoped_refptr<base::SingleThreadTaskRunner>>;

  // This lock guards |request_id_to_task_runner_map_|
  base::Lock request_id_to_task_runner_map_lock_;
  RequestIdToTaskRunnerMap request_id_to_task_runner_map_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  base::WeakPtr<ResourceDispatcher> resource_dispatcher_;
  base::WeakPtrFactory<ResourceSchedulingFilter> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceSchedulingFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_RESOURCE_SCHEDULING_FILTER_H_
