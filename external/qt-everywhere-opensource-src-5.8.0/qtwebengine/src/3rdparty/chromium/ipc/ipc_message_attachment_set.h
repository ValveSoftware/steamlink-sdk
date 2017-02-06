// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_ATTACHMENT_SET_H_
#define IPC_IPC_MESSAGE_ATTACHMENT_SET_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ipc/ipc_export.h"

#if defined(OS_POSIX)
#include "base/files/file.h"
#endif

namespace IPC {

class BrokerableAttachment;
class MessageAttachment;

// -----------------------------------------------------------------------------
// A MessageAttachmentSet is an ordered set of MessageAttachment objects
// associated with an IPC message. There are three types of MessageAttachments:
//   1) TYPE_PLATFORM_FILE is transmitted over the Channel's underlying
//   UNIX domain socket
//   2) TYPE_MOJO_HANDLE is transmitted over the Mojo MessagePipe.
//   3) TYPE_BROKERABLE_ATTACHMENT is transmitted by the Attachment Broker.
// Any given IPC Message can have attachments of type (1) or (2), but not both.
// These are stored in |attachments_|. Attachments of type (3) are stored in
// |brokerable_attachments_|.
//
// To produce a deterministic ordering, all attachments in |attachments_| are
// considered to come before those in |brokerable_attachments_|. These
// attachments are transmitted across different communication channels, and
// multiplexed by the receiver, so ordering between them cannot be guaranteed.
//
// -----------------------------------------------------------------------------
class IPC_EXPORT MessageAttachmentSet
    : public base::RefCountedThreadSafe<MessageAttachmentSet> {
 public:
  MessageAttachmentSet();

  // Return the number of attachments
  unsigned size() const;
  // Return the number of file descriptors
  unsigned num_descriptors() const;
  // Return the number of mojo handles in the attachment set
  unsigned num_mojo_handles() const;
  // Return the number of brokerable attachments in the attachment set.
  unsigned num_brokerable_attachments() const;
  // Return the number of non-brokerable attachments in the attachment set.
  unsigned num_non_brokerable_attachments() const;

  // Return true if no unconsumed descriptors remain
  bool empty() const { return 0 == size(); }

  // Returns whether the attachment was successfully added.
  // |index| is an output variable. On success, it contains the index of the
  // newly added attachment.
  // |brokerable| is an output variable. On success, it describes which vector
  // the attachment was added to.
  bool AddAttachment(scoped_refptr<MessageAttachment> attachment,
                     size_t* index,
                     bool* brokerable);

  // Similar to the above method, but without output variables.
  bool AddAttachment(scoped_refptr<MessageAttachment> attachment);

  // Take the nth non-brokerable attachment from the beginning of the vector,
  // Code using this /must/ access the attachments in order, and must do it at
  // most once.
  //
  // This interface is designed for the deserialising code as it doesn't
  // support close flags.
  //   returns: an attachment, or nullptr on error
  scoped_refptr<MessageAttachment> GetNonBrokerableAttachmentAt(unsigned index);

  // Similar to GetNonBrokerableAttachmentAt, but there are no ordering
  // requirements.
  scoped_refptr<MessageAttachment> GetBrokerableAttachmentAt(unsigned index);

  // This must be called after transmitting the descriptors returned by
  // PeekDescriptors. It marks all the non-brokerable descriptors as consumed
  // and closes those which are auto-close.
  void CommitAllDescriptors();

  // Returns a vector of all brokerable attachments.
  std::vector<scoped_refptr<IPC::BrokerableAttachment>>
  GetBrokerableAttachments() const;

  // Replaces a placeholder brokerable attachment with |attachment|, matching
  // them by their id.
  void ReplacePlaceholderWithAttachment(
      const scoped_refptr<BrokerableAttachment>& attachment);

#if defined(OS_POSIX)
  // This is the maximum number of descriptors per message. We need to know this
  // because the control message kernel interface has to be given a buffer which
  // is large enough to store all the descriptor numbers. Otherwise the kernel
  // tells us that it truncated the control data and the extra descriptors are
  // lost.
  //
  // In debugging mode, it's a fatal error to try and add more than this number
  // of descriptors to a MessageAttachmentSet.
  static const size_t kMaxDescriptorsPerMessage = 7;

  // ---------------------------------------------------------------------------
  // Interfaces for transmission...

  // Fill an array with file descriptors without 'consuming' them.
  // CommitAllDescriptors must be called after these descriptors have been
  // transmitted.
  //   buffer: (output) a buffer of, at least, size() integers.
  void PeekDescriptors(base::PlatformFile* buffer) const;
  // Returns true if any contained file descriptors appear to be handles to a
  // directory.
  bool ContainsDirectoryDescriptor() const;
  // Fetch all filedescriptors with the "auto close" property. Used instead of
  // CommitAllDescriptors() when closing must be handled manually.
  void ReleaseFDsToClose(std::vector<base::PlatformFile>* fds);

  // ---------------------------------------------------------------------------

  // ---------------------------------------------------------------------------
  // Interfaces for receiving...

  // Set the contents of the set from the given buffer. This set must be empty
  // before calling. The auto-close flag is set on all the descriptors so that
  // unconsumed descriptors are closed on destruction.
  void AddDescriptorsToOwn(const base::PlatformFile* buffer, unsigned count);

#endif  // OS_POSIX

  // ---------------------------------------------------------------------------

 private:
  friend class base::RefCountedThreadSafe<MessageAttachmentSet>;

  ~MessageAttachmentSet();

  // All elements either have type TYPE_PLATFORM_FILE or TYPE_MOJO_HANDLE.
  std::vector<scoped_refptr<MessageAttachment>> attachments_;

  // All elements have type TYPE_BROKERABLE_ATTACHMENT.
  std::vector<scoped_refptr<BrokerableAttachment>> brokerable_attachments_;

  // This contains the index of the next descriptor which should be consumed.
  // It's used in a couple of ways. Firstly, at destruction we can check that
  // all the descriptors have been read (with GetNthDescriptor). Secondly, we
  // can check that they are read in order.
  mutable unsigned consumed_descriptor_highwater_;

  DISALLOW_COPY_AND_ASSIGN(MessageAttachmentSet);
};

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_ATTACHMENT_SET_H_
