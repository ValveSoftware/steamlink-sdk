// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextureHolder_h
#define TextureHolder_h

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT TextureHolder {
 public:
  virtual ~TextureHolder() {}

  // Methods overrided by all sub-classes
  virtual bool isSkiaTextureHolder() = 0;
  virtual bool isMailboxTextureHolder() = 0;
  virtual unsigned sharedContextId() = 0;
  virtual IntSize size() const = 0;
  virtual bool currentFrameKnownToBeOpaque(Image::MetadataMode) = 0;

  // Methods overrided by MailboxTextureHolder
  virtual gpu::Mailbox mailbox() {
    NOTREACHED();
    return gpu::Mailbox();
  }
  virtual gpu::SyncToken syncToken() {
    NOTREACHED();
    return gpu::SyncToken();
  }
  virtual void updateSyncToken(gpu::SyncToken) { NOTREACHED(); }

  // Methods overrided by SkiaTextureHolder
  virtual sk_sp<SkImage> skImage() {
    NOTREACHED();
    return nullptr;
  }
  virtual void setSharedContextId(unsigned) { NOTREACHED(); }
  virtual void setImageThread(WebThread*) { NOTREACHED(); }
  virtual void setImageThreadTaskRunner(std::unique_ptr<WebTaskRunner>) {
    NOTREACHED();
  }

  // Methods that have exactly the same impelmentation for all sub-classes
  bool wasTransferred() { return m_wasTransferred; }
  WebTaskRunner* textureThreadTaskRunner() {
    return m_textureThreadTaskRunner.get();
  }
  void setWasTransferred(bool flag) { m_wasTransferred = flag; }
  void setTextureThreadTaskRunner(std::unique_ptr<WebTaskRunner> taskRunner) {
    m_textureThreadTaskRunner = std::move(taskRunner);
  }

 private:
  // Wether the AcceleratedStaticBitmapImage that holds the |m_texture| was
  // transferred to another thread or not. Set to false when the
  // AcceleratedStaticBitmapImage remain on the same thread as it was craeted.
  bool m_wasTransferred = false;
  // Keep a clone of the WebTaskRunner. This is to handle the case where the
  // AcceleratedStaticBitmapImage was created on one thread and transferred to
  // another thread, and the original thread gone out of scope, and that we need
  // to clear the resouces associated with that AcceleratedStaticBitmapImage on
  // the original thread.
  std::unique_ptr<WebTaskRunner> m_textureThreadTaskRunner;
};

}  // namespace blink

#endif  // TextureHolder_h
