// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler_filter.h"

#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_scheduler.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/page_transition_types.h"

namespace content {
namespace {
const uint32 kFilteredMessageClasses[] = {
  FrameMsgStart,
  ViewMsgStart,
};
}  // namespace

ResourceSchedulerFilter::ResourceSchedulerFilter(int child_id)
    : BrowserMessageFilter(
          kFilteredMessageClasses, arraysize(kFilteredMessageClasses)),
      child_id_(child_id) {
}

ResourceSchedulerFilter::~ResourceSchedulerFilter() {
}

bool ResourceSchedulerFilter::OnMessageReceived(const IPC::Message& message) {
  ResourceScheduler* scheduler =
      ResourceDispatcherHostImpl::Get()->scheduler();
  // scheduler can be NULL during shutdown, in which case it's ok to ignore the
  // renderer's messages.
  if (!scheduler)
    return false;

  switch (message.type()) {
    case FrameHostMsg_DidCommitProvisionalLoad::ID: {
      PickleIterator iter(message);
      FrameHostMsg_DidCommitProvisionalLoad_Params params;
      if (!IPC::ParamTraits<FrameHostMsg_DidCommitProvisionalLoad_Params>::Read(
          &message, &iter, &params)) {
        break;
      }
      if (PageTransitionIsMainFrame(params.transition) &&
          !params.was_within_same_page) {
        scheduler->OnNavigate(child_id_, message.routing_id());
      }
      break;
    }

    case ViewHostMsg_WillInsertBody::ID:
      scheduler->OnWillInsertBody(child_id_, message.routing_id());
      break;

    default:
      break;
  }

  return false;
}

}  // namespace content
