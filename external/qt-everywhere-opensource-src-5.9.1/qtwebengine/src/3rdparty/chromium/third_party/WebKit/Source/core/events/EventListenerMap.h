/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef EventListenerMap_h
#define EventListenerMap_h

#include "core/CoreExport.h"
#include "core/events/AddEventListenerOptionsResolved.h"
#include "core/events/EventListenerOptions.h"
#include "core/events/RegisteredEventListener.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class EventTarget;

using EventListenerVector = HeapVector<RegisteredEventListener, 1>;

class CORE_EXPORT EventListenerMap {
  WTF_MAKE_NONCOPYABLE(EventListenerMap);
  DISALLOW_NEW();

 public:
  EventListenerMap();

  bool isEmpty() const { return m_entries.isEmpty(); }
  bool contains(const AtomicString& eventType) const;
  bool containsCapturing(const AtomicString& eventType) const;

  void clear();
  bool add(const AtomicString& eventType,
           EventListener*,
           const AddEventListenerOptionsResolved&,
           RegisteredEventListener* registeredListener);
  bool remove(const AtomicString& eventType,
              const EventListener*,
              const EventListenerOptions&,
              size_t* indexOfRemovedListener,
              RegisteredEventListener* registeredListener);
  EventListenerVector* find(const AtomicString& eventType);
  Vector<AtomicString> eventTypes() const;

  DECLARE_TRACE();

 private:
  friend class EventListenerIterator;

  void checkNoActiveIterators();

  // We use HeapVector instead of HeapHashMap because
  //  - HeapVector is much more space efficient than HeapHashMap.
  //  - An EventTarget rarely has event listeners for many event types, and
  //    HeapVector is faster in such cases.
  HeapVector<std::pair<AtomicString, Member<EventListenerVector>>, 2> m_entries;

#if DCHECK_IS_ON()
  int m_activeIteratorCount = 0;
#endif
};

class EventListenerIterator {
  WTF_MAKE_NONCOPYABLE(EventListenerIterator);
  STACK_ALLOCATED();

 public:
  explicit EventListenerIterator(EventTarget*);
#if DCHECK_IS_ON()
  ~EventListenerIterator();
#endif

  EventListener* nextListener();

 private:
  // This cannot be a Member because it is pointing to a part of object.
  // TODO(haraken): Use Member<EventTarget> instead of EventListenerMap*.
  EventListenerMap* m_map;
  unsigned m_entryIndex;
  unsigned m_index;
};

#if !DCHECK_IS_ON()
inline void EventListenerMap::checkNoActiveIterators() {}
#endif

}  // namespace blink

#endif  // EventListenerMap_h
