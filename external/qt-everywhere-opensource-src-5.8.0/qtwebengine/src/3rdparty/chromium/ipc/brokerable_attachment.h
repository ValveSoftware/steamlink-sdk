// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_BROKERABLE_ATTACHMENT_H_
#define IPC_BROKERABLE_ATTACHMENT_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "build/build_config.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_message_attachment.h"

namespace IPC {

// This subclass of MessageAttachment requires an AttachmentBroker to be
// attached to a Chrome IPC message.
class IPC_EXPORT BrokerableAttachment : public MessageAttachment {
 public:
  static const size_t kNonceSize = 16;
  // An id uniquely identifies an attachment sent via a broker.
  struct IPC_EXPORT AttachmentId {
    uint8_t nonce[kNonceSize];

    // Generates an AttachmentId with an unguessable, random nonce.
    static AttachmentId CreateIdWithRandomNonce();

    // Creates an AttachmentId with a zeroed nonce. This should only be used by
    // the IPC translation system, which requires that classes have a default
    // constructor.
    AttachmentId();

    // Constructs an AttachmentId from a buffer.
    AttachmentId(const char* start_address, size_t size);

    // Writes the nonce into a buffer.
    void SerializeToBuffer(char* start_address, size_t size);

    bool operator==(const AttachmentId& rhs) const {
      return std::equal(nonce, nonce + kNonceSize, rhs.nonce);
    }

    bool operator<(const AttachmentId& rhs) const {
      return std::lexicographical_compare(nonce, nonce + kNonceSize, rhs.nonce,
                                          rhs.nonce + kNonceSize);
    }
  };

  enum BrokerableType {
    PLACEHOLDER,
    WIN_HANDLE,
    MACH_PORT,
  };

  // The identifier is unique across all Chrome processes.
  AttachmentId GetIdentifier() const;

  // Whether the attachment still needs information from the broker before it
  // can be used.
  bool NeedsBrokering() const;

  // Returns TYPE_BROKERABLE_ATTACHMENT
  Type GetType() const override;

  virtual BrokerableType GetBrokerableType() const = 0;

// MessageAttachment override.
#if defined(OS_POSIX)
  base::PlatformFile TakePlatformFile() override;
#endif  // OS_POSIX

 protected:
  BrokerableAttachment();
  BrokerableAttachment(const AttachmentId& id);
  ~BrokerableAttachment() override;

 private:
  // This member uniquely identifies a BrokerableAttachment across all Chrome
  // processes.
  const AttachmentId id_;

  DISALLOW_COPY_AND_ASSIGN(BrokerableAttachment);
};

}  // namespace IPC

#endif  // IPC_BROKERABLE_ATTACHMENT_H_
