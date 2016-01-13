// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/aec_dump_message_filter.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/common/media/aec_dump_messages.h"
#include "content/renderer/media/webrtc_logging.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_sender.h"

namespace {
const int kInvalidDelegateId = -1;
}

namespace content {

AecDumpMessageFilter* AecDumpMessageFilter::g_filter = NULL;

AecDumpMessageFilter::AecDumpMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
    const scoped_refptr<base::MessageLoopProxy>& main_message_loop)
    : sender_(NULL),
      delegate_id_counter_(0),
      io_message_loop_(io_message_loop),
      main_message_loop_(main_message_loop) {
  DCHECK(!g_filter);
  g_filter = this;
}

AecDumpMessageFilter::~AecDumpMessageFilter() {
  DCHECK_EQ(g_filter, this);
  g_filter = NULL;
}

// static
scoped_refptr<AecDumpMessageFilter> AecDumpMessageFilter::Get() {
  return g_filter;
}

void AecDumpMessageFilter::AddDelegate(
    AecDumpMessageFilter::AecDumpDelegate* delegate) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(delegate);
  DCHECK_EQ(kInvalidDelegateId, GetIdForDelegate(delegate));

  int id = delegate_id_counter_++;
  delegates_[id] = delegate;

  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &AecDumpMessageFilter::RegisterAecDumpConsumer,
          this,
          id));
}

void AecDumpMessageFilter::RemoveDelegate(
    AecDumpMessageFilter::AecDumpDelegate* delegate) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(delegate);

  int id = GetIdForDelegate(delegate);
  DCHECK_NE(kInvalidDelegateId, id);
  delegates_.erase(id);

  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &AecDumpMessageFilter::UnregisterAecDumpConsumer,
          this,
          id));
}

void AecDumpMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (sender_)
    sender_->Send(message);
  else
    delete message;
}

void AecDumpMessageFilter::RegisterAecDumpConsumer(int id) {
  Send(new AecDumpMsg_RegisterAecDumpConsumer(id));
}

void AecDumpMessageFilter::UnregisterAecDumpConsumer(int id) {
  Send(new AecDumpMsg_UnregisterAecDumpConsumer(id));
}

bool AecDumpMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AecDumpMessageFilter, message)
    IPC_MESSAGE_HANDLER(AecDumpMsg_EnableAecDump, OnEnableAecDump)
    IPC_MESSAGE_HANDLER(AecDumpMsg_DisableAecDump, OnDisableAecDump)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AecDumpMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = sender;
}

void AecDumpMessageFilter::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  // Once removed, a filter will not be used again.  At this time the
  // observer must be notified so it releases its reference.
  OnChannelClosing();
}

void AecDumpMessageFilter::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = NULL;
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &AecDumpMessageFilter::DoChannelClosingOnDelegates,
          this));
}

void AecDumpMessageFilter::OnEnableAecDump(
    int id,
    IPC::PlatformFileForTransit file_handle) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &AecDumpMessageFilter::DoEnableAecDump,
          this,
          id,
          file_handle));
}

void AecDumpMessageFilter::OnDisableAecDump() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &AecDumpMessageFilter::DoDisableAecDump,
          this));
}

void AecDumpMessageFilter::DoEnableAecDump(
    int id,
    IPC::PlatformFileForTransit file_handle) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DelegateMap::iterator it = delegates_.find(id);
  if (it != delegates_.end()) {
    it->second->OnAecDumpFile(file_handle);
  } else {
    // Delegate has been removed, we must close the file.
    base::File file = IPC::PlatformFileForTransitToFile(file_handle);
    DCHECK(file.IsValid());
    file.Close();
  }
}

void AecDumpMessageFilter::DoDisableAecDump() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  for (DelegateMap::iterator it = delegates_.begin();
       it != delegates_.end(); ++it) {
    it->second->OnDisableAecDump();
  }
}

void AecDumpMessageFilter::DoChannelClosingOnDelegates() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  for (DelegateMap::iterator it = delegates_.begin();
       it != delegates_.end(); ++it) {
    it->second->OnIpcClosing();
  }
  delegates_.clear();
}

int AecDumpMessageFilter::GetIdForDelegate(
    AecDumpMessageFilter::AecDumpDelegate* delegate) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  for (DelegateMap::iterator it = delegates_.begin();
       it != delegates_.end(); ++it) {
    if (it->second == delegate)
      return it->first;
  }
  return kInvalidDelegateId;
}

}  // namespace content
