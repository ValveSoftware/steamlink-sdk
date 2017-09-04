// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/url_response_body_consumer.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/child/resource_dispatcher.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request_completion_status.h"
#include "content/public/child/request_peer.h"

namespace content {

class URLResponseBodyConsumer::ReceivedData final
    : public RequestPeer::ReceivedData {
 public:
  ReceivedData(const char* payload,
               int length,
               scoped_refptr<URLResponseBodyConsumer> consumer)
      : payload_(payload), length_(length), consumer_(consumer) {}

  ~ReceivedData() override { consumer_->Reclaim(length_); }

  const char* payload() const override { return payload_; }
  int length() const override { return length_; }
  // TODO(yhirano): These return incorrect values. Remove these from
  // ReceivedData before enabling Mojo-Loading.
  int encoded_data_length() const override { return length_; }
  int encoded_body_length() const override { return length_; }

 private:
  const char* const payload_;
  const uint32_t length_;

  scoped_refptr<URLResponseBodyConsumer> consumer_;

  DISALLOW_COPY_AND_ASSIGN(ReceivedData);
};

URLResponseBodyConsumer::URLResponseBodyConsumer(
    int request_id,
    ResourceDispatcher* resource_dispatcher,
    mojo::ScopedDataPipeConsumerHandle handle,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : request_id_(request_id),
      resource_dispatcher_(resource_dispatcher),
      handle_(std::move(handle)),
      handle_watcher_(task_runner),
      has_seen_end_of_data_(!handle_.is_valid()) {}

URLResponseBodyConsumer::~URLResponseBodyConsumer() {}

void URLResponseBodyConsumer::Start(base::SingleThreadTaskRunner* task_runner) {
  if (has_been_cancelled_)
    return;
  handle_watcher_.Start(
      handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::Bind(&URLResponseBodyConsumer::OnReadable, base::Unretained(this)));
  task_runner->PostTask(
      FROM_HERE, base::Bind(&URLResponseBodyConsumer::OnReadable, AsWeakPtr(),
                            MOJO_RESULT_OK));
}

void URLResponseBodyConsumer::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  if (has_been_cancelled_)
    return;
  has_received_completion_ = true;
  completion_status_ = status;
  NotifyCompletionIfAppropriate();
}

void URLResponseBodyConsumer::Cancel() {
  has_been_cancelled_ = true;
  handle_watcher_.Cancel();
}

void URLResponseBodyConsumer::Reclaim(uint32_t size) {
  MojoResult result = mojo::EndReadDataRaw(handle_.get(), size);
  DCHECK_EQ(MOJO_RESULT_OK, result);
}

void URLResponseBodyConsumer::OnReadable(MojoResult unused) {
  if (has_been_cancelled_ || has_seen_end_of_data_)
    return;

  // Protect |this| as RequestPeer::OnReceivedData may call deref.
  scoped_refptr<URLResponseBodyConsumer> protect(this);

  // TODO(yhirano): Suppress notification when deferred.
  while (!has_been_cancelled_) {
    const void* buffer = nullptr;
    uint32_t available = 0;
    MojoResult result = mojo::BeginReadDataRaw(
        handle_.get(), &buffer, &available, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT)
      return;
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      has_seen_end_of_data_ = true;
      NotifyCompletionIfAppropriate();
      return;
    }
    if (result != MOJO_RESULT_OK) {
      completion_status_.error_code = net::ERR_FAILED;
      has_seen_end_of_data_ = true;
      has_received_completion_ = true;
      NotifyCompletionIfAppropriate();
      return;
    }
    ResourceDispatcher::PendingRequestInfo* request_info =
        resource_dispatcher_->GetPendingRequestInfo(request_id_);
    DCHECK(request_info);
    request_info->peer->OnReceivedData(base::MakeUnique<ReceivedData>(
        static_cast<const char*>(buffer), available, this));
  }
}

void URLResponseBodyConsumer::NotifyCompletionIfAppropriate() {
  if (has_been_cancelled_)
    return;
  if (!has_received_completion_ || !has_seen_end_of_data_)
    return;
  // Cancel this instance in order not to notify twice.
  Cancel();

  resource_dispatcher_->OnMessageReceived(
      ResourceMsg_RequestComplete(request_id_, completion_status_));
  // |this| may be deleted.
}

}  // namespace content
