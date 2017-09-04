// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGraphicsContext3DProviderWrapper_h
#define WebGraphicsContext3DProviderWrapper_h

#include "public/platform/WebGraphicsContext3DProvider.h"
#include "wtf/WeakPtr.h"

namespace blink {

class PLATFORM_EXPORT WebGraphicsContext3DProviderWrapper {
 public:
  WebGraphicsContext3DProviderWrapper(
      std::unique_ptr<WebGraphicsContext3DProvider> provider)
      : m_contextProvider(std::move(provider)), m_weakPtrFactory(this) {}
  WeakPtr<WebGraphicsContext3DProviderWrapper> createWeakPtr() {
    return m_weakPtrFactory.createWeakPtr();
  }
  WebGraphicsContext3DProvider* contextProvider() {
    return m_contextProvider.get();
  }

 private:
  std::unique_ptr<WebGraphicsContext3DProvider> m_contextProvider;
  WeakPtrFactory<WebGraphicsContext3DProviderWrapper> m_weakPtrFactory;
};

}  // namespace blink

#endif  // WebGraphicsContext3DProviderWrapper_h
