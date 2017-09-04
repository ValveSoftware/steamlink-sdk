// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleSharingDepthScope_h
#define StyleSharingDepthScope_h

#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Node.h"

namespace blink {

// Maintains the style sharing depth in the style resolver.
class StyleSharingDepthScope final {
  STACK_ALLOCATED();

 public:
  explicit StyleSharingDepthScope(Node& parent)
      : m_resolver(parent.document().styleResolver()) {
    m_resolver->increaseStyleSharingDepth();
  }

  ~StyleSharingDepthScope() { m_resolver->decreaseStyleSharingDepth(); }

 private:
  Member<StyleResolver> m_resolver;
};

}  // namespace blink

#endif  // StyleSharingDepthScope_h
