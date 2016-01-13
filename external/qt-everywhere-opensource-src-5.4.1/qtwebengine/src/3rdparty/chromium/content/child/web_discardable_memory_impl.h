// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_DISCARDABLE_MEMORY_IMPL_H_
#define CONTENT_CHILD_WEB_DISCARDABLE_MEMORY_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebDiscardableMemory.h"

namespace blink {
class WebDiscardableMemory;
}

namespace content {

// Implementation of WebDiscardableMemory that is responsible for allocating
// discardable memory.
class WebDiscardableMemoryImpl : public blink::WebDiscardableMemory {
 public:
  virtual ~WebDiscardableMemoryImpl();

  static scoped_ptr<WebDiscardableMemoryImpl> CreateLockedMemory(size_t size);

  // blink::WebDiscardableMemory:
  virtual bool lock();
  virtual void unlock();
  virtual void* data();

 private:
  WebDiscardableMemoryImpl(scoped_ptr<base::DiscardableMemory> memory);

  scoped_ptr<base::DiscardableMemory> discardable_;

  DISALLOW_COPY_AND_ASSIGN(WebDiscardableMemoryImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_DISCARDABLE_MEMORY_IMPL_H_
