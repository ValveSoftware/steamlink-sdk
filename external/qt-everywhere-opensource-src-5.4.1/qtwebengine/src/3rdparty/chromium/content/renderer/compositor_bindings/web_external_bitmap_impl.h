// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_BITMAP_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_BITMAP_IMPL_H_

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebExternalBitmap.h"

namespace base {
class SharedMemory;
}

namespace content {

typedef scoped_ptr<base::SharedMemory>(*SharedMemoryAllocationFunction)(size_t);

// Sets the function that this will use to allocate shared memory.
CONTENT_EXPORT void SetSharedMemoryAllocationFunction(
    SharedMemoryAllocationFunction);

class WebExternalBitmapImpl : public blink::WebExternalBitmap {
 public:
  CONTENT_EXPORT explicit WebExternalBitmapImpl();
  virtual ~WebExternalBitmapImpl();

  // blink::WebExternalBitmap implementation.
  virtual blink::WebSize size() OVERRIDE;
  virtual void setSize(blink::WebSize size) OVERRIDE;
  virtual uint8* pixels() OVERRIDE;

  base::SharedMemory* shared_memory() { return shared_memory_.get(); }

 private:
  scoped_ptr<base::SharedMemory> shared_memory_;
  blink::WebSize size_;

  DISALLOW_COPY_AND_ASSIGN(WebExternalBitmapImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_BITMAP_IMPL_H_

