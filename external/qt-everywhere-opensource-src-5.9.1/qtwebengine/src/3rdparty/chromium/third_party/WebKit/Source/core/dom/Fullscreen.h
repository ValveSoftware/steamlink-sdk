/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Fullscreen_h
#define Fullscreen_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Deque.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class LayoutFullScreen;
class ComputedStyle;

class CORE_EXPORT Fullscreen final
    : public GarbageCollectedFinalized<Fullscreen>,
      public Supplement<Document>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(Fullscreen);

 public:
  virtual ~Fullscreen();
  static const char* supplementName();
  static Fullscreen& from(Document&);
  static Fullscreen* fromIfExists(Document&);
  static Element* fullscreenElementFrom(Document&);
  static Element* fullscreenElementForBindingFrom(TreeScope&);
  static Element* currentFullScreenElementFrom(Document&);
  static Element* currentFullScreenElementForBindingFrom(Document&);
  static bool isCurrentFullScreenElement(const Element&);

  enum RequestType {
    // Element.requestFullscreen()
    UnprefixedRequest,
    // Element.webkitRequestFullscreen()/webkitRequestFullScreen() and
    // HTMLVideoElement.webkitEnterFullscreen()/webkitEnterFullScreen()
    PrefixedRequest,
  };

  // |forCrossProcessDescendant| is used in OOPIF scenarios and is set to
  // true when fullscreen is requested for an out-of-process descendant
  // element.
  static void requestFullscreen(Element&,
                                RequestType,
                                bool forCrossProcessDescendant = false);

  static void fullyExitFullscreen(Document&);
  static void exitFullscreen(Document&);

  static bool fullscreenEnabled(Document&);
  // TODO(foolip): The fullscreen element stack is modified synchronously in
  // requestFullscreen(), which is not per spec and means that
  // |fullscreenElement()| is not always the same as
  // |currentFullScreenElement()|, see https://crbug.com/402421.
  Element* fullscreenElement() const {
    return !m_fullscreenElementStack.isEmpty()
               ? m_fullscreenElementStack.last().first.get()
               : nullptr;
  }

  void didEnterFullscreenForElement(Element*);
  void didExitFullscreen();

  void setFullScreenLayoutObject(LayoutFullScreen*);
  LayoutFullScreen* fullScreenLayoutObject() const {
    return m_fullScreenLayoutObject;
  }
  void fullScreenLayoutObjectDestroyed();

  void elementRemoved(Element&);

  // Returns true if the current fullscreen element stack corresponds to a
  // container for an actual fullscreen element in a descendant
  // out-of-process iframe.
  bool forCrossProcessDescendant() { return m_forCrossProcessDescendant; }

  // Mozilla API
  // TODO(foolip): |currentFullScreenElement()| is a remnant from before the
  // fullscreen element stack. It is still maintained separately from the
  // stack and is is what the :-webkit-full-screen pseudo-class depends on. It
  // should be removed, see https://crbug.com/402421.
  Element* currentFullScreenElement() const {
    return m_currentFullScreenElement.get();
  }

  // ContextLifecycleObserver:
  void contextDestroyed() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  static Fullscreen* fromIfExistsSlow(Document&);

  explicit Fullscreen(Document&);

  Document* document();

  void clearFullscreenElementStack();
  void popFullscreenElementStack();
  void pushFullscreenElementStack(Element&, RequestType);

  void enqueueChangeEvent(Document&, RequestType);
  void enqueueErrorEvent(Element&, RequestType);
  void eventQueueTimerFired(TimerBase*);

  HeapVector<std::pair<Member<Element>, RequestType>> m_fullscreenElementStack;
  Member<Element> m_currentFullScreenElement;
  LayoutFullScreen* m_fullScreenLayoutObject;
  Timer<Fullscreen> m_eventQueueTimer;
  HeapDeque<Member<Event>> m_eventQueue;
  LayoutRect m_savedPlaceholderFrameRect;
  RefPtr<ComputedStyle> m_savedPlaceholderComputedStyle;

  // TODO(alexmos, dcheng): Currently, this assumes that if fullscreen was
  // entered for an element in an out-of-process iframe, then it's not
  // possible to re-enter fullscreen for a different element in this
  // document, since that requires a user gesture, which can't be obtained
  // since nothing in this document is visible, and since user gestures can't
  // be forwarded across processes. However, the latter assumption could
  // change if https://crbug.com/161068 is fixed so that cross-process
  // postMessage can carry user gestures.  If that happens, this should be
  // moved to be part of |m_fullscreenElementStack|.
  bool m_forCrossProcessDescendant;
};

inline Fullscreen* Fullscreen::fromIfExists(Document& document) {
  if (!document.hasFullscreenSupplement())
    return nullptr;
  return fromIfExistsSlow(document);
}

inline bool Fullscreen::isCurrentFullScreenElement(const Element& element) {
  if (Fullscreen* found = fromIfExists(element.document()))
    return found->currentFullScreenElement() == &element;
  return false;
}

}  // namespace blink

#endif  // Fullscreen_h
