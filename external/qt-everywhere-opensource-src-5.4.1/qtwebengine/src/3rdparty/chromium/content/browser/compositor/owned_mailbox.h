// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_OWNED_MAILBOX_H_
#define CONTENT_BROWSER_COMPOSITOR_OWNED_MAILBOX_H_

#include "base/memory/ref_counted.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"

namespace content {

class GLHelper;

// This class holds a texture id and gpu::Mailbox, and deletes the texture
// id when the object itself is destroyed. Should only be created if a GLHelper
// exists on the ImageTransportFactory.
class CONTENT_EXPORT OwnedMailbox : public base::RefCounted<OwnedMailbox>,
                                    public ImageTransportFactoryObserver {
 public:
  explicit OwnedMailbox(GLHelper* gl_helper);

  const gpu::MailboxHolder& holder() const { return mailbox_holder_; }
  const gpu::Mailbox& mailbox() const { return mailbox_holder_.mailbox; }
  uint32 texture_id() const { return texture_id_; }
  uint32 target() const { return mailbox_holder_.texture_target; }
  uint32 sync_point() const { return mailbox_holder_.sync_point; }
  void UpdateSyncPoint(uint32 sync_point);
  void Destroy();

 protected:
  virtual ~OwnedMailbox();

  // ImageTransportFactoryObserver implementation.
  virtual void OnLostResources() OVERRIDE;

 private:
  friend class base::RefCounted<OwnedMailbox>;

  uint32 texture_id_;
  gpu::MailboxHolder mailbox_holder_;
  GLHelper* gl_helper_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OWNED_MAILBOX_H_
