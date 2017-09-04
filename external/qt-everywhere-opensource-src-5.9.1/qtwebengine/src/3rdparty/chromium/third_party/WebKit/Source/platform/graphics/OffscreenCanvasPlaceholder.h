// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasPlaceholder_h
#define OffscreenCanvasPlaceholder_h

#include "platform/PlatformExport.h"
#include "wtf/RefPtr.h"
#include "wtf/WeakPtr.h"
#include <memory>

namespace blink {

class Image;
class IntSize;
class OffscreenCanvasFrameDispatcher;
class WebTaskRunner;

class PLATFORM_EXPORT OffscreenCanvasPlaceholder {
 public:
  ~OffscreenCanvasPlaceholder();

  void setPlaceholderFrame(RefPtr<Image>,
                           WeakPtr<OffscreenCanvasFrameDispatcher>,
                           std::unique_ptr<WebTaskRunner>,
                           unsigned resourceId);
  void releasePlaceholderFrame();

  virtual void setSize(const IntSize&) = 0;

  static OffscreenCanvasPlaceholder* getPlaceholderById(unsigned placeholderId);

  void registerPlaceholder(unsigned placeholderId);
  void unregisterPlaceholder();
  const RefPtr<Image>& placeholderFrame() const { return m_placeholderFrame; }

 private:
  bool isPlaceholderRegistered() const {
    return m_placeholderId != kNoPlaceholderId;
  }

  RefPtr<Image> m_placeholderFrame;
  WeakPtr<OffscreenCanvasFrameDispatcher> m_frameDispatcher;
  std::unique_ptr<WebTaskRunner> m_frameDispatcherTaskRunner;
  unsigned m_placeholderFrameResourceId = 0;

  enum {
    kNoPlaceholderId = -1,
  };
  int m_placeholderId = kNoPlaceholderId;
};

}  // blink

#endif
