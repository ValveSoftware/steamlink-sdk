// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"
#include "webkit/common/gpu/webgraphicscontext3d_impl.h"

namespace gpu {

class ContextSupport;
class TransferBuffer;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
class GLES2Interface;
}
}

namespace content {
class GpuChannelHost;

const size_t kDefaultCommandBufferSize = 1024 * 1024;
const size_t kDefaultStartTransferBufferSize = 1 * 1024 * 1024;
const size_t kDefaultMinTransferBufferSize = 1 * 256 * 1024;
const size_t kDefaultMaxTransferBufferSize = 16 * 1024 * 1024;

class WebGraphicsContext3DCommandBufferImpl
    : public webkit::gpu::WebGraphicsContext3DImpl {
 public:
  enum MappedMemoryReclaimLimit {
    kNoLimit = 0,
  };

  struct CONTENT_EXPORT SharedMemoryLimits {
    SharedMemoryLimits();

    size_t command_buffer_size;
    size_t start_transfer_buffer_size;
    size_t min_transfer_buffer_size;
    size_t max_transfer_buffer_size;
    size_t mapped_memory_reclaim_limit;
  };

  class ShareGroup : public base::RefCountedThreadSafe<ShareGroup> {
   public:
    ShareGroup();

    WebGraphicsContext3DCommandBufferImpl* GetAnyContextLocked() {
      // In order to ensure that the context returned is not removed while
      // in use, the share group's lock should be aquired before calling this
      // function.
      lock_.AssertAcquired();
      if (contexts_.empty())
        return NULL;
      return contexts_.front();
    }

    void AddContextLocked(WebGraphicsContext3DCommandBufferImpl* context) {
      lock_.AssertAcquired();
      contexts_.push_back(context);
    }

    void RemoveContext(WebGraphicsContext3DCommandBufferImpl* context) {
      base::AutoLock auto_lock(lock_);
      contexts_.erase(std::remove(contexts_.begin(), contexts_.end(), context),
          contexts_.end());
    }

    void RemoveAllContexts() {
      base::AutoLock auto_lock(lock_);
      contexts_.clear();
    }

    base::Lock& lock() {
      return lock_;
    }

   private:
    friend class base::RefCountedThreadSafe<ShareGroup>;
    virtual ~ShareGroup();

    std::vector<WebGraphicsContext3DCommandBufferImpl*> contexts_;
    base::Lock lock_;

    DISALLOW_COPY_AND_ASSIGN(ShareGroup);
  };

  WebGraphicsContext3DCommandBufferImpl(
      int surface_id,
      const GURL& active_url,
      GpuChannelHost* host,
      const Attributes& attributes,
      bool lose_context_when_out_of_memory,
      const SharedMemoryLimits& limits,
      WebGraphicsContext3DCommandBufferImpl* share_context);

  virtual ~WebGraphicsContext3DCommandBufferImpl();

  CommandBufferProxyImpl* GetCommandBufferProxy() {
    return command_buffer_.get();
  }

  CONTENT_EXPORT gpu::ContextSupport* GetContextSupport();

  gpu::gles2::GLES2Implementation* GetImplementation() {
    return real_gl_.get();
  }

  // Return true if GPU process reported context lost or there was a
  // problem communicating with the GPU process.
  bool IsCommandBufferContextLost();

  // Create & initialize a WebGraphicsContext3DCommandBufferImpl.  Return NULL
  // on any failure.
  static CONTENT_EXPORT WebGraphicsContext3DCommandBufferImpl*
      CreateOffscreenContext(
          GpuChannelHost* host,
          const WebGraphicsContext3D::Attributes& attributes,
          bool lose_context_when_out_of_memory,
          const GURL& active_url,
          const SharedMemoryLimits& limits,
          WebGraphicsContext3DCommandBufferImpl* share_context);

  size_t GetMappedMemoryLimit() {
    return mem_limits_.mapped_memory_reclaim_limit;
  }

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods

  // Must be called after initialize() and before any of the following methods.
  // Permanently binds to the first calling thread. Returns false if the
  // graphics context fails to create. Do not call from more than one thread.
  virtual bool makeContextCurrent();

  virtual bool isContextLost();

  virtual WGC3Denum getGraphicsResetStatusARB();

 private:
  // These are the same error codes as used by EGL.
  enum Error {
    SUCCESS               = 0x3000,
    BAD_ATTRIBUTE         = 0x3004,
    CONTEXT_LOST          = 0x300E
  };
  // WebGraphicsContext3DCommandBufferImpl configuration attributes. Those in
  // the 16-bit range are the same as used by EGL. Those outside the 16-bit
  // range are unique to Chromium. Attributes are matched using a closest fit
  // algorithm.
  // Changes to this enum should also be copied to
  // gpu/command_buffer/common/gles2_cmd_utils.cc and to
  // gpu/command_buffer/client/gl_in_process_context.cc
  enum Attribute {
    ALPHA_SIZE = 0x3021,
    BLUE_SIZE = 0x3022,
    GREEN_SIZE = 0x3023,
    RED_SIZE = 0x3024,
    DEPTH_SIZE = 0x3025,
    STENCIL_SIZE = 0x3026,
    SAMPLES = 0x3031,
    SAMPLE_BUFFERS = 0x3032,
    HEIGHT = 0x3056,
    WIDTH = 0x3057,
    NONE = 0x3038,  // Attrib list = terminator
    SHARE_RESOURCES = 0x10000,
    BIND_GENERATES_RESOURCES = 0x10001,
    FAIL_IF_MAJOR_PERF_CAVEAT = 0x10002,
    LOSE_CONTEXT_WHEN_OUT_OF_MEMORY = 0x10003,
  };

  // Initialize the underlying GL context. May be called multiple times; second
  // and subsequent calls are ignored. Must be called from the thread that is
  // going to use this object to issue GL commands (which might not be the main
  // thread).
  bool MaybeInitializeGL();

  bool InitializeCommandBuffer(bool onscreen,
      WebGraphicsContext3DCommandBufferImpl* share_context);

  void Destroy();

  // Create a CommandBufferProxy that renders directly to a view. The view and
  // the associated window must not be destroyed until the returned
  // CommandBufferProxy has been destroyed, otherwise the GPU process might
  // attempt to render to an invalid window handle.
  //
  // NOTE: on Mac OS X, this entry point is only used to set up the
  // accelerated compositor's output. On this platform, we actually pass
  // a gfx::PluginWindowHandle in place of the gfx::NativeViewId,
  // because the facility to allocate a fake PluginWindowHandle is
  // already in place. We could add more entry points and messages to
  // allocate both fake PluginWindowHandles and NativeViewIds and map
  // from fake NativeViewIds to PluginWindowHandles, but this seems like
  // unnecessary complexity at the moment.
  bool CreateContext(bool onscreen);

  virtual void OnGpuChannelLost();

  bool lose_context_when_out_of_memory_;
  blink::WebGraphicsContext3D::Attributes attributes_;

  bool visible_;

  // State needed by MaybeInitializeGL.
  scoped_refptr<GpuChannelHost> host_;
  int32 surface_id_;
  GURL active_url_;

  gfx::GpuPreference gpu_preference_;

  base::WeakPtrFactory<WebGraphicsContext3DCommandBufferImpl> weak_ptr_factory_;

  scoped_ptr<CommandBufferProxyImpl> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  scoped_ptr<gpu::TransferBuffer> transfer_buffer_;
  scoped_ptr<gpu::gles2::GLES2Implementation> real_gl_;
  scoped_ptr<gpu::gles2::GLES2Interface> trace_gl_;
  Error last_error_;
  SharedMemoryLimits mem_limits_;
  scoped_refptr<ShareGroup> share_group_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_
