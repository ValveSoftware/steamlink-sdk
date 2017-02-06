// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_attachment_set.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_message_attachment.h"

#if defined(OS_POSIX)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif // OS_POSIX

namespace IPC {

namespace {

unsigned count_attachments_of_type(
    const std::vector<scoped_refptr<MessageAttachment>>& attachments,
    MessageAttachment::Type type) {
  unsigned count = 0;
  for (const scoped_refptr<MessageAttachment>& attachment : attachments) {
    if (attachment->GetType() == type)
      ++count;
  }
  return count;
}

}  // namespace

MessageAttachmentSet::MessageAttachmentSet()
    : consumed_descriptor_highwater_(0) {
}

MessageAttachmentSet::~MessageAttachmentSet() {
  if (consumed_descriptor_highwater_ == num_non_brokerable_attachments())
    return;

  // We close all the owning descriptors. If this message should have
  // been transmitted, then closing those with close flags set mirrors
  // the expected behaviour.
  //
  // If this message was received with more descriptors than expected
  // (which could a DOS against the browser by a rogue renderer) then all
  // the descriptors have their close flag set and we free all the extra
  // kernel resources.
  LOG(WARNING) << "MessageAttachmentSet destroyed with unconsumed descriptors: "
               << consumed_descriptor_highwater_ << "/" << num_descriptors();
}

unsigned MessageAttachmentSet::num_descriptors() const {
  return count_attachments_of_type(attachments_,
                                   MessageAttachment::TYPE_PLATFORM_FILE);
}

unsigned MessageAttachmentSet::num_mojo_handles() const {
  return count_attachments_of_type(attachments_,
                                   MessageAttachment::TYPE_MOJO_HANDLE);
}

unsigned MessageAttachmentSet::num_brokerable_attachments() const {
  return static_cast<unsigned>(brokerable_attachments_.size());
}

unsigned MessageAttachmentSet::num_non_brokerable_attachments() const {
  return static_cast<unsigned>(attachments_.size());
}

unsigned MessageAttachmentSet::size() const {
  return static_cast<unsigned>(attachments_.size() +
                               brokerable_attachments_.size());
}

bool MessageAttachmentSet::AddAttachment(
    scoped_refptr<MessageAttachment> attachment,
    size_t* index,
    bool* brokerable) {
#if defined(OS_POSIX)
  if (attachment->GetType() == MessageAttachment::TYPE_PLATFORM_FILE &&
      num_descriptors() == kMaxDescriptorsPerMessage) {
    DLOG(WARNING) << "Cannot add file descriptor. MessageAttachmentSet full.";
    return false;
  }
#endif

  switch (attachment->GetType()) {
    case MessageAttachment::TYPE_PLATFORM_FILE:
    case MessageAttachment::TYPE_MOJO_HANDLE:
      attachments_.push_back(attachment);
      *index = attachments_.size() - 1;
      *brokerable = false;
      return true;
    case MessageAttachment::TYPE_BROKERABLE_ATTACHMENT:
      BrokerableAttachment* brokerable_attachment =
          static_cast<BrokerableAttachment*>(attachment.get());
      scoped_refptr<BrokerableAttachment> a(brokerable_attachment);
      brokerable_attachments_.push_back(a);
      *index = brokerable_attachments_.size() - 1;
      *brokerable = true;
      return true;
  }
  return false;
}

bool MessageAttachmentSet::AddAttachment(
    scoped_refptr<MessageAttachment> attachment) {
  bool brokerable;
  size_t index;
  return AddAttachment(attachment, &index, &brokerable);
}

scoped_refptr<MessageAttachment>
MessageAttachmentSet::GetNonBrokerableAttachmentAt(unsigned index) {
  if (index >= num_non_brokerable_attachments()) {
    DLOG(WARNING) << "Accessing out of bound index:" << index << "/"
                  << num_non_brokerable_attachments();
    return scoped_refptr<MessageAttachment>();
  }

  // We should always walk the descriptors in order, so it's reasonable to
  // enforce this. Consider the case where a compromised renderer sends us
  // the following message:
  //
  //   ExampleMsg:
  //     num_fds:2 msg:FD(index = 1) control:SCM_RIGHTS {n, m}
  //
  // Here the renderer sent us a message which should have a descriptor, but
  // actually sent two in an attempt to fill our fd table and kill us. By
  // setting the index of the descriptor in the message to 1 (it should be
  // 0), we would record a highwater of 1 and then consider all the
  // descriptors to have been used.
  //
  // So we can either track of the use of each descriptor in a bitset, or we
  // can enforce that we walk the indexes strictly in order.
  //
  // There's one more wrinkle: When logging messages, we may reparse them. So
  // we have an exception: When the consumed_descriptor_highwater_ is at the
  // end of the array and index 0 is requested, we reset the highwater value.
  // TODO(morrita): This is absurd. This "wringle" disallow to introduce clearer
  // ownership model. Only client is NaclIPCAdapter. See crbug.com/415294
  if (index == 0 &&
      consumed_descriptor_highwater_ == num_non_brokerable_attachments()) {
    consumed_descriptor_highwater_ = 0;
  }

  if (index != consumed_descriptor_highwater_)
    return scoped_refptr<MessageAttachment>();

  consumed_descriptor_highwater_ = index + 1;

  return attachments_[index];
}

scoped_refptr<MessageAttachment>
MessageAttachmentSet::GetBrokerableAttachmentAt(unsigned index) {
  if (index >= num_brokerable_attachments()) {
    DLOG(WARNING) << "Accessing out of bound index:" << index << "/"
                  << num_brokerable_attachments();
    return scoped_refptr<MessageAttachment>();
  }

  scoped_refptr<BrokerableAttachment> brokerable_attachment(
      brokerable_attachments_[index]);
  return scoped_refptr<MessageAttachment>(brokerable_attachment.get());
}

void MessageAttachmentSet::CommitAllDescriptors() {
  attachments_.clear();
  consumed_descriptor_highwater_ = 0;
}

std::vector<scoped_refptr<IPC::BrokerableAttachment>>
MessageAttachmentSet::GetBrokerableAttachments() const {
  return brokerable_attachments_;
}

void MessageAttachmentSet::ReplacePlaceholderWithAttachment(
    const scoped_refptr<BrokerableAttachment>& attachment) {
  DCHECK_NE(BrokerableAttachment::PLACEHOLDER, attachment->GetBrokerableType());
  for (auto it = brokerable_attachments_.begin();
       it != brokerable_attachments_.end(); ++it) {
    if ((*it)->GetBrokerableType() == BrokerableAttachment::PLACEHOLDER &&
        (*it)->GetIdentifier() == attachment->GetIdentifier()) {
      *it = attachment;
      return;
    }
  }

  // This function should only be called if there is a placeholder ready to be
  // replaced.
  NOTREACHED();
}

#if defined(OS_POSIX)

void MessageAttachmentSet::PeekDescriptors(base::PlatformFile* buffer) const {
  for (size_t i = 0; i != attachments_.size(); ++i)
    buffer[i] = internal::GetPlatformFile(attachments_[i]);
}

bool MessageAttachmentSet::ContainsDirectoryDescriptor() const {
  struct stat st;

  for (auto i = attachments_.begin(); i != attachments_.end(); ++i) {
    if (fstat(internal::GetPlatformFile(*i), &st) == 0 && S_ISDIR(st.st_mode))
      return true;
  }

  return false;
}

void MessageAttachmentSet::ReleaseFDsToClose(
    std::vector<base::PlatformFile>* fds) {
  for (size_t i = 0; i < attachments_.size(); ++i) {
    internal::PlatformFileAttachment* file =
        static_cast<internal::PlatformFileAttachment*>(attachments_[i].get());
    if (file->Owns())
      fds->push_back(file->TakePlatformFile());
  }

  CommitAllDescriptors();
}

void MessageAttachmentSet::AddDescriptorsToOwn(const base::PlatformFile* buffer,
                                               unsigned count) {
  DCHECK(count <= kMaxDescriptorsPerMessage);
  DCHECK_EQ(num_descriptors(), 0u);
  DCHECK_EQ(consumed_descriptor_highwater_, 0u);

  attachments_.reserve(count);
  for (unsigned i = 0; i < count; ++i)
    AddAttachment(
        new internal::PlatformFileAttachment(base::ScopedFD(buffer[i])));
}

#endif  // OS_POSIX

}  // namespace IPC


