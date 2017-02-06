// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_unprivileged.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_endpoint.h"

#if defined(OS_WIN)
#include "ipc/attachment_broker_unprivileged_win.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "ipc/attachment_broker_unprivileged_mac.h"
#endif

namespace IPC {

namespace {

// On platforms that support attachment brokering, returns a new instance of
// a platform-specific attachment broker. Otherwise returns |nullptr|.
// The caller takes ownership of the newly created instance, and is
// responsible for ensuring that the attachment broker lives longer than
// every IPC::Channel. The new instance automatically registers itself as the
// global attachment broker.
std::unique_ptr<AttachmentBrokerUnprivileged> CreateBroker() {
#if defined(OS_WIN)
  return base::WrapUnique(new IPC::AttachmentBrokerUnprivilegedWin);
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  return base::WrapUnique(new IPC::AttachmentBrokerUnprivilegedMac);
#else
  return nullptr;
#endif
}

// This class is wrapped in a LazyInstance to ensure that its constructor is
// only called once. The constructor creates an attachment broker and sets it as
// the global broker.
class AttachmentBrokerMakeOnce {
 public:
  AttachmentBrokerMakeOnce() {
    // Single process tests can cause an attachment broker to already exist.
    if (AttachmentBroker::GetGlobal())
      return;
    attachment_broker_ = CreateBroker();
  }

 private:
  std::unique_ptr<IPC::AttachmentBrokerUnprivileged> attachment_broker_;
};

base::LazyInstance<AttachmentBrokerMakeOnce>::Leaky
    g_attachment_broker_make_once = LAZY_INSTANCE_INITIALIZER;

}  // namespace

AttachmentBrokerUnprivileged::AttachmentBrokerUnprivileged()
    : sender_(nullptr) {
  IPC::AttachmentBroker::SetGlobal(this);
}

AttachmentBrokerUnprivileged::~AttachmentBrokerUnprivileged() {
  IPC::AttachmentBroker::SetGlobal(nullptr);
}

// static
void AttachmentBrokerUnprivileged::CreateBrokerIfNeeded() {
  g_attachment_broker_make_once.Get();
}

void AttachmentBrokerUnprivileged::RegisterBrokerCommunicationChannel(
    Endpoint* endpoint) {
  DCHECK(endpoint);
  DCHECK(!sender_);
  sender_ = endpoint;
  endpoint->SetAttachmentBrokerEndpoint(true);
}

void AttachmentBrokerUnprivileged::DeregisterBrokerCommunicationChannel(
    Endpoint* endpoint) {
  DCHECK(endpoint);
  DCHECK_EQ(endpoint, sender_);
  sender_ = nullptr;
}

bool AttachmentBrokerUnprivileged::IsPrivilegedBroker() {
  return false;
}

void AttachmentBrokerUnprivileged::LogError(UMAError error) {
  UMA_HISTOGRAM_ENUMERATION(
      "IPC.AttachmentBrokerUnprivileged.BrokerAttachmentError", error,
      ERROR_MAX);
}

}  // namespace IPC
