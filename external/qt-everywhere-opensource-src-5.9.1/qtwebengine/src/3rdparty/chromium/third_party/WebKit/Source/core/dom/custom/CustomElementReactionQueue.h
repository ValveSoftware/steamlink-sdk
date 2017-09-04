// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementReactionQueue_h
#define CustomElementReactionQueue_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CustomElementReaction;
class Element;

class CORE_EXPORT CustomElementReactionQueue final
    : public GarbageCollectedFinalized<CustomElementReactionQueue> {
  WTF_MAKE_NONCOPYABLE(CustomElementReactionQueue);

 public:
  CustomElementReactionQueue();
  ~CustomElementReactionQueue();

  DECLARE_TRACE();

  void add(CustomElementReaction*);
  void invokeReactions(Element*);
  bool isEmpty() { return m_reactions.isEmpty(); }
  void clear();

 private:
  HeapVector<Member<CustomElementReaction>, 1> m_reactions;
  size_t m_index;
};

}  // namespace blink

#endif  // CustomElementReactionQueue_h
