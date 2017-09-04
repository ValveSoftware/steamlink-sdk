// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectedFrames_h
#define InspectedFrames_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT InspectedFrames final
    : public GarbageCollected<InspectedFrames> {
  WTF_MAKE_NONCOPYABLE(InspectedFrames);

 public:
  class CORE_EXPORT Iterator {
    STACK_ALLOCATED();

   public:
    Iterator operator++(int);
    Iterator& operator++();
    bool operator==(const Iterator& other);
    bool operator!=(const Iterator& other);
    LocalFrame* operator*() { return m_current; }
    LocalFrame* operator->() { return m_current; }

   private:
    friend class InspectedFrames;
    Iterator(LocalFrame* root, LocalFrame* current);
    Member<LocalFrame> m_root;
    Member<LocalFrame> m_current;
  };

  static InspectedFrames* create(LocalFrame* root) {
    return new InspectedFrames(root);
  }

  LocalFrame* root() { return m_root; }
  bool contains(LocalFrame*) const;
  LocalFrame* frameWithSecurityOrigin(const String& originRawString);
  Iterator begin();
  Iterator end();

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit InspectedFrames(LocalFrame*);

  Member<LocalFrame> m_root;
};

}  // namespace blink

#endif  // InspectedFrames_h
